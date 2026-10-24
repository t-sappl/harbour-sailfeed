// SPDX-License-Identifier: GPL-3.0-or-later
// "Subscribe to website" (release 1.1, replaces the "Add feed" dialog): the
// address of a website, an article or a feed (from the clipboard or typed);
// FeedFinder looks for the site's feeds and checks them, every feed found is
// shown with its newest articles and can be subscribed on its own, into the
// chosen folder. Podcasts are listed last, with a note.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    // Folder preselected in the combo box (0 = no folder)
    property int preselectedFolderId: 0
    // Address to look at right away (web view pulley); otherwise the clipboard's
    property string initialUrl: ""
    // Where the address came from, to say so below the field (15 Oct):
    // 0 typed, 1 clipboard, 2 the page shown in the web view
    property int filledFrom: 0
    property string filledText: ""

    readonly property var folders: FeedManager.folders()
    readonly property int folderId: folderBox.currentIndex <= 0 ? 0 : folders[folderBox.currentIndex - 1].id
    property string subscribingUrl: ""
    property string lastSearch: ""

    function search() {
        var t = urlField.text.trim()
        if (t === "" || (t === lastSearch && (finder.busy || finder.searched)))
            return
        lastSearch = t
        finder.find(t)
    }

    function subscribe(url) {
        if (FeedManager.busy)
            return
        subscribingUrl = url
        FeedManager.addFeed(url, folderId)
    }

    function isWebAddress(text) {
        return text.length < 1000 && /^(https?|feed):\/\/\S+$/i.test(text)
    }
    // Clipboard text as an address: also a bare domain ("linmob.net",
    // copied without https://, device test 24 Oct), "" if it is none
    function clipboardAddress() {
        var t = Clipboard.hasText ? Clipboard.text.trim() : ""
        if (isWebAddress(t))
            return t
        if (t.length < 300 && /^(www\.)?[a-z0-9-]+(\.[a-z0-9-]+)*\.[a-z]{2,}(\/\S*)?$/i.test(t))
            return "https://" + t
        return ""
    }

    Component.onCompleted: {
        var start = initialUrl !== "" ? initialUrl : clipboardAddress()
        if (isWebAddress(start)) {
            urlField.text = start
            filledFrom = initialUrl !== "" ? 2 : 1
            filledText = start
        }
        for (var i = 0; i < folders.length; i++) {
            if (folders[i].id === preselectedFolderId)
                folderBox.currentIndex = i + 1
        }
        if (urlField.text !== "")
            search()
        else
            urlField.forceActiveFocus()
        // Focus set once here: "focus: text === ''" took the focus away after
        // the first letter (device test 14 Oct)
    }

    FeedFinder {
        id: finder
        knownUrls: app.feeds.feedUrls()
        // Address, website and title of each subscribed feed: "subscribed"
        // also by redirects and self links, "likely" by title and website
        knownFeeds: app.feeds.subscribedFeeds()
    }

    Connections {
        target: FeedManager
        onSucceeded: {
            if (action !== "addFeed" || page.subscribingUrl === "")
                return
            finder.knownUrls = finder.knownUrls.concat([page.subscribingUrl])
            page.subscribingUrl = ""
            hint.show(qsTr("Subscribed"))
        }
        onFailed: {
            if (action !== "addFeed" || page.subscribingUrl === "")
                return
            page.subscribingUrl = ""
            app.showMessage(message !== "" ? message : qsTr("The server rejected the feed"), true)
        }
    }

    // Search shortly after typing stops
    Timer {
        id: searchTimer
        interval: 900
        onTriggered: page.search()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Subscribe to website") }

            TextField {
                id: urlField
                width: parent.width
                // Where a filled-in address came from, while it is unchanged
                // (one line under the field; device test 15 Oct: a second
                // line below the field's own label was too much)
                label: page.filledFrom !== 0 && text === page.filledText
                       ? (page.filledFrom === 1 ? qsTr("Address from the clipboard") : qsTr("Address of the page shown"))
                       : qsTr("Address of a website, article or feed")
                placeholderText: "example.org"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-search"
                EnterKey.onClicked: { searchTimer.stop(); page.search(); focus = false }
                onTextChanged: searchTimer.restart()
                rightItem: IconButton {
                    width: icon.width + 2 * Theme.paddingMedium
                    height: icon.height
                    icon.source: "image://theme/icon-m-clipboard"
                    enabled: Clipboard.hasText
                    onClicked: {
                        page.filledFrom = 1
                        page.filledText = Clipboard.text.trim()
                        urlField.text = page.filledText
                    }
                }
            }

            ComboBox {
                id: folderBox
                width: parent.width
                label: qsTr("Folder")
                menu: ContextMenu {
                    MenuItem { text: qsTr("No folder") }
                    Repeater {
                        model: page.folders
                        MenuItem { text: modelData.name }
                    }
                }
            }

            // --- State ---------------------------------------------------
            Item { width: 1; height: Theme.paddingMedium }
            Row {
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: finder.busy
                BusyIndicator { size: BusyIndicatorSize.Small; running: finder.busy; anchors.verticalCenter: parent.verticalCenter }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.highlightColor
                    text: qsTr("Looking for feeds …")
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: urlField.text === "" && !finder.busy
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Enter the address of a website or of one of its articles. Sailfeed looks for its feeds and shows them with their newest articles.")
                      + "\n\n" + qsTr("Copied an address before? It is filled in when this page opens, or with the clipboard button next to the field.")
            }

            // Nothing found
            Column {
                width: parent.width
                visible: finder.searched && !finder.busy && finder.feeds.length === 0 && urlField.text !== ""
                spacing: Theme.paddingMedium
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    color: Theme.highlightColor
                    text: finder.error
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Nextcloud News can still try the address itself.")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Add anyway")
                    enabled: !FeedManager.busy && FeedManager.normalizeUrl(urlField.text) !== ""
                    onClicked: page.subscribe(FeedManager.normalizeUrl(urlField.text))
                }
            }

            // --- Results ------------------------------------------------
            SectionHeader {
                visible: finder.feeds.length > 0
                text: finder.feeds.length === 1 ? qsTr("1 feed found") : qsTr("%1 feeds found").arg(finder.feeds.length)
            }

            Repeater {
                model: finder.feeds
                delegate: Column {
                    id: result
                    readonly property var feed: modelData
                    readonly property bool done: feed.subscribed
                    readonly property bool working: page.subscribingUrl === feed.url
                    readonly property var latestDate: feed.latest
                    width: column.width
                    spacing: Theme.paddingSmall

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        truncationMode: TruncationMode.Elide
                        color: Theme.primaryColor
                        text: result.feed.title !== "" ? result.feed.title : result.feed.url
                    }
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: String(result.feed.url).replace(/^https?:\/\//, "")
                    }
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryHighlightColor
                        text: {
                            var parts = [result.feed.format, (result.feed.itemCount === 1 ? qsTr("1 article") : qsTr("%1 articles").arg(result.feed.itemCount))]
                            var d = result.latestDate
                            if (d && !isNaN(d.getTime()))
                                parts.push(qsTr("newest %1").arg(Format.formatDate(d, Formatter.DurationElapsed)))
                            if (result.feed.comments)
                                parts.push(qsTr("Comments"))
                            return parts.join(" · ")
                        }
                    }
                    // Same title and website as a subscribed feed, another
                    // address: a note, the button stays (it may be another feed)
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        visible: !result.done && (result.feed.likelySubscribedAs || "") !== ""
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.highlightColor
                        text: qsTr("Probably already subscribed as \"%1\"").arg(result.feed.likelySubscribedAs || "")
                    }
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        visible: result.feed.podcast
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.highlightColor
                        text: qsTr("Podcast – its episodes are audio or video, made for a podcast app. Subscribing here shows them as articles.")
                    }
                    Repeater {
                        model: result.feed.recent
                        Label {
                            x: Theme.horizontalPageMargin + Theme.paddingMedium
                            width: column.width - x - Theme.horizontalPageMargin
                            truncationMode: TruncationMode.Fade
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                            text: "· " + modelData
                        }
                    }
                    Item {
                        width: parent.width
                        height: Math.max(subscribeButton.height, doneLabel.height) + Theme.paddingSmall
                        Button {
                            id: subscribeButton
                            anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                            visible: !result.done
                            preferredWidth: Theme.buttonWidthSmall
                            text: qsTr("Subscribe")
                            enabled: !FeedManager.busy
                            onClicked: page.subscribe(result.feed.url)
                        }
                        BusyIndicator {
                            anchors { right: subscribeButton.left; rightMargin: Theme.paddingMedium; verticalCenter: parent.verticalCenter }
                            size: BusyIndicatorSize.ExtraSmall
                            running: result.working
                        }
                        Label {
                            id: doneLabel
                            anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                            visible: result.done
                            color: Theme.highlightColor
                            text: "✓ " + qsTr("Subscribed")
                        }
                    }
                    Item { width: 1; height: Theme.paddingLarge }
                }
            }
        }
        VerticalScrollDecorator { }
    }

    Hint { id: hint }
}
