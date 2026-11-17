// SPDX-License-Identifier: GPL-3.0-or-later
// The subtle arrow at the right edge of a row that opens a page of its own
// (as Sailcast's PageArrow; rows that only open a choice menu have none).
import QtQuick 2.6
import Sailfish.Silica 1.0

Label {
    anchors {
        right: parent.right
        rightMargin: Theme.horizontalPageMargin
        verticalCenter: parent.verticalCenter
    }
    text: "›"
    font.pixelSize: Theme.fontSizeLarge
    color: Theme.secondaryColor
    opacity: 0.8
}
