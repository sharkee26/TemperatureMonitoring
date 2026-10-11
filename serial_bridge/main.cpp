// Windows-мост: читает температуру из COM-порта Arduino (9600 бод, строка на
// значение) и пересылает её по TCP клиенту Aurora (приложение TemperatureMonitoring).
//
// Запуск: serial_bridge.exe [COM-порт] [TCP-порт]
//         по умолчанию COM13 и 5555.

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QPointer>
#include <QTimer>
#include <QDebug>

#include <windows.h>

namespace {
const char  *kDefaultComPort  = "COM13";
const quint16 kDefaultTcpPort = 5555;
const int    kPollPeriodMs    = 20;
const double kMinCelsius      = -55.0;
const double kMaxCelsius      = 150.0;
}

// Открывает COM-порт и настраивает 9600 8N1 с короткими таймаутами чтения.
static HANDLE openSerialLine(const QString &name)
{
    const QString devicePath = QStringLiteral("\\\\.\\") + name;

    HANDLE h = CreateFileW(
        reinterpret_cast<LPCWSTR>(devicePath.utf16()),
        GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    DCB state = {};
    state.DCBlength = sizeof(DCB);

    if (!GetCommState(h, &state)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    state.BaudRate = CBR_9600;
    state.ByteSize = 8;
    state.Parity   = NOPARITY;
    state.StopBits = ONESTOPBIT;

    if (!SetCommState(h, &state)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS limits = {};
    limits.ReadIntervalTimeout         = 1;
    limits.ReadTotalTimeoutConstant    = 1;
    limits.ReadTotalTimeoutMultiplier  = 0;

    SetCommTimeouts(h, &limits);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return h;
}

// Печатает IPv4-адреса компьютера, чтобы их можно было указать в Aurora.
static void printLocalAddresses()
{
    qInfo() << "IPv4 адреса Windows:";

    const QList<QHostAddress> all = QNetworkInterface::allAddresses();

    for (const QHostAddress &a : all) {
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
            qInfo() << a.toString();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString comName = QString::fromLatin1(kDefaultComPort);
    quint16 tcpPort = kDefaultTcpPort;

    const QStringList args = app.arguments();

    if (args.size() >= 2)
        comName = args.at(1);

    if (args.size() >= 3) {
        bool ok = false;
        const int candidate = args.at(2).toInt(&ok);

        if (ok && candidate > 0 && candidate <= 65535)
            tcpPort = static_cast<quint16>(candidate);
    }

    HANDLE serialLine = openSerialLine(comName);

    if (serialLine == INVALID_HANDLE_VALUE) {
        qCritical() << "Не удалось открыть" << comName;
        qCritical() << "Код ошибки Windows:" << GetLastError();
        qCritical() << "Проверь, что Serial Monitor Arduino IDE закрыт.";
        return 1;
    }

    qInfo() << "Arduino открыта:" << comName;
    qInfo() << "Скорость: 9600 baud";

    QTcpServer tcpServer;

    if (!tcpServer.listen(QHostAddress::AnyIPv4, tcpPort)) {
        qCritical() << "Не удалось открыть TCP порт" << tcpPort;
        CloseHandle(serialLine);
        return 1;
    }

    qInfo() << "TCP сервер запущен, порт:" << tcpPort;
    printLocalAddresses();
    qInfo() << "Ожидание подключения Aurora...";

    // Активный клиент (Aurora). Новое подключение вытесняет предыдущее.
    QPointer<QTcpSocket> activeClient;

    QObject::connect(&tcpServer, &QTcpServer::newConnection, [&]() {
        while (tcpServer.hasPendingConnections()) {
            QTcpSocket *incoming = tcpServer.nextPendingConnection();

            if (activeClient)
                activeClient->disconnectFromHost();

            activeClient = incoming;

            qInfo() << "Aurora подключилась:" << incoming->peerAddress().toString();

            QObject::connect(incoming, &QTcpSocket::disconnected, [&, incoming]() {
                qInfo() << "Aurora отключилась";

                if (activeClient == incoming)
                    activeClient = nullptr;

                incoming->deleteLater();
            });
        }
    });

    // Периодический опрос COM-порта
    QByteArray rxBuffer;
    QTimer pollTimer;
    pollTimer.setInterval(kPollPeriodMs);

    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        char chunk[256];
        DWORD received = 0;

        if (!ReadFile(serialLine, chunk, sizeof(chunk), &received, nullptr)) {
            qWarning() << "Ошибка чтения COM";
            return;
        }

        if (received == 0)
            return;

        rxBuffer.append(chunk, static_cast<int>(received));

        int eol = rxBuffer.indexOf('\n');

        while (eol >= 0) {
            const QByteArray line = rxBuffer.left(eol).trimmed();
            rxBuffer.remove(0, eol + 1);
            eol = rxBuffer.indexOf('\n');

            if (line.isEmpty())
                continue;

            bool ok = false;
            const double celsius = line.toDouble(&ok);

            if (!ok) {
                qWarning() << "Не число:" << line;
                continue;
            }

            if (celsius < kMinCelsius || celsius > kMaxCelsius) {
                qWarning() << "Некорректная температура:" << celsius;
                continue;
            }

            qInfo() << "Температура:" << celsius << "C";

            if (activeClient &&
                activeClient->state() == QAbstractSocket::ConnectedState) {
                QByteArray packet = QByteArray::number(celsius, 'f', 1);
                packet.append('\n');

                activeClient->write(packet);
                activeClient->flush();
            }
        }
    });

    pollTimer.start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        if (serialLine != INVALID_HANDLE_VALUE)
            CloseHandle(serialLine);
    });

    return app.exec();
}
