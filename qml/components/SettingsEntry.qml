// SPDX-License-Identifier: GPL-3.0-or-later
// Entry of the main settings page that opens a sub-page: title, a one-line
// summary of the current values below it (secondary color) and an arrow on
// the right, like "About Sailfeed" and the rows of the Sailfish settings.
import QtQuick 2.6
import Sailfish.Silica 1.0

BackgroundItem {
    id: root

    property alias title: titleLabel.text
    property alias summary: summaryLabel.text
    property url target

    width: parent ? parent.width : Screen.width
    height: Math.max(Theme.itemSizeMedium, textColumn.height + 2 * Theme.paddingMedium)
    onClicked: if (target.toString() !== "") pageStack.push(target)

    Column {
        id: textColumn
        anchors {
            left: parent.left; leftMargin: Theme.horizontalPageMargin
            right: arrow.left; rightMargin: Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        Label {
            id: titleLabel
            width: parent.width
            wrapMode: Text.Wrap
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
        Label {
            id: summaryLabel
            width: parent.width
            visible: text !== ""
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }
    Icon {
        id: arrow
        anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
        source: "image://theme/icon-m-right"
        color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
    }
}
