// SPDX-License-Identifier: GPL-3.0-or-later
// Small line under a page header that names the pages to the left and to the
// right ("‹ Feeds" … "Articles ›", like Sailcast and Sailwave), so the swipe
// between the feed list and the article list is visible (blind test 11 Nov:
// both testers could only guess where the feed list is). A tap on a text
// goes to that page, as in Sailcast (the row is below the header, so the
// corners themselves stay Silica's navigation).
import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: root
    property string leftText
    property string rightText
    signal leftClicked()
    signal rightClicked()

    width: parent ? parent.width : Screen.width
    height: visible ? Theme.itemSizeExtraSmall * 0.7 : 0

    BackgroundItem {
        id: leftItem
        visible: root.leftText !== ""
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: leftLabel.width + Theme.horizontalPageMargin + Theme.paddingLarge
        onClicked: root.leftClicked()
        Label {
            id: leftLabel
            anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
            text: "‹ " + root.leftText
            font.pixelSize: Theme.fontSizeSmall
            color: leftItem.highlighted ? Theme.highlightColor : Theme.secondaryHighlightColor
        }
    }
    BackgroundItem {
        id: rightItem
        visible: root.rightText !== ""
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: rightLabel.width + Theme.horizontalPageMargin + Theme.paddingLarge
        onClicked: root.rightClicked()
        Label {
            id: rightLabel
            anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
            text: root.rightText + " ›"
            font.pixelSize: Theme.fontSizeSmall
            color: rightItem.highlighted ? Theme.highlightColor : Theme.secondaryHighlightColor
        }
    }
}
