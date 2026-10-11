import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    objectName: "summaryCover"

    CoverTemplate {
        objectName: "summaryCoverContent"

        primaryText: temperatureStorage.updatedAt !== ""
                     ? temperatureStorage.currentValue.toFixed(1) + " " + temperatureStorage.unitLabel
                     : "—"

        secondaryText: "Термометр"

        icon {
            source: Qt.resolvedUrl("../icons/TemperatureMonitoring.svg")
            sourceSize { width: icon.width; height: icon.height }
        }
    }
}
