// SPDX-License-Identifier: GPL-3.0-or-later
// Article preview: title, feed / date, image, body; then "Share" and "Show full article"
// (reader mode; "Show website" / "Open in browser" for the other open modes).
import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Share 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    // Snapshot from ArticleModel.get(row): itemId, title, feedTitle, dateText,
    // body (cleaned HTML), url, thumb, unread, starred
    property var articleData: ({})
    property var articleModel

    property bool starred: articleData.starred === true
    property bool unread: articleData.unread === true

    // Set by WebPage when it was reached from here: going back from the web
    // view then skips this preview and returns straight to the article list.
    property bool skipOnReturn: false

    // Reader mode: download the web page in the background while the preview
    // is being read, so "Open website" can show the article right away.
    Component.onCompleted: {
        app.lastOpenedItemId = articleData.itemId || -1
        if (app.openMode === 0 && articleData.url)
            Prefetch.prefetch(articleData.url)
    }

    // The web view (reader mode or original) is attached: swipe left to open it.
    onStatusChanged: {
        if (status !== PageStatus.Active)
            return
        if (skipOnReturn) {
            backTimer.start()
            return
        }
        if (app.openMode !== 2 && articleData.url && !pageStack.nextPage(page))
            pageStack.pushAttached(Qt.resolvedUrl("WebPage.qml"),
                                   { url: articleData.url, readerMode: app.openMode === 0,
                                     itemId: articleData.itemId, feedId: articleData.feedId || -1,
                                     feedTitle: articleData.feedTitle || "", articleModel: articleModel,
                                     starred: page.starred })
    }

    Timer {
        id: backTimer
        interval: 0
        onTriggered: pageStack.navigateBack(PageStackAction.Immediate)
    }

    // Text size of this article's feed (or the general one, see app.previewStepFor)
    readonly property int feedId: articleData.feedId || -1
    readonly property int fontStep: app.previewStepFor(feedId)
    readonly property int bodyFontPx: Math.round(Theme.fontSizeSmall * (1 + 0.12 * fontStep))

    function changeFontSize(delta) {
        var step = Math.max(-2, Math.min(6, fontStep + delta))
        app.setPreviewStep(feedId, step)
        var percent = Math.round(100 * (1 + 0.12 * step))
        hint.show(app.textSizePerFeed && feedId > 0
                  ? qsTr("Text size %1 % for %2").arg(percent).arg(articleData.feedTitle || "")
                  : qsTr("Text size %1 %").arg(percent))
    }

    function openWebsite() {
        if (app.openMode === 2)
            Qt.openUrlExternally(articleData.url)
        else if (pageStack.nextPage(page))
            pageStack.navigateForward()
    }

    // Opens the next (delta = 1) or previous (-1) article of the list in place.
    function openSibling(delta) {
        var row = articleModel.rowOfId(articleData.itemId)
        if (row < 0)
            return
        row += delta
        if (row < 0 || row >= articleModel.count) {
            hint.show(delta > 0 ? qsTr("Last article") : qsTr("First article"))
            return
        }
        articleModel.markRead(row)
        pageStack.replace(Qt.resolvedUrl("ArticlePage.qml"),
                          { articleData: articleModel.get(row), articleModel: articleModel })
    }

    readonly property string siblingInfo: {
        var row = articleModel ? articleModel.rowOfId(articleData.itemId) : -1
        return row < 0 ? "" : (row + 1) + " / " + articleModel.count
    }

    SilicaFlickable {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: navBar.top }
        clip: true
        contentHeight: column.height + Theme.paddingLarge
        // Hidden while skipped on the way back from the web view
        opacity: page.skipOnReturn ? 0 : 1

        PullDownMenu {
            MenuItem {
                text: page.starred ? qsTr("Remove bookmark") : qsTr("Bookmark")
                onClicked: {
                    page.starred = !page.starred
                    articleModel.setStarredById(articleData.itemId, page.starred)
                }
            }
            MenuItem {
                text: page.unread ? qsTr("Mark as read") : qsTr("Mark as unread")
                onClicked: {
                    page.unread = !page.unread
                    articleModel.setUnreadById(articleData.itemId, page.unread)
                }
            }
            MenuItem {
                text: qsTr("Smaller text")
                enabled: page.fontStep > -2
                onClicked: page.changeFontSize(-1)
            }
            MenuItem {
                text: qsTr("Larger text")
                enabled: page.fontStep < 6
                onClicked: page.changeFontSize(1)
            }
        }

        // In landscape and on tablets the text column keeps a readable width.
        Column {
            id: column
            width: Math.min(parent.width, Screen.width * 1.3)
            x: (parent.width - width) / 2
            spacing: Theme.paddingLarge

            Item { width: 1; height: Theme.paddingLarge }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                text: articleData.title || ""
                textFormat: Text.PlainText
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
                wrapMode: Text.Wrap
                color: Theme.highlightColor
            }
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: Theme.paddingSmall
                Label {
                    width: parent.width - (starIcon.visible ? starIcon.width + parent.spacing : 0)
                    text: (articleData.feedTitle || "") + " / " + (articleData.dateText || "")
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
                Icon {
                    id: starIcon
                    visible: page.starred
                    source: "image://theme/icon-s-favorite"
                    color: Theme.highlightColor
                }
            }
            Image {
                id: leadImage
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                height: visible ? Math.min(width * 9 / 16, Screen.width * 0.6) : 0
                visible: (articleData.thumb || "") !== "" && status !== Image.Error
                source: articleData.thumb || ""
                sourceSize.width: width
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                clip: true
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                // linkColor only works for StyledText; RichText takes the link
                // color from CSS (links were dark blue, hard to read).
                text: "<style>a:link { color: " + Theme.highlightColor + "; }</style>" + (articleData.body || "")
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                font.pixelSize: page.bodyFontPx
                linkColor: Theme.highlightColor
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Row {
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingLarge
                Button {
                    width: (column.width - 2 * Theme.horizontalPageMargin - Theme.paddingLarge) / 2
                    text: qsTr("Share")
                    onClicked: shareAction.trigger()
                }
                Button {
                    width: (column.width - 2 * Theme.horizontalPageMargin - Theme.paddingLarge) / 2
                    // Says what it does, so it is not mixed up with "open in
                    // browser" (globe in the bottom bar, device test 11 Oct)
                    text: app.openMode === 2 ? qsTr("Open in browser")
                        : app.openMode === 1 ? qsTr("Show website")
                        : qsTr("Show full article")
                    enabled: (articleData.url || "").toString() !== ""
                    onClicked: page.openWebsite()
                }
            }
        }
        // At the end of the article: push up for the next one.
        PushUpMenu {
            MenuItem { text: qsTr("Next article"); onClicked: page.openSibling(1) }
            MenuItem { text: qsTr("Previous article"); onClicked: page.openSibling(-1) }
        }

        VerticalScrollDecorator { }
    }

    ArticleNavBar {
        id: navBar
        visible: page.siblingInfo !== "" && !page.skipOnReturn
        info: page.siblingInfo
        showTextSize: true
        onPrevious: page.openSibling(-1)
        onNext: page.openSibling(1)
        onSmaller: if (page.fontStep > -2) page.changeFontSize(-1)
        onLarger: if (page.fontStep < 6) page.changeFontSize(1)
        showBookmark: true
        bookmarked: page.starred
        onToggleBookmark: {
            page.starred = !page.starred
            articleModel.setStarredById(articleData.itemId, page.starred)
            hint.show(page.starred ? qsTr("Bookmark set") : qsTr("Bookmark removed"))
        }
        showBrowser: !!articleData.url
        onOpenBrowser: Qt.openUrlExternally(articleData.url)
    }

    Hint { id: hint; anchors.bottomMargin: navBar.height }

    ShareAction {
        id: shareAction
        mimeType: "text/x-url"
        resources: [{ "type": "text/x-url",
                      "linkTitle": articleData.title || "",
                      "status": (articleData.url || "").toString() }]
    }
}
