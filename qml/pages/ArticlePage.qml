// SPDX-License-Identifier: GPL-3.0-or-later
// Article preview: title, feed / date, image, body; then a calm row "Show full
// article" (reader mode; "Show website" / "Open in browser" for the other open
// modes). Share, copy link and the browser are in the pulley (15 Oct).
import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Share 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    // Snapshot from ArticleModel.get(row): itemId, title, feedTitle, dateText,
    // body (cleaned HTML), url, thumb, hasContent, unread, starred
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
        if (enterFrom !== 0) {
            slideIn.from = enterFrom * (page.width > 0 ? page.width : Screen.width)
            slideIn.start()
        }
        app.lastOpenedItemId = articleData.itemId || -1
        // Counts toward the one-time hint "skip the preview" (10th preview)
        app.notePreviewOpened()
        if (page.openMode === 0 && articleData.url)
            Prefetch.prefetch(articleData.url)
    }

    // The web view (reader mode or original) is attached: swipe left to open it.
    onStatusChanged: {
        if (status !== PageStatus.Active)
            return
        if (!skipOnReturn && page.openMode !== 2 && articleData.url)
            fullArticleTipTimer.restart()
        if (skipOnReturn) {
            backTimer.start()
            return
        }
        if (page.openMode !== 2 && articleData.url && !pageStack.nextPage(page))
            pageStack.pushAttached(Qt.resolvedUrl("WebPage.qml"),
                                   { url: articleData.url, readerMode: page.openMode === 0,
                                     itemId: articleData.itemId, feedId: articleData.feedId || -1,
                                     feedTitle: articleData.feedTitle || "", articleModel: articleModel,
                                     starred: page.starred, articleTitle: articleData.title || "" })
    }

    Timer {
        id: backTimer
        interval: 0
        onTriggered: pageStack.navigateBack(PageStackAction.Immediate)
    }

    // Text size of this article's feed (or the general one, see app.previewStepFor)
    readonly property int feedId: articleData.feedId || -1
    // "Full article" of this feed (feed settings) or the general one
    readonly property int openMode: app.openModeFor(feedId)
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
        if (page.openMode === 2)
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
        // Without the page stack's animation (always from the right): the new
        // article slides in from the side of the button, the previous one
        // from the left (device test 13 Oct)
        pageStack.replace(Qt.resolvedUrl("ArticlePage.qml"),
                          { articleData: articleModel.get(row), articleModel: articleModel, enterFrom: delta },
                          PageStackAction.Immediate)
    }

    // Set by openSibling(): 1 next (slides in from the right), -1 previous (from the left)
    property int enterFrom: 0

    // Links in feed texts are usually absolute; "/path" and "path" are
    // taken relative to the article's address.
    function absoluteLink(link) {
        var s = String(link)
        var base = (articleData.url || "").toString()
        if (/^[a-z][a-z0-9+.-]*:/i.test(s) || base === "")
            return s
        var m = base.match(/^([a-z]+:\/\/[^\/]+)(.*\/)?/i)
        if (!m)
            return s
        if (s.indexOf("//") === 0)
            return base.split(":")[0] + ":" + s
        return s.charAt(0) === "/" ? m[1] + s : m[1] + (m[2] || "/") + s
    }

    readonly property string siblingInfo: {
        var row = articleModel ? articleModel.rowOfId(articleData.itemId) : -1
        return row < 0 ? "" : (row + 1) + " / " + articleModel.count
    }

    SilicaFlickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: navBar.top }
        transform: Translate { id: slide }
        clip: true
        contentHeight: column.height + Theme.paddingLarge
        // Hidden while skipped on the way back from the web view
        opacity: page.skipOnReturn ? 0 : 1

        // About the article and its link (15 Oct: bookmark and text size are
        // in the bottom bar and left the pulley). The lowest entry is reached first.
        PullDownMenu {
            MenuItem {
                visible: (articleData.url || "").toString() !== ""
                text: qsTr("Copy link")
                onClicked: {
                    Clipboard.text = articleData.url.toString()
                    hint.show(qsTr("Link copied"))
                }
            }
            MenuItem {
                visible: (articleData.url || "").toString() !== ""
                text: qsTr("Share")
                onClicked: shareAction.trigger()
            }
            MenuItem {
                visible: (articleData.url || "").toString() !== ""
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally(articleData.url)
            }
            MenuItem {
                text: page.unread ? qsTr("Mark as read") : qsTr("Mark as unread")
                onClicked: {
                    page.unread = !page.unread
                    articleModel.setUnreadById(articleData.itemId, page.unread)
                }
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
            // Reached by paging from an article with text: say why it is
            // empty instead of jumping to the full article (1.2)
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: articleData.hasContent === false
                text: qsTr("The feed sends no text for this article.")
                wrapMode: Text.Wrap
                font.pixelSize: page.bodyFontPx
                font.italic: true
                color: Theme.secondaryHighlightColor
            }
            Label {
                id: bodyLabel
                visible: articleData.hasContent !== false
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                // linkColor only works for StyledText; RichText takes the link
                // color from CSS (links were dark blue, hard to read).
                text: "<style>a:link { color: " + Theme.highlightColor + "; }</style>" + (articleData.body || "")
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                font.pixelSize: page.bodyFontPx
                linkColor: Theme.highlightColor

                // Links: tap as set ("Links in articles"), long press the
                // other way (a Label alone has no long press on links)
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var link = bodyLabel.linkAt(mouse.x, mouse.y)
                        if (link !== "")
                            app.openLink(page.absoluteLink(link), false)
                    }
                    onPressAndHold: {
                        var link = bodyLabel.linkAt(mouse.x, mouse.y)
                        if (link !== "")
                            app.openLink(page.absoluteLink(link), true)
                    }
                }
            }

            // A calm row instead of two buttons "Share" / "Show full article"
            // (device test 15 Oct: dominant and redundant – the full article is
            // also a swipe to the left, sharing is in the pulley). Says what it
            // does, so it is not mixed up with "Open in browser" in the pulley.
            ListActionItem {
                width: parent.width
                visible: (articleData.url || "").toString() !== ""
                text: page.openMode === 2 ? qsTr("Open in browser")
                    : page.openMode === 1 ? qsTr("Show website")
                    : qsTr("Show full article")
                // The same as a swipe to the left – said here, so the gesture
                // is learnt (device test 15 Oct); no swipe for the browser
                description: page.openMode === 2 ? "" : qsTr("or swipe left")
                onClicked: page.openWebsite()
            }
        }
        // At the end of the article: push up for the next one.
        PushUpMenu {
            MenuItem { text: qsTr("Next article"); onClicked: page.openSibling(1) }
            MenuItem { text: qsTr("Previous article"); onClicked: page.openSibling(-1) }
        }

        VerticalScrollDecorator { }
    }

    NumberAnimation {
        id: slideIn
        target: slide
        property: "x"
        to: 0
        duration: 250
        easing.type: Easing.OutCubic
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
            // No hint: the filled star in the bar shows it (device test 15 Oct)
        }
    }

    Hint { id: hint; anchors.bottomMargin: navBar.height }

    FirstRunHint {
        id: fullArticleTip
        key: "fullArticle"
        direction: TouchInteraction.Left
        interactionMode: TouchInteraction.Swipe
        text: qsTr("Swipe left for the full article")
    }
    Timer {
        id: fullArticleTipTimer
        interval: 1200
        onTriggered: if (page.status === PageStatus.Active) fullArticleTip.show()
    }

    ShareAction {
        id: shareAction
        mimeType: "text/x-url"
        resources: [{ "type": "text/x-url",
                      "linkTitle": articleData.title || "",
                      "status": (articleData.url || "").toString() }]
    }
}
