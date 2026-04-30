/*
 * Project: Crankshaft
 * Audio settings tab content.
 */

import QtQuick
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var preferencesFacade

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Components.SettingsCard {
            Layout.fillWidth: true
            title: qsTr("Volume", "SettingsPanel")

            Components.SettingsSlider {
                Layout.fillWidth: true
                label: ""
                value: root.preferencesFacade ? root.preferencesFacade.audioVolume : 50
                onUserValueChanged: (newValue) => {
                    if (root.preferencesFacade) {
                        root.preferencesFacade.audioVolume = newValue
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
