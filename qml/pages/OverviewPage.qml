import QtQuick 2.0
import Sailfish.Silica 1.0

// Главный экран: текущее значение температуры.
Page {
    id: overview
    objectName: "overviewPage"

    allowedOrientations: Orientation.All

    readonly property bool hasReading: temperatureStorage.updatedAt !== ""

    function openJournal() {
        pageStack.push(Qt.resolvedUrl("JournalPage.qml"))
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: "История изменений"
                onClicked: overview.openJournal()
            }
        }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: "Температура"
            }

            Item {
                width: 1
                height: Theme.itemSizeLarge
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Сейчас"
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeLarge
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: overview.hasReading
                      ? temperatureStorage.currentValue.toFixed(1) + " " + temperatureStorage.unitLabel
                      : "—"
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeHuge * 2
            }

            Item {
                width: 1
                height: Theme.itemSizeLarge
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "История изменений"
                onClicked: overview.openJournal()
            }
        }

        VerticalScrollDecorator {}
    }
}
