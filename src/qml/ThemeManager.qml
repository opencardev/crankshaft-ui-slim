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

/**
 * ThemeManager - Centralized theme management for Crankshaft Slim UI
 * 
 * Provides color palettes, font definitions, and spacing for light and dark modes.
 * Follows Material Design guidelines adapted for automotive use (Design for Driving).
 */
QtObject {
    id: themeManager
    
    // Current theme mode: "LIGHT" or "DARK"
    property string currentMode: "DARK"
    
    // Color Palette - Dark Theme
    readonly property QtObject darkTheme: QtObject {
        // Background colors
        readonly property color background: "#111418"        // Primary background
        readonly property color surface: "#1A2026"          // Elevated surfaces
        readonly property color surfaceVariant: "#252D36"   // Surface variants
        
        // Primary colors
        readonly property color primary: "#4DA3FF"          // Primary accent
        readonly property color primaryVariant: "#2C7FD8"   // Primary variant
        readonly property color textOnPrimary: "#061321"    // Text on primary
        
        // Secondary colors
        readonly property color secondary: "#6DD8C8"        // Secondary accent
        readonly property color secondaryVariant: "#3EB2A0" // Secondary variant
        readonly property color textOnSecondary: "#000000"  // Text on secondary
        
        // Text colors
        readonly property color textPrimary: "#F4F7FA"      // Primary text
        readonly property color textSecondary: "#B6C0CC"    // Secondary text
        readonly property color textDisabled: "#64707E"     // Disabled text
        
        // UI element colors
        readonly property color border: "#2E3742"           // Borders
        readonly property color divider: "#262E38"          // Dividers
        readonly property color overlay: "#00000080"        // Overlay background
        
        // State colors
        readonly property color error: "#CF6679"            // Error state
        readonly property color warning: "#FFB74D"          // Warning state
        readonly property color success: "#81C784"          // Success state
        readonly property color info: "#64B5F6"             // Info state
    }
    
    // Color Palette - Light Theme
    readonly property QtObject lightTheme: QtObject {
        // Background colors
        readonly property color background: "#F2F5F8"       // Primary background
        readonly property color surface: "#FFFFFF"          // Elevated surfaces
        readonly property color surfaceVariant: "#EDF2F7"   // Surface variants
        
        // Primary colors
        readonly property color primary: "#005BBB"          // Primary accent
        readonly property color primaryVariant: "#00408A"   // Primary variant
        readonly property color textOnPrimary: "#FFFFFF"    // Text on primary
        
        // Secondary colors
        readonly property color secondary: "#0A7A70"        // Secondary accent
        readonly property color secondaryVariant: "#06655D" // Secondary variant
        readonly property color textOnSecondary: "#FFFFFF"  // Text on secondary
        
        // Text colors
        readonly property color textPrimary: "#0E1722"      // Primary text
        readonly property color textSecondary: "#3A4756"    // Secondary text
        readonly property color textDisabled: "#7A8795"     // Disabled text
        
        // UI element colors
        readonly property color border: "#B8C4D1"           // Borders
        readonly property color divider: "#D3DDE7"          // Dividers
        readonly property color overlay: "#00000055"        // Overlay background
        
        // State colors
        readonly property color error: "#B3261E"            // Error state
        readonly property color warning: "#B26A00"          // Warning state
        readonly property color success: "#2E7D32"          // Success state
        readonly property color info: "#0B57D0"             // Info state
    }
    
    // Active theme (switches based on currentMode)
    readonly property QtObject colors: currentMode === "LIGHT" ? lightTheme : darkTheme
    
    // Typography
    readonly property QtObject typography: QtObject {
        // Font families
        readonly property string fontFamily: "Roboto"
        readonly property string monospaceFontFamily: "Roboto Mono"
        
        // Font sizes (scaled for automotive displays)
        readonly property int h1: 32  // Large headings
        readonly property int h2: 28  // Section headings
        readonly property int h3: 24  // Subsection headings
        readonly property int h4: 20  // Card titles
        readonly property int body1: 16  // Primary body text
        readonly property int body2: 14  // Secondary body text
        readonly property int caption: 12  // Captions and labels
        readonly property int button: 16  // Button text
        
        // Font weights
        readonly property int light: Font.Light       // 300
        readonly property int normal: Font.Normal     // 400
        readonly property int medium: Font.Medium     // 500
        readonly property int bold: Font.Bold         // 700
    }
    
    // Spacing (following 8dp grid)
    readonly property QtObject spacing: QtObject {
        readonly property int tiny: 4
        readonly property int small: 8
        readonly property int medium: 16
        readonly property int large: 24
        readonly property int xlarge: 32
        readonly property int xxlarge: 48
    }
    
    // Dimensions (Design for Driving compliance)
    readonly property QtObject dimensions: QtObject {
        // Touch targets (minimum 44pt for driving)
        readonly property int minTouchTarget: 44
        readonly property int recommendedTouchTarget: 56
        
        // Border radius
        readonly property int borderRadiusSmall: 4
        readonly property int borderRadiusMedium: 8
        readonly property int borderRadiusLarge: 16
        
        // Icon sizes
        readonly property int iconSmall: 16
        readonly property int iconMedium: 24
        readonly property int iconLarge: 32
        readonly property int iconXLarge: 48
        
        // Component heights
        readonly property int buttonHeight: 56
        readonly property int toolbarHeight: 64
        readonly property int listItemHeight: 72
    }
    
    // Animation durations (milliseconds)
    readonly property QtObject animation: QtObject {
        readonly property int fast: 150
        readonly property int normal: 300
        readonly property int slow: 500
    }
    
    /**
     * Switch between light and dark themes
     */
    function toggleTheme() {
        currentMode = (currentMode === "LIGHT") ? "DARK" : "LIGHT"
    }
    
    /**
     * Set theme explicitly
     * @param mode "LIGHT" or "DARK"
     */
    function setTheme(mode) {
        if (mode === "LIGHT" || mode === "DARK") {
            currentMode = mode
        } else {
            console.warn("ThemeManager: Invalid theme mode:", mode)
        }
    }
}
