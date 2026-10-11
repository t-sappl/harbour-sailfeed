// SPDX-License-Identifier: GPL-3.0-or-later
// Self-hiding message at the bottom of the screen, above every page (like
// Sailwave's MessageBanner). Errors are shown longer, in the error color, and
// can offer one action (e.g. "Retry"). Show it with app.showMessage().
import QtQuick 2.6
import Sailfish.Silica 1.0

Rectangle {
    id: banner

    property bool error: false
    property string actionText
    property var action: null

    // duration in ms; 0 = default (4 s, errors 10 s)
    function show(text, isError, actionLabel, actionFunction, duration) {
        bannerLabel.text = text
        error = isError === true
        actionText = actionLabel ? actionLabel : ""
        action = actionFunction ? actionFunction : null
        hideTimer.interval = duration > 0 ? duration : (error ? 10000 : 4000)
        opacity = 1
        hideTimer.restart()
    }
    function hide() { opacity = 0 }

    width: parent.width - 2 * Theme.horizontalPageMargin
    height: content.height + 2 * Theme.paddingMedium
    radius: Theme.paddingSmall
    // Readable in dark and light ambiences: errors on the dimmed highlight
    // color with a red frame and icon (red text background was hard to read
    // in a light ambience).
    color: error ? Theme.rgba(Theme.highlightDimmerColor, 0.95)
                 : Theme.rgba(Theme.highlightBackgroundColor, 0.92)
    border.width: error ? Math.round(2 * Theme.pixelRatio) : 0
    border.color: Theme.errorColor
    opacity: 0
    visible: opacity > 0

    Behavior on opacity { FadeAnimation { } }

    // Tapping the message dismisses it early
    MouseArea {
        anchors.fill: parent
        onClicked: banner.hide()
    }

    Row {
        id: content
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Icon {
            id: icon
            visible: banner.error
            anchors.verticalCenter: parent.verticalCenter
            source: "image://theme/icon-s-warning"
            color: Theme.errorColor
        }
        Label {
            id: bannerLabel
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - (icon.visible ? icon.width + parent.spacing : 0)
                   - (actionButton.visible ? actionButton.width + parent.spacing : 0)
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }
        Button {
            id: actionButton
            visible: banner.actionText !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: banner.actionText
            onClicked: {
                var f = banner.action
                banner.hide()
                if (f)
                    f()
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 4000
        onTriggered: banner.hide()
    }
}
