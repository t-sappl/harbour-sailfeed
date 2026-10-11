// SPDX-License-Identifier: GPL-3.0-or-later
// Bar at the bottom of the article preview and the web view:
// ‹ | A- | bookmark | "3 / 120" | open in browser | light/dark | A+ | ›
// A bar instead of a swipe gesture: horizontal swipes belong to the page
// stack (back / reader mode) and vertical ones to scrolling the content.
// Everything one-handed that is also in the pulley: text size, bookmark,
// open in the browser (device test 11 Oct: "Open in browser" was hard to find).
import QtQuick 2.6
import Sailfish.Silica 1.0

Rectangle {
    id: bar
    property string info
    property bool showTextSize: false
    // Light/dark toggle of the reader mode
    property bool showColorToggle: false
    property bool darkColors: true
    property bool showBookmark: false
    property bool bookmarked: false
    property bool showBrowser: false
    signal toggleColors()
    signal previous()
    signal next()
    signal smaller()
    signal larger()
    signal toggleBookmark()
    signal openBrowser()

    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    height: visible ? Theme.itemSizeSmall : 0
    color: Theme.rgba(Theme.highlightDimmerColor, 0.92)
    z: 5

    Row {
        id: leftGroup
        anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin - Theme.paddingMedium; verticalCenter: parent.verticalCenter }
        spacing: Theme.paddingSmall
        IconButton {
            icon.source: "image://theme/icon-m-left"
            onClicked: bar.previous()
        }
        TextSizeButton {
            visible: bar.showTextSize
            large: false
            onClicked: bar.smaller()
        }
        IconButton {
            visible: bar.showBookmark
            width: Theme.itemSizeExtraSmall
            // Small like the text size "A" and the light/dark circle: the
            // medium icons were too dominant (device test 11 Oct)
            icon.width: Theme.iconSizeSmallPlus
            icon.height: Theme.iconSizeSmallPlus
            icon.sourceSize.width: Theme.iconSizeSmallPlus
            icon.sourceSize.height: Theme.iconSizeSmallPlus
            icon.source: bar.bookmarked ? "image://theme/icon-m-favorite-selected" : "image://theme/icon-m-favorite"
            onClicked: bar.toggleBookmark()
        }
    }

    Label {
        anchors { left: leftGroup.right; right: rightGroup.left; verticalCenter: parent.verticalCenter
                  leftMargin: Theme.paddingSmall; rightMargin: Theme.paddingSmall }
        horizontalAlignment: Text.AlignHCenter
        truncationMode: TruncationMode.Fade
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryHighlightColor
        text: bar.info
    }

    Row {
        id: rightGroup
        anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin - Theme.paddingMedium; verticalCenter: parent.verticalCenter }
        spacing: Theme.paddingSmall
        IconButton {
            visible: bar.showBrowser
            width: Theme.itemSizeExtraSmall
            icon.width: Theme.iconSizeSmallPlus
            icon.height: Theme.iconSizeSmallPlus
            icon.sourceSize.width: Theme.iconSizeSmallPlus
            icon.sourceSize.height: Theme.iconSizeSmallPlus
            icon.source: "image://theme/icon-m-website"
            onClicked: bar.openBrowser()
        }
        // Light/dark toggle: a half filled circle (drawn, no theme icon needed)
        BackgroundItem {
            id: colorButton
            visible: bar.showColorToggle
            width: Theme.itemSizeExtraSmall
            height: width
            onClicked: bar.toggleColors()

            Rectangle {
                id: ring
                anchors.centerIn: parent
                width: Theme.iconSizeSmall
                height: width
                radius: width / 2
                color: "transparent"
                border.width: Math.round(2 * Theme.pixelRatio)
                border.color: colorButton.highlighted ? Theme.highlightColor : Theme.primaryColor
                Item {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: parent.width / 2
                    clip: true
                    Rectangle {
                        width: ring.width
                        height: ring.height
                        radius: ring.radius
                        color: ring.border.color
                    }
                }
            }
        }
        TextSizeButton {
            visible: bar.showTextSize
            large: true
            onClicked: bar.larger()
        }
        IconButton {
            icon.source: "image://theme/icon-m-right"
            onClicked: bar.next()
        }
    }
}
