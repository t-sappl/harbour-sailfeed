// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Storage (1.4): deleting old read articles, the image cache.
// Own page since the blind test (11 Nov): the image cache was looked for
// everywhere but under "Article list".
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
            PageHeader { title: qsTr("Storage") }

            // Deleting old read articles (DESIGN): at the start, at most once
            // a day, so no "Clean up now" (author, 15 Oct)
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
                // With remorse like the other deleting actions (device test 12 Nov)
                onClicked: clearRemorse.execute(qsTr("Clearing the image cache"), function() {
                    var freed = Cleanup.clearImageCache()
                    page.imageBytes = Cleanup.imageCacheBytes()
                    app.showMessage(qsTr("Image cache cleared (%1 MB)").arg(page.mb(freed)))
                })
            }
        }
        VerticalScrollDecorator { }
    }

    RemorsePopup { id: clearRemorse }
}
