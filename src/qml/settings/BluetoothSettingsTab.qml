/*
 * Project: Crankshaft
 * Bluetooth settings tab content.
 *
 * Layout notes:
 * - Uses the same card rhythm as other settings tabs for consistency.
 * - Keeps scan, pairing, and status grouped into clear sections.
 * - Preserves explicit discovered-device selection across model refreshes.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var bluetoothService
    // Local alias keeps service access concise across many bindings.
    readonly property var bt: root.bluetoothService
    // Tracks user-initiated scan flow so completion can trigger one refresh pass.
    property bool scanSessionActive: false
    property bool pendingPostScanRefresh: false
    property bool postRefreshRequested: false
    // Keep explicit selection state so model refreshes do not break pairing UX.
    property string selectedDiscoveredAddress: ""

    function hasDiscoveredAddress(address) {
        if (!address || !root.bt) {
            return false
        }

        var devices = root.bt.discoveredAudioDevices
        for (var i = 0; i < devices.length; i++) {
            var d = devices[i]
            if (d && d.address === address) {
                return true
            }
        }
        return false
    }

    Connections {
        target: root.bt

        function onDiscoveringChanged(discovering) {
            if (discovering) {
                if (!root.scanSessionActive) {
                    return
                }
                scanSafetyTimer.start()
                return
            }

            if (!root.scanSessionActive) {
                return
            }

            scanSafetyTimer.stop()
            root.scanSessionActive = false

            if (root.pendingPostScanRefresh && !root.postRefreshRequested && root.bt) {
                root.postRefreshRequested = true
                root.bt.refreshDevices()
            }
        }

        function onDevicesChanged() {
            // Finish UI refresh cycle once device data arrives after scanning.
            if (root.postRefreshRequested) {
                root.postRefreshRequested = false
                root.pendingPostScanRefresh = false
            }

            // Drop stale selection if the selected device is no longer discovered.
            if (root.selectedDiscoveredAddress !== "" && !root.hasDiscoveredAddress(root.selectedDiscoveredAddress)) {
                root.selectedDiscoveredAddress = ""
            }
        }
    }

    Timer {
        id: scanSafetyTimer
        interval: 60000
        repeat: false
        onTriggered: {
            if (!root.scanSessionActive) {
                return
            }

            if (root.bt) {
                root.bt.stopDiscovery()
            }

            // Fallback finish path when backend does not emit discoveringChanged(false).
            root.scanSessionActive = false
            if (root.pendingPostScanRefresh && !root.postRefreshRequested && root.bt) {
                root.postRefreshRequested = true
                root.bt.refreshDevices()
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 8

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: contentColumn
            width: flick.width
            spacing: 12

            Components.SettingsCard {
                Layout.fillWidth: true
                title: qsTr("Bluetooth Status", "SettingsPanel")

                RowLayout {
                    width: parent.width
                    spacing: 10

                    Rectangle {
                        radius: 6
                        color: root.bt && root.bt.btEnabled
                               ? Qt.rgba(0.18, 0.66, 0.35, 0.2)
                               : Qt.rgba(0.72, 0.20, 0.20, 0.2)
                        border.width: 1
                        border.color: root.bt && root.bt.btEnabled
                                      ? Qt.rgba(0.18, 0.66, 0.35, 0.7)
                                      : Qt.rgba(0.72, 0.20, 0.20, 0.7)
                        implicitWidth: btStatusLabel.implicitWidth + 14
                        implicitHeight: btStatusLabel.implicitHeight + 8

                        Label {
                            id: btStatusLabel
                            anchors.centerIn: parent
                            text: root.bt && root.bt.btEnabled
                                  ? qsTr("Enabled", "SettingsPanel")
                                  : qsTr("Disabled", "SettingsPanel")
                            font.pointSize: 10
                            font.bold: true
                        }
                    }

                    Item { Layout.fillWidth: true }

                    BusyIndicator {
                        running: root.bt ? root.bt.discovering : false
                        visible: running
                        implicitWidth: 22
                        implicitHeight: 22
                    }

                    Label {
                        visible: root.bt ? root.bt.discovering : false
                        text: qsTr("Scanning", "SettingsPanel")
                        font.pointSize: 10
                        opacity: 0.8
                    }
                }

                Label {
                    width: parent.width
                    text: root.bt && root.bt.hasConnectedAudioDevice
                          ? qsTr("Active device: %1", "SettingsPanel").arg(root.bt.connectedAudioName)
                          : qsTr("No device currently connected", "SettingsPanel")
                    font.pointSize: 11
                    opacity: 0.85
                    wrapMode: Text.WordWrap
                }

                Label {
                    width: parent.width
                    visible: !(!!root.bt && root.bt.btEnabled)
                    text: qsTr("Bluetooth is currently disabled at system level. Enable Bluetooth from system settings to manage pairing and audio routing here.", "SettingsPanel")
                    font.pointSize: 11
                    opacity: 0.82
                    wrapMode: Text.WordWrap
                }
            }

            Components.SettingsCard {
                Layout.fillWidth: true
                title: qsTr("Connected Audio", "SettingsPanel")
                visible: !!root.bt && root.bt.btEnabled
                enabled: visible

                ComboBox {
                    id: pairedDeviceCombo
                    width: parent.width
                    model: root.bt ? root.bt.pairedAudioDevices : []
                    textRole: "name"
                    valueRole: "address"
                }

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Button {
                        text: qsTr("Connect", "SettingsPanel")
                        enabled: Boolean(pairedDeviceCombo.currentValue && pairedDeviceCombo.currentValue !== "")
                        onClicked: {
                            if (root.bt) {
                                root.bt.connectAudioDevice(pairedDeviceCombo.currentValue)
                            }
                        }
                    }

                    Button {
                        text: qsTr("Disconnect", "SettingsPanel")
                        enabled: root.bt && root.bt.hasConnectedAudioDevice
                        onClicked: {
                            if (root.bt) {
                                root.bt.disconnectAudioDevice()
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("Unpair", "SettingsPanel")
                        enabled: Boolean(pairedDeviceCombo.currentValue && pairedDeviceCombo.currentValue !== "")
                        onClicked: {
                            if (root.bt) {
                                root.bt.unpairDevice(pairedDeviceCombo.currentValue)
                            }
                        }
                    }
                }
            }

            Components.SettingsCard {
                Layout.fillWidth: true
                title: qsTr("Discover and Pair", "SettingsPanel")
                visible: !!root.bt && root.bt.btEnabled
                enabled: visible

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Button {
                        text: root.bt && root.bt.discovering
                              ? qsTr("Stop Scan", "SettingsPanel")
                              : qsTr("Start Scan", "SettingsPanel")
                        font.bold: true
                        enabled: !!root.bt
                        onClicked: {
                            if (!root.bt) {
                                return
                            }

                            if (root.bt.discovering) {
                                root.bt.stopDiscovery()
                            } else {
                                root.scanSessionActive = true
                                root.pendingPostScanRefresh = true
                                root.postRefreshRequested = false
                                scanSafetyTimer.start()
                                root.bt.startDiscovery()
                            }
                        }
                    }

                    Button {
                        text: qsTr("Refresh", "SettingsPanel")
                        enabled: !!root.bt
                        onClicked: {
                            if (root.bt) {
                                root.bt.refreshDevices()
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.bt && root.bt.discovering
                              ? qsTr("Searching for nearby devices...", "SettingsPanel")
                              : qsTr("Choose a device below, then pair.", "SettingsPanel")
                        font.pointSize: 10
                        opacity: 0.8
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 150
                    radius: 8
                      color: root.Material.theme === Material.Dark
                          ? Qt.rgba(1, 1, 1, 0.03)
                          : Qt.rgba(0, 0, 0, 0.03)
                    border.width: 1
                      border.color: root.Material.theme === Material.Dark
                              ? Qt.rgba(1, 1, 1, 0.12)
                              : Qt.rgba(0, 0, 0, 0.18)

                    ListView {
                        id: discoveredList
                        anchors.fill: parent
                        anchors.margins: 6
                        clip: true
                        spacing: 6
                        model: root.bt ? root.bt.discoveredAudioDevices : []

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            height: 42
                            highlighted: root.selectedDiscoveredAddress === modelData.address

                            background: Rectangle {
                                radius: 6
                                color: parent.highlighted
                                       ? Qt.rgba(0.30, 0.64, 1.0, 0.22)
                                                    : (parent.hovered
                                                        ? (root.Material.theme === Material.Dark
                                                            ? Qt.rgba(1, 1, 1, 0.08)
                                                            : Qt.rgba(0, 0, 0, 0.06))
                                                        : "transparent")
                                border.width: parent.highlighted ? 1 : 0
                                border.color: Qt.rgba(0.30, 0.64, 1.0, 0.75)
                            }

                            contentItem: RowLayout {
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name && modelData.name !== ""
                                          ? modelData.name
                                          : modelData.address
                                    elide: Text.ElideRight
                                }

                                Label {
                                    visible: modelData.paired === true
                                    text: qsTr("Paired", "SettingsPanel")
                                    font.pointSize: 10
                                    opacity: 0.8
                                }
                            }

                            onClicked: {
                                root.selectedDiscoveredAddress = modelData.address || ""
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: root.selectedDiscoveredAddress !== ""
                              ? qsTr("Selected: %1", "SettingsPanel").arg(root.selectedDiscoveredAddress)
                              : qsTr("No device selected", "SettingsPanel")
                        font.pointSize: 10
                        opacity: 0.8
                        elide: Text.ElideMiddle
                    }

                    Button {
                        text: qsTr("Pair Selected", "SettingsPanel")
                        enabled: root.selectedDiscoveredAddress !== ""
                        onClicked: {
                            if (root.bt) {
                                root.bt.pairDevice(root.selectedDiscoveredAddress)
                            }
                        }
                    }
                }
            }

            Components.SettingsCard {
                Layout.fillWidth: true
                title: qsTr("Status", "SettingsPanel")

                Label {
                    width: parent.width
                    text: root.bt ? root.bt.statusMessage : ""
                    font.pointSize: 11
                    visible: text !== ""
                    wrapMode: Text.WordWrap
                }

                Label {
                    width: parent.width
                    text: root.bt ? root.bt.lastError : ""
                    font.pointSize: 11
                    color: Material.color(Material.Red)
                    visible: text !== ""
                    wrapMode: Text.WordWrap
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
            }
        }
    }
}
