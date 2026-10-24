// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.Configuration 1.0
import Nemo.DBus 2.0
import Sailfish.Share 1.0
import harbour.sailfeed 1.0
import "pages"
import "components"

ApplicationWindow {
    id: app

    // Settings
    property alias autoReadDelay: settings.autoReadDelay
    // 0: mark visible articles as soon as the delay is over,
    // 1: remember them after the delay, grey them out when scrolling on
    property alias autoReadMode: settings.autoReadMode
    property alias showMarkAllButton: settings.showMarkAllButton
    property alias feedSortMode: settings.feedSortMode
    // How "Open website" opens the article: 0 reader mode, 1 original website
    // (both inside the app), 2 external browser
    property alias openMode: settings.openMode
    // Tap in the article list opens the full article (reader mode, original
    // or external browser) without the preview
    property alias skipPreview: settings.skipPreview
    // Article list view: 0 detailed (title, source, excerpt, thumbnail), 1 compact
    property alias listView: settings.listView
    // Links in an article (preview and reader mode): true opens them in the
    // app (reader mode), false in the external browser; a long press on a
    // link does the other.
    property alias linksInApp: settings.linksInApp

    // Opens a link from an article's text. invert: long press, the other way
    // than the setting. Anything but http(s) (mail, phone …) goes to the system.
    function openLink(url, invert) {
        var s = String(url)
        if (!/^https?:/i.test(s) || settings.linksInApp === !!invert) {
            console.log("Sailfeed link: opened in the browser", s)
            Qt.openUrlExternally(s)
            return
        }
        // Pushed from a timer, not here: the call can come from a web view
        // callback (runJavaScript), where errors are swallowed silently – the
        // link page never appeared (device log 13 Oct evening)
        linkTimer.url = s
        linkTimer.tries = 0
        linkTimer.restart()
    }
    Timer {
        id: linkTimer
        property string url
        property int tries: 0
        interval: 50
        onTriggered: {
            if (pageStack.busy && ++tries < 40) {       // page animation still running
                restart()
                return
            }
            try {
                var p = pageStack.push(Qt.resolvedUrl("pages/WebPage.qml"),
                                       { url: url, readerMode: true, linkPage: true })
                console.log("Sailfeed link: opened in the app", url, p ? "" : "(no page)")
            } catch (e) {
                console.log("Sailfeed link: could not open", url, e)
                app.showMessage(qsTr("Could not open the link"), true)
            }
        }
    }
    property alias unreadOnly: settings.unreadOnly
    property alias oldestFirst: settings.oldestFirst
    property alias readerFontPx: settings.readerFontPx
    // Article preview text size: steps of 12 % around the system's small font
    property alias previewFontStep: settings.previewFontStep
    // Text sizes changed by hand in the preview / reader mode are kept per
    // feed (setting): a feed whose size did not fit once will not fit later.
    property alias textSizePerFeed: settings.textSizePerFeed
    readonly property bool hasFeedTextSizes: settings.previewFontPerFeed !== "{}" || settings.readerFontPerFeed !== "{}"

    function _sizeMap(json) {
        try { return JSON.parse(json) } catch (e) { return {} }
    }
    function previewStepFor(feedId) {
        if (textSizePerFeed && feedId > 0) {
            var m = _sizeMap(settings.previewFontPerFeed)
            if (m[feedId] !== undefined)
                return m[feedId]
        }
        return settings.previewFontStep
    }
    function setPreviewStep(feedId, step) {
        if (textSizePerFeed && feedId > 0) {
            var m = _sizeMap(settings.previewFontPerFeed)
            m[feedId] = step
            settings.previewFontPerFeed = JSON.stringify(m)
        } else {
            settings.previewFontStep = step
        }
    }
    function readerPxFor(feedId) {
        if (textSizePerFeed && feedId > 0) {
            var m = _sizeMap(settings.readerFontPerFeed)
            if (m[feedId] !== undefined)
                return m[feedId]
        }
        return settings.readerFontPx
    }
    function setReaderPx(feedId, px) {
        if (textSizePerFeed && feedId > 0) {
            var m = _sizeMap(settings.readerFontPerFeed)
            m[feedId] = px
            settings.readerFontPerFeed = JSON.stringify(m)
        } else {
            settings.readerFontPx = px
        }
    }
    function resetFeedTextSizes() {
        settings.previewFontPerFeed = "{}"
        settings.readerFontPerFeed = "{}"
    }
    function feedHasTextSize(feedId) {
        return _sizeMap(settings.previewFontPerFeed)[feedId] !== undefined
            || _sizeMap(settings.readerFontPerFeed)[feedId] !== undefined
    }
    function resetFeedTextSize(feedId) {
        var p = _sizeMap(settings.previewFontPerFeed)
        var r = _sizeMap(settings.readerFontPerFeed)
        delete p[feedId]
        delete r[feedId]
        settings.previewFontPerFeed = JSON.stringify(p)
        settings.readerFontPerFeed = JSON.stringify(r)
    }

    // --- Feed settings (1.2) -------------------------------------------------
    // Exceptions per feed ID (JSON maps in dconf, in the settings backup);
    // -1 = "like the general setting". Read through these functions, so
    // bindings follow changes (the map property is read inside).
    readonly property var _perFeedKeys: ["previewFontPerFeed", "readerFontPerFeed",
                                         "feedListView", "feedSkipPreview", "feedOpenMode"]
    function feedSetting(name, feedId) {
        if (!(feedId > 0))
            return -1
        var v = _sizeMap(settings[name])[feedId]
        return v === undefined ? -1 : v
    }
    function setFeedSetting(name, feedId, value) {
        if (!(feedId > 0))
            return
        var m = _sizeMap(settings[name])
        if (value < 0)
            delete m[feedId]
        else
            m[feedId] = value
        settings[name] = JSON.stringify(m)
    }
    function listViewFor(feedId) {
        var v = feedSetting("feedListView", feedId)
        return v >= 0 ? v : settings.listView
    }
    function skipPreviewFor(feedId) {
        var v = feedSetting("feedSkipPreview", feedId)
        return v >= 0 ? v === 1 : settings.skipPreview
    }
    function openModeFor(feedId) {
        var v = feedSetting("feedOpenMode", feedId)
        return v >= 0 ? v : settings.openMode
    }
    readonly property int previewFontPx: Math.round(Theme.fontSizeSmall * (1 + 0.12 * previewFontStep))
    property alias readerSerif: settings.readerSerif
    property alias readerLineSpacing: settings.readerLineSpacing   // 0 compact, 1 normal, 2 wide
    property alias readerMargin: settings.readerMargin             // 0 narrow, 1 normal, 2 wide
    property alias readerColors: settings.readerColors             // 0 ambience, 1 light, 2 dark, 3 sepia
    property alias readerJustify: settings.readerJustify

    // Reader colors (light, dark, sepia or like the ambience): used by the
    // reader page (WebPage) and the preview in the settings
    readonly property var readerPalette: {
        function css(c, alpha) {
            var a = alpha !== undefined ? alpha : c.a
            return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + ","
                    + Math.round(c.b * 255) + "," + a.toFixed(2) + ")"
        }
        var dark = Theme.colorScheme === Theme.LightOnDark
        switch (readerColors) {
        case 1: return { background: "#fafafa", text: "#1d1d1f", secondary: "rgba(29,29,31,0.62)", link: "#0a62b0" }
        case 2: return { background: "#111418", text: "#e6e6e6", secondary: "rgba(230,230,230,0.62)", link: "#5ec8ff" }
        case 3: return { background: "#f4ecd8", text: "#4a3a2a", secondary: "rgba(74,58,42,0.65)", link: "#9a4a12" }
        default: return { background: dark ? "#111418" : "#fafafa", text: css(Theme.primaryColor, 1),
                          secondary: css(Theme.secondaryColor), link: css(Theme.highlightColor, 1) }
        }
    }
    readonly property var readerLineHeights: [1.35, 1.55, 1.8]

    // Screen pixels per CSS pixel of the reader page (window.devicePixelRatio
    // of the web view), measured by WebPage the first time a reader page is
    // shown; until then an estimate. For the text size preview in the settings.
    readonly property real readerPixelRatio: settings.readerPixelRatio > 0 ? settings.readerPixelRatio
                                                                           : Theme.pixelRatio * 1.5
    // First-run tips (FirstRunHint): each one once
    function hintShown(key) {
        return ("," + settings.hintsShown + ",").indexOf("," + key + ",") >= 0
    }
    function setHintShown(key) {
        if (!hintShown(key))
            settings.hintsShown = settings.hintsShown === "" ? key : settings.hintsShown + "," + key
    }
    function resetHints() {
        settings.hintsShown = ""
        settings.previewsOpened = 0
        lastTipShown = 0
    }
    // Time of the last first-run tip (FirstRunHint: at most one every 45 s)
    property real lastTipShown: 0
    // No feeds yet (after signing in, before the first subscription): the
    // article and feed lists explain how to subscribe
    readonly property bool noFeeds: config.isAccountValid && feedModel.loaded && feedModel.count === 0
                                    && !synchronizer.inOperation

    // After the 10th preview, once: a hint that the preview can be skipped
    // (advice, not a gesture: a banner instead of a hand; 15 Oct)
    function notePreviewOpened() {
        if (skipPreview || hintShown("skipPreviewAdvice"))
            return
        settings.previewsOpened = settings.previewsOpened + 1
        if (settings.previewsOpened < 10)
            return
        setHintShown("skipPreviewAdvice")
        showMessage(qsTr("Rather the full article right away? Settings → Article preview → Skip the preview."),
                    false, qsTr("Settings"),
                    function() { pageStack.push(Qt.resolvedUrl("pages/SettingsPreviewPage.qml")) }, 10000)
    }

    function noteReaderPixelRatio(ratio) {
        if (ratio > 0.5 && ratio < 8 && Math.abs(ratio - settings.readerPixelRatio) > 0.01)
            settings.readerPixelRatio = ratio
    }
    property alias readerImages: settings.readerImages
    // Sites that always open in the original (site keys, see Html.siteKey)
    readonly property var originalSiteList: settings.originalSites === "" ? [] : settings.originalSites.split("\n")

    function isOriginalSite(url) {
        return Html.isListedSite(url, originalSiteList)
    }
    function setOriginalSite(url, original) {
        var key = Html.siteKey(url)
        if (key === "")
            return
        var list = originalSiteList.filter(function(s) { return s !== key })
        if (original)
            list.push(key)
        list.sort()
        settings.originalSites = list.join("\n")
    }

    // --- Settings backup (package 5) ----------------------------------------
    // The values a backup covers, as SettingsBackup expects them: the keys of
    // SettingsBackup.generalKeys/textSizeKeys, favoriteFeeds, originalSites.
    // Most live in dconf (settings below), the privacy switches (Prefetch)
    // and the cleanup (Cleanup) in the settings file, the favorites in FeedModel.
    function settingsState() {
        var state = {}
        // (plain loops: in Qt 5.6 a QStringList is only array-like in JS)
        var keys = []
        var i
        for (i = 0; i < SettingsBackup.generalKeys.length; i++)
            keys.push(SettingsBackup.generalKeys[i])
        for (i = 0; i < SettingsBackup.textSizeKeys.length; i++)
            keys.push(SettingsBackup.textSizeKeys[i])
        for (i = 0; i < keys.length; i++) {
            var key = keys[i]
            if (key === "shareBrowserCookies")
                state[key] = Prefetch.shareBrowserCookies
            else if (key === "clearCookiesAtStart")
                state[key] = Prefetch.clearAtStart
            else if (key === "deleteReadAfter")
                state[key] = Cleanup.deleteAfterMonths
            else if (key === "imageCacheMB")
                state[key] = Cleanup.imageCacheMB
            else if (_perFeedKeys.indexOf(key) >= 0)
                state[key] = _sizeMap(settings[key])
            else
                state[key] = settings[key]
        }
        state.favoriteFeeds = feedModel.favoriteFeeds()
        state.originalSites = originalSiteList
        return state
    }

    property alias backupTextSizes: settings.backupTextSizes

    // Backs the settings up to the Nextcloud now (result: cloudBackup.backupFinished)
    function backupSettingsNow() {
        return cloudBackup.backup(SettingsBackup.serialize(settingsState(), settings.backupTextSizes),
                                  settingsFingerprint)
    }

    // Automatic backup (package 5d): the fingerprint of the backed-up
    // settings is a binding over all of them (dconf values, privacy
    // switches, sites, favorites via _favoritesTick); CloudBackup backs up a
    // few seconds after it changes, or when the app goes to the background.
    property alias autoBackup: settings.autoBackup
    property int _favoritesTick: 0
    readonly property string settingsFingerprint: {
        _favoritesTick              // dependency: favorites are read through a function
        return SettingsBackup.fingerprint(settingsState(), settings.backupTextSizes)
    }
    onSettingsFingerprintChanged: cloudBackup.noteSettings(settingsFingerprint)
    Binding { target: cloudBackup; property: "autoEnabled"; value: settings.autoBackup }
    Connections {
        target: feedModel
        onFavoritesChanged: app._favoritesTick++
    }
    Connections {
        target: Qt.application
        onActiveChanged: if (!Qt.application.active) cloudBackup.flush()
    }

    // Restore offer (package 5e): after signing in on a new device the
    // server's backup is offered before anything is backed up automatically.
    property bool _offerPending: false
    Connections {
        target: cloudBackup
        onAutoBackupRequested: cloudBackup.backup(SettingsBackup.serialize(app.settingsState(), settings.backupTextSizes),
                                                  app.settingsFingerprint)
        onRestoreOffer: {
            app._offerPending = true
            cloudBackup.fetch(SettingsBackup.currentFileName)
        }
        onFetched: {
            if (!app._offerPending || name !== SettingsBackup.currentFileName)
                return
            app._offerPending = false
            var parsed = ok ? SettingsBackup.parse(content) : null
            if (!parsed || !parsed.ok) {
                cloudBackup.setLinked()     // nothing usable to offer
                return
            }
            var plan = SettingsBackup.plan(app.settingsState(), parsed.state, SettingsBackup.Replace,
                                           parsed.hasTextSizes && settings.backupTextSizes)
            if (!plan.hasChanges) {
                cloudBackup.setLinked()     // same settings already
                return
            }
            offerTimer.parsed = parsed
            offerTimer.tries = 0
            offerTimer.start()
        }
    }
    // Show the offer once the page stack is idle (e.g. after the sign-in page closed)
    Timer {
        id: offerTimer
        property var parsed
        property int tries: 0
        interval: 600
        repeat: true
        onTriggered: {
            if (pageStack.busy && ++tries < 20)
                return
            stop()
            pageStack.push(Qt.resolvedUrl("pages/RestoreSettingsDialog.qml"),
                           { parsed: parsed, sourceTitle: qsTr("Latest backup"), offer: true })
        }
    }

    // Restores a parsed backup (SettingsBackup.parse().state). The current
    // settings are kept on the device first; the banner offers "Undo".
    function restoreSettings(plan) {
        cloudBackup.saveLocalCopy(SettingsBackup.serialize(settingsState(), true))
        applySettingsPlan(plan)
        cloudBackup.setLinked()
        showMessage(qsTr("Settings restored"), false, qsTr("Undo"), function() { undoRestore() }, 8000)
    }

    function undoRestore() {
        var parsed = SettingsBackup.parse(cloudBackup.localCopy())
        if (!parsed.ok)
            return
        applySettingsPlan(SettingsBackup.plan(settingsState(), parsed.state, SettingsBackup.Replace, true))
        showMessage(qsTr("Settings as before the restore"))
    }

    // Applies SettingsBackup.plan()'s result.
    function applySettingsPlan(plan) {
        var changes = plan.changes
        for (var key in changes) {
            var value = changes[key]
            if (key === "shareBrowserCookies")
                Prefetch.shareBrowserCookies = value
            else if (key === "clearCookiesAtStart")
                Prefetch.clearAtStart = value
            else if (key === "deleteReadAfter")
                Cleanup.deleteAfterMonths = value
            else if (key === "imageCacheMB")
                Cleanup.imageCacheMB = value
            else if (_perFeedKeys.indexOf(key) >= 0)
                settings[key] = JSON.stringify(value)
            else
                settings[key] = value
        }
        if (plan.favoriteFeeds !== undefined)
            feedModel.setFavoriteIds(plan.favoriteFeeds)
        if (plan.originalSites !== undefined) {
            var sites = []
            for (var j = 0; j < plan.originalSites.length; j++)
                sites.push(plan.originalSites[j])
            sites.sort()
            settings.originalSites = sites.join("\n")
        }
    }

    // Current selection: feed > folder > all articles (-1 = not selected)
    property int currentFolderId: -1
    property int currentFeedId: -1
    property bool currentStarred: false
    // Set when a feed/folder is selected: the article list may load older articles once.
    property bool autoOlderPending: false
    property string currentSelectionName: qsTr("All articles")
    // Last article opened (preview or web view): the list scrolls to it on return.
    property int lastOpenedItemId: -1

    // Shared models: ArticleListPage shows the articles, FeedsPage (the page
    // behind it, swipe right to reveal) shows the feeds and changes the selection.
    property alias articles: articleModel
    property alias feeds: feedModel
    property alias feedList: feedListModel

    function selectAll() {
        currentStarred = false
        currentFolderId = -1
        currentFeedId = -1
        currentSelectionName = qsTr("All articles")
        articleModel.setSelection(-1, -1)
    }
    function selectFolder(folderId, name) {
        currentStarred = false
        currentFolderId = folderId
        currentFeedId = -1
        currentSelectionName = name
        autoOlderPending = true
        articleModel.setSelection(folderId, -1)
    }
    function selectFeed(feedId, name) {
        currentStarred = false
        currentFolderId = -1
        currentFeedId = feedId
        currentSelectionName = name
        autoOlderPending = true
        articleModel.setSelection(-1, feedId)
    }

    function selectStarred() {
        currentStarred = true
        currentFolderId = -1
        currentFeedId = -1
        currentSelectionName = qsTr("Bookmarks")
        articleModel.selectStarred()
    }

    ConfigurationGroup {
        id: settings
        path: "/apps/harbour-sailfeed"
        property int autoReadDelay: 4          // seconds, 0 = off
        property int autoReadMode: 1           // 0 at once, 1 when scrolling on
        property bool showMarkAllButton: true  // slide-up button on the article list
        property int feedSortMode: 0           // FeedModel.SortMode
        property int settingsVersion: 0
        property int openMode: 0               // 0 reader, 1 original, 2 external
        property bool skipPreview: false       // tap in the list opens the full article
        property int listView: 0               // 0 detailed, 1 compact (title and source only)
        property string feedListView: "{}"     // JSON { feedId: 0|1 }  feed settings: list view
        property string feedSkipPreview: "{}"  // JSON { feedId: 0|1 }  … skip the preview
        property string feedOpenMode: "{}"     // JSON { feedId: 0..2 } … full article
        property bool linksInApp: true         // links in articles: app (reader mode) or browser
        property bool unreadOnly: false
        property bool oldestFirst: false       // article list: oldest first (1.2)
        property int readerFontPx: 20          // reader mode text size (CSS px)
        property int previewFontStep: 0        // -2 … 6
        property bool textSizePerFeed: true
        property string previewFontPerFeed: "{}"   // JSON { feedId: step }
        property string readerFontPerFeed: "{}"    // JSON { feedId: px }
        property bool readerSerif: false
        property int readerLineSpacing: 1
        property int readerMargin: 1
        property int readerColors: 0
        property bool readerJustify: false
        property int previewsOpened: 0          // counted until the "skip the preview" hint (not backed up)
        property string hintsShown: ""          // keys of the first-run tips shown (FirstRunHint), comma separated
        property real readerPixelRatio: 0       // measured in the web view, see app.readerPixelRatio (not backed up: per device)
        property bool readerImages: true
        property string originalSites: ""      // newline separated site keys
        property bool backupTextSizes: true    // settings backup: include the text sizes
        property bool autoBackup: true         // settings backup: automatically after changes
    }

    FeedModel {
        id: feedModel
        storage: localstorage
        sortMode: app.feedSortMode
    }

    // Rows of the feed list: folders (also empty ones) and their feeds
    FeedRowsModel {
        id: feedListModel
        feeds: feedModel
    }

    ArticleModel {
        id: articleModel
        storage: localstorage
        unreadOnly: app.unreadOnly
        oldestFirst: app.oldestFirst
    }

    // Load articles as soon as the local database is open.
    Connections {
        target: localstorage
        onReadyChanged: if (localstorage.ready) app.loadModels()
    }
    Component.onCompleted: {
        // v1: default delay 5 s -> 4 s (only if it was never changed)
        if (settings.settingsVersion < 1) {
            if (settings.autoReadDelay === 5)
                settings.autoReadDelay = 4
            settings.settingsVersion = 1
        }
        if (localstorage.ready)
            app.loadModels()
        // Update to the version with the cleanup: say once that it exists
        if (Cleanup.hintPending)
            cleanupHintTimer.start()
        // Settings backup: current state, and whether this device is linked yet
        cloudBackup.noteSettings(settingsFingerprint)
        if (config.isAccountValid)
            cloudBackup.checkLink()
    }

    // Deleting old read articles is off after an update ("Never"): one hint
    // after the start, with the way to the setting (author, 15 Oct)
    Timer {
        id: cleanupHintTimer
        interval: 3000
        onTriggered: {
            // A sync error has the banner: try again later
            if (messageBanner.visible) {
                restart()
                return
            }
            Cleanup.dismissHint()
            app.showMessage(qsTr("New: Sailfeed can delete old read articles to save space."), false,
                            qsTr("Settings"), function() {
                                pageStack.push(Qt.resolvedUrl("pages/SettingsListPage.qml"))
                            }, 10000)
        }
    }

    // --- Share target (1.2.1, as Sailcast) ------------------------------
    // "Subscribe to website" in the sharing menu (.desktop share method
    // "subscribe"). Own DBusAdaptor instead of ShareProvider: the browser
    // shares a link as {type, linkTitle, status}, which ShareProvider
    // rejects. The D-Bus name is registered in main() after this QML loaded.
    DBusAdaptor {
        service: "org.sappl.harbour-sailfeed"
        path: "/share/subscribe"
        iface: "org.sailfishos.share"
        function share(shareConfiguration) { app.receiveShare(shareConfiguration) }
    }
    ShareAction { id: shareReader }      // only to read a share configuration
    property string _sharedUrl: ""

    // The first web address of the first resource: a link ({status}), raw
    // data ({data}) or text; a bare domain ("cre.fm") gets https://
    function sharedUrlOf(configuration) {
        var resources = []
        try {
            shareReader.loadConfiguration(configuration)
            resources = shareReader.resources
        } catch (e) {
            console.warn("Share: " + e)
        }
        if ((!resources || resources.length === 0) && configuration && configuration.resources)
            resources = configuration.resources
        if (!resources || resources.length === 0)
            return ""
        var r = resources[0]
        var text = ("" + (typeof r === "string" ? r : (r.status || r.data || r.linkTitle || ""))).trim()
        var url = text.match(/(https?|feed):\/\/[^\s"'<>]+/i)
        if (url)
            return url[0].replace(/[.,;:!?)\]]+$/, "").replace(/^feed:\/\/(https?:\/\/)?/i, function(all, inner) {
                return inner ? inner : "https://"
            })
        if (/^[a-z0-9.-]+\.[a-z]{2,}(\/\S*)?$/i.test(text))
            return "https://" + text
        return ""
    }
    function receiveShare(configuration) {
        var url = sharedUrlOf(configuration)
        // Without this the sharing menu keeps waiting and the app stays behind
        app.activate()
        if (url === "") {
            showMessage(qsTr("No web address in the shared content"))
            return
        }
        if (!config.isAccountValid) {
            showMessage(qsTr("Sign in to your Nextcloud first, then share the link again"))
            return
        }
        _sharedUrl = url
        shareTimer.restart()
    }
    Timer {
        id: shareTimer
        interval: 600       // the page stack may still be starting (app started by the share)
        onTriggered: {
            if (pageStack.depth === 0 || pageStack.busy) {
                restart()
                return
            }
            var url = app._sharedUrl
            app._sharedUrl = ""
            pageStack.push(Qt.resolvedUrl("pages/SubscribePage.qml"), { initialUrl: url })
        }
    }

    function loadModels() {
        if (!articleModel.loaded) articleModel.load()
        if (!feedModel.loaded) feedModel.load()
    }

    // A new sign-in may have cleared the database (other account/server).
    Connections {
        target: account
        onLoginSucceeded: { articleModel.reload(); feedModel.reload(); cloudBackup.checkLink() }
    }

    // Keep "now / 5 min / 2 h" up to date while the app is in the foreground.
    Timer {
        interval: 60 * 1000
        repeat: true
        running: Qt.application.active
        onTriggered: articleModel.refreshAges()
    }

    // Message at the bottom of the screen, above every page (MessageBanner.qml)
    function showMessage(text, isError, actionText, action, duration) {
        messageBanner.show(text, isError, actionText, action, duration)
    }

    // A failed sync is shown on every page, with the reason, a solution and
    // "Retry" (the list header alone was easy to miss).
    Connections {
        target: synchronizer
        onErrorChanged: {
            if (synchronizer.error && config.isAccountValid)
                app.showMessage(account.describeError(synchronizer.error), true, qsTr("Retry"), function() {
                    if (!synchronizer.inOperation)
                        synchronizer.sync()
                })
        }
        onSucceeded: if (messageBanner.error) messageBanner.hide()
    }

    // Items of the window itself do not turn with the pages: this layer
    // copies the current page's size and rotation (Silica turns each page
    // itself), so the banner sits at the bottom of the screen in every
    // orientation. (Computing it from app.orientation turned the banner by
    // 180° but not into landscape – device test 11 Oct.)
    Item {
        id: overlay
        readonly property Item page: pageStack.currentPage
        anchors.centerIn: parent
        width: page ? page.width : parent.width
        height: page ? page.height : parent.height
        rotation: page ? page.rotation : 0
        z: 101

        MessageBanner {
            id: messageBanner
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.itemSizeLarge
        }
    }

    // FeedsPage is the first page; it attaches ArticleListPage and shows it at
    // once, so the feed list is "behind" the article list.
    initialPage: Component { FeedsPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    // Portrait and landscape on phones and tablets; the pages keep readable
    // widths in landscape (DESIGN.md). defaultAllowedOrientations is portrait
    // only on phones, so landscape was never offered (device test 11 Oct).
    allowedOrientations: Orientation.All
    _defaultPageOrientations: Orientation.All
}
