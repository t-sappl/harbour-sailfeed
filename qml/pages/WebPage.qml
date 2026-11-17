// SPDX-License-Identifier: GPL-3.0-or-later
// Full article: reader mode (Mozilla Readability, no ads/banners) or the
// original website, inside the app (Sailfish WebView).
import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0
import Sailfish.Share 1.0
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
    property string articleTitle        // shown while the page is prepared
    // Opened from a link in an article (app.openLink): back button instead of
    // previous/next, no article to bookmark
    property bool linkPage: false

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
    // The web view shows the lean copy of the downloaded page (1.4,
    // HtmlUtils::leanPage: no scripts, style sheets, frames, images); if it
    // has no article, the full download is tried next
    property bool leanLoaded: false
    // No extraction before this time (ms): the page before is still on screen
    property double holdUntil: 0
    // Addresses of the previous article's document after next/previous: a
    // plain page ("page" marker) of it must not count as the new article's
    // (second review 16 Nov: the old page could still be read for the new one)
    property var staleHrefs: []
    property int extractTries: 0
    // Slow connection (1.2): the reader mode takes long – the cover offers
    // the feed's own text and the browser instead of only waiting
    property bool slow: false
    // Diagnostics (slow connections): "Sailfeed reader: …" lines with the
    // time since start() in the log
    property double startedAt: 0
    function log(what) {
        console.log("Sailfeed reader:", what, "after", Date.now() - startedAt, "ms",
                    "phase", phase, "loading", webView ? webView.loading : false, page.url)
    }
    // The text from the feed is shown instead of the website's (showFeedText)
    property bool feedText: false
    // The feed's text of the shown article, "" if it has none (no-text
    // feeds, link pages)
    readonly property var feedArticle: {
        if (!articleModel || itemId < 0)
            return null
        var row = articleModel.rowOfId(itemId)
        if (row < 0)
            return null
        var d = articleModel.info(row)      // without the body (1.4), see showFeedText()
        return d.hasContent === false ? null : d
    }

    // Reader colors: like the ambience, light, dark or sepia (setting, app.readerPalette)
    readonly property string readerBackground: app.readerPalette.background

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
        var colors = app.readerPalette
        return {
            background: colors.background,
            text: colors.text,
            secondary: colors.secondary,
            link: colors.link,
            fontPx: app.readerPxFor(page.feedId),
            serif: app.readerSerif,
            lineHeight: app.readerLineHeights[Math.max(0, Math.min(2, app.readerLineSpacing))],
            margin: app.readerMargin,
            justify: app.readerJustify,
            images: app.readerImages
        }
    }

    // Starts loading. Reader mode prefers HTML the Prefetcher downloaded while
    // the article preview was open: no waiting for the page's network load.
    // explicitReader: chosen in the pulley, also for "original" sites
    function start(explicitReader) {
        startedAt = Date.now()
        if (linkPage)
            console.log("Sailfeed link: link page starts", page.url)
        phase = phaseOriginal
        extractTries = 0
        waitingPrefetch = false
        consentStage = 0
        slow = false
        feedText = false
        slowTimer.restart()
        // Sites set to "always open in the original"
        if (readerMode && !explicitReader && app.isOriginalSite(page.url)) {
            readerMode = false
            hint.show(qsTr("This site always opens in the original"))
        }
        leanLoaded = false
        if (readerMode) {
            // Read before in this session (back and forth): no loading, no extraction
            var known = Prefetch.article(page.url)
            var knownHtml = known !== "" ? Html.readerHtml(known, readerStyle()) : ""
            if (knownHtml !== "") {
                log("reader page from memory")
                phase = phaseReader
                loadReader(knownHtml)
                return
            }
            if (loadPrefetched())
                return
            // Nothing downloaded yet (opened without the preview, e.g. "skip
            // the preview" or an article without text): download it now and
            // wait for the lean copy – the real page with all its scripts
            // took 1–4 s to be ready on some sites (device log 11 Nov)
            if (!Prefetch.isPending(page.url))
                Prefetch.prefetch(page.url)
            if (Prefetch.isPending(page.url)) {
                log("waiting for the prefetch")
                waitingPrefetch = true
                prefetchTimeout.restart()
                return
            }
        }
        loadOriginal()
    }

    // Loads a reader page. Its marker gets a new value each time, so
    // confirmReader() can tell it from the reader page of the article before
    // (next/previous from memory, 1.4).
    property string readerToken: ""
    function loadReader(html) {
        readerToken = "r" + Date.now()
        webView.loadHtml(html.replace('<meta name="sailfeed-reader" content="1">',
                                      '<meta name="sailfeed-reader" content="' + readerToken + '">'), page.url)
    }

    property string leanToken: ""
    // Loads the downloaded page, the lean copy first. False if there is none.
    function loadPrefetched() {
        var lean = Prefetch.leanHtml(page.url)
        if (lean !== "") {
            log("prefetched page ready (lean)")
            leanLoaded = true
            // own marker value, as loadReader(): not the lean page of the article before
            leanToken = "l" + Date.now()
            webView.loadHtml(lean.replace('<meta name="sailfeed-lean" content="1">',
                                          '<meta name="sailfeed-lean" content="' + leanToken + '">'), page.url)
            return true
        }
        var full = Prefetch.html(page.url)
        if (full !== "") {
            log("prefetched page ready")
            leanLoaded = false
            webView.loadHtml(full, page.url)
            return true
        }
        return false
    }

    function loadOriginal() {
        leanLoaded = false
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
            holdUntil = Date.now() + 800       // the wall is on screen until the site loads
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
        log(isFinal ? "extracting (page loaded)" : "extracting (DOM ready)")
        var forUrl = String(page.url)
        webView.runJavaScript(Html.readerExtractScript(), function(result) {
            // Another article opened meanwhile (next/previous): drop the
            // result – it showed and cached the old text under the new
            // article (review 16 Nov)
            if (String(page.url) !== forUrl || page.phase !== page.phaseExtracting)
                return
            if (result && String(result).indexOf('"consentWall":true') >= 0) {
                page.consentWall()
                return
            }
            var html = result ? Html.readerHtml(result, page.readerStyle()) : ""
            if (html !== "") {
                Prefetch.setArticle(page.url, String(result))
                page.leanLoaded = false
                page.phase = page.phaseReader
                page.loadReader(html)
            } else if (!isFinal && webView.loading) {
                page.phase = page.phaseOriginal        // try again after loading
            } else {
                page.noArticle()
            }
        }, function(error) {
            if (String(page.url) !== forUrl || page.phase !== page.phaseExtracting)
                return
            console.warn("Reader mode failed:", error)
            if (!isFinal && webView.loading)
                page.phase = page.phaseOriginal
            else
                page.noArticle()
        })
    }

    // No article found. In the lean page (pages that build their text with
    // scripts) the full download comes next, with its scripts.
    function noArticle() {
        if (!leanLoaded) {
            fail()
            return
        }
        log("no article in the lean page, loading the full page")
        leanLoaded = false
        phase = phaseOriginal
        extractTries = 0
        var full = Prefetch.html(page.url)
        if (full !== "")
            webView.loadHtml(full, page.url)
        else
            loadOriginal()
    }

    // Script for the checks below: "<readyState>|<reader|lean value|page>|<address>"
    readonly property string docStateScript: "var l = document.querySelector('meta[name=\"sailfeed-lean\"]');"
        + "return document.readyState + '|' + "
        + "(document.querySelector('meta[name=\"sailfeed-reader\"]') ? 'reader' : "
        + "l ? l.getAttribute('content') : 'page') + '|' + location.href"
    // Whether a docStateScript result is the document being waited for, and
    // not the one before it (previous article's reader page, the lean page
    // before the full one): with the short waits since 1.4 the check
    // could otherwise run on the old document
    function isCurrentDoc(state, needReady) {
        var parts = String(state).split("|")
        if (parts.length < 3 || Date.now() < holdUntil)
            return false
        if (needReady && parts[0] !== "interactive" && parts[0] !== "complete")
            return false
        var href = parts.slice(2).join("|")
        if (href === "about:blank")
            return false
        if (staleHrefs.indexOf(href) >= 0 && href !== String(page.url))
            return false
        return parts[1] === (leanLoaded ? leanToken : "page")
    }
    // The page finished loading: extract, if it is the page waited for
    function extractWhenCurrent() {
        webView.runJavaScript(docStateScript, function(state) {
            if (page.isCurrentDoc(state, false))
                page.extract(true)
        })
    }

    // Shows the article's text from the feed in the reader layout, instead of
    // waiting for the website (slow connection). The website is not loaded
    // any further; "Reader mode" / the mode toggle tries it again.
    function showFeedText() {
        if (!feedArticle)
            return
        var row = articleModel.rowOfId(itemId)
        var d = row >= 0 ? articleModel.get(row) : null      // with the body
        if (!d)
            return
        var html = Html.readerHtml(JSON.stringify({ title: d.title || "", byline: d.author || "",
                                                    siteName: d.feedTitle || "", content: d.body || "" }),
                                   readerStyle())
        if (html === "")
            return
        waitingPrefetch = false
        prefetchTimeout.stop()
        leanLoaded = false
        feedText = true
        slow = false
        phase = phaseReader
        loadReader(html)
        hint.show(qsTr("Text from the feed – the website loads too slowly"))
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
        feedText = false
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
        var d = articleModel.info(row)
        itemId = d.itemId
        feedId = d.feedId || -1
        feedTitle = d.feedTitle || ""
        starred = d.starred === true
        articleTitle = d.title || ""
        app.lastOpenedItemId = d.itemId
        // The cover slides in from the side of the button (previous: from
        // the left), so the change of article can be followed (device test 13 Oct)
        sliding = true
        coverSlide.from = delta * (page.width > 0 ? page.width : Screen.width)
        cover.x = coverSlide.from
        coverSlide.restart()
        staleHrefs = [String(page.url), String(webView.url)]
        page.url = d.url
        start()
    }

    // Download the following and the previous article while this one is
    // being read (previous: 1.4, so going back is as fast). The
    // Prefetcher skips pages it has or is downloading.
    function prefetchNext() {
        if (!articleModel || itemId < 0)
            return
        var row = articleModel.rowOfId(itemId)
        if (row < 0)
            return
        var rows = [row + 1, row - 1]
        for (var i = 0; i < rows.length; i++) {
            if (rows[i] < 0 || rows[i] >= articleModel.count)
                continue
            var d = articleModel.info(rows[i])
            if (app.openModeFor(d.feedId || -1) === 0 && !Prefetch.article(d.url))
                Prefetch.prefetch(d.url)
        }
    }

    onPhaseChanged: {
        log("phase changed")
        if (phase === phaseShown || phase === phaseFailed)
            prefetchNext()
        if (phase === phaseShown && readerMode)
            pulleyTipTimer.restart()
        // CSS pixel size of the web view, for the text size preview in the settings
        if (phase === phaseShown)
            webView.runJavaScript("return window.devicePixelRatio", function(r) { app.noteReaderPixelRatio(Number(r)) })
    }

    // Start once the page is on screen (the WebView is only active then).
    // The web view itself is made when the page starts to come in (1.4): the
    // preview attaches this page at once, and a Gecko view was created for
    // every preview, also when the full article was never opened.
    property bool webWanted: false
    onStatusChanged: {
        if (status === PageStatus.Activating || status === PageStatus.Active)
            webWanted = true
        if (status !== PageStatus.Active)
            return
        // Reached from the article preview: going back skips the preview.
        // (Not for a link opened from the preview: back leads to the preview.)
        var prev = linkPage ? null : pageStack.previousPage(page)
        if (prev && prev.hasOwnProperty("skipOnReturn"))
            prev.skipOnReturn = true
        if (!started) {
            started = true
            start()
        }
    }

    Loader {
        id: flickLoader
        anchors.fill: parent
        active: page.webWanted
        sourceComponent: flickComponent
    }

    Component {
        id: flickComponent
    WebViewFlickable {
        anchors.fill: parent
        webView.footerMargin: navBar.height

        // Things about the page and its link (15 Oct: text size, colors and
        // reader mode ↔ original are in the bottom bar and left the pulley;
        // eight entries were too many). The lowest entry is reached first.
        PullDownMenu {
            // Look for the feeds of the shown website (e.g. a blog linked in an article)
            MenuItem {
                text: qsTr("Subscribe to website")
                onClicked: pageStack.push(Qt.resolvedUrl("SubscribePage.qml"), { initialUrl: page.url.toString() })
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
                text: qsTr("Copy link")
                onClicked: {
                    Clipboard.text = page.url.toString()
                    hint.show(qsTr("Link copied"))
                }
            }
            MenuItem {
                text: qsTr("Share")
                onClicked: shareAction.trigger()
            }
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally(page.url)
            }
        }
    }
    }

    // null until the page comes in (flickLoader)
    readonly property var webView: flickLoader.item ? flickLoader.item.webView : null

    Connections {
        target: page.webView
        onLoadingChanged: {
            page.log(page.webView.loading ? "web view loading" : "web view loaded")
            if (page.webView.loading)
                return
            if (page.readerMode && page.phase === page.phaseOriginal)
                page.extractWhenCurrent()
            else if (page.phase === page.phaseReader)
                page.confirmReader()
        }
        // Link in the reader page tapped or long pressed (script in
        // HtmlUtils::readerHtml): "sailfeed:tap|long:<id>:<url>"
        onTitleChanged: {
            var t = String(page.webView.title)
            if (t.indexOf("sailfeed:") === 0)
                page.handleLinkMessage(t, "title")
        }
    }

    // Link messages of the reader page, from the title or from polling
    // (linkPoll); each id is handled once.
    property var handledLinks: ({})
    function handleLinkMessage(msg, channel) {
        var m = String(msg).match(/^sailfeed:(tap|long|swipe):([a-z0-9]+-[0-9]+):(.+)$/)
        if (!m) {
            console.log("Sailfeed link: unreadable message via", channel, msg)
            return
        }
        if (handledLinks[m[2]])
            return
        handledLinks[m[2]] = true
        if (!readerMode || phase !== phaseShown || status !== PageStatus.Active) {
            console.log("Sailfeed link: ignored (page not ready)", m[1], m[3])
            return
        }
        console.log("Sailfeed link:", m[1], "via", channel, m[3])
        // Swipe to the left in the reader page: next article (1.2)
        if (m[1] === "swipe") {
            app.setHintShown("readerSwipe")     // knows it, no tip needed
            if (articleModel && itemId >= 0 && !linkPage)
                openSibling(1)
            return
        }
        try {
            app.openLink(m[3], m[1] === "long")
        } catch (e) {
            console.log("Sailfeed link: error", e)
        }
    }

    // Second channel: the page also keeps its messages in an attribute of
    // <html>; fetched and cleared here while the reader page is on screen.
    Timer {
        id: linkPoll
        interval: 500       // 1.4: was 250 (a round trip into the web view 4× a second while reading)
        repeat: true
        running: page.readerMode && page.phase === page.phaseShown
                 && page.status === PageStatus.Active && Qt.application.active
        onTriggered: {
            webView.runJavaScript("var e=document.documentElement,q=e?e.getAttribute('data-sf-links'):null;"
                                  + "if(q)e.removeAttribute('data-sf-links');return q||''", function(result) {
                var lines = String(result || "").split("\n")
                for (var i = 0; i < lines.length; i++) {
                    if (lines[i] !== "")
                        page.handleLinkMessage(lines[i], "poll")
                }
            })
        }
    }

    // The reader page finished loading: uncover it only once the web view
    // really shows it (the marker in its head), and a moment later, when it is
    // painted. Before, the end of the interrupted original page's load already
    // uncovered it and the original flashed up (device test 13 Oct).
    function confirmReader() {
        webView.runJavaScript("var m = document.querySelector('meta[name=\"sailfeed-reader\"]');"
                              + "return m ? m.getAttribute('content') : ''", function(result) {
            if (page.phase === page.phaseReader && String(result) === page.readerToken && !revealTimer.running)
                revealTimer.restart()
        })
    }
    Timer {
        id: revealTimer
        interval: 60        // 1.4: was 200
        onTriggered: if (page.phase === page.phaseReader) page.phase = page.phaseShown
    }
    // The reader page may finish loading late (its images on a slow
    // connection) or the check above may not answer: ask again and again
    // whether the reader page is there, and uncover only then. Until 1.1 the
    // cover went away after 4 s regardless – on a slow connection a blank
    // page with busy indicators instead of the title and "Preparing reader
    // mode" (device test 17 Oct). Last resort after 30 s: uncover anyway.
    Timer {
        interval: 100       // 1.4: was 600; the reader page has no waiting images any more (loading="lazy")
        repeat: true
        running: page.phase === page.phaseReader
        onTriggered: page.confirmReader()
    }
    Timer {
        interval: 30000
        running: page.phase === page.phaseReader
        onTriggered: if (page.phase === page.phaseReader) page.phase = page.phaseShown
    }

    Connections {
        target: Prefetch
        onFinished: {
            if (!page.waitingPrefetch || url.toString() !== page.url.toString())
                return
            page.waitingPrefetch = false
            prefetchTimeout.stop()
            if (!ok || !page.loadPrefetched())
                page.loadOriginal()
        }
    }

    // Prefetch still running when the page opened: wait while data still
    // comes in (slow connection: starting over in the web view would throw
    // away what has arrived – until 1.1 it gave up after 2.5 s); load
    // normally when nothing came for a while.
    Timer {
        id: prefetchTimeout
        interval: 1000
        repeat: true
        onTriggered: {
            if (!page.waitingPrefetch) {
                stop()
                return
            }
            var idle = Prefetch.idleMs(page.url)
            if (idle < 0 || idle > 6000) {
                page.log("prefetch gave up, idle " + idle)
                stop()
                page.waitingPrefetch = false
                if (!page.loadPrefetched())
                    page.loadOriginal()
            }
        }
    }

    // Reader mode not ready after 10 s: say so and offer alternatives
    Timer {
        id: slowTimer
        interval: 10000
        onTriggered: {
            if (page.covered && page.readerMode) {
                page.slow = true
                page.log("slow")
            }
        }
    }

    // Extract as soon as the DOM is ready instead of waiting for ads and images.
    // 1.4: every 150 ms (was 400); isCurrentDoc() makes sure it is the new page.
    Timer {
        interval: 150
        repeat: true
        running: page.started && page.readerMode && page.phase === page.phaseOriginal
                 && !page.waitingPrefetch && page.extractTries === 0 && page.consentStage < 2
        onTriggered: {
            webView.runJavaScript(page.docStateScript, function(state) {
                if (page.isCurrentDoc(state, true))
                    page.extract(false)
            })
        }
    }

    // Pages that never finish loading (endless ad requests): extract anyway.
    // Counted from start() of this article (a plain 8 s timer kept running
    // across next/previous and fired at once on the new article), and only
    // on this article's document (review 16 Nov)
    Timer {
        interval: 1000
        repeat: true
        running: page.started && page.readerMode && page.phase === page.phaseOriginal
                 && !page.waitingPrefetch && page.consentStage < 2
        onTriggered: if (Date.now() - page.startedAt >= 8000) page.extractWhenCurrent()
    }

    // Covers the original page until the reader page is ready, and slides in
    // on next/previous. Fades out, so the reader page appears calmly.
    property bool sliding: false
    readonly property bool coverShown: covered || sliding
    onCoverShownChanged: {
        if (coverShown) {
            coverFade.stop()
            cover.opacity = 1
        } else {
            coverFade.start()
        }
    }

    Rectangle {
        id: cover
        width: parent.width
        height: parent.height
        opacity: page.coverShown ? 1 : 0
        visible: opacity > 0
        color: page.readerBackground

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.horizontalPageMargin
            spacing: Theme.paddingLarge
            Label {
                width: parent.width
                visible: text !== ""
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 4
                truncationMode: TruncationMode.Elide
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                text: page.articleTitle
            }
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Large
                running: cover.visible
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                text: page.readerMode ? qsTr("Preparing reader mode …") : qsTr("Loading website …")
            }
            // Slow connection: alternatives to waiting
            Label {
                width: parent.width
                visible: page.slow
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("This takes long – the connection or the website is slow.")
            }
            ActionPill {
                kind: "primary"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.slow && page.feedArticle !== null
                text: qsTr("Read the text from the feed")
                onClicked: page.showFeedText()
            }
            ActionPill {
                kind: "secondary"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.slow
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally(page.url)
            }
        }
    }

    NumberAnimation {
        id: coverFade
        target: cover
        property: "opacity"
        to: 0
        duration: 120       // 1.4: was 200
    }
    NumberAnimation {
        id: coverSlide
        target: cover
        property: "x"
        to: 0
        duration: 250
        easing.type: Easing.OutCubic
        // The original website shows while loading: keep the cover a moment
        onStopped: {
            if (page.readerMode)
                page.sliding = false
            else
                originalHold.restart()
        }
    }
    Timer {
        id: originalHold
        interval: 600
        onTriggered: page.sliding = false
    }

    // Only for the original website: in reader mode the cover shows the
    // waiting, and the shown reader page may still load its images
    PageBusyIndicator { running: !page.readerMode && !page.coverShown && page.webView !== null && page.webView.loading }

    // Next / previous article without leaving the web view.
    ShareAction {
        id: shareAction
        mimeType: "text/x-url"
        resources: [{ "type": "text/x-url",
                      "linkTitle": page.articleTitle !== "" ? page.articleTitle : page.url.toString(),
                      "status": page.url.toString() }]
    }

    ArticleNavBar {
        id: navBar
        visible: (page.articleModel !== null && page.itemId >= 0) || page.linkPage
        info: page.linkPage ? Html.siteKey(page.url) : page.siblingInfo
        showBack: page.linkPage
        onBack: pageStack.navigateBack()
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
            // No hint: the filled star in the bar shows it (device test 15 Oct)
        }
        // Reader mode ↔ original (15 Oct: replaces "open in browser", now in
        // the pulley). Not offered when the reader mode found no article.
        showModeToggle: !(page.readerMode && page.phase === page.phaseFailed)
        readerShown: page.readerMode
        onToggleMode: page.readerMode ? page.showOriginal() : page.showReader()
    }

    Hint { id: hint; anchors.bottomMargin: navBar.height }

    // First-run tip: the pulley holds "Subscribe to website", browser, colors …
    FirstRunHint {
        id: pulleyTip
        key: "webPulley"
        direction: TouchInteraction.Down
        interactionMode: TouchInteraction.Pull
        text: qsTr("Pull down for more: subscribe to this website, share it, open it in the browser")
    }
    // Next tip (1.2), not together with the pulley's: swipe left for the
    // next article. Only with a next article; not for someone who swiped
    // already (handleLinkMessage marks it).
    FirstRunHint {
        id: swipeTip
        key: "readerSwipe"
        direction: TouchInteraction.Left
        interactionMode: TouchInteraction.Swipe
        labelBottomMargin: navBar.height
        text: qsTr("Swipe left for the next article")
    }
    readonly property bool hasNext: {
        if (!articleModel || itemId < 0 || linkPage)
            return false
        var row = articleModel.rowOfId(itemId)
        return row >= 0 && row + 1 < articleModel.count
    }
    Timer {
        id: pulleyTipTimer
        interval: 1500
        onTriggered: {
            if (page.status !== PageStatus.Active || page.phase !== page.phaseShown || !page.readerMode)
                return
            if (!app.hintShown("webPulley"))
                pulleyTip.show()
            else if (page.hasNext)
                swipeTip.show()
        }
    }
}
