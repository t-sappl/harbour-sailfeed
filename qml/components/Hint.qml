// SPDX-License-Identifier: GPL-3.0-or-later
// Short transient hint at the bottom of a page: hint.show("text")
import QtQuick 2.6
import Sailfish.Silica 1.0

Rectangle {
    id: hint
    // duration in ms (optional, default 2.5 s)
    function show(text, duration) {
        hintLabel.text = text
        hintTimer.interval = duration > 0 ? duration : 2500
        opacity = 1
        hintTimer.restart()
    }

    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    height: hintLabel.height + 2 * Theme.paddingLarge
    color: Theme.rgba(Theme.highlightDimmerColor, 0.9)
    opacity: 0
    visible: opacity > 0
    z: 10
    Behavior on opacity { FadeAnimator { } }

    Label {
        id: hintLabel
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeSmall
    }
    Timer { id: hintTimer; interval: 2500; onTriggered: hint.opacity = 0 }
    // Tap to close a long hint early
    MouseArea { anchors.fill: parent; enabled: hint.visible; onClicked: hint.opacity = 0 }
}
