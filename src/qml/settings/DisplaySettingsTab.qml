/*
 * Project: Crankshaft
 * Display settings tab content.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var preferencesFacade
    // Local alias shortens repetitive facade checks and keeps bindings readable.
    readonly property var prefs: root.preferencesFacade

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: availableWidth
            spacing: 12

            Components.SettingsCard {
                Layout.fillWidth: true
                title: qsTr("Display Rotation", "SettingsPanel")

                Components.DisplayRotationToggle {
                    Layout.fillWidth: true
                    showLabel: false
                    currentRotation: root.prefs ? root.prefs.displayRotation : 0
                    onRotationChanged: (newRotation) => {
                        if (root.prefs) {
                            root.prefs.displayRotation = newRotation
                        }
                    }
                }
            }

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
        }
    }
}
