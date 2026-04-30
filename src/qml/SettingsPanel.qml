/*
 * Project: Crankshaft
 * Settings panel shell.
 *
 * This dialog only handles frame/navigation/reset. Tab content is split into
 * dedicated files under qml/settings for maintainability.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "settings" as SettingsTabs

Dialog {
    id: settingsPanel
    title: ""
    modal: true
    parent: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(760, parent.width * 0.96)
    height: Math.min(460, parent.height * 0.94)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    padding: 0
    z: 1000

    standardButtons: Dialog.NoButton

    background: Rectangle {
        radius: 16
        color: settingsPanel.Material.theme === Material.Dark
               ? settingsPanel.Material.background
               : settingsPanel.Material.dialogColor
        border.width: 1
        border.color: Qt.rgba(settingsPanel.Material.accent.r,
                              settingsPanel.Material.accent.g,
                              settingsPanel.Material.accent.b, 0.65)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Settings", "SettingsPanel")
                font.pointSize: 18
                font.bold: true
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Slim UI", "SettingsPanel")
                font.pointSize: 10
                opacity: 0.7
            }
        }

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            spacing: 2

            TabButton { text: qsTr("Display", "SettingsPanel") }
            TabButton { text: qsTr("Audio", "SettingsPanel") }
            TabButton { text: qsTr("Connection", "SettingsPanel") }
            TabButton { text: qsTr("Bluetooth", "SettingsPanel") }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
             color: settingsPanel.Material.theme === Material.Dark
                 ? Qt.rgba(1, 1, 1, 0.02)
                 : Qt.rgba(0, 0, 0, 0.025)
            border.width: 1
             border.color: settingsPanel.Material.theme === Material.Dark
                     ? Qt.rgba(1, 1, 1, 0.08)
                     : Qt.rgba(0, 0, 0, 0.14)

            StackLayout {
                anchors.fill: parent
                anchors.margins: 14
                currentIndex: tabBar.currentIndex

                SettingsTabs.DisplaySettingsTab {
                    preferencesFacade: _preferencesFacade
                }

                SettingsTabs.AudioSettingsTab {
                    preferencesFacade: _preferencesFacade
                }

                SettingsTabs.ConnectionSettingsTab {
                    preferencesFacade: _preferencesFacade
                    onRequestFactoryReset: resetConfirmationDialog.open()
                }

                SettingsTabs.BluetoothSettingsTab {
                    bluetoothService: _bluetoothService
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Close", "SettingsPanel")
                onClicked: settingsPanel.close()
            }
        }
    }

    Dialog {
        id: resetConfirmationDialog
        title: qsTr("Confirm Reset", "SettingsPanel")
        modal: true
        parent: settingsPanel.parent
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        z: 1001

        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            text: qsTr("Reset all settings to factory defaults?\n\nBrightness: 50%\nVolume: 50%\nConnection: USB\nTheme: Dark", "SettingsPanel")
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            _preferencesFacade.resetToDefaults()
            _preferencesFacade.saveSettings()
        }
    }
}
