// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Article list: marking as read, unread only, the mark-all
// button, the feed list's sort order, deleting old read articles.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page
    // Article count and database size, read when the page opens
    property string storageInfo
    property double imageBytes: 0
    onStatusChanged: {
        if (status === PageStatus.Activating) {
            storageInfo = Cleanup.storageInfo()
            imageBytes = Cleanup.imageCacheBytes()
        }
    }
    function mb(bytes) {
        var v = bytes / (1024 * 1024)
        return v < 10 ? v.toLocaleString(Qt.locale(), "f", 1) : Math.round(v).toLocaleString(Qt.locale(), "f", 0)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            PageHeader { title: qsTr("Article list") }

            // A choice of steps instead of a slider (author, 14 Oct): the
            // usual values at a glance, one tap; a value set before that is not
            // in the list shows the nearest step
            ComboBox {
                id: autoReadBox
                readonly property var steps: [0, 2, 3, 4, 5, 7, 10, 15]
                width: parent.width
                label: qsTr("Mark as read after")
                description: app.autoReadDelay === 0
                             ? qsTr("Articles are only marked as read when you open them or mark them yourself.")
                             : qsTr("Articles that are fully visible in the list for this time count as read.")
                currentIndex: {
                    var best = 0
                    for (var i = 1; i < steps.length; i++)
                        if (Math.abs(steps[i] - app.autoReadDelay) < Math.abs(steps[best] - app.autoReadDelay))
                            best = i
                    return best
                }
                // Written on tap only (see "Links in articles" in the reader settings)
                menu: ContextMenu {
                    Repeater {
                        model: autoReadBox.steps
                        MenuItem {
                            text: modelData === 0 ? qsTr("Off") : qsTr("%1 seconds").arg(modelData)
                            onClicked: app.autoReadDelay = modelData
                        }
                    }
                }
            }
            ComboBox {
                width: parent.width
                enabled: app.autoReadDelay > 0
                label: qsTr("Grey out read articles")
                description: app.autoReadMode === 1
                             ? qsTr("Articles seen for the time above are greyed out when you scroll on, not while you are still reading.")
                             : qsTr("Articles are greyed out as soon as the time above is over.")
                currentIndex: app.autoReadMode
                menu: ContextMenu {
                    MenuItem { text: qsTr("At once"); onClicked: app.autoReadMode = 0 }
                    MenuItem { text: qsTr("When scrolling on"); onClicked: app.autoReadMode = 1 }
                }
            }
            // Written on tap only (see the reader settings)
            ComboBox {
                width: parent.width
                label: qsTr("List view")
                description: app.listView === 1
                             ? qsTr("Title and source only, about twice as many articles per page.")
                             : qsTr("Title, source, the beginning of the text and an image.")
                currentIndex: app.listView
                menu: ContextMenu {
                    MenuItem { text: qsTr("Detailed"); onClicked: app.listView = 0 }
                    MenuItem { text: qsTr("Compact"); onClicked: app.listView = 1 }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Order")
                description: app.oldestFirst
                             ? qsTr("Read from the oldest article on. New articles come at the end of the list.")
                             : ""
                currentIndex: app.oldestFirst ? 1 : 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("Newest first"); onClicked: app.oldestFirst = false }
                    MenuItem { text: qsTr("Oldest first"); onClicked: app.oldestFirst = true }
                }
            }
            TextSwitch {
                text: qsTr("Show unread articles only")
                description: qsTr("Articles read in this session stay visible until the next refresh.")
                checked: app.unreadOnly
                automaticCheck: false
                onClicked: app.unreadOnly = !app.unreadOnly
            }
            TextSwitch {
                text: qsTr("Button \"Mark all as read\"")
                description: qsTr("Push it up to mark all as read, with one hand.")
                checked: app.showMarkAllButton
                automaticCheck: false
                onClicked: app.showMarkAllButton = !app.showMarkAllButton
            }

            SectionHeader { text: qsTr("Feed list") }
            FeedSortComboBox {
                description: app.feedSortMode === 2
                             ? qsTr("Feeds with the most articles in the last 30 days first. Counts articles synced to this device.")
                             : app.feedSortMode === 4
                               ? qsTr("Feeds with unread articles first. Inside both groups by activity: articles of the last 30 days, each bookmarked article counts three times.")
                               : qsTr("Can also be changed at the top of the feed list.")
            }

            // Deleting old read articles (DESIGN): at the start, at most once
            // a day, so no "Clean up now" (author, 15 Oct)
            SectionHeader { text: qsTr("Storage") }
            ComboBox {
                id: cleanupBox
                readonly property var steps: [0, 1, 3, 6, 12]
                width: parent.width
                label: qsTr("Delete read articles after")
                description: Cleanup.deleteAfterMonths === 0
                             ? qsTr("Read articles stay on this device.")
                             : qsTr("Read articles older than this are deleted from this device when the app starts. Bookmarked and unread articles always stay. A change takes effect at the next start.")
                currentIndex: {
                    var i = steps.indexOf(Cleanup.deleteAfterMonths)
                    if (i >= 0)
                        return i
                    // A restored value between the steps: the nearest one
                    var best = 0
                    for (var j = 1; j < steps.length; j++)
                        if (Math.abs(steps[j] - Cleanup.deleteAfterMonths) < Math.abs(steps[best] - Cleanup.deleteAfterMonths))
                            best = j
                    return best
                }
                menu: ContextMenu {
                    MenuItem { text: qsTr("Never"); onClicked: Cleanup.deleteAfterMonths = 0 }
                    MenuItem { text: qsTr("1 month"); onClicked: Cleanup.deleteAfterMonths = 1 }
                    MenuItem { text: qsTr("3 months"); onClicked: Cleanup.deleteAfterMonths = 3 }
                    MenuItem { text: qsTr("6 months"); onClicked: Cleanup.deleteAfterMonths = 6 }
                    MenuItem { text: qsTr("1 year"); onClicked: Cleanup.deleteAfterMonths = 12 }
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: text !== ""
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: page.storageInfo
            }

            // Image cache (1.2): thumbnails, article images, favicons
            ComboBox {
                id: imageCacheBox
                readonly property var steps: [50, 100, 250, 500]
                width: parent.width
                label: qsTr("Image cache")
                description: qsTr("Images already loaded are not downloaded again. A new size applies at the next start.")
                currentIndex: {
                    var best = 0
                    for (var j = 1; j < steps.length; j++)
                        if (Math.abs(steps[j] - Cleanup.imageCacheMB) < Math.abs(steps[best] - Cleanup.imageCacheMB))
                            best = j
                    return best
                }
                menu: ContextMenu {
                    Repeater {
                        model: imageCacheBox.steps
                        MenuItem {
                            text: qsTr("%1 MB").arg(modelData)
                            onClicked: Cleanup.imageCacheMB = modelData
                        }
                    }
                }
            }
            ListActionItem {
                text: qsTr("Clear image cache")
                description: qsTr("Now %1 MB").arg(page.mb(page.imageBytes))
                enabled: page.imageBytes > 0
                onClicked: {
                    var freed = Cleanup.clearImageCache()
                    page.imageBytes = Cleanup.imageCacheBytes()
                    app.showMessage(qsTr("Image cache cleared (%1 MB)").arg(page.mb(freed)))
                }
            }
        }
        VerticalScrollDecorator { }
    }
}
