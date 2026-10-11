#ifndef BRIDGECLIENT_H
#define BRIDGECLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QString>

// TCP-клиент: получает строки с температурой от Windows-моста (serial_bridge),
// который в свою очередь читает данные из COM-порта Arduino.
class BridgeClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool linked READ isLinked NOTIFY linkStateChanged)

public:
    explicit BridgeClient(QObject *parent = nullptr);

    bool isLinked() const;

    Q_INVOKABLE void openLink(const QString &host, int port);
    Q_INVOKABLE void closeLink();

signals:
    void readingArrived(double celsius);
    void linkStateChanged();
    void linkFailed(const QString &reason);

private slots:
    void onReadyRead();
    void onSocketUp();
    void onSocketDown();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void retryConnect();

private:
    void updateLinked(bool value);
    void handleLine(const QByteArray &line);

private:
    QTcpSocket *m_socket = nullptr;
    QTimer *m_retryTimer = nullptr;

    QByteArray m_pending;

    QString m_host;
    quint16 m_port = 0;

    bool m_linked = false;
    bool m_keepTrying = false;
};

#endif // BRIDGECLIENT_H
