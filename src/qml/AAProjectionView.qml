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
import QtMultimedia

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
    property var androidAutoWebRtcReceiver: _androidAutoWebRtcReceiver
    readonly property bool webRtcSelected: androidAutoFacade && androidAutoFacade.videoTransportMode && androidAutoFacade.videoTransportMode.toLowerCase() === "webrtc"
    readonly property bool webRtcHealthy: androidAutoWebRtcReceiver && androidAutoWebRtcReceiver.active && androidAutoWebRtcReceiver.healthy
    readonly property bool webRtcFallbackActive: webRtcSelected && androidAutoWebRtcReceiver && androidAutoWebRtcReceiver.fallbackRecommended
    readonly property bool webRtcActive: webRtcSelected && webRtcHealthy
    
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

        var geometry = projectionGeometrySnapshot()
        if (!geometry.valid) {
            return
        }

        touchForwarder.displaySize = Qt.size(geometry.width, geometry.height)

        var aaWidth = androidAutoFacade && androidAutoFacade.projectionWidth > 0
            ? androidAutoFacade.projectionWidth
            : geometry.width
        var aaHeight = androidAutoFacade && androidAutoFacade.projectionHeight > 0
            ? androidAutoFacade.projectionHeight
            : geometry.height
        touchForwarder.androidAutoSize = Qt.size(aaWidth, aaHeight)
    }

    function projectionGeometrySnapshot() {
        var mappedWidth = webRtcActive && projectionVideoOutput.contentRect.width > 0
            ? projectionVideoOutput.contentRect.width
            : (projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : projectionImage.width)
        var mappedHeight = webRtcActive && projectionVideoOutput.contentRect.height > 0
            ? projectionVideoOutput.contentRect.height
            : (projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : projectionImage.height)

        var frameLeft = webRtcActive && projectionVideoOutput.contentRect.width > 0
            ? projectionVideoOutput.contentRect.x
            : (projectionImage.width - mappedWidth) / 2
        var frameTop = webRtcActive && projectionVideoOutput.contentRect.height > 0
            ? projectionVideoOutput.contentRect.y
            : (projectionImage.height - mappedHeight) / 2

        var valid = isFinite(mappedWidth) && isFinite(mappedHeight) &&
                    isFinite(frameLeft) && isFinite(frameTop) &&
                    mappedWidth > 0 && mappedHeight > 0

        return {
            valid: valid,
            width: mappedWidth,
            height: mappedHeight,
            left: frameLeft,
            top: frameTop
        }
    }

    function applyTouchForwarderGeometry(geometry) {
        if (!touchForwarder || !geometry || !geometry.valid) {
            return
        }

        touchForwarder.displaySize = Qt.size(geometry.width, geometry.height)

        var aaWidth = androidAutoFacade && androidAutoFacade.projectionWidth > 0
            ? androidAutoFacade.projectionWidth
            : geometry.width
        var aaHeight = androidAutoFacade && androidAutoFacade.projectionHeight > 0
            ? androidAutoFacade.projectionHeight
            : geometry.height
        touchForwarder.androidAutoSize = Qt.size(aaWidth, aaHeight)
    }

    // updateTouchForwarderDisplaySize() is called from onPaintedWidthChanged /
    // onPaintedHeightChanged on the projectionImage (below) which fire whenever
    // the rendered frame size changes.  We deliberately do NOT hook
    // onProjectionFrameChanged here: that signal fires on every decoded video
    // frame (~30 fps) and would trigger a WebSocket publish to crankshaft-core
    // at frame rate, causing GStreamer pipeline reconfiguration events that
    // produce visible HDMI flicker.

    function mapToProjectionCoordinates(rawX, rawY, geometry) {
        var snapshot = geometry ? geometry : projectionGeometrySnapshot()
        if (!snapshot.valid) {
            return { valid: false, x: 0, y: 0 }
        }

        var localX = Math.max(0, Math.min(snapshot.width - 1, rawX - snapshot.left))
        var localY = Math.max(0, Math.min(snapshot.height - 1, rawY - snapshot.top))

        return { valid: true, x: localX, y: localY }
    }
    
    VideoOutput {
        id: projectionVideoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: webRtcActive
        videoSink: androidAutoWebRtcReceiver ? androidAutoWebRtcReceiver.videoSinkObject : null

        onContentRectChanged: projectionView.updateTouchForwarderDisplaySize()
    }

    // Projection frame output fallback
    Image {
        id: projectionImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: androidAutoFacade ? androidAutoFacade.projectionFrameUrl : ""
        smooth: true
        cache: false
        // Decode each frame synchronously to avoid blanking between rapidly
        // changing data URLs on the projection surface.
        asynchronous: false
        visible: (!webRtcActive || webRtcFallbackActive) && source !== ""

        onPaintedWidthChanged: projectionView.updateTouchForwarderDisplaySize()
        onPaintedHeightChanged: projectionView.updateTouchForwarderDisplaySize()
        
        // Placeholder background when no video
        Rectangle {
            anchors.fill: parent
            color: palette.backgroundColor
            visible: (!webRtcActive && (projectionImage.source === "" || !androidAutoFacade || !androidAutoFacade.isVideoActive)) ||
                     (webRtcActive && projectionVideoOutput.contentRect.width <= 0)
            
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
        property bool hasSeenTouchPointEvents: false
        
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

            var geometry = projectionView.projectionGeometrySnapshot()
            if (!geometry.valid) {
                console.warn("[AA][touchCapture] dropping " + eventType + " due to invalid projection geometry")
                return
            }

            projectionView.applyTouchForwarderGeometry(geometry)

            if (eventType === "press" && touchPoints.length > 0) {
                hasSeenTouchPointEvents = true
            }
            
            // Convert touch points to array of objects
            var points = []
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i]
                var mapped = projectionView.mapToProjectionCoordinates(tp.x, tp.y, geometry)
                if (!mapped.valid) {
                    continue
                }
                points.push({
                    id: tp.pointId,
                    x: mapped.x,
                    y: mapped.y,
                    pressure: tp.pressure,
                    areaWidth: tp.area.width,
                    areaHeight: tp.area.height
                })
            }

            if (points.length === 0) {
                console.warn("[AA][touchCapture] dropping " + eventType + " due to empty mapped touch points")
                return
            }
            
            // Forward to TouchEventForwarder
            touchForwarder.forwardTouchEvent(eventType, points)

            if (eventType === "press") {
                console.debug("[AA][touchCapture] AAProjectionView touchpoints active count=" + points.length)
            }
        }
    }
    
    // Mouse area for desktop testing (single-point touch simulation)
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: touchForwarder && !touchArea.hasSeenTouchPointEvents
        
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

            var geometry = projectionView.projectionGeometrySnapshot()
            if (!geometry.valid) {
                console.warn("[AA][mouseCapture] dropping " + eventType + " due to invalid projection geometry")
                return
            }

            projectionView.applyTouchForwarderGeometry(geometry)

            var mapped = projectionView.mapToProjectionCoordinates(mouse.x, mouse.y, geometry)
            if (!mapped.valid) {
                return
            }
            
            // Forward to TouchEventForwarder as mouse event
            touchForwarder.forwardMouseEvent(eventType, mapped.x, mapped.y)
        }
    }

    onVisibleChanged: {
        if (!visible) {
            touchArea.hasSeenTouchPointEvents = false
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
