// SPDX-License-Identifier: GPL-3.0-or-later
// Bar at the bottom of the article preview and the web view:
// ‹ | A- | bookmark | "3 / 120" | light/dark | A+ | [reader/original] | ›
// For a page opened from a link in an article: back | A- | site | light/dark | A+ | [reader/original]
// A bar instead of a swipe gesture: horizontal swipes belong to the page
// stack (back / reader mode) and vertical ones to scrolling the content.
// What is used while reading, one-handed: text size, colors, bookmark and,
// in the web view, switching between the reader mode and the original
// website (15 Oct: replaces "open in browser", which moved to the pulley –
// the switch is needed per article, the external browser rarely).
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
    // Web view: switch between reader mode and original website. The button
    // shows where it leads: a globe (the website) in the reader mode, lines
    // of text (the reader mode) on the original.
    property bool showModeToggle: false
    property bool readerShown: true
    // Link page: a back button instead of previous/next
    property bool showBack: false
    signal back()
    signal toggleColors()
    signal previous()
    signal next()
    signal smaller()
    signal larger()
    signal toggleBookmark()
    signal toggleMode()

    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    height: visible ? Theme.itemSizeSmall : 0
    // Opaque: with 8 % transparency the page showed through in light
    // ambiences (device test 15 Oct, "Snow White": the website's buttons and
    // the reader text behind the icons)
    color: Theme.highlightDimmerColor
    z: 5

    Row {
        id: leftGroup
        anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin - Theme.paddingMedium; verticalCenter: parent.verticalCenter }
        spacing: Theme.paddingSmall
        IconButton {
            icon.source: bar.showBack ? "image://theme/icon-m-back" : "image://theme/icon-m-left"
            onClicked: bar.showBack ? bar.back() : bar.previous()
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
        // highlightColor: the secondary highlight color was too pale on the
        // light bar of light ambiences ("37 / 64", device test 15 Oct)
        color: Theme.highlightColor
        text: bar.info
    }

    Row {
        id: rightGroup
        anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin - Theme.paddingMedium; verticalCenter: parent.verticalCenter }
        spacing: Theme.paddingSmall
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
        // Always right before "›", in both modes the same place for the thumb
        // (A−/A+ and light/dark are hidden on the original)
        BackgroundItem {
            id: modeButton
            visible: bar.showModeToggle
            width: Theme.itemSizeExtraSmall
            height: width
            onClicked: bar.toggleMode()

            Icon {
                anchors.centerIn: parent
                visible: bar.readerShown
                width: Theme.iconSizeSmallPlus
                height: Theme.iconSizeSmallPlus
                sourceSize.width: Theme.iconSizeSmallPlus
                sourceSize.height: Theme.iconSizeSmallPlus
                source: "image://theme/icon-m-website"
                color: modeButton.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
            // Reader mode: a page with lines of text (drawn, no theme icon
            // for it on every release)
            Rectangle {
                id: readerGlyph
                anchors.centerIn: parent
                visible: !bar.readerShown
                width: Math.round(Theme.iconSizeSmall * 0.8)
                height: Theme.iconSizeSmall
                radius: Math.round(2 * Theme.pixelRatio)
                color: "transparent"
                border.width: Math.round(2 * Theme.pixelRatio)
                border.color: modeButton.highlighted ? Theme.highlightColor : Theme.primaryColor
                Column {
                    anchors.centerIn: parent
                    spacing: Math.round(3 * Theme.pixelRatio)
                    Repeater {
                        model: [1, 1, 0.6]
                        Rectangle {
                            width: Math.round((readerGlyph.width - 4 * readerGlyph.border.width) * modelData)
                            height: Math.max(1, Math.round(1.5 * Theme.pixelRatio))
                            color: readerGlyph.border.color
                        }
                    }
                }
            }
        }
        IconButton {
            visible: !bar.showBack
            icon.source: "image://theme/icon-m-right"
            onClicked: bar.next()
        }
    }
}
