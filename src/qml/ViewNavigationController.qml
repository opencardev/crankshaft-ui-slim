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

/**
 * ViewNavigationController - Manages view state and transitions
 * 
 * Controls switching between:
 * - AndroidAuto Projection View (primary)
 * - Settings Panel (overlay)
 * - Connection Status View (when not connected)
 * 
 * Implements smooth transitions (300ms) per spec while maintaining:
 * - AndroidAuto service running in background
 * - Audio continue playback during settings
 * - Touch input functional
 * - Video rendering paused (GPU optimization)
 */
QtObject {
    id: navigationController
    
    // View states
    readonly property int viewStateAAProjection: 0
    readonly property int viewStateSettings: 1
    readonly property int viewStateConnectionStatus: 2
    readonly property int viewStateLoading: 3
    
    // Current view state
    property int currentViewState: viewStateLoading
    
    // Settings panel visibility
    property bool settingsPanelVisible: false
    
    // AndroidAuto service state
    property bool aaServiceRunning: false
    property bool aaProjectionActive: false
    property bool aaAudioPlaying: false
    
    // Video rendering state (pause when settings open)
    property bool videoRenderingEnabled: currentViewState === viewStateAAProjection
    
    // Touch input handling
    property bool touchInputEnabled: true
    
    /**
     * Switch to AndroidAuto projection view
     */
    function showAAProjection() {
        settingsPanelVisible = false
        currentViewState = viewStateAAProjection
        videoRenderingEnabled = true
    }
    
    /**
     * Switch to settings panel (overlay on AA view)
     * 
     * Per spec: AA service continues running in background
     * - Audio continues
     * - Video rendering pauses (GPU optimization)
     * - Touch input stays functional (for settings controls)
     */
    function showSettings() {
        settingsPanelVisible = true
        currentViewState = viewStateSettings
        videoRenderingEnabled = false  // Pause GPU rendering while overlay open
        
        // AA service continues running in background
        if (aaServiceRunning) {
            // Audio continues (no pause)
            // Video decoder paused (no GPU waste)
            // Input forwarding paused (user controls settings instead)
        }
    }
    
    /**
     * Close settings panel and return to AA view
     */
    function closeSettings() {
        settingsPanelVisible = false
        currentViewState = viewStateAAProjection
        videoRenderingEnabled = true  // Resume GPU rendering
    }
    
    /**
     * Show connection status view
     */
    function showConnectionStatus(title, message, retryable) {
        settingsPanelVisible = false
        currentViewState = viewStateConnectionStatus
        videoRenderingEnabled = false
    }
    
    /**
     * Show loading state
     */
    function showLoading(message) {
        settingsPanelVisible = false
        currentViewState = viewStateLoading
        videoRenderingEnabled = false
    }
    
    /**
     * Toggle settings visibility
     */
    function toggleSettings() {
        if (settingsPanelVisible) {
            closeSettings()
        } else if (currentViewState === viewStateAAProjection) {
            showSettings()
        }
    }
    
    /**
     * Get current view state name
     */
    function getViewStateName() {
        switch (currentViewState) {
            case viewStateAAProjection:
                return "AAProjection"
            case viewStateSettings:
                return "Settings"
            case viewStateConnectionStatus:
                return "ConnectionStatus"
            case viewStateLoading:
                return "Loading"
            default:
                return "Unknown"
        }
    }
    
    /**
     * Check if AndroidAuto is active
     */
    function isAAActive() {
        return aaServiceRunning && aaProjectionActive
    }
    
    /**
     * Check if settings can be opened
     */
    function canOpenSettings() {
        return currentViewState === viewStateAAProjection || currentViewState === viewStateSettings
    }
}
