#include "bridgeclient.h"

#include <QDebug>

namespace {
const int    kRetryDelayMs   = 2000;
const double kMinCelsius     = -55.0;
const double kMaxCelsius     = 150.0;
}

BridgeClient::BridgeClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);

    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(kRetryDelayMs);

    connect(m_socket, &QTcpSocket::readyRead,
            this, &BridgeClient::onReadyRead);

    connect(m_socket, &QTcpSocket::connected,
            this, &BridgeClient::onSocketUp);

    connect(m_socket, &QTcpSocket::disconnected,
            this, &BridgeClient::onSocketDown);

    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));

    connect(m_retryTimer, &QTimer::timeout,
            this, &BridgeClient::retryConnect);
}

bool BridgeClient::isLinked() const
{
    return m_linked;
}

void BridgeClient::openLink(const QString &host, int port)
{
    if (host.isEmpty() || port < 1 || port > 65535) {
        emit linkFailed(QStringLiteral("Некорректный адрес или порт"));
        return;
    }

    m_host = host;
    m_port = static_cast<quint16>(port);
    m_keepTrying = true;

    m_retryTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();

    retryConnect();
}

void BridgeClient::closeLink()
{
    m_keepTrying = false;

    m_retryTimer->stop();
    m_socket->abort();

    updateLinked(false);
}

void BridgeClient::retryConnect()
{
    if (!m_keepTrying)
        return;

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        return;

    qDebug() << "Подключение к мосту:" << m_host << m_port;

    m_socket->connectToHost(m_host, m_port);
}

void BridgeClient::onSocketUp()
{
    qDebug() << "Связь с мостом установлена";

    m_retryTimer->stop();
    updateLinked(true);
}

void BridgeClient::onSocketDown()
{
    qDebug() << "Связь с мостом потеряна";

    updateLinked(false);

    if (m_keepTrying)
        m_retryTimer->start();
}

void BridgeClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)

    const QString reason = m_socket->errorString();

    qDebug() << "Ошибка TCP:" << reason;

    updateLinked(false);

    emit linkFailed(reason);

    if (m_keepTrying && !m_retryTimer->isActive())
        m_retryTimer->start();
}

void BridgeClient::onReadyRead()
{
    m_pending.append(m_socket->readAll());

    int newlinePos = m_pending.indexOf('\n');

    while (newlinePos >= 0) {
        const QByteArray line = m_pending.left(newlinePos).trimmed();

        m_pending.remove(0, newlinePos + 1);

        if (!line.isEmpty())
            handleLine(line);

        newlinePos = m_pending.indexOf('\n');
    }
}

void BridgeClient::handleLine(const QByteArray &line)
{
    bool parsed = false;
    const double celsius = line.toDouble(&parsed);

    if (!parsed) {
        qDebug() << "Некорректные данные от моста:" << line;
        return;
    }

    if (celsius < kMinCelsius || celsius > kMaxCelsius) {
        qDebug() << "Температура вне диапазона:" << celsius;
        return;
    }

    qDebug() << "Получена температура:" << celsius << "C";

    emit readingArrived(celsius);
}

void BridgeClient::updateLinked(bool value)
{
    if (m_linked == value)
        return;

    m_linked = value;

    emit linkStateChanged();
}
