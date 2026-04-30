/*
 * Project: Crankshaft
 * Reusable segmented toggle button with strong selected-state styling.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: root

    property string baseText: ""
    property string selectedPrefix: "\u2713 "

    checkable: true
    font.bold: checked

    text: checked ? (selectedPrefix + baseText) : baseText

    background: Rectangle {
        radius: 10
        color: root.checked ? Material.accent : "transparent"
        border.width: root.checked ? 2 : 1
        border.color: root.checked
                      ? Qt.darker(Material.accent, 1.15)
                      : Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.35)
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.checked ? "white" : Material.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
