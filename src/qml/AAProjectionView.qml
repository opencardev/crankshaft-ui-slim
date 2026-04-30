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

Item {
    id: projectionView

    property var themeManager: null
    readonly property var t: themeManager ? themeManager : fallbackTheme

    ThemeManager {
        id: fallbackTheme
    }

    QtObject {
        id: palette
        readonly property color backgroundColor: t.colors.background
        readonly property color textSecondaryColor: t.colors.textSecondary
        readonly property int spacingMedium: t.spacing.medium
        readonly property int radiusSmall: t.dimensions.borderRadiusSmall
        readonly property int fontSizeHeading: t.typography.h3
        readonly property int fontSizeSmall: t.typography.caption
    }
    
    // AndroidAutoFacade reference (set by parent)
    property var androidAutoFacade: null
    
    // TouchEventForwarder reference (set by parent or use global _touchForwarder)
    property var touchForwarder: _touchForwarder
    
    // Update touch forwarder display size when view size changes
    onWidthChanged: {
        updateTouchForwarderDisplaySize()
    }
    
    onHeightChanged: {
        updateTouchForwarderDisplaySize()
    }

    function updateTouchForwarderDisplaySize() {
        if (!touchForwarder) {
            return
        }

        var mappedWidth = projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : width
        var mappedHeight = projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : height
        touchForwarder.displaySize = Qt.size(mappedWidth, mappedHeight)

        var aaWidth = androidAutoFacade && androidAutoFacade.projectionWidth > 0
            ? androidAutoFacade.projectionWidth
            : mappedWidth
        var aaHeight = androidAutoFacade && androidAutoFacade.projectionHeight > 0
            ? androidAutoFacade.projectionHeight
            : mappedHeight
        touchForwarder.androidAutoSize = Qt.size(aaWidth, aaHeight)
    }

    Connections {
        target: androidAutoFacade
        function onProjectionFrameChanged() {
            projectionView.updateTouchForwarderDisplaySize()
        }
    }

    function mapToProjectionCoordinates(rawX, rawY) {
        var frameWidth = projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : projectionImage.width
        var frameHeight = projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : projectionImage.height

        if (frameWidth <= 0 || frameHeight <= 0) {
            return { x: 0, y: 0 }
        }

        var frameLeft = (projectionImage.width - frameWidth) / 2
        var frameTop = (projectionImage.height - frameHeight) / 2

        var localX = Math.max(0, Math.min(frameWidth - 1, rawX - frameLeft))
        var localY = Math.max(0, Math.min(frameHeight - 1, rawY - frameTop))

        return { x: localX, y: localY }
    }
    
    // Projection frame output
    Image {
        id: projectionImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: androidAutoFacade ? androidAutoFacade.projectionFrameUrl : ""
        smooth: true
        cache: false
        asynchronous: true

        onPaintedWidthChanged: projectionView.updateTouchForwarderDisplaySize()
        onPaintedHeightChanged: projectionView.updateTouchForwarderDisplaySize()
        
        // Placeholder background when no video
        Rectangle {
            anchors.fill: parent
            color: palette.backgroundColor
            visible: projectionImage.source === "" || !androidAutoFacade || !androidAutoFacade.isVideoActive
            
            Text {
                anchors.centerIn: parent
                text: qsTr("Android Auto\n\nAwaiting video stream...")
                font.pixelSize: palette.fontSizeHeading
                color: palette.textSecondaryColor
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.5
            }
        }
    }
    
    // Touch area for forwarding input to AndroidAuto
    MultiPointTouchArea {
        id: touchArea
        anchors.fill: parent
        
        // Enable multi-touch (required for gestures like pinch-zoom)
        minimumTouchPoints: 1
        maximumTouchPoints: 10
        
        onPressed: (touchPoints) => {
            forwardTouchEvent("press", touchPoints)
        }
        
        onUpdated: (touchPoints) => {
            forwardTouchEvent("move", touchPoints)
        }
        
        onReleased: (touchPoints) => {
            forwardTouchEvent("release", touchPoints)
        }
        
        onCanceled: (touchPoints) => {
            forwardTouchEvent("cancel", touchPoints)
        }
        
        function forwardTouchEvent(eventType, touchPoints) {
            if (!touchForwarder) {
                console.warn("TouchEventForwarder not available")
                return
            }
            
            // Convert touch points to array of objects
            var points = []
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i]
                var mapped = projectionView.mapToProjectionCoordinates(tp.x, tp.y)
                points.push({
                    id: tp.pointId,
                    x: mapped.x,
                    y: mapped.y,
                    pressure: tp.pressure,
                    areaWidth: tp.area.width,
                    areaHeight: tp.area.height
                })
            }
            
            // Forward to TouchEventForwarder
            touchForwarder.forwardTouchEvent(eventType, points)
        }
    }
    
    // Mouse area for desktop testing (single-point touch simulation)
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: !touchArea.enabled // Only active if touch is not available
        
        property bool isPressed: false
        
        onPressed: (mouse) => {
            isPressed = true
            sendMouseAsTouchEvent("press", mouse)
        }
        
        onPositionChanged: (mouse) => {
            if (isPressed) {
                sendMouseAsTouchEvent("move", mouse)
            }
        }
        
        onReleased: (mouse) => {
            isPressed = false
            sendMouseAsTouchEvent("release", mouse)
        }
        
        function sendMouseAsTouchEvent(eventType, mouse) {
            if (!touchForwarder) {
                console.warn("TouchEventForwarder not available")
                return
            }

            var mapped = projectionView.mapToProjectionCoordinates(mouse.x, mouse.y)
            
            // Forward to TouchEventForwarder as mouse event
            touchForwarder.forwardMouseEvent(eventType, mapped.x, mapped.y)
        }
    }
    
    // Video state indicator (top-right corner)
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: palette.spacingMedium
        width: 80
        height: 30
        radius: palette.radiusSmall
        color: androidAutoFacade && androidAutoFacade.isVideoActive 
               ? Qt.rgba(0.3, 0.8, 0.3, 0.8)  // Green
               : Qt.rgba(0.5, 0.5, 0.5, 0.8)  // Gray
        visible: androidAutoFacade !== null
        
        Text {
            anchors.centerIn: parent
            text: parent.parent.androidAutoFacade && parent.parent.androidAutoFacade.isVideoActive 
                  ? qsTr("VIDEO ON") 
                  : qsTr("NO VIDEO")
            font.pixelSize: palette.fontSizeSmall
            color: "white"
            font.bold: true
        }
    }
    
    // Audio state indicator (top-left corner)
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: palette.spacingMedium
        width: 80
        height: 30
        radius: palette.radiusSmall
        color: androidAutoFacade && androidAutoFacade.isAudioActive 
               ? Qt.rgba(0.3, 0.8, 0.3, 0.8)  // Green
               : Qt.rgba(0.5, 0.5, 0.5, 0.8)  // Gray
        visible: androidAutoFacade !== null
        
        Text {
            anchors.centerIn: parent
            text: parent.parent.androidAutoFacade && parent.parent.androidAutoFacade.isAudioActive 
                  ? qsTr("AUDIO ON") 
                  : qsTr("NO AUDIO")
            font.pixelSize: palette.fontSizeSmall
            color: "white"
            font.bold: true
        }
    }
    
    Component.onCompleted: {
        console.log("AAProjectionView loaded")
        updateTouchForwarderDisplaySize()
    }
}
