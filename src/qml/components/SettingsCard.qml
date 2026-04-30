/*
 * Project: Crankshaft
 * Reusable content card for settings sections.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Rectangle {
    id: card

    property string title: ""
    default property alias contentData: contentColumn.data
    readonly property bool isDarkTheme: card.Material.theme === Material.Dark

    radius: 10
    color: isDarkTheme ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.03)
    border.width: 1
    border.color: isDarkTheme ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0, 0, 0, 0.16)

    implicitHeight: contentColumn.implicitHeight + 20

    Column {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Label {
            text: card.title
            visible: text !== ""
            font.pointSize: 12
            font.bold: true
            color: card.Material.foreground
        }
    }
}
