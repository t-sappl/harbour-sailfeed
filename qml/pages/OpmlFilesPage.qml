// SPDX-License-Identifier: GPL-3.0-or-later
// OPML files to import (1.4): in the folder "Sailfeed" of the user's
// Nextcloud (like the settings backups) or in the Downloads folder. Tapping
// one reads it and opens OpmlImportDialog with what would be subscribed.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0

Page {
    id: page

    property string source: "nextcloud"     // or "downloads"
    property Item returnPage                // OpmlPage: shown again when the import starts
    readonly property bool nextcloud: source === "nextcloud"
    property var files: []
    property string errorText: ""
    property bool listed: false
    property string pendingName: ""

    function refresh() {
        if (nextcloud) {
            if (feedTransfer.listNextcloud())
                listed = false
        } else {
            files = feedTransfer.downloadFiles()
            listed = true
        }
    }

    function openPlan(name, plan) {
        if (!plan.ok) {
            app.showMessage(qsTr("Could not read %1: %2").arg(name).arg(plan.error), true)
            return
        }
        pageStack.push(Qt.resolvedUrl("OpmlImportDialog.qml"),
                       { plan: plan, fileName: name, returnPage: page.returnPage })
    }

    Component.onCompleted: refresh()

    Connections {
        target: feedTransfer
        onNextcloudListed: {
            if (!page.nextcloud)
                return
            page.errorText = ok ? "" : message
            page.files = feedTransfer.nextcloudFiles
            page.listed = true
        }
        onLoaded: {
            if (name !== page.pendingName)
                return
            page.pendingName = ""
            if (!ok && plan.ok === undefined)
                app.showMessage(qsTr("Could not load %1: %2").arg(name).arg(message), true)
            else
                page.openPlan(name, plan)
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: page.files

        header: PageHeader {
            title: page.nextcloud ? qsTr("Import from your Nextcloud") : qsTr("Import from Downloads")
            description: page.nextcloud ? qsTr("Folder \"Sailfeed\"") : qsTr("Downloads folder")
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeMedium
            enabled: !feedTransfer.busy
            onClicked: {
                if (page.nextcloud) {
                    page.pendingName = modelData.name
                    feedTransfer.loadNextcloud(modelData.name)
                } else {
                    page.openPlan(modelData.name, feedTransfer.loadFile(modelData.path))
                }
            }
            Column {
                anchors { left: parent.left; right: busy.left; leftMargin: Theme.horizontalPageMargin
                          rightMargin: Theme.paddingMedium; verticalCenter: parent.verticalCenter }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: modelData.name
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: Format.formatDate(modelData.modified, Formatter.Timepoint) + " · " + Format.formatFileSize(modelData.size)
                }
            }
            BusyIndicator {
                id: busy
                anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                size: BusyIndicatorSize.ExtraSmall
                running: feedTransfer.busy && page.pendingName === modelData.name
            }
        }

        PullDownMenu {
            busy: feedTransfer.busy
            MenuItem {
                text: qsTr("Refresh")
                enabled: !feedTransfer.busy
                onClicked: page.refresh()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && page.listed && !feedTransfer.busy
            text: page.errorText !== "" ? qsTr("Could not read the files")
                  : (page.nextcloud ? qsTr("No OPML files on your Nextcloud") : qsTr("No OPML files in Downloads"))
            hintText: page.errorText !== "" ? page.errorText
                      : (page.nextcloud ? qsTr("Put the OPML file of the other feed reader into the folder \"Sailfeed\" of your Nextcloud files, e.g. in the web interface.")
                                        : qsTr("Save the OPML file of the other feed reader in the Downloads folder, e.g. from an e-mail or the browser."))
        }

        VerticalScrollDecorator { }
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: list.count === 0 && feedTransfer.busy
    }
}
