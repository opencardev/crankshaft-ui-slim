/*
 * Project: Crankshaft
 * Display settings tab content.
 */

import QtQuick
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var preferencesFacade
    // Local alias shortens repetitive facade checks and keeps bindings readable.
    readonly property var prefs: root.preferencesFacade

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Components.SettingsCard {
            Layout.fillWidth: true
            title: qsTr("Brightness", "SettingsPanel")

            Components.SettingsSlider {
                Layout.fillWidth: true
                label: ""
                value: root.prefs ? root.prefs.displayBrightness : 50
                onUserValueChanged: (newValue) => {
                    if (root.prefs) {
                        root.prefs.displayBrightness = newValue
                    }
                }
            }
        }

        Components.SettingsCard {
            Layout.fillWidth: true
            title: qsTr("Theme", "SettingsPanel")

            Components.ThemeToggle {
                Layout.fillWidth: true
                showLabel: false
                currentMode: root.prefs ? root.prefs.themeMode : "DARK"
                onModeChanged: (newMode) => {
                    if (root.prefs) {
                        root.prefs.themeMode = newMode
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
