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
import QtMultimedia
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
    // Touch diagnostics are disabled by default. Enable with --touch-debug or F12.
    property bool touchDebugOverlay: _touchDebugEnabled === true || Qt.application.arguments.indexOf("--touch-debug") >= 0
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
                    readonly property bool h264Selected: _androidAutoFacade && _androidAutoFacade.videoTransportMode && _androidAutoFacade.videoTransportMode.toLowerCase() === "websocket-h264"
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

                    function projectionFrameSize() {
                        var width = _androidAutoFacade ? _androidAutoFacade.projectionWidth : 0
                        var height = _androidAutoFacade ? _androidAutoFacade.projectionHeight : 0
                        if (width > 0 && height > 0) {
                            return Qt.size(width, height)
                        }

                        var videoRect = videoContentRect()
                        if ((webRtcActive || h264Selected) && videoRect.width > 0 && videoRect.height > 0) {
                            return Qt.size(Math.round(videoRect.width), Math.round(videoRect.height))
                        }

                        return Qt.size(Math.round(projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : projectionImage.width),
                                       Math.round(projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : projectionImage.height))
                    }

                    function projectionFrameRect() {
                        var videoRect = videoContentRect()
                        var frameWidth = (webRtcActive || h264Selected) && videoRect.width > 0
                            ? videoRect.width
                            : (projectionImage.paintedWidth > 0 ? projectionImage.paintedWidth : projectionImage.width)
                        var frameHeight = (webRtcActive || h264Selected) && videoRect.height > 0
                            ? videoRect.height
                            : (projectionImage.paintedHeight > 0 ? projectionImage.paintedHeight : projectionImage.height)
                        if (frameWidth <= 0 || frameHeight <= 0) {
                            return Qt.rect(0, 0, 0, 0)
                        }
                        var frameLeft = (webRtcActive || h264Selected) && videoRect.width > 0
                            ? videoRect.x
                            : (projectionImage.width - frameWidth) / 2
                        var frameTop = (webRtcActive || h264Selected) && videoRect.height > 0
                            ? videoRect.y
                            : (projectionImage.height - frameHeight) / 2
                        return Qt.rect(frameLeft, frameTop, frameWidth, frameHeight)
                    }

                    function updateTouchForwarderDisplaySize() {
                        if (!_touchForwarder) {
                            return
                        }

                        // displaySize is the coordinate space of the projection image,
                        // not its painted QML size.  TouchEventForwarder scales this
                        // native projection space to the published AA touch space.
                        var frameSize = projectionFrameSize()
                        if (frameSize.width > 0 && frameSize.height > 0) {
                            _touchForwarder.displaySize = frameSize
                        }
                    }

                    function mapToProjectionCoordinates(rawX, rawY) {
                        var rect = projectionFrameRect()
                        var nativeSize = projectionFrameSize()
                        if (rect.width <= 0 || rect.height <= 0 ||
                            nativeSize.width <= 0 || nativeSize.height <= 0) {
                            return { x: 0, y: 0 }
                        }

                        // Stage 1: QML surface -> actual painted video rectangle.
                        // Stage 2: painted rectangle -> native AA video frame.
                        var localX = Math.max(0, Math.min(rect.width - 1, rawX - rect.x))
                        var localY = Math.max(0, Math.min(rect.height - 1, rawY - rect.y))
                        return {
                            x: localX * nativeSize.width / rect.width,
                            y: localY * nativeSize.height / rect.height
                        }
                    }

                    function debugAaCoordinates(nativeX, nativeY) {
                        var nativeSize = projectionFrameSize()
                        var aa = _touchForwarder ? _touchForwarder.androidAutoSize : Qt.size(0, 0)
                        if (nativeSize.width <= 0 || nativeSize.height <= 0 ||
                            aa.width <= 0 || aa.height <= 0) {
                            return { x: -1, y: -1 }
                        }

                        return {
                            x: nativeX * aa.width / nativeSize.width,
                            y: nativeY * aa.height / nativeSize.height
                        }
                    }

                    function debugAaPointInFrame(nativeX, nativeY) {
                        var nativeSize = projectionFrameSize()
                        var rect = projectionFrameRect()
                        if (nativeSize.width <= 0 || nativeSize.height <= 0 ||
                            rect.width <= 0 || rect.height <= 0) {
                            return { x: -1, y: -1 }
                        }

                        return {
                            x: rect.x + nativeX * rect.width / nativeSize.width,
                            y: rect.y + nativeY * rect.height / nativeSize.height
                        }
                    }

                    function debugAaPointBackInFrame(aaX, aaY) {
                        var nativeSize = projectionFrameSize()
                        var aa = _touchForwarder ? _touchForwarder.androidAutoSize : Qt.size(0, 0)
                        var rect = projectionFrameRect()
                        if (nativeSize.width <= 0 || nativeSize.height <= 0 ||
                            aa.width <= 0 || aa.height <= 0 ||
                            rect.width <= 0 || rect.height <= 0) {
                            return { x: -1, y: -1 }
                        }

                        var nativeX = aaX * nativeSize.width / aa.width
                        var nativeY = aaY * nativeSize.height / aa.height
                        return {
                            x: rect.x + nativeX * rect.width / nativeSize.width,
                            y: rect.y + nativeY * rect.height / nativeSize.height
                        }
                    }

                    function debugAaAspectGuide() {
                        var rect = projectionFrameRect()
                        var aa = _touchForwarder ? _touchForwarder.androidAutoSize : Qt.size(0, 0)
                        if (rect.width <= 0 || rect.height <= 0 || aa.width <= 0 || aa.height <= 0) {
                            return Qt.rect(0, 0, 0, 0)
                        }

                        // Diagnostic only: shows where a 16:9 AA space would fit
                        // inside the native frame. It does not alter touch mapping.
                        var aaAspect = aa.width / aa.height
                        var frameAspect = rect.width / rect.height
                        if (frameAspect > aaAspect) {
                            var guideWidth = rect.height * aaAspect
                            return Qt.rect(rect.x + (rect.width - guideWidth) / 2,
                                           rect.y, guideWidth, rect.height)
                        }

                        var guideHeight = rect.width / aaAspect
                        return Qt.rect(rect.x,
                                       rect.y + (rect.height - guideHeight) / 2,
                                       rect.width, guideHeight)
                    }

                    property real debugRawX: -1
                    property real debugRawY: -1
                    property real debugMappedX: -1
                    property real debugMappedY: -1
                    property string debugEventType: ""

                    function updateTouchDebug(rawX, rawY, eventType, mapped) {
                        if (!root.touchDebugOverlay) {
                            return
                        }
                        // The diagnostic overlay must not turn touch moves into a
                        // second high-frequency render/logging workload on RPi3.
                        // Keep press/release/cancel immediate, but sample move updates
                        // at <=10 Hz.  The actual touch event is still forwarded at full
                        // rate by TouchEventForwarder.
                        var nowMs = Date.now()
                        if (eventType === "move" && (nowMs - debugLastMoveUpdateMs) < 100) {
                            return
                        }
                        debugLastMoveUpdateMs = nowMs

                        debugRawX = rawX
                        debugRawY = rawY
                        debugMappedX = mapped.x
                        debugMappedY = mapped.y
                        debugEventType = eventType

                        var aa = debugAaCoordinates(mapped.x, mapped.y)
                        console.log("[TouchDebug] event=" + eventType +
                                    " raw=" + rawX.toFixed(2) + "," + rawY.toFixed(2) +
                                    " frameRect=" + projectionFrameRect().x.toFixed(2) + "," +
                                        projectionFrameRect().y.toFixed(2) + " " +
                                        projectionFrameRect().width.toFixed(2) + "x" +
                                        projectionFrameRect().height.toFixed(2) +
                                    " native=" + mapped.x.toFixed(2) + "," + mapped.y.toFixed(2) +
                                    " aa=" + aa.x.toFixed(2) + "," + aa.y.toFixed(2))
                    }

                    focus: visible
                    onVisibleChanged: {
                        if (visible) {
                            forceActiveFocus()
                            updateTouchForwarderDisplaySize()
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

                        if (event.key === Qt.Key_F12) {
                            root.touchDebugOverlay = !root.touchDebugOverlay
                            console.log("[TouchDebug] overlay=" + root.touchDebugOverlay)
                            event.accepted = true
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

                    VideoOutput {
                        id: h264VideoOutput
                        anchors.fill: parent
                        fillMode: VideoOutput.PreserveAspectFit
                        visible: projectionSurface.h264Selected

                        Component.onCompleted: {
          console.log("[H264-DIAG] VideoOutput completed; videoSink=", videoSink,
                      "contentRect=", contentRect)
          if (_androidAutoFacade) {
              _androidAutoFacade.setProjectionVideoSink(videoSink)
              _androidAutoFacade.logProjectionVideoSinkState()
          } else {
              console.log("[H264-DIAG] _androidAutoFacade is null")
          }
      }

      onContentRectChanged: {
          console.log("[H264-DIAG] contentRectChanged:", contentRect)
          projectionSurface.updateTouchForwarderDisplaySize()
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
                        // The fallback image is already decoded to a small AA frame.
                        // Avoid Qt Quick filtering on the RPi3 render path; this reduces
                        // GPU work while touch/input and JPEG upload are active.
                        smooth: false
                        cache: false
                        source: _androidAutoFacade ? _androidAutoFacade.projectionFrameUrl : ""
                        // Decode incoming JPEG fallback frames off the QML GUI thread
                        // and retain the last completed frame while the next frame
                        // is loading.  On low-power hardware (notably RPi3),
                        // synchronous base64/JPEG decoding can block the render loop
                        // and make the projection visibly flicker between frames.
                        asynchronous: true
                        retainWhileLoading: true
                        visible: !projectionSurface.webRtcActive && !projectionSurface.h264Selected && source !== ""

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
                        visible: (!projectionSurface.webRtcActive && !projectionSurface.h264Selected && !projectionImage.visible) ||
                                 (projectionSurface.webRtcActive && projectionSurface.videoContentRect().width <= 0)
                    }

                    MultiPointTouchArea {
                        id: projectionTouchArea
                        anchors.fill: parent
                        minimumTouchPoints: 1
                        maximumTouchPoints: 10

                        onPressed: (touchPoints) => forwardTouchEvent("press", touchPoints)
                        onUpdated: (touchPoints) => forwardTouchEvent("move", touchPoints)
                        onReleased: (touchPoints) => forwardTouchEvent("release", touchPoints)
                        onCanceled: (touchPoints) => forwardTouchEvent("cancel", touchPoints)

                        function forwardTouchEvent(eventType, touchPoints) {
                            if (!_touchForwarder) {
                                return
                            }

                            var points = []
                            for (var i = 0; i < touchPoints.length; i++) {
                                var tp = touchPoints[i]
                                var mapped = projectionSurface.mapToProjectionCoordinates(tp.x, tp.y)
                                projectionSurface.updateTouchDebug(tp.x, tp.y, eventType, mapped)
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
                        }
                    }

                    Rectangle {
                        id: touchDebugOverlay
                        anchors.fill: parent
                        z: 1000
                        visible: root.touchDebugOverlay
                        color: "transparent"
                        border.color: "#ffcc00"
                        border.width: 2
                        opacity: 0.95
                        // Diagnostic only: never consume touch/mouse input.
                        enabled: false

                        Rectangle {
                            id: touchDebugPanel
                            x: 8
                            y: 8
                            width: Math.min(parent.width - 16, 520)
                            height: 184
                            color: "#cc101010"
                            radius: 4

                            Text {
                                anchors.fill: parent
                                anchors.margins: 8
                                color: "white"
                                font.pixelSize: 12
                                text: {
                                    var frameSize = projectionSurface.projectionFrameSize()
                                    var rect = projectionSurface.projectionFrameRect()
                                    var aa = _touchForwarder ? _touchForwarder.androidAutoSize : Qt.size(0, 0)
                                    var display = _touchForwarder ? _touchForwarder.displaySize : Qt.size(0, 0)
                                    var sx = display.width > 0 ? aa.width / display.width : 0
                                    var sy = display.height > 0 ? aa.height / display.height : 0
                                    var aaPoint = projectionSurface.debugAaCoordinates(
                                                projectionSurface.debugMappedX,
                                                projectionSurface.debugMappedY)
                                    var aaFramePoint = projectionSurface.debugAaPointInFrame(
                                                projectionSurface.debugMappedX,
                                                projectionSurface.debugMappedY)
                                    var guide = projectionSurface.debugAaAspectGuide()
                                    return "TOUCH DEBUG  [F12 toggles]
" +
                                           "surface: " + Math.round(projectionSurface.width) + "x" +
                                               Math.round(projectionSurface.height) +
                                           "  frameRect: " + Math.round(rect.x) + "," + Math.round(rect.y) +
                                               " " + Math.round(rect.width) + "x" + Math.round(rect.height) + "
" +
                                           "native frame: " + frameSize.width + "x" + frameSize.height +
                                           "  AA: " + aa.width + "x" + aa.height + "
" +
                                           "display/native: " + display.width + "x" + display.height +
                                           "  scale: " + sx.toFixed(3) + "x" + sy.toFixed(3) + "
" +
                                           "event: " + projectionSurface.debugEventType +
                                           "  raw=" + projectionSurface.debugRawX.toFixed(1) + "," +
                                               projectionSurface.debugRawY.toFixed(1) + "
" +
                                           "native=" + projectionSurface.debugMappedX.toFixed(1) + "," +
                                               projectionSurface.debugMappedY.toFixed(1) +
                                           "  AA=" + aaPoint.x.toFixed(1) + "," + aaPoint.y.toFixed(1) + "
" +
                                           "AA point in frame=" + aaFramePoint.x.toFixed(1) + "," +
                                               aaFramePoint.y.toFixed(1) +
                                           "  16:9 guide=" + Math.round(guide.x) + "," + Math.round(guide.y) +
                                               " " + Math.round(guide.width) + "x" + Math.round(guide.height)
                                }
                            }
                        }

                        // Green = actual painted projection frame.
                        Rectangle {
                            id: touchDebugFrame
                            x: projectionSurface.projectionFrameRect().x
                            y: projectionSurface.projectionFrameRect().y
                            width: projectionSurface.projectionFrameRect().width
                            height: projectionSurface.projectionFrameRect().height
                            color: "transparent"
                            border.color: "#00ff66"
                            border.width: 2
                        }

                        // Blue = diagnostic 16:9 AA aspect-fit guide inside the native
                        // frame. This is deliberately visual-only; it does not affect
                        // the touch transform.
                        Rectangle {
                            id: touchDebugAaGuide
                            x: projectionSurface.debugAaAspectGuide().x
                            y: projectionSurface.debugAaAspectGuide().y
                            width: projectionSurface.debugAaAspectGuide().width
                            height: projectionSurface.debugAaAspectGuide().height
                            color: "transparent"
                            border.color: "#38a8ff"
                            border.width: 2
                        }

                        // Thin centre lines make it easy to identify any unexpected
                        // crop/offset independently of the touch marker.
                        Rectangle {
                            x: touchDebugFrame.x + touchDebugFrame.width / 2 - 1
                            y: touchDebugFrame.y
                            width: 2
                            height: touchDebugFrame.height
                            color: "#66ffffff"
                        }
                        Rectangle {
                            x: touchDebugFrame.x
                            y: touchDebugFrame.y + touchDebugFrame.height / 2 - 1
                            width: touchDebugFrame.width
                            height: 2
                            color: "#66ffffff"
                        }

                        // Red = raw QML event position.
                        Rectangle {
                            visible: projectionSurface.debugRawX >= 0 && projectionSurface.debugRawY >= 0
                            width: 18
                            height: 18
                            radius: 9
                            x: projectionSurface.debugRawX - width / 2
                            y: projectionSurface.debugRawY - height / 2
                            color: "#ff3030"
                            border.color: "white"
                            border.width: 2
                        }

                        // Yellow = native-frame coordinate after removing the painted
                        // frame's offset/letterbox.
                        Rectangle {
                            property point p: projectionSurface.debugAaPointInFrame(
                                                 projectionSurface.debugMappedX,
                                                 projectionSurface.debugMappedY)
                            visible: p.x >= 0 && p.y >= 0
                            width: 14
                            height: 14
                            radius: 7
                            x: p.x - width / 2
                            y: p.y - height / 2
                            color: "#ffd400"
                            border.color: "black"
                            border.width: 2
                        }

                        // Cyan = final AA coordinate projected back into the native
                        // frame. If yellow and cyan separate, the native->AA transform
                        // is introducing an unexpected geometry change.
                        Rectangle {
                            property var aaPoint: projectionSurface.debugAaCoordinates(
                                                       projectionSurface.debugMappedX,
                                                       projectionSurface.debugMappedY)
                            property point p: projectionSurface.debugAaPointBackInFrame(
                                                 aaPoint.x, aaPoint.y)
                            visible: aaPoint.x >= 0 && aaPoint.y >= 0 && p.x >= 0 && p.y >= 0
                            width: 10
                            height: 10
                            radius: 5
                            x: p.x - width / 2
                            y: p.y - height / 2
                            color: "#00e5ff"
                            border.color: "black"
                            border.width: 1
                        }

                        // Labels for the diagnostic points.
                        Text {
                            visible: projectionSurface.debugRawX >= 0
                            x: projectionSurface.debugRawX + 10
                            y: projectionSurface.debugRawY - 18
                            color: "#ff6060"
                            font.pixelSize: 11
                            text: "RAW"
                        }
                        Text {
                            visible: projectionSurface.debugMappedX >= 0
                            property point p: projectionSurface.debugAaPointInFrame(
                                                 projectionSurface.debugMappedX,
                                                 projectionSurface.debugMappedY)
                            x: p.x + 10
                            y: p.y - 14
                            color: "#ffe000"
                            font.pixelSize: 11
                            text: "NATIVE"
                        }

                        Text {
                            x: touchDebugFrame.x + 6
                            y: touchDebugFrame.y + touchDebugFrame.height - 20
                            color: "#00e5ff"
                            font.pixelSize: 11
                            text: "AA coordinate guide / final transform"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !projectionTouchArea.enabled

                        property bool isPressed: false

                        onPressed: (mouse) => {
                            isPressed = true
                            if (_touchForwarder) {
                                var mapped = projectionSurface.mapToProjectionCoordinates(mouse.x, mouse.y)
                                projectionSurface.updateTouchDebug(mouse.x, mouse.y, "press", mapped)
                                _touchForwarder.forwardMouseEvent("press", mapped.x, mapped.y)
                            }
                        }

                        onPositionChanged: (mouse) => {
                            if (isPressed && _touchForwarder) {
                                var mapped = projectionSurface.mapToProjectionCoordinates(mouse.x, mouse.y)
                                projectionSurface.updateTouchDebug(mouse.x, mouse.y, "move", mapped)
                                _touchForwarder.forwardMouseEvent("move", mapped.x, mapped.y)
                            }
                        }

                        onReleased: (mouse) => {
                            isPressed = false
                            if (_touchForwarder) {
                                var mapped = projectionSurface.mapToProjectionCoordinates(mouse.x, mouse.y)
                                projectionSurface.updateTouchDebug(mouse.x, mouse.y, "release", mapped)
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

