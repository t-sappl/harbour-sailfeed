// SPDX-License-Identifier: GPL-3.0-or-later
// Sort order of the feed list (FeedModel.SortMode), on the feed list and in
// the settings.
import QtQuick 2.6
import Sailfish.Silica 1.0

ComboBox {
    width: parent ? parent.width : Screen.width
    label: qsTr("Sort feed list")
    currentIndex: app.feedSortMode
    menu: ContextMenu {
        MenuItem { text: qsTr("Unread first") }
        MenuItem { text: qsTr("Alphabetical") }
        MenuItem { text: qsTr("Most active") }
        MenuItem { text: qsTr("Most bookmarked") }
        MenuItem { text: qsTr("Unread first, then by activity") }
    }
    onCurrentIndexChanged: if (currentIndex !== app.feedSortMode) app.feedSortMode = currentIndex
}
