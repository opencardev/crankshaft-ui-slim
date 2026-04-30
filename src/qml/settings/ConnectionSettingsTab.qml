/*
 * Project: Crankshaft
 * Connection settings tab content.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var preferencesFacade
    signal requestFactoryReset()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Components.SettingsCard {
            Layout.fillWidth: true
            title: qsTr("Connection Preference", "SettingsPanel")

            Components.ConnectionPreferenceToggle {
                Layout.fillWidth: true
                showLabel: false
                currentPreference: root.preferencesFacade ? root.preferencesFacade.connectionPreference : "USB"
                onPreferenceChanged: (newPreference) => {
                    if (root.preferencesFacade) {
                        root.preferencesFacade.connectionPreference = newPreference
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        Button {
            Layout.fillWidth: true
            text: qsTr("Reset to Factory Defaults", "SettingsPanel")
            Material.foreground: Material.Red
            onClicked: root.requestFactoryReset()
        }
    }
}
