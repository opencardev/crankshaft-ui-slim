/*
 * Project: Crankshaft
 * Reusable toggle component for physical display rotation.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: rotationToggle
    spacing: 8
    width: parent ? parent.width : implicitWidth

    property int currentRotation: 0
    property bool showLabel: true
    property string titleText: qsTr("Display Rotation", "DisplayRotationToggle")

    signal rotationChanged(int newRotation)

    ButtonGroup {
        id: rotationGroup
        exclusive: true
    }

    Label {
        visible: rotationToggle.showLabel
        text: rotationToggle.titleText
        font.pointSize: 12
        font.bold: true
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        SegmentedOptionButton {
            baseText: qsTr("0", "DisplayRotationToggle")
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            ButtonGroup.group: rotationGroup
            checked: rotationToggle.currentRotation === 0

            onClicked: {
                if (checked) {
                    rotationToggle.currentRotation = 0
                    rotationToggle.rotationChanged(0)
                }
            }
        }

        SegmentedOptionButton {
            baseText: qsTr("90", "DisplayRotationToggle")
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            ButtonGroup.group: rotationGroup
            checked: rotationToggle.currentRotation === 90

            onClicked: {
                if (checked) {
                    rotationToggle.currentRotation = 90
                    rotationToggle.rotationChanged(90)
                }
            }
        }

        SegmentedOptionButton {
            baseText: qsTr("180", "DisplayRotationToggle")
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            ButtonGroup.group: rotationGroup
            checked: rotationToggle.currentRotation === 180

            onClicked: {
                if (checked) {
                    rotationToggle.currentRotation = 180
                    rotationToggle.rotationChanged(180)
                }
            }
        }

        SegmentedOptionButton {
            baseText: qsTr("270", "DisplayRotationToggle")
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            ButtonGroup.group: rotationGroup
            checked: rotationToggle.currentRotation === 270

            onClicked: {
                if (checked) {
                    rotationToggle.currentRotation = 270
                    rotationToggle.rotationChanged(270)
                }
            }
        }
    }
}