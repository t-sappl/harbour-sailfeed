// SPDX-License-Identifier: GPL-3.0-or-later
// Full-width action row at the end of a list (or in an empty list), e.g.
// "Show read articles" or "Load older articles". Looks like a list entry:
// action text in the highlight color, an optional explanation below, an icon
// on the left. Calmer than a Silica Button, which lights up strongly when
// pressed. While busy, a small busy indicator replaces the icon and the row
// does not react.
import QtQuick 2.6
import Sailfish.Silica 1.0

BackgroundItem {
    id: root

    property alias text: actionLabel.text
    property alias description: descriptionLabel.text
    property string iconSource
    property bool busy: false

    width: parent ? parent.width : Screen.width
    height: Math.max(Theme.itemSizeMedium, textColumn.height + 2 * Theme.paddingMedium)
    enabled: !busy

    Item {
        id: iconBox
        width: Theme.iconSizeMedium
        height: Theme.iconSizeMedium
        anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
        visible: root.iconSource !== "" || root.busy

        Icon {
            anchors.centerIn: parent
            visible: !root.busy
            source: root.iconSource
            color: root.highlighted ? Theme.highlightColor : Theme.secondaryHighlightColor
        }
        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Small
            running: root.busy
        }
    }

    Column {
        id: textColumn
        anchors {
            left: iconBox.visible ? iconBox.right : parent.left
            leftMargin: iconBox.visible ? Theme.paddingMedium : Theme.horizontalPageMargin
            right: parent.right; rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        Label {
            id: actionLabel
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            opacity: root.busy ? Theme.opacityHigh : 1
        }
        Label {
            id: descriptionLabel
            width: parent.width
            visible: text !== ""
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }
}
