// SPDX-License-Identifier: GPL-3.0-or-later
// Search (release 1.1): over the articles on this device – titles, or
// titles and text – in all feeds or in the current selection, plus feeds
// whose title matches. The results use their own ArticleModel (searchText
// filter), so preview, next/previous and marking work as in the list.
// The News API has no server-side search.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    // Opened from the feed list's pulley: always all feeds (the selection of
    // the article list means little there), and a feed tapped in the results
    // replaces this page with the article list (no detour over the feed list).
    property Item feedsPage: null
    readonly property bool hasSelection: !feedsPage
                                         && (app.currentFeedId >= 0 || app.currentFolderId >= 0 || app.currentStarred)
    property string query: ""
    // Values of the header's fields: items inside the ListView header are not
    // reachable by id from the page (the search never ran – found 14 Oct)
    property string typed: ""
    property bool titlesOnly: false
    property bool inSelection: false
    readonly property var matchingFeeds: query.trim().length >= 2 ? app.feeds.feedsMatching(query).slice(0, 5) : []

    function applyScope() {
        if (inSelection && hasSelection) {
            if (app.currentStarred)
                results.selectStarred()
            else
                results.setSelection(app.currentFolderId, app.currentFeedId)
        } else {
            results.setSelection(-1, -1)
        }
        if (!results.loaded && !results.inOperation)
            results.load()
    }

    ArticleModel {
        id: results
        storage: localstorage
        unreadOnly: false
        searchOnly: true
        searchFullText: !page.titlesOnly
        searchText: page.query.trim().length >= 2 ? page.query : ""
    }

    Component.onCompleted: applyScope()
    // Keyboard up once the page is there (not a "focus" binding, see SubscribePage)


    // Filter shortly after typing stops
    Timer {
        id: queryTimer
        interval: 350
        onTriggered: page.query = page.typed
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        // Always the same model (empty without a search): switching the
        // model while typing could take the focus from the search field
        model: results
        // No current item: otherwise the list gives the focus to row 0 when the
        // first results appear and the search field loses it (as solved in
        // Sailwave's AdvancedSearchPage)
        currentIndex: -1

        header: ListHeaderColumn {
            flickable: list
            PageHeader { title: qsTr("Search") }
            SearchField {
                id: searchField
                width: parent.width
                placeholderText: qsTr("Search articles and feeds")
                inputMethodHints: Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: { queryTimer.stop(); page.query = text; focus = false }
                onTextChanged: { page.typed = text; queryTimer.restart() }
                // Keyboard up once (not a "focus" binding, see SubscribePage)
                Component.onCompleted: forceActiveFocus()
            }
            ComboBox {
                id: modeBox
                width: parent.width
                label: qsTr("Search in")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Titles and text"); onClicked: page.titlesOnly = false }
                    MenuItem { text: qsTr("Titles only"); onClicked: page.titlesOnly = true }
                }
            }
            ComboBox {
                id: scopeBox
                width: parent.width
                visible: page.hasSelection
                label: qsTr("Articles of")
                currentIndex: 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("All feeds"); onClicked: { page.inSelection = false; page.applyScope() } }
                    MenuItem { text: app.currentSelectionName; onClicked: { page.inSelection = true; page.applyScope() } }
                }
            }

            // Feeds whose title matches: tap selects the feed
            SectionHeader {
                visible: page.matchingFeeds.length > 0
                text: qsTr("Feeds")
            }
            Repeater {
                model: page.matchingFeeds
                FolderItem {
                    name: modelData.title
                    unreadCount: modelData.unread
                    iconSource: modelData.favicon
                    onClicked: {
                        var fromFeeds = page.feedsPage !== null
                        app.selectFeed(modelData.id, modelData.title)
                        // From the feed list (its attached article list was
                        // dropped when this page was pushed): this page is
                        // replaced by the article list, which slides in from
                        // the right. Going back to the feed list first showed
                        // it for a moment (device test 15 Oct). Swiping back
                        // from there leads to the feed list as usual.
                        if (fromFeeds)
                            pageStack.replace(Qt.resolvedUrl("ArticleListPage.qml"))
                        else
                            pageStack.pop()
                    }
                }
            }

            SectionHeader {
                visible: results.searchText !== "" && list.count > 0
                text: list.count === 1 ? qsTr("1 article") : qsTr("%1 articles").arg(list.count)
            }
        }

        delegate: ArticleDelegate {
            onClicked: {
                results.markRead(index)
                pageStack.push(Qt.resolvedUrl("ArticlePage.qml"),
                               { articleData: results.get(index), articleModel: results })
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !results.inOperation
            text: results.searchText === "" ? qsTr("Search your articles")
                                            : qsTr("No articles found")
            hintText: qsTr("Searches the articles on this device, also read ones.")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: results.inOperation && results.searchText !== ""
        }

        VerticalScrollDecorator { }
    }
}
