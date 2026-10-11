// SPDX-License-Identifier: GPL-3.0-or-later
// Small "A" button for smaller (small A) or larger (large A) text.
import QtQuick 2.6
import Sailfish.Silica 1.0

BackgroundItem {
    id: button
    property bool large
    width: Theme.itemSizeExtraSmall
    height: Theme.itemSizeExtraSmall

    Label {
        anchors.centerIn: parent
        text: "A"
        font.pixelSize: button.large ? Theme.fontSizeLarge : Theme.fontSizeExtraSmall
        font.bold: true
        color: button.highlighted ? Theme.highlightColor : Theme.primaryColor
    }
}
