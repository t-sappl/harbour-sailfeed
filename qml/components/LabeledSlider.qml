// SPDX-License-Identifier: GPL-3.0-or-later
// Slider in the layout of the Sailfish settings: label on the left, the
// current value right-aligned in the highlight color, the bar below and an
// optional explanation underneath (like a TextSwitch/ComboBox description).
// The value follows the finger while dragging (sliderValue); "released" is
// emitted when the finger lifts, so the setting is written once.
import QtQuick 2.6
import Sailfish.Silica 1.0

Column {
    id: root

    property string label
    property string description
    // Text for the shown value; the default is the number
    property string valueText: Math.round(slider.sliderValue)
    property alias minimumValue: slider.minimumValue
    property alias maximumValue: slider.maximumValue
    property alias stepSize: slider.stepSize
    property alias value: slider.value
    readonly property alias sliderValue: slider.sliderValue
    signal released()

    width: parent ? parent.width : Screen.width

    Item {
        width: parent.width
        height: Math.max(labelItem.height, valueItem.height) + Theme.paddingSmall

        Label {
            id: labelItem
            anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin
                      right: valueItem.left; rightMargin: Theme.paddingMedium; bottom: parent.bottom }
            wrapMode: Text.Wrap
            color: root.enabled ? Theme.primaryColor : Theme.secondaryColor
            text: root.label
        }
        Label {
            id: valueItem
            anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; baseline: labelItem.baseline }
            horizontalAlignment: Text.AlignRight
            color: root.enabled ? Theme.highlightColor : Theme.secondaryHighlightColor
            text: root.valueText
        }
    }

    Slider {
        id: slider
        width: parent.width
        // label and value are shown above, not by the slider itself
        label: ""
        valueText: ""
        onReleased: root.released()
    }

    Label {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * x
        visible: text !== ""
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
        text: root.description
    }

    Item { width: 1; height: Theme.paddingMedium; visible: root.description !== "" }
}
