// SPDX-License-Identifier: GPL-3.0-or-later
// A small rounded label to tap (as Sailcast's Chip), e.g. "All" / "None" in
// the feed choice of a folder. "on" fills it with the highlight background.
import QtQuick 2.6
import Sailfish.Silica 1.0

Rectangle {
    id: chip
    property alias text: label.text
    property bool on: false
    signal clicked()

    width: label.implicitWidth + 2 * Theme.paddingMedium
    height: label.implicitHeight + Theme.paddingSmall * 2
    radius: height / 2
    color: mouse.pressed ? Theme.rgba(Theme.highlightBackgroundColor, 0.5)
                         : (on ? Theme.rgba(Theme.highlightBackgroundColor, 0.35) : "transparent")
    border.width: on ? 0 : 1
    border.color: Theme.rgba(Theme.primaryColor, 0.3)

    Label {
        id: label
        anchors.centerIn: parent
        font.pixelSize: Theme.fontSizeSmall
        color: chip.on || mouse.pressed ? Theme.highlightColor : Theme.primaryColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: chip.clicked()
    }
}
