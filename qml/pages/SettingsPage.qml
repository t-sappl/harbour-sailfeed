// SPDX-License-Identifier: GPL-3.0-or-later
// Settings: a short main page with one entry per subject, each with a summary
// of its current values; the settings themselves are on sub-pages (author,
// 14 Oct: one long page stayed hard to take in after two reorderings). The
// sub-pages follow Sailwave's airy settings: switches and choices with
// descriptions, no collapsed sections.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: settingsPage

    function join(parts) {
        return parts.filter(function(p) { return p !== "" }).join(" · ")
    }
    function openModeName(mode) {
        return [qsTr("Reader mode"), qsTr("Original website"), qsTr("External browser")][Math.max(0, Math.min(2, mode))]
    }
    function colorsName(colors) {
        return [qsTr("Like the ambience"), qsTr("Light"), qsTr("Dark"), qsTr("Sepia")][Math.max(0, Math.min(3, colors))]
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            PageHeader { title: qsTr("Settings") }

            SettingsEntry {
                title: qsTr("Article list")
                summary: settingsPage.join([app.autoReadDelay === 0 ? qsTr("Not marked as read by itself")
                                                                    : qsTr("Read after %1 s").arg(app.autoReadDelay),
                                            app.unreadOnly ? qsTr("unread only") : "",
                                            app.listView === 1 ? qsTr("compact") : ""])
                target: Qt.resolvedUrl("SettingsListPage.qml")
            }
            SettingsEntry {
                title: qsTr("Article preview")
                summary: settingsPage.join([app.skipPreview ? qsTr("Skipped") : qsTr("Shown"),
                                            qsTr("text %1 %").arg(Math.round(100 * (1 + 0.12 * app.previewFontStep)))])
                target: Qt.resolvedUrl("SettingsPreviewPage.qml")
            }
            SettingsEntry {
                title: qsTr("Reader mode")
                summary: settingsPage.join([settingsPage.openModeName(app.openMode),
                                            settingsPage.colorsName(app.readerColors),
                                            qsTr("size %1").arg(app.readerFontPx)])
                target: Qt.resolvedUrl("SettingsReaderPage.qml")
            }
            // Sign-in to Nextcloud News and the settings backup on the same
            // Nextcloud (15 Oct: "Account" alone was unclear)
            SettingsEntry {
                title: qsTr("Nextcloud")
                summary: !config.isAccountValid ? qsTr("not signed in")
                         : settingsPage.join([config.username,
                                              !app.autoBackup ? qsTr("backup by hand")
                                              : (isNaN(cloudBackup.lastBackup) ? qsTr("not backed up yet")
                                                 : qsTr("backed up %1").arg(Format.formatDate(cloudBackup.lastBackup, Formatter.DurationElapsed)))])
                target: Qt.resolvedUrl("SettingsNextcloudPage.qml")
            }
            SettingsEntry {
                title: qsTr("Privacy")
                summary: settingsPage.join([Prefetch.shareBrowserCookies ? qsTr("Browser cookies for the reader mode")
                                                                          : qsTr("No browser cookies for the reader mode"),
                                            Prefetch.clearAtStart ? qsTr("deleted at every start") : ""])
                target: Qt.resolvedUrl("SettingsPrivacyPage.qml")
            }

            Item { width: 1; height: Theme.paddingLarge }

            SettingsEntry {
                title: qsTr("About Sailfeed")
                target: Qt.resolvedUrl("AboutPage.qml")
            }
        }
        VerticalScrollDecorator { }
    }

}
