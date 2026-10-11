import QtQuick 2.0
import Sailfish.Silica 1.0

// Журнал изменений температуры (стандартный список Silica).
Page {
    id: journal
    objectName: "journalPage"

    allowedOrientations: Orientation.All

    function formatStamp(iso) {
        if (!iso)
            return ""

        return Qt.formatDateTime(new Date(iso), "dd.MM.yyyy  hh:mm:ss")
    }

    SilicaListView {
        id: journalView
        objectName: "journalView"

        anchors.fill: parent

        model: temperatureStorage.changeJournal

        header: PageHeader {
            title: "История изменений"
            description: journalView.count > 0 ? "Записей: " + journalView.count : ""
        }

        delegate: ListItem {
            id: entryRow

            contentHeight: Theme.itemSizeMedium

            property real fromValue: modelData ? Number(modelData.before) : 0
            property real toValue:   modelData ? Number(modelData.after)  : 0
            property real diffValue: modelData ? Number(modelData.shift)  : 0
            property string whenText: modelData ? journal.formatStamp(modelData.stamp) : ""

            Label {
                id: rangeLabel

                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: diffLabel.left
                    rightMargin: Theme.paddingMedium
                    top: parent.top
                    topMargin: Theme.paddingMedium
                }

                text: entryRow.fromValue.toFixed(1) + " → " + entryRow.toValue.toFixed(1) + " °C"
                color: entryRow.highlighted ? Theme.highlightColor : Theme.primaryColor
                truncationMode: TruncationMode.Fade
            }

            Label {
                id: diffLabel

                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: rangeLabel.verticalCenter
                }

                text: (entryRow.diffValue >= 0 ? "+" : "") + entryRow.diffValue.toFixed(1) + " °C"
                color: Theme.highlightColor
            }

            Label {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    top: rangeLabel.bottom
                }

                text: entryRow.whenText
                color: entryRow.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }
        }

        ViewPlaceholder {
            enabled: journalView.count === 0
            text: "Журнал пока пуст"
            hintText: "Записи появятся при изменении температуры"
        }

        VerticalScrollDecorator {}
    }
}
