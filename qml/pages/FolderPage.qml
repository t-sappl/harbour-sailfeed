// SPDX-License-Identifier: GPL-3.0-or-later
// Selection: "All articles" or one Nextcloud News folder, with unread counters.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    function select(folderId, name) {
        app.currentFolderName = name
        app.currentFolderId = folderId      // ArticleModel reloads on change
        pageStack.pop()
    }

    FolderListFilterModel {
        id: folders
        storage: localstorage
        sortingRole: Fuoten.Name
        sortOrder: Qt.AscendingOrder
        Component.onCompleted: load()
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: folders

        header: Column {
            width: list.width
            PageHeader { title: qsTr("Folders") }
            FolderItem {
                name: qsTr("All articles")
                unreadCount: localstorage.totalUnread
                current: app.currentFolderId < 0
                onClicked: page.select(-1, name)
            }
        }

        delegate: FolderItem {
            name: model.display ? model.display.name : ""
            unreadCount: model.display ? model.display.unreadCount : 0
            current: model.display && model.display.id == app.currentFolderId
            onClicked: page.select(model.display.id, model.display.name)
        }

        VerticalScrollDecorator { }
    }
}
