// SPDX-License-Identifier: GPL-3.0-or-later
// A tip shown once (per key): Silica's animated hand (TouchInteractionHint)
// with a text at the bottom (InteractionHintLabel), like the hints of the
// Sailfish apps and Sailwave. show() does nothing once the tip was shown;
// a long press on the icon of the About page resets them (app.resetHints()).
// At most one tip every 45 s across the app: a tip asked for sooner is not
// shown and not marked, so it comes at a later visit (15 Oct: the first
// minutes should not be a row of hands).
import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: root

    property string key
    property alias text: label.text
    // TouchInteraction.Down (pulley), .Left / .Right (swipes)
    property int direction: TouchInteraction.Down
    property int interactionMode: TouchInteraction.Pull
    // false: the text alone (e.g. long press, for which there is no hand)
    property bool showHand: true
    // Horizontal center of the hand for vertical gestures; default the middle
    property real handCenterX: -1
    // Text at the top instead of the bottom
    property bool labelAtTop: false
    // Room kept free at the bottom right of the text, e.g. so it stands right
    // beside the mark-all button instead of covering it (device test 15 Oct:
    // the text at the top was too far from the button)
    property real labelRightMargin: 0
    property real labelBottomMargin: 0
    // Shown even within 45 s of another tip: the next step of a tip just
    // followed (no feeds yet: swipe to the feed list → pull down there)
    property bool ignorePause: false
    readonly property bool vertical: direction === TouchInteraction.Down || direction === TouchInteraction.Up

    anchors.fill: parent
    z: 50

    // Returns true when the tip is shown now
    function show() {
        if (key === "" || app.hintShown(key) || running)
            return false
        if (!ignorePause && app.lastTipShown > 0 && Date.now() - app.lastTipShown < 45000)
            return false
        app.setHintShown(key)
        app.lastTipShown = Date.now()
        running = true
        if (showHand)
            touchHint.start()
        label.opacity = 1
        hideTimer.restart()
        return true
    }
    function hide() {
        running = false
        touchHint.stop()
        label.opacity = 0
    }
    property bool running: false

    TouchInteractionHint {
        id: touchHint
        direction: root.direction
        interactionMode: root.interactionMode
        loops: 3
        x: root.vertical ? (root.handCenterX >= 0 ? root.handCenterX - width / 2 : (root.width - width) / 2) : 0
        y: root.direction === TouchInteraction.Up ? root.height - height
                                                  : (root.vertical ? 0 : (root.height - height) / 2)
    }

    InteractionHintLabel {
        id: label
        anchors.bottom: root.labelAtTop ? undefined : parent.bottom
        anchors.bottomMargin: root.labelBottomMargin
        anchors.top: root.labelAtTop ? parent.top : undefined
        width: parent.width - root.labelRightMargin
        opacity: 0
        Behavior on opacity { FadeAnimation { duration: 400 } }
    }

    // No input of its own: the gesture it shows must stay possible meanwhile.
    // It ends by itself after a few seconds.
    Timer {
        id: hideTimer
        interval: 7000
        onTriggered: root.hide()
    }
}
