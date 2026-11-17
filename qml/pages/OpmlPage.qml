// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Import and export feeds (1.4): the feed list as an OPML file.
// Export to the Nextcloud (folder "Sailfeed", like the settings backup), to
// the Downloads folder or through the share dialog; import from the same
// Nextcloud folder or from Downloads (OpmlFilesPage → OpmlImportDialog).
// While an import runs, its progress is shown here; afterwards the result
// with the feeds that could not be added. FeedTransfer in C++.
import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Share 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    // Counts on the device, read when the page opens and after an import
    property var counts: feedTransfer.summary()
    readonly property var last: feedTransfer.lastImport
    readonly property bool hasLast: last.time !== undefined

    function feedsText(n) { return n === 1 ? qsTr("1 feed") : qsTr("%1 feeds").arg(n) }
    function foldersText(n) { return n === 1 ? qsTr("1 folder") : qsTr("%1 folders").arg(n) }
    function todayName() {
        return "sailfeed-feeds-" + Qt.formatDate(new Date(), "yyyy-MM-dd") + ".opml"
    }
    function lastSummary() {
        var parts = []
        parts.push(last.added === 1 ? qsTr("1 feed subscribed") : qsTr("%1 feeds subscribed").arg(last.added))
        if (last.skipped > 0)
            parts.push(last.skipped === 1 ? qsTr("1 was subscribed already") : qsTr("%1 were subscribed already").arg(last.skipped))
        var failed = 0
        for (var i = 0; i < last.failed.length; ++i)
            if (last.failed[i].url !== "")
                failed++
        if (failed > 0)
            parts.push(failed === 1 ? qsTr("1 could not be added") : qsTr("%1 could not be added").arg(failed))
        return parts.join(" · ")
    }

    Connections {
        target: feedTransfer
        onExported: {
            if (ok)
                app.showMessage(qsTr("Saved on your Nextcloud: Sailfeed/%1").arg(name))
            else
                app.showMessage(qsTr("Export failed: %1").arg(message), true)
        }
        onImportingChanged: if (!importing) page.counts = feedTransfer.summary()
    }

    ShareAction {
        id: shareAction
        title: qsTr("Share feed list")
        mimeType: "text/x-opml"
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Import and export feeds")
                description: qsTr("OPML file")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("OPML is the usual file format for feed lists: other feed readers can import it, and Sailfeed can subscribe to the feeds of a file from another reader.")
            }

            // --- Import running -------------------------------------------
            SectionHeader {
                visible: feedTransfer.importing
                text: qsTr("Import")
            }
            Column {
                visible: feedTransfer.importing
                width: parent.width
                ProgressBar {
                    width: parent.width
                    minimumValue: 0
                    maximumValue: Math.max(1, feedTransfer.importTotal)
                    value: feedTransfer.importDone
                    label: qsTr("Subscribing %1 of %2 …").arg(Math.min(feedTransfer.importDone + 1, feedTransfer.importTotal))
                                                       .arg(feedTransfer.importTotal)
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    truncationMode: TruncationMode.Fade
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: feedTransfer.importCurrent
                }
                ListActionItem {
                    text: qsTr("Stop the import")
                    description: qsTr("The feed being subscribed is finished first.")
                    onClicked: feedTransfer.cancelImport()
                }
            }

            // --- Export ---------------------------------------------------
            SectionHeader { text: qsTr("Export") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: page.counts.feeds > 0
                      ? qsTr("All your feeds and folders: %1, %2.").arg(page.feedsText(page.counts.feeds))
                                                                   .arg(page.foldersText(page.counts.folders))
                      : qsTr("No feeds to export yet.")
            }
            ListActionItem {
                enabled: page.counts.feeds > 0 && config.isAccountValid && !feedTransfer.busy
                busy: feedTransfer.busy
                text: qsTr("Save in your Nextcloud")
                description: qsTr("Folder \"Sailfeed\", file %1. Replaces the previous export; Nextcloud keeps the older versions.")
                             .arg("sailfeed-feeds.opml")
                onClicked: feedTransfer.exportToNextcloud()
            }
            ListActionItem {
                enabled: page.counts.feeds > 0
                text: qsTr("Save in Downloads")
                description: qsTr("File %1 in the Downloads folder").arg(page.todayName())
                onClicked: {
                    var r = feedTransfer.exportToDownloads()
                    if (r.ok)
                        app.showMessage(qsTr("Saved in Downloads: %1").arg(r.name))
                    else
                        app.showMessage(qsTr("Export failed: %1").arg(r.error), true)
                }
            }
            ListActionItem {
                enabled: page.counts.feeds > 0
                text: qsTr("Share …")
                description: qsTr("By e-mail or to another app")
                onClicked: {
                    shareAction.resources = [{ "data": feedTransfer.opmlText(),
                                               "name": page.todayName(),
                                               "type": "text/x-opml" }]
                    shareAction.trigger()
                }
            }

            // --- Import ---------------------------------------------------
            SectionHeader { text: qsTr("Import") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Subscribes to the feeds of a file in your Nextcloud News. Feeds you have already are skipped, missing folders are created.")
            }
            ListActionItem {
                opensPage: true
                enabled: config.isAccountValid && !feedTransfer.importing
                text: qsTr("From your Nextcloud")
                description: qsTr("OPML files in the folder \"Sailfeed\" of your Nextcloud files")
                onClicked: pageStack.push(Qt.resolvedUrl("OpmlFilesPage.qml"), { source: "nextcloud", returnPage: page })
            }
            ListActionItem {
                opensPage: true
                enabled: config.isAccountValid && !feedTransfer.importing
                text: qsTr("From Downloads")
                description: qsTr("OPML files in the Downloads folder")
                onClicked: pageStack.push(Qt.resolvedUrl("OpmlFilesPage.qml"), { source: "downloads", returnPage: page })
            }

            // --- Result of the last import --------------------------------
            SectionHeader {
                visible: page.hasLast && !feedTransfer.importing
                text: qsTr("Last import")
            }
            Label {
                visible: page.hasLast && !feedTransfer.importing
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
                text: page.hasLast ? page.lastSummary() + (page.last.cancelled ? "\n" + qsTr("Stopped before the end.") : "") : ""
            }
            Repeater {
                model: page.hasLast && !feedTransfer.importing ? page.last.failed : []
                delegate: Column {
                    x: Theme.horizontalPageMargin
                    width: column.width - 2 * Theme.horizontalPageMargin
                    Item { width: 1; height: Theme.paddingMedium }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeSmall
                        text: modelData.title
                    }
                    Label {
                        width: parent.width
                        visible: modelData.url !== ""
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: modelData.url
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: modelData.reason
                    }
                }
            }
        }
        VerticalScrollDecorator { }
    }
}
