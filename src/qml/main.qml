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
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import "components"

ApplicationWindow {
    id: root
    visible: true
    width: 800
    height: 480
    title: qsTr("Crankshaft Slim UI - AndroidAuto")
    property string lastShownConnectionError: ""
    property int displayRotation: 0
    // Configurable delay before AA projection switches the app window to fullscreen.
    // 0 means disabled.
    property int aaProjectionFullscreenDelaySeconds: 0
    property bool fullscreenDelayPending: false
    property int fullscreenCountdownSeconds: 0
    readonly property bool immersiveProjectionMode:
        root.visibility === Window.FullScreen &&
        navigationController.currentViewState === navigationController.viewStateAAProjection &&
        !navigationController.settingsPanelVisible
    
    // Theme Manager - centralized theme control
    ThemeManager {
        id: theme
    }
    
    // View Navigation Controller - manages AA/Settings/Connection views
    ViewNavigationController {
        id: navigationController
    }

    // FontLoader-based Material icon support.
    IconFontManager {
        id: iconFonts
    }
    
    // Error Dialog - displays errors from ErrorHandler
    ErrorDialog {
        id: errorDialog
        themeManager: theme
        overlayColor: theme.colors.overlay
    }

    ReconnectionPrompt {
        id: reconnectionPrompt
        themeManager: theme
        overlayColor: theme.colors.overlay
        connectionStateMachine: _connectionStateMachine
        onManualConnectRequested: {
            if (_connectionStateMachine) {
                _connectionStateMachine.startConnection()
            }
        }
    }
    
    // Watch for theme mode changes from PreferencesFacade
    Connections {
        target: _preferencesFacade
        function onThemeModeChanged(mode) {
            theme.setTheme(mode)
        }
        function onDisplayRotationChanged(rotation) {
            root.displayRotation = rotation
        }
        function onAaProjectionFullscreenDelaySecondsChanged(delaySeconds) {
            root.aaProjectionFullscreenDelaySeconds = delaySeconds
            root.updateFullscreenModeForCurrentState()
        }
    }
    
    // Watch for errors from ErrorHandler
    Connections {
        target: _errorHandler
        function onErrorOccurred(code, message, severity, retryable) {
            errorDialog.showError(code, message, severity, retryable)
        }
    }
    
    // Initialize theme from preferences on startup
    Component.onCompleted: {
        if (_preferencesFacade) {
            theme.setTheme(_preferencesFacade.themeMode)
            displayRotation = _preferencesFacade.displayRotation
            aaProjectionFullscreenDelaySeconds =
                _preferencesFacade.aaProjectionFullscreenDelaySeconds
        }

        if (_connectionStateMachine) {
            _connectionStateMachine.startConnection()
            updateViewForConnectionState(_connectionStateMachine.currentState)
        } else {
            navigationController.showConnectionStatus("", "", true)
        }
    }

    function updateViewForConnectionState(state) {
        if (state === 3) { // Connected
            navigationController.showAAProjection()
            updateFullscreenModeForCurrentState()
            return
        }

        if (state === 4) { // Error
            navigationController.showConnectionStatus("", "", true)
            updateFullscreenModeForCurrentState()
            if (_connectionStateMachine && _connectionStateMachine.lastError !== "" &&
                    _connectionStateMachine.lastError !== lastShownConnectionError) {
                lastShownConnectionError = _connectionStateMachine.lastError
                errorDialog.showError("CORE_CONNECTION", _connectionStateMachine.lastError, 2, true)
            }
            return
        }

        // Disconnected / Searching / Connecting
        navigationController.showConnectionStatus("", "", false)
        updateFullscreenModeForCurrentState()
    }

    Connections {
        target: _connectionStateMachine

        function onCurrentStateChanged(state) {
            updateViewForConnectionState(state)
        }

        function onLastErrorChanged(error) {
            if (!error || error === "") {
                return
            }

            if (error !== lastShownConnectionError) {
                lastShownConnectionError = error
                errorDialog.showError("CORE_CONNECTION", error, 2, true)
            }
        }

        function onMaxRetriesReached() {
            if (!reconnectionPrompt.visible) {
                reconnectionPrompt.open()
            }
        }

        function onConnectionRecovered() {
            if (reconnectionPrompt.visible) {
                reconnectionPrompt.close()
            }
        }
    }

    Connections {
        target: navigationController

        function onSettingsPanelVisibleChanged() {
            if (navigationController.settingsPanelVisible) {
                settingsPanel.open()
            } else {
                settingsPanel.close()
            }
            // Leaving projection-only mode should always cancel pending fullscreen.
            root.updateFullscreenModeForCurrentState()
        }

        function onCurrentViewStateChanged() {
            root.updateFullscreenModeForCurrentState()
        }
    }

    Timer {
        id: delayedFullscreenTimer
        repeat: false
        onTriggered: {
            root.fullscreenDelayPending = false
            root.fullscreenCountdownSeconds = 0
            // Final guard: only enter fullscreen if we are still on projection view.
            if (navigationController.currentViewState === navigationController.viewStateAAProjection &&
                    !navigationController.settingsPanelVisible) {
                root.visibility = Window.FullScreen
            }
        }
    }

    Timer {
        id: fullscreenCountdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (root.fullscreenCountdownSeconds > 0) {
                root.fullscreenCountdownSeconds -= 1
            }
            if (root.fullscreenCountdownSeconds <= 0) {
                stop()
            }
        }
    }

    function cancelDelayedFullscreen() {
        delayedFullscreenTimer.stop()
        fullscreenCountdownTimer.stop()
        fullscreenDelayPending = false
        fullscreenCountdownSeconds = 0
    }

    function scheduleDelayedFullscreen() {
        cancelDelayedFullscreen()

        if (aaProjectionFullscreenDelaySeconds <= 0) {
            return
        }

        fullscreenDelayPending = true
        fullscreenCountdownSeconds = aaProjectionFullscreenDelaySeconds
        delayedFullscreenTimer.interval = aaProjectionFullscreenDelaySeconds * 1000
        delayedFullscreenTimer.start()
        fullscreenCountdownTimer.start()
    }

    function updateFullscreenModeForCurrentState() {
        const isProjectionOnlyView =
            navigationController.currentViewState === navigationController.viewStateAAProjection &&
            !navigationController.settingsPanelVisible

        if (!isProjectionOnlyView) {
            cancelDelayedFullscreen()
            root.visibility = Window.Windowed
            return
        }

        // Keep fullscreen if already active, but restart the delayed transition if not.
        if (root.visibility !== Window.FullScreen) {
            scheduleDelayedFullscreen()
        }
    }

    Material.theme: theme.currentMode === "LIGHT" ? Material.Light : Material.Dark
    Material.primary: theme.colors.primary
    Material.accent: theme.colors.secondary
    
    color: theme.colors.background
    
    
    // Main content area with state-based view switching
    Item {
        id: mainContent
        anchors.centerIn: parent
        width: (root.displayRotation === 90 || root.displayRotation === 270) ? parent.height : parent.width
        height: (root.displayRotation === 90 || root.displayRotation === 270) ? parent.width : parent.height
        clip: true

        transform: Rotation {
            angle: root.displayRotation
            origin.x: mainContent.width / 2
            origin.y: mainContent.height / 2
        }
        
        // Background
        Rectangle {
            anchors.fill: parent
            color: theme.colors.background
        }
        
        // Loading view
        Rectangle {
            id: loadingView
            anchors.fill: parent
            color: theme.colors.background
            visible: navigationController.currentViewState === navigationController.viewStateLoading
            
            ColumnLayout {
                anchors.centerIn: parent
                spacing: theme.spacing.large
                
                // Loading indicator
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: navigationController.currentViewState === navigationController.viewStateLoading
                    width: 64
                    height: 64
                }
                
                // Status text
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Slim AndroidAuto UI")
                    color: theme.colors.textPrimary
                    font.pixelSize: theme.typography.h3
                    font.family: theme.typography.fontFamily
                }
                
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: navigationController.currentViewState === navigationController.viewStateLoading ?
                          "Initializing..." : "Waiting for connection..."
                    color: theme.colors.textSecondary
                    font.pixelSize: theme.typography.body1
                    font.family: theme.typography.fontFamily
                }
            }
        }

        ConnectionStatusView {
            id: connectionStatusView
            anchors.fill: parent
            visible: navigationController.currentViewState === navigationController.viewStateConnectionStatus
            themeManager: theme
            androidAutoFacade: _androidAutoFacade
        }
        
        // AndroidAuto Projection View (primary)
        Rectangle {
            id: aaProjectionView
            anchors.fill: parent
            color: theme.colors.background
            visible: navigationController.currentViewState === navigationController.viewStateAAProjection ||
                     navigationController.currentViewState === navigationController.viewStateSettings
            
            // Placeholder for actual AA projection content
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                
                // Toolbar
                Rectangle {
                    visible: !root.immersiveProjectionMode
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? theme.dimensions.toolbarHeight : 0
                    height: visible ? theme.dimensions.toolbarHeight : 0
                    color: theme.colors.surface
                    border.color: theme.colors.border
                    border.width: 1
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: theme.spacing.small
                        spacing: theme.spacing.medium
                        
                        Text {
                            Layout.fillWidth: true
                            // Reflect delayed fullscreen state directly in projection header.
                            text: root.fullscreenDelayPending
                                  ? qsTr("AndroidAuto Projection (fullscreen in %1s)")
                                        .arg(root.fullscreenCountdownSeconds)
                                  : qsTr("AndroidAuto Projection")
                            color: theme.colors.textPrimary
                            font.pixelSize: theme.typography.body1
                            font.family: theme.typography.fontFamily
                        }

                        Button {
                            id: settingsButton
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: theme.dimensions.recommendedTouchTarget
                            Layout.preferredHeight: theme.dimensions.recommendedTouchTarget

                            contentItem: Text {
                                text: iconFonts.loadedFamily !== "" ? iconFonts.settingsGlyph : "\u2699"
                                color: theme.colors.textPrimary
                                font.family: iconFonts.loadedFamily !== "" ? iconFonts.family : theme.typography.fontFamily
                                font.pixelSize: 24
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Settings")

                            onClicked: navigationController.toggleSettings()

                            background: Rectangle {
                                color: settingsButton.pressed ? theme.colors.primaryVariant : theme.colors.surface
                                border.color: theme.colors.border
                                border.width: 1
                                radius: theme.dimensions.borderRadiusMedium
                            }
                        }
                    }
                }
                
                // AA Content area (would show actual projection here)
                Rectangle {
                    id: projectionSurface
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: theme.colors.background
                    readonly property bool webRtcSelected: _androidAutoFacade && _androidAutoFacade.videoTransportMode && _androidAutoFacade.videoTransportMode.toLowerCase() === "webrtc"
                    readonly property bool webRtcHealthy: _androidAutoWebRtcReceiver && _androidAutoWebRtcReceiver.active && _androidAutoWebRtcReceiver.healthy
                    readonly property bool webRtcFallbackRequested: webRtcSelected && _androidAutoWebRtcReceiver && _androidAutoWebRtcReceiver.fallbackRecommended
                    readonly property int webRtcFallbackDelayMs: 2500
                    property bool webRtcDisplayLatched: false
                    readonly property bool webRtcActive: webRtcSelected && webRtcDisplayLatched
                    readonly property bool webRtcFallbackActive: webRtcSelected && !webRtcActive && webRtcFallbackRequested

                    function logRenderState(reason) {
                        console.log("[AAProjectionView] render-state reason=" + reason +
                                    " selected=" + webRtcSelected +
                                    " healthy=" + webRtcHealthy +
                                    " fallbackRequested=" + webRtcFallbackRequested +
                                    " latched=" + webRtcDisplayLatched +
                                    " webrtcActive=" + webRtcActive +
                                    " fallbackActive=" + webRtcFallbackActive)
                    }

                    function recomputeRenderMode(reason) {
                        if (!webRtcSelected) {
                            if (webRtcFallbackDelayTimer.running) {
                                webRtcFallbackDelayTimer.stop()
                            }
                            if (webRtcDisplayLatched) {
                                webRtcDisplayLatched = false
                                logRenderState(reason + ":transport-not-webrtc")
                            }
                            return
                        }

                        if (webRtcHealthy) {
                            if (webRtcFallbackDelayTimer.running) {
                                webRtcFallbackDelayTimer.stop()
                            }
                            if (!webRtcDisplayLatched) {
                                webRtcDisplayLatched = true
                            }
                            logRenderState(reason + ":healthy")
                            return
                        }

                        if (webRtcDisplayLatched && webRtcFallbackRequested) {
                            if (!webRtcFallbackDelayTimer.running) {
                                webRtcFallbackDelayTimer.start()
                                logRenderState(reason + ":fallback-delay-start")
                            }
                            return
                        }

                        if (!webRtcDisplayLatched && webRtcFallbackRequested) {
                            logRenderState(reason + ":fallback-active")
                        }
                    }

                    Timer {
                        id: webRtcFallbackDelayTimer
                        interval: projectionSurface.webRtcFallbackDelayMs
                        repeat: false
                        running: false
                        onTriggered: {
                            if (projectionSurface.webRtcSelected &&
                                projectionSurface.webRtcDisplayLatched &&
                                projectionSurface.webRtcFallbackRequested &&
                                !projectionSurface.webRtcHealthy) {
                                projectionSurface.webRtcDisplayLatched = false
                                projectionSurface.logRenderState("fallback-delay-triggered")
                            }
                        }
                    }

                    onWebRtcSelectedChanged: recomputeRenderMode("webRtcSelectedChanged")
                    onWebRtcHealthyChanged: recomputeRenderMode("webRtcHealthyChanged")
                    onWebRtcFallbackRequestedChanged: recomputeRenderMode("webRtcFallbackRequestedChanged")
                    onWebRtcDisplayLatchedChanged: logRenderState("webRtcDisplayLatchedChanged")
                    Component.onCompleted: recomputeRenderMode("componentCompleted")

                    Connections {
                        target: _androidAutoWebRtcReceiver
                        ignoreUnknownSignals: true

                        function onActiveChanged() {
                            projectionSurface.recomputeRenderMode("receiver.activeChanged")
                        }

                        function onHealthyChanged() {
                            projectionSurface.recomputeRenderMode("receiver.healthyChanged")
                        }

                        function onStalledChanged() {
                            projectionSurface.logRenderState("receiver.stalledChanged")
                        }

                        function onRecoverableErrorChanged() {
                            projectionSurface.logRenderState("receiver.recoverableErrorChanged")
                        }

                        function onFallbackRecommendedChanged() {
                            projectionSurface.recomputeRenderMode("receiver.fallbackRecommendedChanged")
                        }
                    }

                    function videoContentRect() {
                        if (projectionVideoLoader.item && projectionVideoLoader.item.contentRect) {
                            return projectionVideoLoader.item.contentRect
                        }
                        return Qt.rect(0, 0, 0, 0)
                    }

                    function updateTouchForwarderDisplaySize() {
                        if (!_touchForwarder) {
                            return
                        }

                        var videoRect = videoContentRect()
                        var mappedWidth = webRtcActive && videoRect.width > 0
                            ? videoRect.width
                            : (projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : projectionImage.width)
                        var mappedHeight = webRtcActive && videoRect.height > 0
                            ? videoRect.height
                            : (projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : projectionImage.height)
                        _touchForwarder.displaySize = Qt.size(mappedWidth, mappedHeight)

                        var aaWidth = _androidAutoFacade && _androidAutoFacade.projectionWidth > 0
                            ? _androidAutoFacade.projectionWidth
                            : mappedWidth
                        var aaHeight = _androidAutoFacade && _androidAutoFacade.projectionHeight > 0
                            ? _androidAutoFacade.projectionHeight
                            : mappedHeight
                        _touchForwarder.androidAutoSize = Qt.size(aaWidth, aaHeight)
                    }

                    function mapToProjectionCoordinates(rawX, rawY) {
                        var videoRect = videoContentRect()
                        var frameWidth = webRtcActive && videoRect.width > 0
                            ? videoRect.width
                            : (projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : projectionImage.width)
                        var frameHeight = webRtcActive && videoRect.height > 0
                            ? videoRect.height
                            : (projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : projectionImage.height)

                        if (frameWidth <= 0 || frameHeight <= 0) {
                            return { x: 0, y: 0 }
                        }

                        var frameLeft = webRtcActive && videoRect.width > 0
                            ? videoRect.x
                            : (projectionImage.width - frameWidth) / 2
                        var frameTop = webRtcActive && videoRect.height > 0
                            ? videoRect.y
                            : (projectionImage.height - frameHeight) / 2

                        var localX = Math.max(0, Math.min(frameWidth - 1, rawX - frameLeft))
                        var localY = Math.max(0, Math.min(frameHeight - 1, rawY - frameTop))

                        return { x: localX, y: localY }
                    }

                    focus: visible
                    onVisibleChanged: {
                        if (visible) {
                            forceActiveFocus()
                            updateTouchForwarderDisplaySize()
                        } else {
                            projectionTouchArea.hasSeenTouchPointEvents = false
                        }
                    }

                    function qtKeyToAndroidKeyCode(key) {
                        if (key >= Qt.Key_A && key <= Qt.Key_Z) {
                            return 29 + (key - Qt.Key_A)
                        }
                        if (key >= Qt.Key_0 && key <= Qt.Key_9) {
                            return 7 + (key - Qt.Key_0)
                        }

                        switch (key) {
                        case Qt.Key_Back:
                        case Qt.Key_Escape:
                            return 4
                        case Qt.Key_Home:
                            return 3
                        case Qt.Key_Up:
                            return 19
                        case Qt.Key_Down:
                            return 20
                        case Qt.Key_Left:
                            return 21
                        case Qt.Key_Right:
                            return 22
                        case Qt.Key_Return:
                        case Qt.Key_Enter:
                            return 66
                        case Qt.Key_Backspace:
                            return 67
                        case Qt.Key_Tab:
                            return 61
                        case Qt.Key_Space:
                            return 62
                        case Qt.Key_Comma:
                            return 55
                        case Qt.Key_Period:
                            return 56
                        case Qt.Key_Minus:
                            return 69
                        case Qt.Key_Equal:
                            return 70
                        case Qt.Key_BracketLeft:
                            return 71
                        case Qt.Key_BracketRight:
                            return 72
                        case Qt.Key_Backslash:
                            return 73
                        case Qt.Key_Semicolon:
                            return 74
                        case Qt.Key_Apostrophe:
                            return 75
                        case Qt.Key_Slash:
                            return 76
                        case Qt.Key_Plus:
                            return 81
                        case Qt.Key_QuoteLeft:
                            return 68
                        case Qt.Key_Delete:
                            return 112
                        default:
                            return -1
                        }
                    }

                    function forwardKeyboardKey(action, event) {
                        if (!_touchForwarder) {
                            return false
                        }

                        var keyCode = qtKeyToAndroidKeyCode(event.key)
                        if (keyCode < 0) {
                            return false
                        }

                        _touchForwarder.forwardKeyEvent("", action, keyCode)
                        return true
                    }

                    Keys.onReleased: (event) => {
                        if (!_touchForwarder) {
                            return
                        }

                        if (event.isAutoRepeat) {
                            return
                        }

                        if (forwardKeyboardKey("up", event)) {
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
                            _touchForwarder.forwardKeyEvent("BACK")
                            event.accepted = true
                        } else if (event.key === Qt.Key_Home) {
                            _touchForwarder.forwardKeyEvent("HOME")
                            event.accepted = true
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.isAutoRepeat) {
                            return
                        }

                        if (forwardKeyboardKey("down", event)) {
                            event.accepted = true
                        }
                    }
                    
                    Loader {
                        id: projectionVideoLoader
                        anchors.fill: parent
                        anchors.margins: root.immersiveProjectionMode ? 0 : theme.spacing.small
                        active: projectionSurface.webRtcActive
                        source: "qrc:/qml/components/WebRtcVideoOutput.qml"

                        onLoaded: {
                            if (item) {
                                item.sinkObject = _androidAutoWebRtcReceiver ? _androidAutoWebRtcReceiver.videoSinkObject : null
                                projectionSurface.updateTouchForwarderDisplaySize()
                            }
                        }

                        onItemChanged: {
                            projectionSurface.updateTouchForwarderDisplaySize()
                        }

                        onActiveChanged: {
                            projectionSurface.logRenderState("projectionVideoLoader.activeChanged")
                        }
                    }

                    Connections {
                        target: projectionVideoLoader.item
                        ignoreUnknownSignals: true
                        function onContentRectChanged() {
                            projectionSurface.updateTouchForwarderDisplaySize()
                        }
                    }

                    Image {
                        id: projectionImage
                        anchors.fill: parent
                        anchors.margins: root.immersiveProjectionMode ? 0 : theme.spacing.small
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        cache: false
                        source: _androidAutoFacade ? _androidAutoFacade.projectionFrameUrl : ""
                        // Keep the frame swap synchronous so the inline projection
                        // surface does not flash between successive video frames.
                        asynchronous: false
                        visible: !projectionSurface.webRtcActive && source !== ""

                        onPaintedWidthChanged: projectionSurface.updateTouchForwarderDisplaySize()
                        onPaintedHeightChanged: projectionSurface.updateTouchForwarderDisplaySize()

                        onVisibleChanged: {
                            projectionSurface.logRenderState("projectionImage.visibleChanged")
                        }
                    }

                    // Note: onProjectionFrameChanged is intentionally NOT connected here.
                    // That signal fires on every decoded video frame (~30 fps).  Calling
                    // updateTouchForwarderDisplaySize() at frame rate publishes a WebSocket
                    // 'android-auto/display/resolution' message to crankshaft-core on every
                    // frame, which triggers GStreamer pipeline reconfiguration events that
                    // cause visible HDMI flicker.  Genuine resize events are covered by the
                    // onPaintedWidth/HeightChanged handlers above.

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("AndroidAuto Projection\nAwaiting video stream...")
                        color: theme.colors.textSecondary
                        font.pixelSize: theme.typography.h4
                        font.family: theme.typography.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                        visible: (!projectionSurface.webRtcActive && !projectionImage.visible) ||
                                 (projectionSurface.webRtcActive && projectionSurface.videoContentRect().width <= 0)
                    }

                    MultiPointTouchArea {
                        id: projectionTouchArea
                        anchors.fill: parent
                        minimumTouchPoints: 1
                        maximumTouchPoints: 10

                        property bool hasSeenTouchPointEvents: false

                        onPressed: (touchPoints) => forwardTouchEvent("press", touchPoints)
                        onUpdated: (touchPoints) => forwardTouchEvent("move", touchPoints)
                        onReleased: (touchPoints) => forwardTouchEvent("release", touchPoints)
                        onCanceled: (touchPoints) => forwardTouchEvent("cancel", touchPoints)

                        function forwardTouchEvent(eventType, touchPoints) {
                            if (!_touchForwarder) {
                                return
                            }

                            if (eventType === "press" && touchPoints.length > 0) {
                                hasSeenTouchPointEvents = true
                            }

                            var points = []
                            for (var i = 0; i < touchPoints.length; i++) {
                                var tp = touchPoints[i]
                                var mapped = projectionSurface.mapToProjectionCoordinates(tp.x, tp.y)
                                points.push({
                                    id: tp.pointId,
                                    x: mapped.x,
                                    y: mapped.y,
                                    pressure: tp.pressure,
                                    areaWidth: tp.area.width,
                                    areaHeight: tp.area.height
                                })
                            }

                            _touchForwarder.forwardTouchEvent(eventType, points)

                            if (eventType === "press") {
                                console.debug("[AA][touchCapture] projection touchpoints active count=" + points.length)
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: _touchForwarder && !projectionTouchArea.hasSeenTouchPointEvents

                        property bool isPressed: false

                        onPressed: (mouse) => {
                            isPressed = true
                            if (_touchForwarder) {
                                var mapped = projectionSurface.mapToProjectionCoordinates(mouse.x, mouse.y)
                                _touchForwarder.forwardMouseEvent("press", mapped.x, mapped.y)
                            }
                        }

                        onPositionChanged: (mouse) => {
                            if (isPressed && _touchForwarder) {
                                var mapped = projectionSurface.mapToProjectionCoordinates(mouse.x, mouse.y)
                                _touchForwarder.forwardMouseEvent("move", mapped.x, mapped.y)
                            }
                        }

                        onReleased: (mouse) => {
                            isPressed = false
                            if (_touchForwarder) {
                                var mapped = projectionSurface.mapToProjectionCoordinates(mouse.x, mouse.y)
                                _touchForwarder.forwardMouseEvent("release", mapped.x, mapped.y)
                            }
                        }
                    }

                }
            }
        }
        
        // Settings Panel Overlay (T054-T055)
        SettingsPanel {
            id: settingsPanel
            displayRotation: root.displayRotation
            onClosed: navigationController.closeSettings()
        }
    }
}

