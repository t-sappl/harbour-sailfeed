// SPDX-License-Identifier: GPL-3.0-or-later
// Confirmation before restoring the settings: where the backup comes from,
// the restore mode (as in Sailwave), whether the text sizes are included,
// and what exactly would change (SettingsBackup.plan()).
//  - "Replace current settings" (default): everything as in the backup.
//  - "Add missing only": favorite feeds, sites in the original and per-feed
//    text sizes are added; nothing else changes.
// The current settings are kept on the device first (undo in the banner and
// on the restore page).
// offer: shown by itself after signing in on a device that is not linked to
// the account yet (package 5e). Declining links the device as well; its own
// settings are backed up from then on.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0

Dialog {
    id: dialog

    property var parsed                 // SettingsBackup.parse()
    property string sourceTitle
    property bool offer: false

    // The settings as they are now, taken once when the dialog opens
    property var current: app.settingsState()
    readonly property int mode: modeCombo.currentIndex === 0 ? SettingsBackup.Replace : SettingsBackup.AddMissing
    readonly property bool includeTextSizes: parsed.hasTextSizes && sizesSwitch.checked
    readonly property var plan: SettingsBackup.plan(current, parsed.state, mode, includeTextSizes)

    canAccept: plan.hasChanges

    function names(list) {
        var out = []
        for (var i = 0; i < list.length; i++)
            out.push(list[i])
        return out.join(", ")
    }

    function changeLines() {
        var lines = []
        if (plan.settingsChanged > 0)
            lines.push(qsTr("%n setting(s) will change", "", plan.settingsChanged))
        if (plan.textSizesChanged)
            lines.push(qsTr("Text sizes will change"))
        if (plan.favoritesAdded.length > 0)
            lines.push(qsTr("Favorite feeds added: %1").arg(names(plan.favoritesAdded)))
        if (plan.favoritesRemoved.length > 0)
            lines.push(qsTr("Favorite feeds removed: %1").arg(names(plan.favoritesRemoved)))
        if (plan.sitesAdded.length > 0)
            lines.push(qsTr("Sites in the original added: %1").arg(names(plan.sitesAdded)))
        if (plan.sitesRemoved.length > 0)
            lines.push(qsTr("Sites in the original removed: %1").arg(names(plan.sitesRemoved)))
        return lines.length > 0 ? lines.join("\n") : qsTr("No changes – the settings are already like this.")
    }

    onAccepted: app.restoreSettings(plan)
    onRejected: if (offer) cloudBackup.setLinked()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingSmall

            DialogHeader {
                title: dialog.offer ? qsTr("Settings found") : qsTr("Restore settings")
                acceptText: dialog.offer ? qsTr("Use them") : qsTr("Restore")
                cancelText: dialog.offer ? qsTr("Not now") : qsTr("Cancel")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: dialog.offer
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("Your Nextcloud has saved Sailfeed settings, from another device or an earlier installation. Use them on this device? If not, the settings of this device are backed up from now on.")
            }

            DetailItem {
                label: qsTr("Backup")
                value: dialog.sourceTitle
            }
            DetailItem {
                visible: !isNaN(dialog.parsed.savedAt)
                label: qsTr("Saved")
                value: Format.formatDate(dialog.parsed.savedAt, Formatter.Timepoint)
            }
            DetailItem {
                visible: dialog.parsed.device !== ""
                label: qsTr("Device")
                value: dialog.parsed.device
            }

            ComboBox {
                id: modeCombo
                width: parent.width
                label: qsTr("Restore mode")
                currentIndex: 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("Replace current settings") }
                    MenuItem { text: qsTr("Add missing only") }
                }
            }

            TextSwitch {
                id: sizesSwitch
                text: qsTr("Include text sizes")
                enabled: dialog.parsed.hasTextSizes
                checked: dialog.parsed.hasTextSizes && app.backupTextSizes
                description: dialog.parsed.hasTextSizes
                             ? qsTr("Off: the text sizes on this device stay as they are.")
                             : qsTr("This backup contains no text sizes.")
            }

            SectionHeader { text: qsTr("Changes") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: dialog.changeLines()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: dialog.mode === SettingsBackup.Replace
                      ? qsTr("All settings are set as in the backup; favorite feeds and sites in the original exactly as there. The current settings are kept on this device first, so you can undo it.")
                      : qsTr("Missing favorite feeds and sites in the original are added, as well as text sizes of feeds that have none on this device yet. Nothing else changes, nothing is removed. The current settings are kept on this device first.")
            }
        }

        VerticalScrollDecorator { }
    }
}
