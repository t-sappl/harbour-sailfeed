// SPDX-License-Identifier: GPL-3.0-or-later
// Full article: reader mode (Mozilla Readability, no ads/banners) or the
// original website, inside the app (Sailfish WebView).
import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0
import harbour.sailfeed 1.0
import "../components"

WebViewPage {
    id: page

    property url url
    property bool readerMode: true
    // For next/previous article navigation (optional)
    property int itemId: -1
    property int feedId: -1             // for the per-feed text size
    property string feedTitle
    property var articleModel: null
    property bool starred: false        // bookmark of the shown article

    // reader mode phases
    readonly property int phaseOriginal: 0    // loading/showing the original page
    readonly property int phaseExtracting: 1  // running Readability
    readonly property int phaseReader: 2      // loading the generated reader page
    readonly property int phaseShown: 3       // reader page visible
    readonly property int phaseFailed: 4      // no readable article: original shown
    property int phase: phaseOriginal

    // Consent wall ("with ads or subscription", e.g. golem.de): 0 none,
    // 1 found in the downloaded page, now loading the site in the web view
    // (its cookies may already hold the choice), 2 wall shown, waiting for
    // the user's choice; the reader page follows once the article loads.
    property int consentStage: 0

    readonly property bool covered: readerMode && phase !== phaseShown && phase !== phaseFailed
                                    && consentStage < 2

    property bool started: false
    property bool waitingPrefetch: false
    property int extractTries: 0

    function cssColor(c, alpha) {
        var a = alpha !== undefined ? alpha : c.a
        return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + ","
                + Math.round(c.b * 255) + "," + a.toFixed(2) + ")"
    }

    // Reader colors: like the ambience, light, dark or sepia (setting)
    readonly property string readerBackground: {
        switch (app.readerColors) {
        case 1: return "#fafafa"
        case 2: return "#111418"
        case 3: return "#f4ecd8"
        default: return Theme.colorScheme === Theme.LightOnDark ? "#111418" : "#fafafa"
        }
    }

    // Effective reader colors are dark (dark scheme, or "like the ambience" in a dark ambience)
    readonly property bool readerDark: app.readerColors === 2
                                       || (app.readerColors === 0 && Theme.colorScheme === Theme.LightOnDark)

    // Light/dark toggle: switches the setting and recolors the shown page
    // without reloading it (scroll position stays).
    function toggleReaderColors() {
        app.readerColors = readerDark ? 1 : 2
        if (phase === phaseShown) {
            var st = readerStyle()
            var css = "html,body{background:" + st.background + " !important;color:" + st.text + " !important}"
                    + "a{color:" + st.link + " !important}"
                    + ".site,.byline,figcaption,small,blockquote{color:" + st.secondary + " !important;border-color:" + st.secondary + " !important}"
            webView.runJavaScript("var s=document.getElementById('sf-colors');"
                                  + "if(!s){s=document.createElement('style');s.id='sf-colors';document.head.appendChild(s);}"
                                  + "s.textContent=" + JSON.stringify(css) + ";return true")
        }
        hint.show(readerDark ? qsTr("Dark colors") : qsTr("Light colors"))
    }

    function readerStyle() {
        var colors
        switch (app.readerColors) {
        case 1: colors = { text: "#1d1d1f", secondary: "rgba(29,29,31,0.62)", link: "#0a62b0" }; break
        case 2: colors = { text: "#e6e6e6", secondary: "rgba(230,230,230,0.62)", link: "#5ec8ff" }; break
        case 3: colors = { text: "#4a3a2a", secondary: "rgba(74,58,42,0.65)", link: "#9a4a12" }; break
        default:
            colors = { text: cssColor(Theme.primaryColor, 1), secondary: cssColor(Theme.secondaryColor),
                       link: cssColor(Theme.highlightColor, 1) }
        }
        return {
            background: readerBackground,
            text: colors.text,
            secondary: colors.secondary,
            link: colors.link,
            fontPx: app.readerPxFor(page.feedId),
            serif: app.readerSerif,
            lineHeight: [1.35, 1.55, 1.8][Math.max(0, Math.min(2, app.readerLineSpacing))],
            margin: app.readerMargin,
            justify: app.readerJustify,
            images: app.readerImages
        }
    }

    // Starts loading. Reader mode prefers HTML the Prefetcher downloaded while
    // the article preview was open: no waiting for the page's network load.
    // explicitReader: chosen in the pulley, also for "original" sites
    function start(explicitReader) {
        phase = phaseOriginal
        extractTries = 0
        waitingPrefetch = false
        consentStage = 0
        // Sites set to "always open in the original"
        if (readerMode && !explicitReader && app.isOriginalSite(page.url)) {
            readerMode = false
            hint.show(qsTr("This site always opens in the original"))
        }
        if (readerMode) {
            var cached = Prefetch.html(page.url)
            if (cached !== "") {
                webView.loadHtml(cached, page.url)
                return
            }
            if (Prefetch.isPending(page.url)) {
                waitingPrefetch = true
                prefetchTimeout.restart()
                return
            }
        }
        loadOriginal()
    }

    function loadOriginal() {
        // After loadHtml(html, url) the web view's url already is the
        // article's: setting it again would be ignored, reload() loads the
        // real page. (webView.load() did nothing on the device, 09.10.)
        if (webView.url.toString() === page.url.toString())
            webView.reload()
        else
            webView.url = page.url
    }

    function consentWall() {
        Prefetch.clear()        // other pages of the site were downloaded as the wall, too
        phase = phaseOriginal
        if (consentStage === 0) {
            consentStage = 1
            extractTries = 0
            loadOriginal()
        } else if (consentStage === 1) {
            consentStage = 2
            hint.show(qsTr("Please make your choice on the page, the reader mode follows automatically"))
        }
    }

    // Runs Readability. An early attempt (DOM ready, page still loading) that
    // finds no article is retried once the page has finished loading.
    function extract(isFinal) {
        if (phase !== phaseOriginal || waitingPrefetch)
            return
        phase = phaseExtracting
        extractTries++
        webView.runJavaScript(Html.readerExtractScript(), function(result) {
            if (result && String(result).indexOf('"consentWall":true') >= 0) {
                page.consentWall()
                return
            }
            var html = result ? Html.readerHtml(result, page.readerStyle()) : ""
            if (html !== "") {
                page.phase = page.phaseReader
                webView.loadHtml(html, page.url)
            } else if (!isFinal && webView.loading) {
                page.phase = page.phaseOriginal        // try again after loading
            } else {
                page.fail()
            }
        }, function(error) {
            console.warn("Reader mode failed:", error)
            if (!isFinal && webView.loading)
                page.phase = page.phaseOriginal
            else
                page.fail()
        })
    }

    function fail() {
        phase = phaseFailed
        hint.show(qsTr("Reader mode not available for this page"))
    }

    // Changes the reader text size, applies it live and remembers it.
    function changeFontSize(delta) {
        var px = Math.max(14, Math.min(32, app.readerPxFor(feedId) + delta))
        app.setReaderPx(feedId, px)
        if (phase === phaseShown)
            webView.runJavaScript("document.body.style.fontSize = '" + px + "px'; return true")
        hint.show(app.textSizePerFeed && feedId > 0 && feedTitle !== ""
                  ? qsTr("Text size %1 for %2").arg(px).arg(feedTitle)
                  : qsTr("Text size %1").arg(px))
    }

    function showOriginal() {
        readerMode = false
        phase = phaseOriginal
        waitingPrefetch = false
        consentStage = 0
        loadOriginal()
    }

    function showReader() {
        readerMode = true
        start(true)
    }

    // "3 / 120" for the nav bar
    readonly property string siblingInfo: {
        if (!articleModel || itemId < 0)
            return ""
        var row = articleModel.rowOfId(itemId)
        return row < 0 ? "" : (row + 1) + " / " + articleModel.count
    }

    // Next (1) / previous (-1) article of the list, shown in this page.
    function openSibling(delta) {
        if (!articleModel)
            return
        var row = articleModel.rowOfId(itemId)
        if (row < 0)
            return
        row += delta
        if (row < 0 || row >= articleModel.count) {
            hint.show(delta > 0 ? qsTr("Last article") : qsTr("First article"))
            return
        }
        articleModel.markRead(row)
        var d = articleModel.get(row)
        itemId = d.itemId
        feedId = d.feedId || -1
        feedTitle = d.feedTitle || ""
        starred = d.starred === true
        app.lastOpenedItemId = d.itemId
        page.url = d.url
        start()
    }

    // Download the following article while this one is being read.
    function prefetchNext() {
        if (!articleModel || itemId < 0 || app.openMode !== 0)
            return
        var row = articleModel.rowOfId(itemId) + 1
        if (row > 0 && row < articleModel.count)
            Prefetch.prefetch(articleModel.get(row).url)
    }

    onPhaseChanged: if (phase === phaseShown || phase === phaseFailed) prefetchNext()

    // Start once the page is on screen (the WebView is only active then).
    onStatusChanged: {
        if (status !== PageStatus.Active)
            return
        // Reached from the article preview: going back skips the preview.
        var prev = pageStack.previousPage(page)
        if (prev && prev.hasOwnProperty("skipOnReturn"))
            prev.skipOnReturn = true
        if (!started) {
            started = true
            start()
        }
    }

    WebViewFlickable {
        id: flick
        anchors.fill: parent
        webView.footerMargin: navBar.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally(page.url)
            }
            MenuItem {
                text: qsTr("Copy link")
                onClicked: {
                    Clipboard.text = page.url.toString()
                    hint.show(qsTr("Link copied"))
                }
            }
            MenuItem {
                text: page.readerMode ? qsTr("Show original website") : qsTr("Reader mode")
                onClicked: page.readerMode ? page.showOriginal() : page.showReader()
            }
            MenuItem {
                readonly property bool listed: app.originalSiteList.length >= 0 && app.isOriginalSite(page.url)
                text: listed ? qsTr("Use reader mode for this site") : qsTr("Always open this site in the original")
                onClicked: {
                    // Taken first: "listed" is a binding and changes as soon as
                    // the site list does (it took the wrong branch, so the page
                    // stayed in reader mode – device test 11 Oct)
                    var wasListed = listed
                    app.setOriginalSite(page.url, !wasListed)
                    if (wasListed) {
                        page.showReader()
                    } else {
                        hint.show(qsTr("%1 opens in the original from now on").arg(Html.siteKey(page.url)))
                        if (page.readerMode)
                            page.showOriginal()
                    }
                }
            }
            MenuItem {
                visible: page.readerMode && page.phase === page.phaseShown
                text: page.readerDark ? qsTr("Light colors") : qsTr("Dark colors")
                onClicked: page.toggleReaderColors()
            }
            MenuItem {
                visible: page.readerMode && page.phase === page.phaseShown
                text: qsTr("Smaller text")
                onClicked: page.changeFontSize(-2)
            }
            MenuItem {
                visible: page.readerMode && page.phase === page.phaseShown
                text: qsTr("Larger text")
                onClicked: page.changeFontSize(2)
            }
        }
    }

    property alias webView: flick.webView

    Connections {
        target: flick.webView
        onLoadingChanged: {
            if (flick.webView.loading)
                return
            if (page.readerMode && page.phase === page.phaseOriginal)
                page.extract(true)
            else if (page.phase === page.phaseReader)
                page.phase = page.phaseShown
        }
    }

    Connections {
        target: Prefetch
        onFinished: {
            if (!page.waitingPrefetch || url.toString() !== page.url.toString())
                return
            page.waitingPrefetch = false
            prefetchTimeout.stop()
            if (ok)
                webView.loadHtml(Prefetch.html(page.url), page.url)
            else
                page.loadOriginal()
        }
    }

    // Prefetch still running when the page opened: wait a little, then load normally.
    Timer {
        id: prefetchTimeout
        interval: 2500
        onTriggered: {
            if (page.waitingPrefetch) {
                page.waitingPrefetch = false
                page.loadOriginal()
            }
        }
    }

    // Extract as soon as the DOM is ready instead of waiting for ads and images.
    Timer {
        interval: 400
        repeat: true
        running: page.started && page.readerMode && page.phase === page.phaseOriginal
                 && !page.waitingPrefetch && page.extractTries === 0 && page.consentStage < 2
        onTriggered: {
            webView.runJavaScript("return document.readyState + '|' + location.href", function(state) {
                var s = String(state)
                if ((s.indexOf("interactive|") === 0 || s.indexOf("complete|") === 0)
                        && s !== "complete|about:blank" && s !== "interactive|about:blank")
                    page.extract(false)
            })
        }
    }

    // Pages that never finish loading (endless ad requests): extract anyway.
    Timer {
        interval: 8000
        running: page.started && page.readerMode && page.phase === page.phaseOriginal
                 && !page.waitingPrefetch && page.consentStage < 2
        onTriggered: page.extract(true)
    }

    // Covers the original page until the reader page is ready.
    Rectangle {
        anchors.fill: parent
        visible: page.covered
        color: page.readerBackground

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.horizontalPageMargin
            spacing: Theme.paddingLarge
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Large
                running: page.covered
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                text: qsTr("Preparing reader mode …")
            }
        }
    }

    PageBusyIndicator { running: !page.covered && flick.webView.loading }

    // Next / previous article without leaving the web view.
    ArticleNavBar {
        id: navBar
        visible: page.articleModel !== null && page.itemId >= 0
        info: page.siblingInfo
        showTextSize: page.readerMode && page.phase === page.phaseShown
        showColorToggle: showTextSize
        darkColors: page.readerDark
        onToggleColors: page.toggleReaderColors()
        onPrevious: page.openSibling(-1)
        onNext: page.openSibling(1)
        onSmaller: page.changeFontSize(-2)
        onLarger: page.changeFontSize(2)
        showBookmark: page.itemId >= 0
        bookmarked: page.starred
        onToggleBookmark: {
            page.starred = !page.starred
            page.articleModel.setStarredById(page.itemId, page.starred)
            hint.show(page.starred ? qsTr("Bookmark set") : qsTr("Bookmark removed"))
        }
        showBrowser: true
        onOpenBrowser: Qt.openUrlExternally(page.url)
    }

    Hint { id: hint; anchors.bottomMargin: navBar.height }
}
