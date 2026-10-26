/*
 * Project: Crankshaft
 * Audio settings tab content.
 *
 * Core owns the audio state. These controls send commands through audioBridge
 * and show the values core reports back.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var audioBridge

    // Core reports availability; until it does, the controls are shown disabled.
    readonly property bool audioAvailable: root.audioBridge ? root.audioBridge.isAudioAvailable : false

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            visible: !root.audioAvailable
            text: qsTr("Audio unavailable: core is not reporting an audio output.", "SettingsPanel")
            wrapMode: Text.WordWrap
            font.italic: true
        }

        Components.SettingsCard {
            Layout.fillWidth: true
            enabled: root.audioAvailable
            title: qsTr("Volume", "SettingsPanel")

            Components.SettingsSlider {
                Layout.fillWidth: true
                label: ""
                value: root.audioBridge ? root.audioBridge.volume : 50
                onUserValueChanged: (newValue) => {
                    if (root.audioBridge) {
                        root.audioBridge.setVolume(newValue)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Mute", "SettingsPanel")
                }

                Switch {
                    checked: root.audioBridge ? root.audioBridge.muted : false
                    onToggled: {
                        if (root.audioBridge) {
                            root.audioBridge.setMuted(checked)
                        }
                    }
                }
            }
        }

        Components.SettingsCard {
            Layout.fillWidth: true
            enabled: root.audioAvailable
            title: qsTr("Balance", "SettingsPanel")

            Components.SettingsSlider {
                Layout.fillWidth: true
                label: ""
                from: -100
                to: 100
                valueSuffix: ""
                value: root.audioBridge ? root.audioBridge.balance : 0
                onUserValueChanged: (newValue) => {
                    if (root.audioBridge) {
                        root.audioBridge.setBalance(newValue)
                    }
                }
            }
        }

        Components.SettingsCard {
            Layout.fillWidth: true
            enabled: root.audioAvailable
            title: qsTr("Guidance", "SettingsPanel")

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Lower music during navigation prompts", "SettingsPanel")
                    wrapMode: Text.WordWrap
                }

                Switch {
                    checked: root.audioBridge ? root.audioBridge.duckingEnabled : true
                    onToggled: {
                        if (root.audioBridge) {
                            root.audioBridge.setDuckingEnabled(checked)
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
