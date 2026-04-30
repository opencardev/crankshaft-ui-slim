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
    id: reconnectionPrompt

    property var themeManager: null
    readonly property var t: themeManager ? themeManager : fallbackTheme
    property color overlayColor: t.colors.overlay

    ThemeManager {
        id: fallbackTheme
    }

    QtObject {
        id: palette
        readonly property color surfaceColor: t.colors.surface
        readonly property color borderColor: t.colors.border
        readonly property color errorColor: t.colors.error
        readonly property color textPrimaryColor: t.colors.textPrimary
        readonly property color textSecondaryColor: t.colors.textSecondary
        readonly property color accentColor: t.colors.primary
        readonly property color accentColorHover: Qt.lighter(t.colors.primary, 1.12)
        readonly property color accentColorPressed: Qt.darker(t.colors.primary, 1.18)
        readonly property int spacing: t.spacing.medium
        readonly property int spacingMedium: t.spacing.medium
        readonly property int spacingLarge: t.spacing.large
        readonly property int radiusSmall: t.dimensions.borderRadiusSmall
        readonly property int radiusLarge: t.dimensions.borderRadiusLarge
        readonly property int touchTargetMinimum: t.dimensions.minTouchTarget
        readonly property int fontSizeHeading: t.typography.h3
        readonly property int fontSizeBody: t.typography.body1
        readonly property int fontSizeCaption: t.typography.body2
    }
    
    title: qsTr("Connection Failed")
    modal: true
    parent: Overlay.overlay

    Overlay.modal: Rectangle {
        color: reconnectionPrompt.overlayColor
    }
    
    width: Math.min(620, parent.width * 0.9)
    height: Math.min(390, parent.height * 0.82)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    padding: 0
    
    // ConnectionStateMachine reference (set by parent)
    property var connectionStateMachine: null
    
    // Error message
    property string errorMessage: ""
    property int retryCount: 0
    
    signal manualConnectRequested()
    signal dismissed()
    
    background: Rectangle {
        color: palette.surfaceColor
        radius: palette.radiusLarge
        border.color: palette.errorColor
        border.width: 1
    }
    
    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: palette.spacing
        spacing: palette.spacingLarge
        
        // Error icon
        Text {
            text: "⚠️"
            font.pixelSize: 64
            Layout.alignment: Qt.AlignHCenter
        }
        
        // Title
        Label {
            text: qsTr("Reconnection Failed")
            font.pixelSize: palette.fontSizeHeading
            font.bold: true
            color: palette.errorColor
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        
        // Description
        Label {
            text: qsTr("Unable to reconnect to your AndroidAuto device after %1 attempts.").arg(retryCount)
            font.pixelSize: palette.fontSizeBody
            color: palette.textPrimaryColor
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        
        // Error message
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: errorLabel.height + palette.spacingMedium * 2
            color: Qt.rgba(palette.errorColor.r, palette.errorColor.g, palette.errorColor.b, 0.1)
            radius: palette.radiusSmall
            border.color: palette.errorColor
            border.width: 1
            
            Label {
                id: errorLabel
                anchors.fill: parent
                anchors.margins: palette.spacingMedium
                text: errorMessage || qsTr("Connection error - please check your device")
                font.pixelSize: palette.fontSizeCaption
                color: palette.errorColor
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        
        // Suggestions
        Label {
            text: qsTr("Suggestions:\n• Check USB cable connection\n• Enable Android Auto on your device\n• Restart your device")
            font.pixelSize: palette.fontSizeCaption
            color: palette.textSecondaryColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            lineHeight: 1.5
        }
        
        // Spacer
        Item {
            Layout.fillHeight: true
        }
        
        // Action buttons
        RowLayout {
            spacing: palette.spacingMedium
            Layout.fillWidth: true
            
            // Dismiss button
            Button {
                text: qsTr("Dismiss")
                Layout.fillWidth: true
                Layout.preferredHeight: palette.touchTargetMinimum
                
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(palette.borderColor, 1.2) :
                           parent.hovered ? Qt.lighter(palette.borderColor, 1.2) :
                           palette.borderColor
                    radius: palette.radiusSmall
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: palette.fontSizeBody
                    color: palette.textPrimaryColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    reconnectionPrompt.dismissed()
                    reconnectionPrompt.close()
                }
            }
            
            // Manual Connect button
            Button {
                text: qsTr("Try Again")
                Layout.fillWidth: true
                Layout.preferredHeight: palette.touchTargetMinimum
                
                background: Rectangle {
                    color: parent.pressed ? palette.accentColorPressed :
                           parent.hovered ? palette.accentColorHover :
                           palette.accentColor
                    radius: palette.radiusSmall
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: palette.fontSizeBody
                    font.bold: true
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    reconnectionPrompt.manualConnectRequested()
                    reconnectionPrompt.close()
                }
            }
        }
    }
    
    // Update error message from connection state machine
    Connections {
        target: connectionStateMachine
        
        function onLastErrorChanged(error) {
            errorMessage = error
        }
        
        function onRetryCountChanged(count) {
            retryCount = count
        }
    }
    
    Component.onCompleted: {
        if (connectionStateMachine) {
            errorMessage = connectionStateMachine.lastError
            retryCount = connectionStateMachine.retryCount
        }
    }
}
