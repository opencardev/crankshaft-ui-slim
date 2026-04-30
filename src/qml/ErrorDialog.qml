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

/**
 * ErrorDialog - Modal dialog for displaying errors and warnings
 *
 * Supports different severity levels with appropriate styling
 */
Dialog {
    id: errorDialog

    // Injected from main.qml (ThemeManager instance)
    required property var themeManager
    readonly property var t: themeManager
    property color overlayColor: t.colors.overlay
    
    // Properties
    property string errorCode: ""
    property string errorMessage: ""
    property int severity: 2  // 0=Info, 1=Warning, 2=Error, 3=Critical
    property bool retryable: false
    
    // Dialog configuration
    modal: true
    closePolicy: Popup.CloseOnEscape
    
    width: Math.min(500, parent.width * 0.9)
    height: Math.min(300, parent.height * 0.7)
    
    anchors.centerIn: parent

    Overlay.modal: Rectangle {
        color: errorDialog.overlayColor
    }
    
    // Signals
    signal retryRequested()
    signal dismissed()
    
    // Background
    background: Rectangle {
        color: t.colors.surface
        border.color: getSeverityColor()
        border.width: 2
        radius: t.dimensions.borderRadiusLarge
        
        layer.enabled: true
        layer.effect: null
    }
    
    // Header with icon and title
    header: Item {
        height: 80
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: t.spacing.large
            spacing: t.spacing.medium
            
            // Severity icon
            Text {
                Layout.alignment: Qt.AlignVCenter
                text: getSeverityIcon()
                color: getSeverityColor()
                font.pixelSize: 48
            }
            
            // Title
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: t.spacing.small
                
                Text {
                    Layout.fillWidth: true
                    text: getSeverityTitle()
                    color: t.colors.textPrimary
                    font.pixelSize: t.typography.h4
                    font.family: t.typography.fontFamily
                    font.weight: Font.Medium
                }
                
                Text {
                    Layout.fillWidth: true
                    text: errorCode
                    color: t.colors.textSecondary
                    font.pixelSize: t.typography.caption
                    font.family: t.typography.monospaceFontFamily
                    visible: errorCode !== ""
                }
            }
        }
    }
    
    // Content - error message
    contentItem: Item {
        ScrollView {
            anchors.fill: parent
            anchors.margins: t.spacing.medium
            clip: true
            
            Text {
                width: parent.width
                text: errorMessage
                color: t.colors.textPrimary
                font.pixelSize: t.typography.body1
                font.family: t.typography.fontFamily
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignLeft
            }
        }
    }
    
    // Footer with action buttons
    footer: Item {
        height: 80
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: t.spacing.large
            spacing: t.spacing.medium
            
            Item {
                Layout.fillWidth: true
            }
            
            // Retry button (if retryable)
            Button {
                visible: retryable
                Layout.preferredWidth: 120
                Layout.preferredHeight: t.dimensions.recommendedTouchTarget
                
                text: qsTr("Retry")
                font.pixelSize: t.typography.button
                font.family: t.typography.fontFamily
                
                onClicked: {
                    errorDialog.retryRequested()
                    errorDialog.close()
                }
                
                background: Rectangle {
                    color: parent.pressed ? t.colors.primaryVariant : t.colors.primary
                    radius: t.dimensions.borderRadiusMedium
                }
                
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: t.colors.textOnPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            // OK/Dismiss button
            Button {
                Layout.preferredWidth: 120
                Layout.preferredHeight: t.dimensions.recommendedTouchTarget
                
                text: retryable ? qsTr("Cancel") : qsTr("OK")
                font.pixelSize: t.typography.button
                font.family: t.typography.fontFamily
                
                onClicked: {
                    errorDialog.dismissed()
                    errorDialog.close()
                }
                
                background: Rectangle {
                    color: parent.pressed ? t.colors.surfaceVariant : t.colors.surface
                    border.color: t.colors.border
                    border.width: 1
                    radius: t.dimensions.borderRadiusMedium
                }
                
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: t.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    
    // Helper functions
    function getSeverityIcon() {
        switch (severity) {
            case 0: return "ℹ️"  // Info
            case 1: return "⚠️"  // Warning
            case 2: return "❌"  // Error
            case 3: return "🛑"  // Critical
            default: return "❓"
        }
    }
    
    function getSeverityTitle() {
        switch (severity) {
            case 0: return qsTr("Information")
            case 1: return qsTr("Warning")
            case 2: return qsTr("Error")
            case 3: return qsTr("Critical Error")
            default: return qsTr("Error")
        }
    }
    
    function getSeverityColor() {
        switch (severity) {
            case 0: return t.colors.info || "#2196F3"     // Info - blue
            case 1: return t.colors.warning || "#FF9800"  // Warning - orange
            case 2: return t.colors.error || "#F44336"    // Error - red
            case 3: return t.colors.error || "#D32F2F"    // Critical - dark red
            default: return t.colors.error || "#F44336"
        }
    }
    
    /**
     * Show error dialog with specified parameters
     */
    function showError(code, message, severityLevel, isRetryable) {
        errorCode = code
        errorMessage = message
        severity = severityLevel
        retryable = isRetryable
        open()
    }
}
