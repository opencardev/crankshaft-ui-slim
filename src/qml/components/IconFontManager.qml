/*
 * Project: Crankshaft
 * Lightweight icon font manager.
 *
 * Prefer Qt's built-in Material icon family and avoid file-based FontLoader
 * paths that are frequently unavailable in target images.
 */

import QtQuick

Item {
    id: iconFonts
    visible: false
    width: 0
    height: 0

    // Qt Material style ships an icon font family; this works without file paths.
    readonly property string loadedFamily: "Material Icons"
    readonly property string family: "Material Icons"

    // Material Icons codepoint for settings/cog glyph.
    readonly property string settingsGlyph: "\ue8b8"
}
