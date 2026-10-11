// SPDX-License-Identifier: GPL-3.0-or-later
// Restore the settings: the backups in the folder "Sailfeed" of the user's
// Nextcloud (the latest one and the dated copies of the last days), and the
// settings as they were before the last restore (kept on the device).
// Tapping one opens RestoreSettingsDialog, which shows what would change.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    property string errorText: ""
    property string pendingName: ""

    function openBackup(content, title) {
        var parsed = SettingsBackup.parse(content)
        if (!parsed.ok) {
            app.showMessage(parsed.error, true)
            return
        }
        pageStack.push(Qt.resolvedUrl("RestoreSettingsDialog.qml"), { parsed: parsed, sourceTitle: title })
    }

    function titleFor(entry) {
        // Copies: date and time of the state (several a day are possible)
        return entry.current ? qsTr("Latest backup") : Format.formatDate(entry.stateTime, Formatter.Timepoint)
    }

    Component.onCompleted: cloudBackup.refresh()

    Connections {
        target: cloudBackup
        onRefreshed: page.errorText = ok ? "" : message
        onFetched: {
            if (name !== page.pendingName)
                return
            var title = page.pendingTitle
            page.pendingName = ""
            if (ok)
                page.openBackup(content, title)
            else
                app.showMessage(qsTr("Could not load the backup: %1").arg(message), true)
        }
    }
    property string pendingTitle: ""

    SilicaListView {
        id: list
        anchors.fill: parent
        model: cloudBackup.backups

        header: Column {
            width: list.width
            PageHeader {
                title: qsTr("Restore settings")
                description: qsTr("Folder \"Sailfeed\" on your Nextcloud")
            }
            // Undo the last restore
            ListItem {
                id: localItem
                visible: !isNaN(cloudBackup.localCopyTime)
                contentHeight: Theme.itemSizeMedium
                onClicked: page.openBackup(cloudBackup.localCopy(), qsTr("Before the last restore"))
                Column {
                    anchors { left: parent.left; right: parent.right; leftMargin: Theme.horizontalPageMargin
                              rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        text: qsTr("Before the last restore")
                        color: localItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: localItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        text: qsTr("On this device") + " · " + Format.formatDate(cloudBackup.localCopyTime, Formatter.Timepoint)
                    }
                }
            }
            SectionHeader {
                visible: list.count > 0
                text: qsTr("On your Nextcloud")
            }
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeMedium
            enabled: !cloudBackup.busy
            onClicked: {
                page.pendingName = modelData.name
                page.pendingTitle = page.titleFor(modelData)
                cloudBackup.fetch(modelData.name)
            }
            Column {
                anchors { left: parent.left; right: busy.left; leftMargin: Theme.horizontalPageMargin
                          rightMargin: Theme.paddingMedium; verticalCenter: parent.verticalCenter }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: page.titleFor(modelData)
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: modelData.current ? qsTr("Saved %1").arg(Format.formatDate(modelData.modified, Formatter.Timepoint))
                                            : qsTr("Earlier state, kept before a later backup")
                }
            }
            BusyIndicator {
                id: busy
                anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                size: BusyIndicatorSize.ExtraSmall
                running: cloudBackup.busy && page.pendingName === modelData.name
            }
        }

        PullDownMenu {
            busy: cloudBackup.busy
            MenuItem {
                text: qsTr("Refresh")
                enabled: !cloudBackup.busy
                onClicked: cloudBackup.refresh()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !cloudBackup.busy
            text: page.errorText !== "" ? qsTr("Could not read the backups")
                                        : qsTr("No backups on your Nextcloud yet")
            hintText: page.errorText !== "" ? page.errorText
                                            : qsTr("Back up in the settings first")
        }

        VerticalScrollDecorator { }
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: list.count === 0 && cloudBackup.busy
    }
}
