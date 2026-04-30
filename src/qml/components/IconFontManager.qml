/*
 * Project: Crankshaft
 * FontLoader-based icon font manager.
 *
 * This component tries common Material icon font locations and exposes
 * a resolved font family for icon glyph rendering.
 */

import QtQuick

Item {
    id: iconFonts
    visible: false
    width: 0
    height: 0

    // Optional bundled font (if later added to qml/assets/fonts/)
    FontLoader {
        id: bundledMaterialIcons
        source: "qrc:/qml/assets/fonts/MaterialIcons-Regular.ttf"
    }

    // Common Linux packaging paths
    FontLoader {
        id: debianMaterialIcons
        source: "file:///usr/share/fonts/truetype/material-design-icons/MaterialIcons-Regular.ttf"
    }

    FontLoader {
        id: altMaterialIcons
        source: "file:///usr/share/fonts/truetype/material-icons/MaterialIcons-Regular.ttf"
    }

    // Best-effort resolved family from loaded font files.
    readonly property string loadedFamily:
        bundledMaterialIcons.status === FontLoader.Ready ? bundledMaterialIcons.name :
        debianMaterialIcons.status === FontLoader.Ready ? debianMaterialIcons.name :
        altMaterialIcons.status === FontLoader.Ready ? altMaterialIcons.name :
        ""

    // Family used by UI (loaded font first, then common installed family names).
    readonly property string family: loadedFamily !== "" ? loadedFamily : "Material Icons"

    // Material Icons codepoint for settings/cog glyph.
    readonly property string settingsGlyph: "\ue8b8"
}
