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

    // The chosen output is not the one in use, so audio is on the system default.
    readonly property bool outputFallback: root.audioBridge
        ? root.audioBridge.selectedOutput !== "" && root.audioBridge.activeOutput !== root.audioBridge.selectedOutput
        : false

    // Rows are {id, label}. The first row is the system default, with an empty id.
    property var outputChoices: []

    // Rebuild the choices from core's state. Done in one place, not as a binding, so the
    // combo box stays in step with core after the user has picked an item.
    function refreshOutputChoices() {
        const choices = [{ id: "", label: qsTr("System default", "SettingsPanel") }]
        const listed = root.audioBridge ? root.audioBridge.outputs : []
        for (const output of listed) {
            const label = output.bluetooth
                ? qsTr("%1 (Bluetooth)", "SettingsPanel").arg(output.name)
                : output.name
            choices.push({ id: output.id, label: label })
        }
        root.outputChoices = choices

        const selected = root.audioBridge ? root.audioBridge.selectedOutput : ""
        const index = choices.findIndex((choice) => choice.id === selected)
        outputCombo.currentIndex = index < 0 ? 0 : index
    }

    Component.onCompleted: root.refreshOutputChoices()

    Connections {
        target: root.audioBridge
        function onStateChanged() { root.refreshOutputChoices() }
    }

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
            title: qsTr("Output", "SettingsPanel")

            ComboBox {
                id: outputCombo
                Layout.fillWidth: true
                model: root.outputChoices
                textRole: "label"
                valueRole: "id"
                onActivated: (index) => {
                    if (root.audioBridge) {
                        root.audioBridge.setOutputDevice(outputCombo.currentValue)
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.outputFallback
                text: qsTr("The chosen output is not connected. Audio is playing on the system default.", "SettingsPanel")
                wrapMode: Text.WordWrap
                font.italic: true
            }
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
