// SPDX-License-Identifier: GPL-3.0-or-later
// Round button floating at the bottom right of the article list: push it up
// to mark all articles as read, reachable with the thumb of the hand holding
// the phone. A tap only explains what to do (no accidental marking).
// When everything is read, the button is greyed out and cannot be pushed.
import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: root

    signal triggered()
    signal tapped()

    // How far the knob has to travel up
    readonly property real track: Theme.itemSizeLarge * 1.6
    readonly property real knobSize: Theme.itemSizeMedium
    readonly property real restY: track
    readonly property real progress: Math.max(0, Math.min(1, (restY - knob.y) / track))
    readonly property bool armed: progress >= 0.85
    property bool dimmed: false
    // Nothing left to mark: greyed out, no dragging (a tap still explains)
    property bool allRead: false

    width: knobSize
    height: knobSize + track

    // Track and target, only while dragging
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: knobSize / 2
        width: Theme.paddingSmall
        height: track
        radius: width / 2
        color: Theme.rgba(Theme.highlightColor, 0.4)
        opacity: dragArea.drag.active ? 1 : 0
        Behavior on opacity { FadeAnimator { duration: 150 } }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: knobSize * 0.7
        height: width
        y: (knobSize - height) / 2
        radius: width / 2
        color: "transparent"
        border.width: Math.round(2 * Theme.pixelRatio)
        border.color: root.armed ? Theme.highlightColor : Theme.rgba(Theme.highlightColor, 0.5)
        opacity: dragArea.drag.active ? 1 : 0
        Behavior on opacity { FadeAnimator { duration: 150 } }
    }
    Label {
        anchors { right: parent.left; rightMargin: Theme.paddingMedium; verticalCenter: parent.top; verticalCenterOffset: knobSize / 2 }
        text: root.armed ? qsTr("Release: mark all as read") : qsTr("Mark all as read")
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.highlightColor
        opacity: dragArea.drag.active ? 1 : 0
        Behavior on opacity { FadeAnimator { duration: 150 } }
    }

    Rectangle {
        id: knob
        width: root.knobSize
        height: width
        radius: width / 2
        y: root.restY
        color: root.allRead ? Theme.rgba(Theme.highlightDimmerColor, 0.5)
                            : Theme.rgba(Theme.highlightDimmerColor, 0.85)
        border.width: Math.round(2 * Theme.pixelRatio)
        border.color: root.allRead ? Theme.rgba(Theme.secondaryColor, 0.4)
                    : (dragArea.pressed || root.armed ? Theme.highlightColor : Theme.rgba(Theme.highlightColor, 0.6))
        opacity: dragArea.pressed && !root.allRead ? 1 : (root.dimmed ? 0.35 : (root.allRead ? 0.6 : 0.85))
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on opacity { FadeAnimator { duration: 200 } }
        Behavior on y {
            enabled: !dragArea.drag.active
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        Icon {
            anchors.centerIn: parent
            source: "image://theme/icon-m-acknowledge"
            color: root.allRead ? Theme.secondaryColor : Theme.highlightColor
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            // Keep the list (and the page stack) from taking over the gesture.
            preventStealing: true
            drag.target: root.allRead ? null : knob
            drag.axis: Drag.YAxis
            drag.minimumY: 0
            drag.maximumY: root.restY
            drag.filterChildren: false
            onReleased: {
                var fire = root.armed
                knob.y = root.restY
                if (fire)
                    root.triggered()
            }
            onCanceled: knob.y = root.restY
            onClicked: if (root.progress < 0.1) root.tapped()
        }
    }
}
