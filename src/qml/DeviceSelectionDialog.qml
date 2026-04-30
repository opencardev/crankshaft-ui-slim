/*
 * Project: Crankshaft
 * This file is part of Crankshaft project.
 * Copyright (C) 2025 OpenCarDev Team
 *
 *  Crankshaft is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crankshaft is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crankshaft. If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: deviceDialog

    property var themeManager: null
    readonly property var t: themeManager ? themeManager : fallbackTheme

    ThemeManager {
        id: fallbackTheme
    }

    QtObject {
        id: palette
        readonly property color surfaceColor: t.colors.surface
        readonly property color borderColor: t.colors.border
        readonly property color textPrimaryColor: t.colors.textPrimary
        readonly property color textSecondaryColor: t.colors.textSecondary
        readonly property color accentColor: t.colors.primary
        readonly property color accentColorHover: Qt.lighter(t.colors.primary, 1.12)
        readonly property color accentColorPressed: Qt.darker(t.colors.primary, 1.18)
        readonly property int spacing: t.spacing.medium
        readonly property int spacingSmall: t.spacing.small
        readonly property int spacingMedium: t.spacing.medium
        readonly property int radiusSmall: t.dimensions.borderRadiusSmall
        readonly property int radiusLarge: t.dimensions.borderRadiusLarge
        readonly property int touchTargetMinimum: t.dimensions.minTouchTarget
        readonly property int touchTargetRecommended: t.dimensions.recommendedTouchTarget
        readonly property int fontSizeBody: t.typography.body1
        readonly property int fontSizeCaption: t.typography.body2
    }
    
    title: qsTr("Select Device")
    modal: true
    standardButtons: Dialog.Cancel
    parent: Overlay.overlay
    
    width: Math.min(680, parent.width * 0.9)
    height: Math.min(420, parent.height * 0.82)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    padding: 0
    
    // Auto-connect timer (3 seconds for single device)
    property int autoConnectDelay: 3000
    property bool autoConnectEnabled: true
    
    // DeviceManager reference (set by parent)
    property var deviceManager: null
    
    signal deviceSelected(string deviceId)
    
    background: Rectangle {
        color: palette.surfaceColor
        radius: palette.radiusLarge
        border.color: palette.borderColor
        border.width: 1
    }
    
    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: palette.spacing
        spacing: palette.spacingMedium
        
        // Title label
        Label {
            text: deviceManager && deviceManager.hasMultipleDevices 
                  ? qsTr("Multiple devices detected. Select one to connect:")
                  : qsTr("Device detected. Connecting automatically...")
            font.pixelSize: palette.fontSizeBody
            color: palette.textPrimaryColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        
        // Device list
        ListView {
            id: deviceListView
            
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            clip: true
            spacing: palette.spacingSmall
            
            model: deviceManager ? deviceManager.detectedDevices : []
            
            delegate: ItemDelegate {
                width: ListView.view.width
                height: palette.touchTargetRecommended
                
                background: Rectangle {
                    color: parent.hovered ? palette.accentColorHover : "transparent"
                    radius: palette.radiusSmall
                        border.color: modelData.wasConnectedBefore ? palette.accentColor : palette.borderColor
                        border.width: modelData.wasConnectedBefore ? 2 : 1
                }
                
                contentItem: RowLayout {
                    spacing: palette.spacingMedium
                    
                    // Device icon
                    Text {
                        text: modelData.type === "tablet" ? "T" : "P"
                        font.pixelSize: 32
                        Layout.preferredWidth: 48
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    // Device info
                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        
                        Label {
                            text: modelData.name
                            font.pixelSize: palette.fontSizeBody
                            font.bold: modelData.wasConnectedBefore
                            color: palette.textPrimaryColor
                            Layout.fillWidth: true
                        }
                        
                        RowLayout {
                            spacing: palette.spacingSmall
                            
                            Label {
                                text: qsTr("Signal: %1%").arg(modelData.signalStrength)
                                font.pixelSize: palette.fontSizeCaption
                                color: palette.textSecondaryColor
                            }
                            
                            Label {
                                visible: modelData.wasConnectedBefore
                                text: qsTr("(Previously connected)")
                                font.pixelSize: palette.fontSizeCaption
                                color: palette.accentColor
                            }
                        }
                    }
                    
                    // Connect button
                    Button {
                        text: qsTr("Connect")
                        Layout.preferredHeight: palette.touchTargetMinimum
                        Layout.preferredWidth: 120
                        
                        background: Rectangle {
                            color: parent.pressed ? palette.accentColorPressed 
                                  : parent.hovered ? palette.accentColorHover 
                                  : palette.accentColor
                            radius: palette.radiusSmall
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: palette.fontSizeBody
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: {
                            deviceDialog.deviceSelected(modelData.deviceId)
                            deviceDialog.close()
                        }
                    }
                }
            }
            
            // Empty state
            Label {
                visible: parent.count === 0
                anchors.centerIn: parent
                text: qsTr("No devices detected")
                font.pixelSize: palette.fontSizeBody
                color: palette.textSecondaryColor
            }
        }
        
        // Auto-connect countdown (single device only)
        Label {
            visible: deviceManager && !deviceManager.hasMultipleDevices && autoConnectEnabled
            text: qsTr("Auto-connecting in %1 seconds...").arg(Math.ceil(autoConnectTimer.remainingTime / 1000))
            font.pixelSize: palette.fontSizeCaption
            color: palette.textSecondaryColor
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
    
    // Auto-connect timer for single device
    Timer {
        id: autoConnectTimer
        interval: deviceDialog.autoConnectDelay
        running: false
        repeat: false
        
        property int remainingTime: interval
        
        onTriggered: {
            if (deviceManager && deviceManager.deviceCount === 1 && autoConnectEnabled) {
                var deviceId = deviceManager.getTopPriorityDeviceId()
                if (deviceId) {
                    deviceDialog.deviceSelected(deviceId)
                    deviceDialog.close()
                }
            }
        }
        
        // Update remaining time
        Timer {
            interval: 100
            running: autoConnectTimer.running
            repeat: true
            onTriggered: {
                autoConnectTimer.remainingTime = Math.max(0, 
                    autoConnectTimer.interval - (Date.now() - autoConnectTimer.startTime))
            }
        }
        
        property var startTime: 0
        onRunningChanged: {
            if (running) {
                startTime = Date.now()
                remainingTime = interval
            }
        }
    }
    
    // Start auto-connect when dialog opens with single device
    onOpened: {
        if (deviceManager && deviceManager.deviceCount === 1 && autoConnectEnabled) {
            autoConnectTimer.start()
        }
    }
    
    // Stop timer when closed
    onClosed: {
        autoConnectTimer.stop()
    }
}
