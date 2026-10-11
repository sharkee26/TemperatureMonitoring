import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: rootWindow

    objectName: "rootWindow"
    initialPage: Qt.resolvedUrl("pages/OverviewPage.qml")
    cover: Qt.resolvedUrl("cover/SummaryCover.qml")
    allowedOrientations: defaultAllowedOrientations

    // Адрес Windows-моста
    property string linkHost: "10.0.2.2"
    property int linkPort: 5555

    Component.onCompleted: bridgeClient.openLink(linkHost, linkPort)
}
