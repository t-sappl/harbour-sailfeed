// SPDX-License-Identifier: GPL-3.0-or-later
// Row on the feeds page: optional icon, name, unread count on the right.
// Used for "All articles", folder headers (bold) and feeds (indented, favicon).
import QtQuick 2.6
import Sailfish.Silica 1.0

ListItem {
    id: root
    property string name
    property int unreadCount
    property bool current
    property bool bold
    property int indent: 0
    property string iconSource
    // Favorite feed: small star at the bottom right of the icon (or in its
    // place when the feed has no favicon)
    property bool favorite
    readonly property bool hasIcon: iconSource !== "" || favorite

    width: parent ? parent.width : Screen.width
    contentHeight: Theme.itemSizeSmall

    Image {
        id: icon
        visible: root.iconSource !== "" && status === Image.Ready
        anchors {
            left: parent.left; leftMargin: Theme.horizontalPageMargin + root.indent
            verticalCenter: parent.verticalCenter
        }
        width: Theme.iconSizeSmall
        height: Theme.iconSizeSmall
        sourceSize.width: width
        sourceSize.height: height
        source: root.iconSource
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
    }

    Icon {
        id: favStar
        visible: root.favorite
        readonly property bool alone: !icon.visible
        source: "image://theme/icon-s-favorite"
        color: Theme.highlightColor
        // On the favicon three quarters of its size (0.6 was hard to see on
        // the device, 11 Oct)
        width: alone ? Theme.iconSizeSmall : Math.round(Theme.iconSizeSmall * 0.75)
        height: width
        sourceSize.width: width
        sourceSize.height: height
        x: alone ? icon.x : icon.x + Theme.iconSizeSmall - width * 0.55
        y: alone ? (root.contentHeight - height) / 2
                 : (root.contentHeight + Theme.iconSizeSmall) / 2 - height * 0.55
        // Ring in the background color, so the star stands out on the favicon
        Rectangle {
            z: -1
            visible: !favStar.alone
            anchors.centerIn: parent
            width: parent.width * 0.8
            height: width
            radius: width / 2
            // Like the page background, so it separates the star from the
            // favicon without showing as a dark disc in light ambiences
            color: Theme.colorScheme === Theme.LightOnDark ? Theme.rgba("black", 0.6)
                                                           : Theme.rgba("white", 0.85)
        }
    }

    Label {
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            leftMargin: Theme.horizontalPageMargin + root.indent
                        + (root.hasIcon ? Theme.iconSizeSmall + Theme.paddingMedium : 0)
            right: countLabel.left; rightMargin: Theme.paddingMedium
        }
        text: root.name
        font.bold: root.bold
        truncationMode: TruncationMode.Fade
        color: root.current || root.highlighted ? Theme.highlightColor : Theme.primaryColor
    }
    Label {
        id: countLabel
        anchors {
            verticalCenter: parent.verticalCenter
            right: parent.right; rightMargin: Theme.horizontalPageMargin
        }
        text: root.unreadCount > 0 ? root.unreadCount : ""
        font.bold: root.bold
        color: root.current || root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
    }
}
