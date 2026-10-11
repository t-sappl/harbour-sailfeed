// SPDX-License-Identifier: GPL-3.0-or-later
// Feed list. This is the first page on the stack; the article list is
// attached and shown right away, so swiping right on the article list (the
// usual "back" gesture) reveals this page. Tapping a feed, a folder,
// "All articles" or "Starred" selects it and returns to the article list.
// Feeds and folders are managed here: pulley (add) and long press (rename,
// move, delete, mark as read).
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    property bool firstActivation: true

    function showArticles() {
        if (pageStack.nextPage(page))
            pageStack.navigateForward()
    }

    onStatusChanged: {
        // Re-sort while swiping in (not continuously, so nothing moves under
        // the finger): feeds and folders with unread articles on top.
        if (status === PageStatus.Activating && !firstActivation) {
            // Coming back from the article list: mark the articles it
            // remembered as seen ("when scrolling on"), then re-sort.
            var articlesPage = pageStack.nextPage(page)
            if (articlesPage && articlesPage.flushSeen)
                articlesPage.flushSeen()
            app.feeds.resort()
        }
        if (status !== PageStatus.Active)
            return
        if (!pageStack.nextPage(page))
            pageStack.pushAttached(Qt.resolvedUrl("ArticleListPage.qml"))
        if (firstActivation) {
            firstActivation = false
            startTimer.start()      // let the page stack settle first
        }
    }

    Timer {
        id: startTimer
        interval: 0
        onTriggered: pageStack.navigateForward(PageStackAction.Immediate)
    }

    // --- Feed management ---------------------------------------------------
    // folderId: preselected folder (0: none; default: the current folder)
    function addFeed(folderId) {
        pageStack.push(Qt.resolvedUrl("AddFeedDialog.qml"),
                       { preselectedFolderId: folderId !== undefined ? Math.max(0, folderId)
                                                                     : Math.max(0, app.currentFolderId) })
    }
    function addFolder() {
        var d = pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                               { title: qsTr("Add folder"), acceptText: qsTr("Add") })
        d.accepted.connect(function() { FeedManager.addFolder(d.value) })
    }
    function renameFeed(feedId, title) {
        var d = pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                               { title: qsTr("Rename feed"), initialValue: title })
        d.accepted.connect(function() { FeedManager.renameFeed(feedId, d.value) })
    }
    function moveFeed(feedId, title, folderId) {
        var d = pageStack.push(Qt.resolvedUrl("FolderPickerDialog.qml"),
                               { feedTitle: title, currentFolderId: Math.max(0, folderId) })
        d.accepted.connect(function() { FeedManager.moveFeed(feedId, d.folderId) })
    }
    function renameFolder(folderId, name) {
        var d = pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                               { title: qsTr("Rename folder"), initialValue: name })
        d.accepted.connect(function() { FeedManager.renameFolder(folderId, d.value) })
    }

    Connections {
        target: FeedManager
        onSucceeded: {
            switch (action) {
            case "addFeed":
                hint.show(qsTr("Feed added"))
                if (!synchronizer.inOperation)
                    synchronizer.sync()
                break
            case "addFolder":
                hint.show(qsTr("Folder added"))
                break
            case "deleteFeed":
            case "deleteFolder":
                // The selection may be gone; the article list loses their articles.
                if (app.currentFeedId >= 0 || app.currentFolderId >= 0)
                    app.selectAll()
                else
                    app.articles.reload()
                break
            }
            // Folder names and sections come from the database again.
            app.feeds.reload()
        }
        onFailed: hint.show(message !== "" ? message : qsTr("The server rejected the change"))
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: app.feedList

        // Menus in the header (sort order, "All articles") open downwards,
        // see ListHeaderColumn.
        header: ListHeaderColumn {
            flickable: list
            PageHeader { title: qsTr("Feeds") }
            // Sort order: a plain combo box like Sailwave's "Sort" (the tinted
            // band with a line looked heavy, above all in light ambiences –
            // device test 11 Oct); a gap separates it from the list.
            FeedSortComboBox { }
            Item { width: 1; height: Theme.paddingMedium }
            FolderItem {
                id: allItem
                name: qsTr("All articles")
                bold: true
                unreadCount: localstorage.totalUnread
                current: app.currentFeedId < 0 && app.currentFolderId < 0 && !app.currentStarred
                onClicked: { app.selectAll(); page.showArticles() }
                menu: unreadCount > 0 ? allMenu : null
                Component {
                    id: allMenu
                    ContextMenu {
                        MenuItem {
                            text: qsTr("Mark all as read")
                            onClicked: {
                                var feeds = app.feeds       // see the delegate below
                                allItem.remorseAction(qsTr("Marking all as read"), function() { feeds.markAllRead() })
                            }
                        }
                    }
                }
            }
            FolderItem {
                name: qsTr("Bookmarks")
                bold: true
                iconSource: "image://theme/icon-s-favorite"
                unreadCount: localstorage.starred
                current: app.currentStarred
                onClicked: { app.selectStarred(); page.showArticles() }
            }
        }

        // One delegate for folder rows (bold; "Other feeds" for feeds without
        // a folder) and feed rows (indented, favicon), see FeedRowsModel.
        //
        // Remorse callbacks run after the countdown, when the context menu and
        // its QML context are gone: everything they need is taken into local
        // variables first (otherwise "app" is undefined there, the action
        // fails and the row stays covered by the countdown – device test 11 Oct).
        delegate: FolderItem {
            id: row
            readonly property bool isFolder: model.rowType === "folder"
            readonly property bool isOther: model.isOther
            readonly property int rowFeedId: model.feedId
            readonly property int rowFolderId: model.folderId

            name: model.title
            bold: isFolder
            unreadCount: model.unreadCount
            iconSource: isFolder ? "" : model.favicon
            favorite: !isFolder && model.favorite
            indent: isFolder ? 0 : Theme.paddingLarge
            // "Other feeds" is selected as folder 0 (feeds without a folder)
            readonly property int selectFolderId: isOther ? 0 : rowFolderId
            current: isFolder ? (selectFolderId >= 0 && selectFolderId === app.currentFolderId && app.currentFeedId < 0)
                              : rowFeedId === app.currentFeedId
            onClicked: {
                if (!isFolder) {
                    app.selectFeed(rowFeedId, name)
                    page.showArticles()
                } else {
                    // a folder, or "Other feeds" (folder 0: the feeds without a folder)
                    app.selectFolder(selectFolderId, name)
                    page.showArticles()
                }
            }
            menu: !config.isAccountValid ? null : (!isFolder ? feedMenu : (isOther ? otherMenu : folderMenu))

            Component {
                id: folderMenu
                ContextMenu {
                    MenuItem {
                        visible: row.unreadCount > 0
                        text: qsTr("Mark folder as read")
                        onClicked: {
                            var feeds = app.feeds
                            var folderId = row.rowFolderId
                            row.remorseAction(qsTr("Marking folder as read"), function() { feeds.markFolderRead(folderId) })
                        }
                    }
                    MenuItem {
                        text: qsTr("Add feed")
                        enabled: !FeedManager.busy
                        onClicked: page.addFeed(row.rowFolderId)
                    }
                    MenuItem {
                        text: qsTr("Rename")
                        enabled: !FeedManager.busy
                        onClicked: page.renameFolder(row.rowFolderId, row.name)
                    }
                    MenuItem {
                        text: qsTr("Delete")
                        enabled: !FeedManager.busy
                        onClicked: {
                            var manager = FeedManager
                            var folderId = row.rowFolderId
                            row.remorseAction(model.feedCount > 0 ? qsTr("Deleting folder and its feeds")
                                                                  : qsTr("Deleting folder"),
                                              function() { manager.deleteFolder(folderId) })
                        }
                    }
                }
            }
            Component {
                id: otherMenu
                ContextMenu {
                    MenuItem {
                        visible: row.unreadCount > 0
                        text: qsTr("Mark as read")
                        onClicked: {
                            var feeds = app.feeds
                            row.remorseAction(qsTr("Marking as read"), function() { feeds.markFolderRead(0) })
                        }
                    }
                    MenuItem {
                        text: qsTr("Add feed")
                        enabled: !FeedManager.busy
                        onClicked: page.addFeed(0)
                    }
                }
            }
            Component {
                id: feedMenu
                ContextMenu {
                    MenuItem {
                        visible: row.unreadCount > 0
                        text: qsTr("Mark feed as read")
                        onClicked: {
                            var feeds = app.feeds
                            var feedId = row.rowFeedId
                            row.remorseAction(qsTr("Marking feed as read"), function() { feeds.markFeedRead(feedId) })
                        }
                    }
                    MenuItem {
                        text: model.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                        onClicked: app.feeds.setFavorite(row.rowFeedId, !model.favorite)
                    }
                    MenuItem {
                        text: qsTr("Rename")
                        enabled: !FeedManager.busy
                        onClicked: page.renameFeed(row.rowFeedId, row.name)
                    }
                    MenuItem {
                        text: qsTr("Move to folder")
                        enabled: !FeedManager.busy
                        onClicked: page.moveFeed(row.rowFeedId, row.name, row.rowFolderId)
                    }
                    MenuItem {
                        text: qsTr("Delete")
                        enabled: !FeedManager.busy
                        onClicked: {
                            var manager = FeedManager
                            var feedId = row.rowFeedId
                            row.remorseAction(qsTr("Deleting feed"), function() { manager.deleteFeed(feedId) })
                        }
                    }
                }
            }
        }

        PullDownMenu {
            busy: synchronizer.inOperation || FeedManager.busy
            MenuItem { text: qsTr("Settings"); onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml")) }
            MenuItem {
                text: qsTr("Add folder")
                visible: config.isAccountValid
                enabled: !FeedManager.busy
                onClicked: page.addFolder()
            }
            MenuItem {
                text: qsTr("Add feed")
                visible: config.isAccountValid
                enabled: !FeedManager.busy
                onClicked: page.addFeed()
            }
            MenuItem {
                text: qsTr("Refresh")
                enabled: config.isAccountValid && !synchronizer.inOperation
                onClicked: synchronizer.sync()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No feeds")
            hintText: config.isAccountValid ? qsTr("Pull down to add a feed") : qsTr("Sign in first")
        }

        VerticalScrollDecorator { }
    }

    Hint { id: hint }
}
