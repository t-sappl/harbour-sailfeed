// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Nextcloud (15 Oct: "Account" and "Backup on your Nextcloud" in
// one page – both use the same sign-in; the author found "Account" unclear):
// "Nextcloud News" (user, server, last sync, certificate option, change
// account / sign out) and "Backup of the settings" (package 5: automatic
// backup, text sizes, last backup, back up now / restore; only when signed in).
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            PageHeader { title: qsTr("Nextcloud") }

            SectionHeader { text: qsTr("Nextcloud News") }
            DetailItem {
                label: qsTr("User")
                value: config.isAccountValid ? config.username : qsTr("not signed in")
            }
            DetailItem {
                visible: config.serverUrl !== ""
                label: qsTr("Server")
                value: config.serverUrl
            }
            DetailItem {
                visible: config.isAccountValid
                label: qsTr("Last sync")
                value: isNaN(config.lastSync) ? "–" : Format.formatDate(config.lastSync, Formatter.DurationElapsed)
            }

            Item { width: 1; height: Theme.paddingLarge }

            TextSwitch {
                visible: config.isAccountValid
                text: qsTr("Accept self-signed certificates")
                description: qsTr("Only for your own server with a certificate you trust.")
                checked: config.ignoreSslErrors
                automaticCheck: false
                onClicked: config.ignoreSslErrors = !config.ignoreSslErrors
            }

            Item { width: 1; height: Theme.paddingLarge }

            ButtonLayout {
                Button {
                    text: config.isAccountValid ? qsTr("Change account") : qsTr("Sign in")
                    onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"))
                }
                Button {
                    visible: config.isAccountValid
                    text: qsTr("Sign out")
                    onClicked: signOutRemorse.execute(qsTr("Signing out"), function() { account.logout(); app.articles.reload(); app.feeds.reload() })
                }
            }

            SectionHeader {
                visible: config.isAccountValid
                text: qsTr("Backup of the settings")
            }
            Column {
                width: parent.width
                visible: config.isAccountValid
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Settings, favorite feeds and sites in the original, in the folder \"Sailfeed\" of your Nextcloud.")
                }

                Item { width: 1; height: Theme.paddingMedium }

                TextSwitch {
                    text: qsTr("Back up automatically")
                    description: app.autoBackup && !cloudBackup.linked
                                 ? qsTr("A few seconds after a change, and when Sailfeed goes to the background. Starts after the first backup or restore on this device.")
                                 : qsTr("A few seconds after a change, and when Sailfeed goes to the background.")
                    checked: app.autoBackup
                    automaticCheck: false
                    onClicked: app.autoBackup = !app.autoBackup
                }
                TextSwitch {
                    text: qsTr("Include text sizes")
                    description: qsTr("Off: each device keeps its own text sizes (e.g. phone and tablet).")
                    checked: app.backupTextSizes
                    automaticCheck: false
                    onClicked: app.backupTextSizes = !app.backupTextSizes
                }

                SectionHeader { text: qsTr("Last backup") }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    wrapMode: Text.Wrap
                    color: Theme.highlightColor
                    text: cloudBackup.busy && backupConnection.waiting ? qsTr("Saving …")
                        : (isNaN(cloudBackup.lastBackup) ? qsTr("Not backed up yet")
                           : Format.formatDate(cloudBackup.lastBackup, Formatter.DurationElapsed))
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    visible: cloudBackup.lastError !== "" && !cloudBackup.busy
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.errorColor
                    text: qsTr("Last attempt failed: %1").arg(cloudBackup.lastError)
                }

                Item { width: 1; height: Theme.paddingLarge }

                ButtonLayout {
                    Button {
                        text: qsTr("Back up now")
                        enabled: !cloudBackup.busy
                        onClicked: {
                            backupConnection.waiting = true
                            app.backupSettingsNow()
                        }
                    }
                    Button {
                        text: qsTr("Restore …")
                        enabled: !cloudBackup.busy
                        onClicked: pageStack.push(Qt.resolvedUrl("RestoreSettingsPage.qml"))
                    }
                }
            }
        }
        VerticalScrollDecorator { }
    }

    RemorsePopup { id: signOutRemorse }

    // Result of "Back up now" (not of automatic backups)
    Connections {
        id: backupConnection
        property bool waiting: false
        target: cloudBackup
        onBackupFinished: {
            if (!waiting)
                return
            waiting = false
            if (ok)
                hint.show(qsTr("Settings saved on your Nextcloud"))
            else
                app.showMessage(qsTr("Backup failed: %1").arg(message), true, qsTr("Retry"), function() {
                    backupConnection.waiting = true
                    app.backupSettingsNow()
                })
        }
    }

    Hint { id: hint }
}
