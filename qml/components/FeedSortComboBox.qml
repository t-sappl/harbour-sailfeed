// SPDX-License-Identifier: GPL-3.0-or-later
// Sort order of the feed list (FeedModel.SortMode), on the feed list and in
// the settings.
import QtQuick 2.6
import Sailfish.Silica 1.0

ComboBox {
    width: parent ? parent.width : Screen.width
    label: qsTr("Sort feed list")
    currentIndex: app.feedSortMode
    // Written on tap only: onCurrentIndexChanged can fire while a page is
    // built and change the setting by itself (device test 13 Oct)
    menu: ContextMenu {
        MenuItem { text: qsTr("Unread first"); onClicked: app.feedSortMode = 0 }
        MenuItem { text: qsTr("Alphabetical"); onClicked: app.feedSortMode = 1 }
        MenuItem { text: qsTr("Most active"); onClicked: app.feedSortMode = 2 }
        MenuItem { text: qsTr("Most bookmarked"); onClicked: app.feedSortMode = 3 }
        MenuItem { text: qsTr("Unread first, then by activity"); onClicked: app.feedSortMode = 4 }
    }
}
