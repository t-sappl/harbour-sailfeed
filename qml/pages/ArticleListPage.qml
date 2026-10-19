// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    readonly property var articles: app.articles

    // Velocity (px/s) above which scrolling is free instead of page-wise.
    readonly property real fastFlickThreshold: 2500 * Theme.pixelRatio
    readonly property real sectionHeight: Theme.itemSizeExtraSmall
    // Room above the first article of a following page: the camera of the
    // Jolla Phone covered part of it (device test 13 Oct). The display cutout
    // where Silica reports one (Screen.topCutout, not on older releases),
    // at least two large paddings.
    readonly property real topInset: {
        var cut = 0
        if (page.isPortrait && Screen.topCutout !== undefined && Screen.topCutout)
            cut = Screen.topCutout.height + Theme.paddingSmall
        return Math.max(cut, 2 * Theme.paddingLarge)
    }
    // A short swipe turns the page: distance or speed (device test 13 Oct:
    // paging took too much effort)
    readonly property real pageDragDistance: Theme.paddingLarge
    readonly property real pageFlickVelocity: 300 * Theme.pixelRatio

    // "Mark when scrolling on": ids of articles that were fully visible for the
    // delay; they are greyed out when the list moves on (or the page is left
    // for the feed list), not while they are still being read.
    property var seenIds: ({})

    // Older (read) articles can be loaded: not for "starred", and not with
    // "unread only" (they would stay hidden).
    readonly property bool olderAvailable: articles.canFetchOlder && (!app.unreadOnly || articles.showRead)
                                           && !app.currentStarred
    // "Unread only": offer to show the read (and older) articles of this
    // selection once, instead of the "load older" button.
    readonly property bool readOffer: app.unreadOnly && !articles.showRead && !app.currentStarred
                                      && config.isAccountValid
                                      && (articles.hiddenCount > 0 || articles.canFetchOlder)

    function showReadArticles() {
        olderHintWanted = false
        articles.showRead = true
        if (articles.canFetchOlder)
            articles.fetchOlder()
    }

    // "No older articles" only answers a tap on "Load older articles" – not
    // "Show read articles" (which also loads older ones) or the automatic
    // load after selecting a feed (device test 14 Oct: the hint appeared
    // when showing the read articles)
    property bool olderHintWanted: false
    function loadOlder() {
        olderHintWanted = true
        articles.fetchOlder()
    }

    Connections {
        target: articles
        // A feed or folder was selected: if it shows only a few articles
        // (the rest was read before the first sync), load older ones once.
        onInOperationChanged: {
            if (articles.inOperation || !app.autoOlderPending)
                return
            app.autoOlderPending = false
            if (page.olderAvailable && articles.count < 10)
                articles.fetchOlder()
        }
        onOlderFetched: {
            if (newArticles === 0 && list.count > 0 && page.olderHintWanted)
                hint.show(qsTr("No older articles"))
            // Oldest first: they went to the top, far from the button
            else if (newArticles > 0 && app.oldestFirst)
                hint.show(newArticles === 1 ? qsTr("1 older article added at the top")
                                            : qsTr("%1 older articles added at the top").arg(newArticles))
            page.olderHintWanted = false
        }
        onFetchOlderFailed: {
            page.olderHintWanted = false
            hint.show(message !== "" ? message : qsTr("Could not load older articles"))
        }
    }

    onStatusChanged: {
        // Back to the feed list: FeedsPage calls flushSeen() when it appears.
        // (pageStack.nextPage(page) cannot be asked here: an attached page is
        // "not present in stack" for Silica, the call threw and the seen
        // articles were never marked – device log 11 Oct.)
        // Back from an article (maybe after next/previous): keep it in view.
        if (status === PageStatus.Active && app.lastOpenedItemId >= 0) {
            var row = articles.rowOfId(app.lastOpenedItemId)
            app.lastOpenedItemId = -1
            if (row >= 0)
                list.positionViewAtIndex(row, ListView.Contain)
        }
        // First-run tips (one per visit, see showNextTip)
        if (status === PageStatus.Active && config.isAccountValid)
            feedsTipTimer.restart()
        // Ask for the account on first start.
        if (status === PageStatus.Active && !config.isAccountValid && !loginShown) {
            loginShown = true
            pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
        }
    }
    property bool loginShown: false

    // The action rows in the footer: with articles always, in an empty list
    // once loading is over (and signed in)
    readonly property bool actionsShown: list.count > 0
                                         || (!articles.inOperation && !synchronizer.inOperation
                                             && config.isAccountValid)

    SilicaListView {
        id: list
        anchors.fill: parent
        model: articles
        // A press that turns into a swipe within this time no longer
        // highlights the article under the finger (device test 13 Oct)
        pressDelay: 120

        property real dragStartY: 0
        readonly property real pageHeight: height - (headerItem ? headerItem.height : 0)

        header: PageHeader {
            id: listHeader
            // No search button here: the top left corner belongs to Silica's
            // navigation (the glow of the attached feed list, tap = back) and
            // the button took its taps (device test 14 Oct). Search is in the
            // pulley of this page and of the feed list.
            title: app.currentSelectionName
            description: synchronizer.inOperation
                         ? qsTr("Updating …")
                         : (synchronizer.error ? qsTr("Refresh failed – pull down to try again")
                                               : (articles.showRead ? qsTr("Including read articles") : ""))
        }

        // "Show read articles" / "Load older articles" (read ones the first
        // sync did not fetch) as calm list rows, and room below the last
        // article, so it can be scrolled above the mark-all button.
        // Always here, also for an empty list: directly below the articles,
        // but at least at the lower third of the screen (one-handed). Before,
        // an empty list showed it at the bottom of the page and a short one
        // right below its articles, so it jumped (device test 14 Oct).
        footer: Column {
            id: footerColumn
            width: list.width
            // Space down to the lower third; footerColumn.y is where the
            // articles end (in content coordinates)
            Item {
                width: 1
                visible: readRow.visible || olderRow.visible
                height: Math.max(Theme.paddingMedium,
                                 list.height * 0.66 - (footerColumn.y - list.originY))
            }
            ListActionItem {
                id: readRow
                visible: page.readOffer && page.actionsShown
                iconSource: "image://theme/icon-m-acknowledge"
                text: qsTr("Show read articles")
                description: qsTr("Older, read articles available")
                onClicked: page.showReadArticles()
            }
            ListActionItem {
                id: olderRow
                visible: page.olderAvailable && page.actionsShown
                iconSource: "image://theme/icon-m-down"
                text: articles.fetchingOlder ? qsTr("Loading older articles …") : qsTr("Load older articles")
                busy: articles.fetchingOlder
                onClicked: page.loadOlder()
            }
            Item {
                width: parent.width
                height: app.showMarkAllButton ? Theme.itemSizeMedium + Theme.paddingLarge * 2 : Theme.paddingLarge
            }
        }

        section {
            property: "day"
            criteria: ViewSection.FullString
            delegate: SectionHeader {
                height: page.sectionHeight
                text: section
                horizontalAlignment: Text.AlignLeft
            }
        }

        delegate: ArticleDelegate {
            id: articleItem
            readonly property bool firstInSection: ListView.previousSection !== ListView.section
            onClicked: {
                articles.markRead(index)
                var d = articles.get(index)
                // General settings or the feed's own (feed settings)
                var mode = app.openModeFor(d.feedId || -1)
                // Setting "Skip the preview", or nothing to preview (1.2: the
                // feed sends no text, e.g. news.orf.at): straight to the full article
                if ((app.skipPreviewFor(d.feedId || -1) || d.hasContent === false)
                        && (d.url || "").toString() !== "") {
                    app.lastOpenedItemId = d.itemId
                    if (mode === 2) {
                        Qt.openUrlExternally(d.url)
                        return
                    }
                    pageStack.push(Qt.resolvedUrl("WebPage.qml"),
                                   { url: d.url, readerMode: mode === 0, itemId: d.itemId,
                                     feedId: d.feedId || -1, feedTitle: d.feedTitle || "",
                                     articleModel: articles, starred: d.starred === true,
                                     articleTitle: d.title || "" })
                    return
                }
                pageStack.push(Qt.resolvedUrl("ArticlePage.qml"),
                               { articleData: d, articleModel: articles })
            }
            menu: Component {
                ContextMenu {
                    MenuItem {
                        text: articleItem.isUnread ? qsTr("Mark as read") : qsTr("Mark as unread")
                        onClicked: {
                            var itemId = articles.idAt(index)
                            delete page.seenIds[itemId]
                            articles.setUnreadById(itemId, !articleItem.isUnread)
                        }
                    }
                    MenuItem {
                        text: articleItem.isStarred ? qsTr("Remove bookmark") : qsTr("Bookmark")
                        onClicked: articles.setStarredById(articles.idAt(index), !articleItem.isStarred)
                    }
                    MenuItem {
                        visible: index > 0
                        text: qsTr("Mark all above as read")
                        onClicked: page.markAboveRead(index)
                    }
                    MenuItem {
                        visible: index < list.count - 1
                        text: qsTr("Mark all below as read")
                        onClicked: page.markBelowRead(index)
                    }
                }
            }
        }

        PullDownMenu {
            id: pulley
            busy: synchronizer.inOperation
            MenuItem { text: qsTr("Settings"); onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml")) }
            // Only while one feed is selected; at the top: rarely used
            MenuItem {
                visible: app.currentFeedId >= 0
                text: qsTr("Feed settings")
                onClicked: pageStack.push(Qt.resolvedUrl("FeedSettingsPage.qml"), { feedId: app.currentFeedId })
            }
            MenuItem { text: qsTr("Feeds"); onClicked: pageStack.navigateBack() }
            MenuItem {
                text: qsTr("Mark all as read")
                visible: list.count > 0
                enabled: articles.unreadCount > 0
                onClicked: page.markAllWithRemorse()
            }
            // Right above "Refresh": a short pull (device test 14 Oct)
            MenuItem { text: qsTr("Search"); onClicked: pageStack.push(Qt.resolvedUrl("SearchPage.qml")) }
            MenuItem {
                text: config.isAccountValid ? qsTr("Refresh") : qsTr("Sign in")
                enabled: !synchronizer.inOperation
                onClicked: {
                    if (config.isAccountValid)
                        synchronizer.sync()
                    else
                        pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
                }
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !articles.inOperation && !articles.fetchingOlder
            // No feeds yet (first start with an empty Nextcloud News): say
            // how to get some, where it is done (15 Oct)
            text: !config.isAccountValid ? qsTr("No account")
                : (synchronizer.inOperation ? qsTr("Loading articles …")
                   : (app.noFeeds ? qsTr("No feeds yet")
                      : (page.readOffer ? qsTr("No unread articles") : qsTr("No articles"))))
            hintText: !config.isAccountValid ? qsTr("Pull down to sign in")
                : (synchronizer.inOperation ? ""
                   : (app.noFeeds ? qsTr("Swipe right to the feed list, pull down there and choose \"Subscribe to website\".")
                      : (page.readOffer ? qsTr("Older, read articles available") : qsTr("Pull down to refresh"))))
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: list.count === 0 && (articles.inOperation || synchronizer.inOperation || articles.fetchingOlder)
        }

        // --- Rubber-band paging ------------------------------------------
        onDraggingChanged: {
            if (dragging) {
                pageAnim.stop()
                dragStartY = contentY
                return
            }
            // Pulley menu open (pulled beyond the top): leave contentY alone.
            // Silica triggers the selected MenuItem only when the list returns
            // to the top on its own; our page animation would prevent that.
            if (pulley.active || contentY < originY)
                return
            if (Math.abs(verticalVelocity) >= page.fastFlickThreshold)
                return                      // fast flick: let it scroll freely

            var delta = contentY - dragStartY
            var v = verticalVelocity
            var dir = 0
            if (Math.abs(delta) >= page.pageDragDistance)
                dir = delta > 0 ? 1 : -1
            else if (Math.abs(v) >= page.pageFlickVelocity && (v > 0) === (delta >= 0))
                dir = v > 0 ? 1 : -1
            cancelFlick()
            scrollToPage(dragStartY + dir * pageHeight)
        }

        onFlickEnded: {
            // After a free flick, snap to the start of the topmost article.
            if (!pageAnim.running && !pulley.active && contentY >= originY)
                scrollToPage(contentY)
        }

        function scrollToPage(y) {
            var minY = originY
            var maxY = Math.max(originY, originY + contentHeight - height)
            y = Math.max(minY, Math.min(maxY, y))
            var item = itemAt(width / 2, y + 1)
            if (item && y < maxY) {
                var top = item.y
                if (item.firstInSection)
                    top -= page.sectionHeight          // keep the day header visible
                top -= page.topInset                    // clear of the camera
                if (headerItem && top <= headerItem.y + headerItem.height)
                    top = minY                          // first page: show the page header
                y = Math.max(minY, top)
            }
            pageAnim.to = y
            pageAnim.restart()
        }

        NumberAnimation {
            id: pageAnim
            target: list
            property: "contentY"
            duration: 250
            easing.type: Easing.OutCubic
        }

        // --- Auto mark-as-read -------------------------------------------
        onMovementStarted: { autoReadTimer.stop(); page.flushSeen() }
        onMovementEnded: page.restartAutoRead()
        onCountChanged: if (!moving) page.restartAutoRead()

        VerticalScrollDecorator { }
    }

    Timer {
        id: autoReadTimer
        interval: Math.max(1, app.autoReadDelay) * 1000
        onTriggered: {
            if (page.status === PageStatus.Active && Qt.application.active && !list.moving)
                page.markVisibleRead(app.autoReadMode === 1)
        }
    }

    RemorsePopup { id: markAllRemorse }

    function markAllWithRemorse() {
        markAllRemorse.execute(qsTr("Marking all as read"), function() { page.seenIds = ({}); articles.markAllRead() })
    }

    // Push-up button for one-handed "mark all as read"
    MarkAllButton {
        id: markAllButton
        anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; bottom: parent.bottom; bottomMargin: Theme.paddingLarge * 2 }
        visible: app.showMarkAllButton && list.count > 0 && config.isAccountValid && !pulley.active
        dimmed: list.moving
        allRead: articles.unreadCount === 0
        onTriggered: page.markAllWithRemorse()
        onTapped: hint.show(allRead ? qsTr("All articles are read")
                                    : qsTr("Push the button up to mark all as read"))
    }

    Hint { id: hint }

    // First-run tips of this page, one per visit and in this order (the
    // tip component also keeps 45 s between any two tips):
    // 1. swipe right to the feeds (also with no feeds yet: that is where
    //    subscribing starts), 2. push up the mark-all button (it looks like a
    //    button, but a tap does nothing), 3. long press on an article.
    function showNextTip() {
        if (status !== PageStatus.Active)
            return
        if (list.count > 0 || app.noFeeds) {
            if (feedsTip.show())
                return
        }
        if (!app.hintShown("feedList") || list.count === 0)
            return
        if (markAllButton.visible && !markAllButton.allRead) {
            if (markAllTip.show())
                return
        }
        if (app.hintShown("markAll") || !markAllButton.visible)
            articleMenuTip.show()
    }

    FirstRunHint {
        id: feedsTip
        key: "feedList"
        direction: TouchInteraction.Right
        interactionMode: TouchInteraction.Swipe
        text: qsTr("Swipe right for your feeds and folders")
    }
    FirstRunHint {
        id: markAllTip
        key: "markAll"
        direction: TouchInteraction.Up
        interactionMode: TouchInteraction.Swipe
        handCenterX: markAllButton.x + markAllButton.width / 2
        // The text right beside the button, at its height
        labelRightMargin: page.width - markAllButton.x + Theme.paddingSmall
        labelBottomMargin: page.height - markAllButton.y - markAllButton.height
        text: qsTr("Push the round button up to mark all listed articles as read")
    }
    FirstRunHint {
        id: articleMenuTip
        key: "articleMenu"
        showHand: false
        text: qsTr("Press and hold an article: bookmark, mark as unread, mark all above or below as read")
    }
    Timer {
        id: feedsTipTimer
        interval: 1200
        onTriggered: page.showNextTip()
    }

    function flushSeen() {
        var ids = []
        for (var key in seenIds)
            ids.push(Number(key))
        seenIds = ({})
        if (ids.length > 0)
            articles.markIdsRead(ids)
    }

    function markAboveRead(row) {
        var rows = []
        for (var i = 0; i < row; i++)
            rows.push(i)
        articles.markRowsRead(rows)
    }

    function markBelowRead(row) {
        var rows = []
        for (var i = row + 1; i < articles.count; i++)
            rows.push(i)
        articles.markRowsRead(rows)
    }

    function restartAutoRead() {
        if (app.autoReadDelay > 0)
            autoReadTimer.restart()
    }

    // Marks every fully visible article as read; partly visible ones stay
    // unread – except at the end of the list: it cannot scroll further, so an
    // article cut off at the top there (it was cut off at the bottom of the
    // page before) counts when at least half of it is visible. Before, such
    // articles were never marked (device test 14 Oct).
    // deferred: only remember them (seenIds), marked when the list moves on.
    function markVisibleRead(deferred) {
        var top = list.contentY
        var bottom = top + list.height
        var atEnd = list.contentY >= list.originY + list.contentHeight - list.height - 1
        var x = list.width / 2
        var rows = []
        // Walk down the visible area item by item (works without itemAtIndex, Qt 5.6).
        // Section headers are not delegates, so step over gaps where itemAt() is null.
        var y = top + 1
        while (y < bottom) {
            var item = list.itemAt(x, y)
            if (!item) { y += Theme.paddingSmall; continue }
            var shown = Math.min(bottom, item.y + item.height) - Math.max(top, item.y)
            if ((item.y >= top && item.y + item.height <= bottom)
                    || (atEnd && item.y < top && shown >= item.height / 2)) {
                var row = list.indexAt(x, item.y + 1)
                if (row >= 0)
                    rows.push(row)
            }
            y = item.y + item.height + 1
        }
        if (rows.length === 0)
            return
        if (!deferred) {
            articles.markRowsRead(rows)
            return
        }
        for (var r = 0; r < rows.length; r++) {
            var itemId = articles.idAt(rows[r])
            if (itemId >= 0)
                seenIds[itemId] = true
        }
    }
}
