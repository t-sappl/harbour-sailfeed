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
        articles.showRead = true
        if (articles.canFetchOlder)
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
        onOlderFetched: if (newArticles === 0 && list.count > 0) hint.show(qsTr("No older articles"))
        onFetchOlderFailed: hint.show(message !== "" ? message : qsTr("Could not load older articles"))
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
        // Ask for the account on first start.
        if (status === PageStatus.Active && !config.isAccountValid && !loginShown) {
            loginShown = true
            pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
        }
    }
    property bool loginShown: false

    SilicaListView {
        id: list
        anchors.fill: parent
        model: articles

        property real dragStartY: 0
        readonly property real pageHeight: height - (headerItem ? headerItem.height : 0)

        header: PageHeader {
            title: app.currentSelectionName
            description: synchronizer.inOperation
                         ? qsTr("Updating …")
                         : (synchronizer.error ? qsTr("Refresh failed – pull down to try again")
                                               : (articles.showRead ? qsTr("Including read articles") : ""))
        }

        // "Show read articles" / "Load older articles" (read ones the first
        // sync did not fetch) as calm list rows, and room below the last
        // article, so it can be scrolled above the mark-all button.
        footer: Column {
            width: list.width
            Item { width: 1; height: Theme.paddingMedium; visible: readRow.visible || olderRow.visible }
            ListActionItem {
                id: readRow
                visible: page.readOffer && list.count > 0
                iconSource: "image://theme/icon-m-acknowledge"
                text: qsTr("Show read articles")
                description: qsTr("Older, read articles available")
                onClicked: page.showReadArticles()
            }
            ListActionItem {
                id: olderRow
                visible: page.olderAvailable && list.count > 0
                iconSource: "image://theme/icon-m-down"
                text: articles.fetchingOlder ? qsTr("Loading older articles …") : qsTr("Load older articles")
                busy: articles.fetchingOlder
                onClicked: articles.fetchOlder()
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
                if (app.skipPreview && (d.url || "").toString() !== "") {
                    // Setting "Skip the preview": straight to the full article
                    app.lastOpenedItemId = d.itemId
                    if (app.openMode === 2) {
                        Qt.openUrlExternally(d.url)
                        return
                    }
                    pageStack.push(Qt.resolvedUrl("WebPage.qml"),
                                   { url: d.url, readerMode: app.openMode === 0, itemId: d.itemId,
                                     feedId: d.feedId || -1, feedTitle: d.feedTitle || "",
                                     articleModel: articles, starred: d.starred === true })
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
            MenuItem { text: qsTr("Feeds"); onClicked: pageStack.navigateBack() }
            MenuItem {
                text: qsTr("Mark all as read")
                visible: list.count > 0
                enabled: articles.unreadCount > 0
                onClicked: page.markAllWithRemorse()
            }
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
            text: !config.isAccountValid ? qsTr("No account")
                : (synchronizer.inOperation ? qsTr("Loading articles …")
                   : (page.readOffer ? qsTr("No unread articles") : qsTr("No articles")))
            hintText: !config.isAccountValid ? qsTr("Pull down to sign in")
                : (synchronizer.inOperation ? ""
                   : (page.readOffer ? qsTr("Older, read articles available") : qsTr("Pull down to refresh")))
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

            cancelFlick()
            var delta = contentY - dragStartY
            var dir = Math.abs(delta) < Theme.itemSizeSmall ? 0 : (delta > 0 ? 1 : -1)
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

    // Empty list: the same action row, in the lower part of the screen
    // (child of the page, not of the list: a ListView puts its children into
    // its moving content item)
    ListActionItem {
        anchors { bottom: parent.bottom; bottomMargin: Theme.itemSizeHuge }
        visible: list.count === 0 && (page.olderAvailable || page.readOffer)
                 && !articles.inOperation && !articles.fetchingOlder && config.isAccountValid
        iconSource: page.readOffer ? "image://theme/icon-m-acknowledge" : "image://theme/icon-m-down"
        text: page.readOffer ? qsTr("Show read articles") : qsTr("Load older articles")
        onClicked: page.readOffer ? page.showReadArticles() : articles.fetchOlder()
    }

    RemorsePopup { id: markAllRemorse }

    function markAllWithRemorse() {
        markAllRemorse.execute(qsTr("Marking all as read"), function() { page.seenIds = ({}); articles.markAllRead() })
    }

    // Push-up button for one-handed "mark all as read"
    MarkAllButton {
        anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; bottom: parent.bottom; bottomMargin: Theme.paddingLarge * 2 }
        visible: app.showMarkAllButton && list.count > 0 && config.isAccountValid && !pulley.active
        dimmed: list.moving
        allRead: articles.unreadCount === 0
        onTriggered: page.markAllWithRemorse()
        onTapped: hint.show(allRead ? qsTr("All articles are read")
                                    : qsTr("Push the button up to mark all as read"))
    }

    Hint { id: hint }

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

    // Marks every fully visible article as read; partly visible ones stay unread.
    // deferred: only remember them (seenIds), marked when the list moves on.
    function markVisibleRead(deferred) {
        var top = list.contentY
        var bottom = top + list.height
        var x = list.width / 2
        var rows = []
        // Walk down the visible area item by item (works without itemAtIndex, Qt 5.6).
        // Section headers are not delegates, so step over gaps where itemAt() is null.
        var y = top + 1
        while (y < bottom) {
            var item = list.itemAt(x, y)
            if (!item) { y += Theme.paddingSmall; continue }
            if (item.y >= top && item.y + item.height <= bottom) {
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
