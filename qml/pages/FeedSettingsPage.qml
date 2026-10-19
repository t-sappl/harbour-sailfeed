// SPDX-License-Identifier: GPL-3.0-or-later
// Feed settings (1.2): name and folder of one feed (until then "Rename" and
// "Move to folder" in the feed's long-press menu) and its own reading
// settings – list view, preview, full article, text size. Each is "like the
// general setting" until changed here, so only exceptions are stored
// (app.feedSetting / setFeedSetting, in the settings backup).
// Opened from the feed's long-press menu in the feed list and from the
// article list's pulley while one feed is selected.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    property int feedId: -1
    property var info: app.feeds.feedInfo(feedId)
    function refresh() {
        var i = app.feeds.feedInfo(feedId)
        if (i.id !== undefined)
            info = i
        if (app.currentFeedId === feedId && i.title)
            app.currentSelectionName = i.title
    }
    // Rename / move: the feed list reloads the feeds after a change
    Connections {
        target: app.feeds
        onRevisionChanged: page.refresh()
        onCountChanged: page.refresh()
    }
    // The feed list shows its hints under this page; repeat them here
    Connections {
        target: FeedManager
        onSucceeded: {
            if (page.status !== PageStatus.Active)
                return
            if (action === "renameFeed")
                hint.show(qsTr("Feed renamed"))
            else if (action === "moveFeed")
                hint.show(qsTr("Feed moved"))
        }
        onFailed: {
            if (page.status === PageStatus.Active)
                hint.show(message !== "" ? message : qsTr("The server rejected the change"))
        }
    }

    readonly property var openModeNames: [qsTr("Reader mode"), qsTr("Original website"), qsTr("External browser")]

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: page.info.title || qsTr("Feed settings")
                description: qsTr("Feed settings")
            }

            ValueButton {
                label: qsTr("Name")
                value: page.info.title || ""
                enabled: !FeedManager.busy
                onClicked: {
                    var feedId = page.feedId
                    var d = pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                                           { title: qsTr("Rename feed"), initialValue: page.info.title || "" })
                    d.accepted.connect(function() { FeedManager.renameFeed(feedId, d.value) })
                }
            }
            ValueButton {
                label: qsTr("Folder")
                value: (page.info.folderId || 0) > 0 ? (page.info.folderName || "") : qsTr("No folder")
                enabled: !FeedManager.busy
                onClicked: {
                    var feedId = page.feedId
                    var d = pageStack.push(Qt.resolvedUrl("FolderPickerDialog.qml"),
                                           { feedTitle: page.info.title || "", currentFolderId: Math.max(0, page.info.folderId || 0) })
                    d.accepted.connect(function() { FeedManager.moveFeed(feedId, d.folderId) })
                }
            }

            SectionHeader { text: qsTr("Reading") }

            // Every combo box: entry 0 = like the general setting (-1),
            // written on tap only (see the reader settings)
            ComboBox {
                width: parent.width
                label: qsTr("List view")
                currentIndex: app.feedSetting("feedListView", page.feedId) + 1
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Like the general setting (%1)").arg(app.listView === 1 ? qsTr("compact") : qsTr("detailed"))
                        onClicked: app.setFeedSetting("feedListView", page.feedId, -1)
                    }
                    MenuItem { text: qsTr("Detailed"); onClicked: app.setFeedSetting("feedListView", page.feedId, 0) }
                    MenuItem { text: qsTr("Compact"); onClicked: app.setFeedSetting("feedListView", page.feedId, 1) }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Preview")
                currentIndex: app.feedSetting("feedSkipPreview", page.feedId) + 1
                description: app.skipPreviewFor(page.feedId)
                             ? qsTr("Tapping an article opens the full article right away – useful for feeds without texts.")
                             : ""
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Like the general setting (%1)").arg(app.skipPreview ? qsTr("skipped") : qsTr("shown"))
                        onClicked: app.setFeedSetting("feedSkipPreview", page.feedId, -1)
                    }
                    MenuItem { text: qsTr("Show"); onClicked: app.setFeedSetting("feedSkipPreview", page.feedId, 0) }
                    MenuItem { text: qsTr("Skip"); onClicked: app.setFeedSetting("feedSkipPreview", page.feedId, 1) }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Full article")
                currentIndex: app.feedSetting("feedOpenMode", page.feedId) + 1
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Like the general setting (%1)").arg(page.openModeNames[Math.max(0, Math.min(2, app.openMode))])
                        onClicked: app.setFeedSetting("feedOpenMode", page.feedId, -1)
                    }
                    MenuItem { text: page.openModeNames[0]; onClicked: app.setFeedSetting("feedOpenMode", page.feedId, 0) }
                    MenuItem { text: page.openModeNames[1]; onClicked: app.setFeedSetting("feedOpenMode", page.feedId, 1) }
                    MenuItem { text: page.openModeNames[2]; onClicked: app.setFeedSetting("feedOpenMode", page.feedId, 2) }
                }
            }

            // Text size: set with A−/A+ while reading (when sizes are kept per feed)
            ListActionItem {
                visible: app.feedHasTextSize(page.feedId)
                text: qsTr("Reset the text size of this feed")
                description: qsTr("It was changed while reading; afterwards the general size applies.")
                onClicked: {
                    app.resetFeedTextSize(page.feedId)
                    hint.show(qsTr("Text size reset"))
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !app.feedHasTextSize(page.feedId)
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: app.textSizePerFeed
                      ? qsTr("Text size: the general size. A size changed with A− / A+ while reading is kept for this feed.")
                      : qsTr("Text size: the general size (sizes per feed are off in the settings).")
            }

            SectionHeader { text: qsTr("Address") }
            BackgroundItem {
                id: addressItem
                width: parent.width
                height: addressLabel.height + 2 * Theme.paddingMedium
                onClicked: {
                    Clipboard.text = page.info.url || ""
                    hint.show(qsTr("Address copied"))
                }
                Label {
                    id: addressLabel
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: Theme.fontSizeSmall
                    color: addressItem.highlighted ? Theme.highlightColor : Theme.secondaryColor
                    text: page.info.url || ""
                }
            }
        }
        VerticalScrollDecorator { }
    }

    Hint { id: hint }
}
