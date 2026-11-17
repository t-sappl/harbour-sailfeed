// SPDX-License-Identifier: GPL-3.0-or-later
// Confirmation before an OPML import (1.4): how many feeds the file has,
// which are new and which are subscribed already, the folders that would be
// created, and "Keep the folders". Accepting starts the import and goes
// back to OpmlPage, which shows its progress.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0

Dialog {
    id: dialog

    property var plan: ({})
    property string fileName: ""
    property Item returnPage
    property bool keepFolders: true

    readonly property int newCount: plan.newCount || 0
    readonly property var newFolders: plan.newFolders || []

    function feedsText(n) { return n === 1 ? qsTr("1 feed") : qsTr("%1 feeds").arg(n) }

    canAccept: !feedTransfer.importing && (newCount > 0 || (keepFolders && newFolders.length > 0))
    acceptDestination: returnPage
    acceptDestinationAction: PageStackAction.Pop
    onAccepted: {
        if (!feedTransfer.startImport(dialog.keepFolders))
            app.showMessage(qsTr("The import could not be started."), true)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Import")
                title: dialog.fileName
            }

            DetailItem {
                visible: (dialog.plan.title || "") !== ""
                label: qsTr("Title")
                value: dialog.plan.title || ""
            }
            DetailItem {
                label: qsTr("Feeds in the file")
                value: dialog.plan.total || 0
            }
            DetailItem {
                label: qsTr("Subscribed already")
                value: dialog.plan.subscribed || 0
            }
            DetailItem {
                label: qsTr("New")
                value: dialog.newCount
            }
            DetailItem {
                visible: dialog.keepFolders && dialog.newFolders.length > 0
                label: qsTr("New folders")
                value: dialog.newFolders.join(", ")
            }

            Item { width: 1; height: Theme.paddingMedium }

            TextSwitch {
                text: qsTr("Keep the folders")
                description: dialog.keepFolders ? qsTr("On: missing folders are created and the new feeds go into their folders.")
                                             : qsTr("Off: the new feeds go to \"Other feeds\", no folder is created.")
                checked: dialog.keepFolders
                automaticCheck: false
                onClicked: dialog.keepFolders = !dialog.keepFolders
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: dialog.newCount > 0
                      ? qsTr("Your Nextcloud reads every feed while subscribing, so this takes a moment per feed. You can leave the page meanwhile.")
                      : qsTr("All feeds of this file are subscribed already.")
            }

            SectionHeader {
                visible: dialog.newCount > 0
                text: qsTr("New feeds")
            }
            Repeater {
                model: dialog.plan.feeds || []
                delegate: Column {
                    visible: !modelData.subscribed
                    x: Theme.horizontalPageMargin
                    width: column.width - 2 * Theme.horizontalPageMargin
                    Item { width: 1; height: Theme.paddingSmall }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.highlightColor
                        text: modelData.title
                    }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: dialog.keepFolders && modelData.folder !== ""
                              ? modelData.folder + " · " + modelData.url : modelData.url
                    }
                }
            }

            SectionHeader {
                visible: (dialog.plan.subscribed || 0) > 0
                text: qsTr("Subscribed already")
            }
            Repeater {
                model: dialog.plan.feeds || []
                delegate: Label {
                    visible: modelData.subscribed
                    x: Theme.horizontalPageMargin
                    width: column.width - 2 * Theme.horizontalPageMargin
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: modelData.title
                }
            }
        }
        VerticalScrollDecorator { }
    }
}
