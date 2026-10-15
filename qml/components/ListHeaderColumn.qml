// SPDX-License-Identifier: GPL-3.0-or-later
// Column for a ListView header that holds items which change their height,
// e.g. a ComboBox or a ListItem with a context menu.
//
// A ListView header is attached to the first list item, so it grows UPWARDS:
// when a menu in the header opens, everything above it moves up and the menu
// seems to open upwards, out of the screen (the same problem as Sailwave's
// filter sections). Each height change is therefore compensated by moving the
// view by the same amount, as long as the lower edge of the header is on
// screen: the tapped item stays in place and the menu opens downwards.
// Changes completely above the visible area need no compensation.
import QtQuick 2.6
import Sailfish.Silica 1.0

Column {
    id: root

    property Flickable flickable
    // Compensate only while a menu in the header is open (and shortly after,
    // while it closes): otherwise the rows being built or rebuilt (re-sorting)
    // also moved the view and pushed the page header out (feed list, device
    // test 15 Oct). Off: every height change is compensated, as before.
    property bool limitToMenus: false
    property bool menuOpen: false
    readonly property bool _compensate: !limitToMenus || menuOpen || closingTimer.running
    onMenuOpenChanged: if (!menuOpen) closingTimer.restart()
    Timer {
        id: closingTimer
        interval: 800       // longer than the menu's closing animation
    }

    width: flickable ? flickable.width : 0

    property real _lastHeight: -1

    onHeightChanged: {
        var oldHeight = _lastHeight
        _lastHeight = height
        if (oldHeight < 0 || !flickable || !_compensate)
            return
        var delta = height - oldHeight
        if (delta === 0)
            return
        // Lower edge before the change, in view coordinates
        var lowerEdge = root.mapToItem(flickable, 0, oldHeight).y
        if (lowerEdge >= 0)
            flickable.contentY -= delta
    }
}
