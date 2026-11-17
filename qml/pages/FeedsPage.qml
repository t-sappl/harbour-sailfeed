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
            // Remember where the list was: a changed order is a model reset,
            // and a reset throws the view to the top (11 Nov: "the list
            // always jumps to the top when I come back from the articles")
            keptY = (list.contentY - list.originY > (list.headerItem ? list.headerItem.height : 0))
                    ? list.contentY - list.originY : -1
            app.feeds.resort()
            // Safety net while the page slides in (not visible as a jump):
            // header only partly visible → back to the very top. The cause –
            // the header's height compensation acting while rows were built
            // – is limited to open menus now (device test 15 Oct).
            fixTop()
            // A few more times while the page slides in: the re-sorted rows
            // are built in the next frames and pushed the header out (back by
            // a tap on "‹ Feeds", device test 11 Nov). Done during the slide,
            // not when the page is in, where the correction was a visible jump.
            fixTopTimer.runs = 0
            fixTopTimer.restart()
        }
        if (status !== PageStatus.Active)
            return
        if (!pageStack.nextPage(page))
            pageStack.pushAttached(Qt.resolvedUrl("ArticleListPage.qml"))
        if (firstActivation) {
            firstActivation = false
            startTimer.start()      // let the page stack settle first
        } else if (app.noFeeds) {
            subscribeTipTimer.restart()
        }
    }

    // First-run tip while there are no feeds yet: subscribing is in the pulley
    FirstRunHint {
        id: subscribeTip
        key: "subscribe"
        ignorePause: true
        direction: TouchInteraction.Down
        interactionMode: TouchInteraction.Pull
        text: qsTr("Pull down and choose \"Subscribe to website\"")
    }
    Timer {
        id: subscribeTipTimer
        interval: 1000
        onTriggered: if (page.status === PageStatus.Active && app.noFeeds) subscribeTip.show()
    }

    // Distance of the view from the top before the re-sort; -1 = was at the top
    property real keptY: -1

    function fixTop() {
        if (list.dragging || list.flicking)
            return
        if (keptY >= 0) {
            var maxY = Math.max(0, list.contentHeight - list.height)
            list.contentY = list.originY + Math.min(keptY, maxY)
            return
        }
        if (list.headerItem && list.contentY - list.originY < list.headerItem.height)
            list.contentY = list.originY
    }



    Timer {
        id: fixTopTimer
        property int runs: 0
        interval: 40
        repeat: true
        onTriggered: {
            page.fixTop()
            if (++runs >= 5) {
                stop()
                keptY = -1
            }
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
        pageStack.push(Qt.resolvedUrl("SubscribePage.qml"),
                       { preselectedFolderId: folderId !== undefined ? Math.max(0, folderId)
                                                                     : Math.max(0, app.currentFolderId) })
    }
    // New folder (1.4): name, then the feeds to put into it (FolderFeedsDialog),
    // then create it and move the chosen feeds one after the other
    function addFolder() {
        var d = pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                               { title: qsTr("Add folder"), acceptText: qsTr("Next") })
        // The feed choice REPLACES the name dialog: the stack is then just
        // feed list → feed choice, and accepting it pops back to the feed
        // list (11 Nov: with the name dialog still below it, "accept" landed
        // on the name dialog and the feeds were lost)
        d.acceptDestination = Qt.resolvedUrl("FolderFeedsDialog.qml")
        d.acceptDestinationAction = PageStackAction.Replace
        // The name dialog is gone by then: hand over the text as it is typed
        var update = function() {
            d.acceptDestinationProperties = { newName: d.currentText, feedsPage: page }
        }
        d.currentTextChanged.connect(update)
        update()
    }
    property var pendingFolder: null    // { name, ids } while it is created
    property var moveQueue: []          // [{ feed, folder }]
    property int moveFolderId: 0        // > 0 while moves run
    property int movedCount: 0
    function runMoves(moves, folderId) {
        if (moves.length === 0)
            return
        moveFolderId = folderId
        movedCount = 0
        moveQueue = moves
        moveNext()
    }
    function chooseFolderFeeds(folderId, name) {
        pageStack.push(Qt.resolvedUrl("FolderFeedsDialog.qml"),
                       { folderId: folderId, existingName: name, feedsPage: page })
    }
    function createFolderWithFeeds(name, ids) {
        pendingFolder = { name: name, ids: ids }
        if (!FeedManager.addFolder(name)) {
            pendingFolder = null
            hint.show(qsTr("Busy – try again in a moment"))
        }
    }
    // "Delete folder, keep feeds" (12 Nov): the feeds are moved out to "no
    // folder" one after another, then the empty folder is deleted (deleting
    // a folder on the server deletes its feeds, too)
    property int deleteAfterMoves: 0    // folder to delete when the moves are done
    property int keptFeeds: 0
    // Countdown on the feed list, then: with its feeds (the server deletes them
    // with the folder) or the feeds moved out first
    RemorsePopup { id: folderRemorse }
    function deleteFolderWith(folderId, withFeeds) {
        var manager = FeedManager
        if (withFeeds) {
            folderRemorse.execute(qsTr("Deleting folder and its feeds"), function() {
                if (!manager.deleteFolder(folderId))
                    hint.show(qsTr("Busy – try again in a moment"))
            })
        } else {
            folderRemorse.execute(qsTr("Deleting folder"), function() { page.deleteFolderKeepFeeds(folderId) })
        }
    }
    function deleteFolderKeepFeeds(folderId) {
        var all = app.feeds.feedsByFolder()
        var moves = []
        for (var i = 0; i < all.length; i++)
            if (all[i].folderId === folderId)
                moves.push({ feed: all[i].id, folder: 0 })
        keptFeeds = moves.length
        if (moves.length === 0) {
            keptFeeds = 0
            if (!FeedManager.deleteFolder(folderId))
                hint.show(qsTr("Busy – try again in a moment"))
            return
        }
        deleteAfterMoves = folderId
        runMoves(moves, folderId)
    }
    function moveNext() {
        if (moveQueue.length === 0) {
            var del = deleteAfterMoves
            deleteAfterMoves = 0
            if (del > 0) {
                moveFolderId = 0
                if (!FeedManager.deleteFolder(del)) {
                    keptFeeds = 0
                    hint.show(qsTr("Busy – try again in a moment"))
                }
                return
            }
            if (movedCount > 0)
                hint.show(movedCount === 1 ? qsTr("1 feed moved") : qsTr("%1 feeds moved").arg(movedCount))
            moveFolderId = 0
            return
        }
        var q = moveQueue
        var m = q.shift()
        moveQueue = q
        if (!FeedManager.moveFeed(m.feed, m.folder)) {
            moveQueue = []
            moveFolderId = 0
            deleteAfterMoves = 0
            keptFeeds = 0
            hint.show(qsTr("Busy – try again in a moment"))
        }
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
                if (page.pendingFolder && page.pendingFolder.ids.length > 0) {
                    var name = page.pendingFolder.name
                    var folders = FeedManager.folders()
                    var id = 0
                    for (var i = 0; i < folders.length; i++)
                        if (folders[i].name === name)
                            id = folders[i].id
                    if (id > 0) {
                        var moves = []
                        for (var j = 0; j < page.pendingFolder.ids.length; j++)
                            moves.push({ feed: page.pendingFolder.ids[j], folder: id })
                        page.pendingFolder = null
                        hint.show(qsTr("Folder added"))
                        page.runMoves(moves, id)
                        break
                    }
                }
                page.pendingFolder = null
                hint.show(qsTr("Folder added"))
                break
            case "moveFeed":
                if (page.moveFolderId > 0) {
                    page.movedCount++
                    page.moveNext()
                }
                break
            case "deleteFeed":
            case "deleteFolder":
                if (action === "deleteFolder" && page.keptFeeds > 0) {
                    hint.show(page.keptFeeds === 1 ? qsTr("Folder deleted, 1 feed kept")
                                                   : qsTr("Folder deleted, %1 feeds kept").arg(page.keptFeeds))
                    page.keptFeeds = 0
                }
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
        onFailed: {
            page.pendingFolder = null
            page.moveQueue = []
            page.moveFolderId = 0
            page.deleteAfterMoves = 0
            page.keptFeeds = 0
            hint.show(message !== "" ? message : qsTr("The server rejected the change"))
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: app.feedList

        // Menus in the header (sort order, "All articles") open downwards,
        // see ListHeaderColumn.
        header: ListHeaderColumn {
            flickable: list
            limitToMenus: true
            menuOpen: sortLine.menuOpen || allItem.menuOpen
            PageHeader { title: qsTr("Feeds") }
            PageEdgeHints { rightText: qsTr("Articles"); visible: config.isAccountValid; onRightClicked: page.showArticles() }
            // Sort order on the list itself, as in the article list and
            // Sailcast: "Sorted: unread first ›" with the choice as a menu
            SortLine {
                id: sortLine
                options: [ { name: qsTr("Unread first"), short: qsTr("unread first") },
                           { name: qsTr("Alphabetical"), short: qsTr("alphabetical") },
                           { name: qsTr("Most active"), short: qsTr("most active") },
                           { name: qsTr("Most bookmarked"), short: qsTr("most bookmarked") },
                           { name: qsTr("Unread first, then by activity"), short: qsTr("unread first, then activity") } ]
                currentIndex: app.feedSortMode
                onChosen: app.feedSortMode = index
            }
            Item { width: 1; height: Theme.paddingMedium }
            FolderItem {
                id: allItem
                name: qsTr("All articles")
                bold: true
                unreadCount: Math.max(0, localstorage.totalUnread - app.feeds.mutedUnread)
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
            muted: !isFolder && app.mutedIds.indexOf(model.feedId) >= 0
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
                        text: qsTr("Subscribe to website")
                        enabled: !FeedManager.busy
                        onClicked: page.addFeed(row.rowFolderId)
                    }
                    MenuItem {
                        text: qsTr("Choose feeds …")
                        enabled: !FeedManager.busy
                        onClicked: page.chooseFolderFeeds(row.rowFolderId, row.name)
                    }
                    MenuItem {
                        text: qsTr("Rename")
                        enabled: !FeedManager.busy
                        onClicked: page.renameFolder(row.rowFolderId, row.name)
                    }
                    // With feeds: "Delete folder …" asks first whether the feeds
                    // go, too (FolderDeleteDialog, as Sailcast); an empty folder
                    // goes with the countdown only
                    MenuItem {
                        text: model.feedCount > 0 ? qsTr("Delete folder …") : qsTr("Delete")
                        enabled: !FeedManager.busy
                        onClicked: {
                            var folderId = row.rowFolderId
                            var feedsPage = page    // the delegate may be gone on accept
                            if (model.feedCount > 0) {
                                var d = pageStack.push(Qt.resolvedUrl("FolderDeleteDialog.qml"),
                                                       { folderName: row.name, feedCount: model.feedCount })
                                d.accepted.connect(function() { feedsPage.deleteFolderWith(folderId, d.withFeeds) })
                            } else {
                                page.deleteFolderWith(folderId, false)
                            }
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
                        text: qsTr("Subscribe to website")
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
                        text: app.isMuted(row.rowFeedId) ? qsTr("Unmute feed") : qsTr("Mute feed")
                        onClicked: app.setFeedSetting("feedMuted", row.rowFeedId, app.isMuted(row.rowFeedId) ? -1 : 1)
                    }
                    // Name, folder and the feed's own reading settings
                    MenuItem {
                        text: qsTr("Feed settings …")
                        onClicked: pageStack.push(Qt.resolvedUrl("FeedSettingsPage.qml"), { feedId: row.rowFeedId })
                    }
                    MenuItem {
                        text: qsTr("Delete (unsubscribe)")
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
                text: qsTr("Subscribe to website")
                visible: config.isAccountValid
                enabled: !FeedManager.busy
                onClicked: page.addFeed()
            }
            // Right above "Refresh", as in the article list; searches all feeds
            MenuItem {
                text: qsTr("Search")
                visible: config.isAccountValid
                onClicked: pageStack.push(Qt.resolvedUrl("SearchPage.qml"), { feedsPage: page })
            }
            MenuItem {
                text: qsTr("Refresh")
                enabled: config.isAccountValid && !synchronizer.inOperation
                onClicked: synchronizer.sync()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: config.isAccountValid ? qsTr("No feeds yet") : qsTr("No feeds")
            hintText: config.isAccountValid ? qsTr("Pull down and choose \"Subscribe to website\": Sailfeed finds the feeds of a website for you. Coming from another feed reader? Settings → Import and export feeds.")
                                            : qsTr("Sign in first")
        }

        VerticalScrollDecorator { }
    }

    Hint { id: hint }
}
