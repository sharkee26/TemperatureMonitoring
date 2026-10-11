#include <auroraapp.h>
#include <QtQuick>
#include <QQmlContext>

#include "temperaturestorage.h"
#include "bridgeclient.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(
        Aurora::Application::application(argc, argv));

    QScopedPointer<QQuickView> view(
        Aurora::Application::createView());

    TemperatureStorage temperatureStorage;
    BridgeClient bridgeClient;

    QQmlContext *context = view->rootContext();

    context->setContextProperty("temperatureStorage", &temperatureStorage);
    context->setContextProperty("bridgeClient", &bridgeClient);

    // Каждое принятое значение сразу сохраняется в хранилище
    QObject::connect(
        &bridgeClient,
        &BridgeClient::readingArrived,
        &temperatureStorage,
        [&temperatureStorage](double celsius) {
            temperatureStorage.storeReading(celsius);
        });

    view->setSource(
        Aurora::Application::pathTo(
            QStringLiteral("qml/TemperatureMonitoring.qml")));

    view->show();

    return app->exec();
}
