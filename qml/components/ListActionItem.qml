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
    // A tinted card instead of a plain row (1.4, preview: "Show full
    // article" was mixed up with the feed's own "Read more" links)
    property bool framed: false
    // Opens a page of its own: subtle arrow on the right (as Sailcast)
    property bool opensPage: false

    width: parent ? parent.width : Screen.width
    height: Math.max(Theme.itemSizeMedium, textColumn.height + 2 * Theme.paddingMedium)
    enabled: !busy

    Rectangle {
        visible: root.framed
        anchors { fill: parent; leftMargin: Theme.horizontalPageMargin - Theme.paddingMedium
                  rightMargin: Theme.horizontalPageMargin - Theme.paddingMedium
                  topMargin: Theme.paddingSmall; bottomMargin: Theme.paddingSmall }
        radius: Theme.paddingMedium
        color: Theme.rgba(Theme.highlightBackgroundColor, root.highlighted ? 0.3 : 0.15)
    }

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
            color: root.highlighted ? Theme.highlightColor : Theme.secondaryColor
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
            right: parent.right
            rightMargin: root.opensPage ? 2 * Theme.horizontalPageMargin + Theme.paddingMedium : Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        Label {
            id: actionLabel
            width: parent.width
            wrapMode: Text.Wrap
            // Primary color, blue only while pressed (1.4: with the blue
            // values of the combo boxes everything on a settings page was blue)
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
            // Dimmed while busy, more when not available (e.g. nothing to export)
            opacity: root.busy ? Theme.opacityHigh : (root.enabled ? 1 : Theme.opacityLow)
        }
        Label {
            id: descriptionLabel
            width: parent.width
            visible: text !== ""
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            opacity: root.enabled || root.busy ? 1 : Theme.opacityLow
        }
    }

    PageArrow { visible: root.opensPage }
}
