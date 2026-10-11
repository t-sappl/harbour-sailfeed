// SPDX-License-Identifier: GPL-3.0-or-later
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
            PageHeader { title: qsTr("Settings") }

            SectionHeader { text: qsTr("Account") }
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
            TextSwitch {
                visible: config.isAccountValid
                text: qsTr("Accept self-signed certificates")
                description: qsTr("Only for your own server with a certificate you trust.")
                checked: config.ignoreSslErrors
                onCheckedChanged: config.ignoreSslErrors = checked
            }
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

            SectionHeader { text: qsTr("Reading") }
            Slider {
                width: parent.width
                minimumValue: 0; maximumValue: 15; stepSize: 1
                value: app.autoReadDelay
                valueText: value === 0 ? qsTr("Off") : qsTr("%1 s").arg(value)
                label: qsTr("Mark visible articles as read after")
                onReleased: app.autoReadDelay = value
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
                    MenuItem { text: qsTr("At once") }
                    MenuItem { text: qsTr("When scrolling on") }
                }
                onCurrentIndexChanged: app.autoReadMode = currentIndex
            }
            TextSwitch {
                text: qsTr("Button \"Mark all as read\"")
                description: qsTr("Round button at the bottom right of the article list: push it up to mark all as read, with one hand.")
                checked: app.showMarkAllButton
                onCheckedChanged: app.showMarkAllButton = checked
            }
            FeedSortComboBox {
                description: app.feedSortMode === 2
                             ? qsTr("Feeds with the most articles in the last 30 days first. Counts articles synced to this device.")
                             : app.feedSortMode === 4
                               ? qsTr("Feeds with unread articles first. Inside both groups by activity: articles of the last 30 days, each bookmarked article counts three times.")
                               : ""
            }
            TextSwitch {
                text: qsTr("Show unread articles only")
                description: qsTr("Articles read in this session stay visible until the next refresh.")
                checked: app.unreadOnly
                onCheckedChanged: app.unreadOnly = checked
            }
            Slider {
                width: parent.width
                minimumValue: -2; maximumValue: 6; stepSize: 1
                value: app.previewFontStep
                valueText: Math.round(100 * (1 + 0.12 * value)) + " %"
                label: qsTr("Text size in the article preview")
                onReleased: app.previewFontStep = value
            }

            TextSwitch {
                text: qsTr("Remember text size per feed")
                description: qsTr("Sizes changed in the preview or the reader mode apply to that feed only. The sliders set the size for all other feeds.")
                checked: app.textSizePerFeed
                onCheckedChanged: app.textSizePerFeed = checked
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: app.hasFeedTextSizes
                text: qsTr("Reset text sizes of all feeds")
                onClicked: feedSizesRemorse.execute(qsTr("Resetting text sizes"), function() { app.resetFeedTextSizes() })
            }

            SectionHeader { text: qsTr("Reader mode") }
            ComboBox {
                width: parent.width
                label: qsTr("Full article")
                description: qsTr("Reader mode shows only the article text and images, without ads.")
                currentIndex: app.openMode
                menu: ContextMenu {
                    MenuItem { text: qsTr("Reader mode") }
                    MenuItem { text: qsTr("Original website") }
                    MenuItem { text: qsTr("External browser") }
                }
                onCurrentIndexChanged: app.openMode = currentIndex
            }
            TextSwitch {
                text: qsTr("Skip the preview")
                description: qsTr("Tapping an article opens the full article right away.")
                checked: app.skipPreview
                onCheckedChanged: app.skipPreview = checked
            }
            Slider {
                width: parent.width
                minimumValue: 14; maximumValue: 32; stepSize: 1
                value: app.readerFontPx
                valueText: value
                label: qsTr("Text size")
                onReleased: app.readerFontPx = value
            }
            ComboBox {
                width: parent.width
                label: qsTr("Font")
                currentIndex: app.readerSerif ? 1 : 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("Sans serif") }
                    MenuItem { text: qsTr("Serif") }
                }
                onCurrentIndexChanged: app.readerSerif = currentIndex === 1
            }
            ComboBox {
                width: parent.width
                label: qsTr("Line spacing")
                currentIndex: app.readerLineSpacing
                menu: ContextMenu {
                    MenuItem { text: qsTr("Compact") }
                    MenuItem { text: qsTr("Normal") }
                    MenuItem { text: qsTr("Wide") }
                }
                onCurrentIndexChanged: app.readerLineSpacing = currentIndex
            }
            ComboBox {
                width: parent.width
                label: qsTr("Margins")
                currentIndex: app.readerMargin
                menu: ContextMenu {
                    MenuItem { text: qsTr("Narrow") }
                    MenuItem { text: qsTr("Normal") }
                    MenuItem { text: qsTr("Wide") }
                }
                onCurrentIndexChanged: app.readerMargin = currentIndex
            }
            ComboBox {
                width: parent.width
                label: qsTr("Colors")
                currentIndex: app.readerColors
                menu: ContextMenu {
                    MenuItem { text: qsTr("Like the ambience") }
                    MenuItem { text: qsTr("Light") }
                    MenuItem { text: qsTr("Dark") }
                    MenuItem { text: qsTr("Sepia") }
                }
                onCurrentIndexChanged: app.readerColors = currentIndex
            }
            TextSwitch {
                text: qsTr("Justified text")
                description: qsTr("With hyphenation where the page states its language.")
                checked: app.readerJustify
                onCheckedChanged: app.readerJustify = checked
            }
            TextSwitch {
                text: qsTr("Show images")
                description: qsTr("Off: text only, saves mobile data.")
                checked: app.readerImages
                onCheckedChanged: app.readerImages = checked
            }
            BackgroundItem {
                id: sitesItem
                width: parent.width
                height: Theme.itemSizeMedium
                onClicked: pageStack.push(Qt.resolvedUrl("ReaderSitesPage.qml"))
                Column {
                    anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin
                              right: sitesIcon.left; rightMargin: Theme.paddingMedium
                              verticalCenter: parent.verticalCenter }
                    Label {
                        width: parent.width
                        text: qsTr("Sites in the original")
                        color: sitesItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                    Label {
                        width: parent.width
                        text: qsTr("%n site(s) always open without reader mode", "", app.originalSiteList.length)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: sitesItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }
                Icon {
                    id: sitesIcon
                    anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                    source: "image://theme/icon-m-right"
                    color: sitesItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
            }
            SectionHeader { text: qsTr("Privacy") }
            TextSwitch {
                text: qsTr("Use the in-app browser's cookies for the reader mode")
                description: qsTr("So that choices on consent banners (e.g. \"with ads or subscription\") also apply to articles loaded in advance. Off: those downloads are made without cookies.")
                checked: Prefetch.shareBrowserCookies
                onCheckedChanged: Prefetch.shareBrowserCookies = checked
            }
            TextSwitch {
                text: qsTr("Delete cookies at every start")
                description: qsTr("Websites forget your choices and sign-ins; consent banners appear again.")
                checked: Prefetch.clearAtStart
                onCheckedChanged: Prefetch.clearAtStart = checked
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !Prefetch.clearScheduled
                text: qsTr("Delete cookies and website data")
                onClicked: clearRemorse.execute(qsTr("Deleting website data"), function() { Prefetch.scheduleClearWebData() })
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: Prefetch.clearScheduled
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
                text: qsTr("Cookies and website data are deleted the next time Sailfeed starts (the in-app browser must not be running).")
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: Prefetch.clearScheduled
                text: qsTr("Keep them")
                onClicked: Prefetch.cancelClearWebData()
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: Prefetch.shareBrowserCookies && Prefetch.clearScheduled !== undefined ? Prefetch.cookieInfo() : ""
                visible: text !== ""
            }

            // --- Settings backup on the Nextcloud (package 5) ---------------
            SectionHeader {
                visible: config.isAccountValid
                text: qsTr("Backup on your Nextcloud")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: config.isAccountValid
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Settings, favorite feeds and sites in the original are saved in the folder \"Sailfeed\" of your Nextcloud, with the account you signed in with. Restore them on another device or after a new installation.")
            }
            TextSwitch {
                visible: config.isAccountValid
                text: qsTr("Include text sizes")
                description: qsTr("Off: each device keeps its own text sizes (e.g. phone and tablet).")
                checked: app.backupTextSizes
                onCheckedChanged: app.backupTextSizes = checked
            }
            TextSwitch {
                visible: config.isAccountValid
                text: qsTr("Back up automatically")
                description: qsTr("A few seconds after a change, and when Sailfeed goes to the background.")
                checked: app.autoBackup
                onCheckedChanged: app.autoBackup = checked
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: config.isAccountValid && app.autoBackup && !cloudBackup.linked
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Starts after the first backup or restore on this device.")
            }
            DetailItem {
                visible: config.isAccountValid
                label: qsTr("Last backup")
                value: cloudBackup.busy && backupConnection.waiting ? qsTr("Saving …")
                     : (isNaN(cloudBackup.lastBackup) ? qsTr("never")
                        : Format.formatDate(cloudBackup.lastBackup, Formatter.DurationElapsed))
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: config.isAccountValid && cloudBackup.lastError !== "" && !cloudBackup.busy
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.errorColor
                text: qsTr("Last attempt failed: %1").arg(cloudBackup.lastError)
            }
            ButtonLayout {
                visible: config.isAccountValid
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

            // At the very bottom: entry to the "About" page
            Item { width: 1; height: Theme.paddingLarge }

            BackgroundItem {
                id: aboutItem
                width: parent.width
                height: Theme.itemSizeSmall
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Theme.horizontalPageMargin
                    text: qsTr("About Sailfeed")
                    color: aboutItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Icon {
                    anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                    source: "image://theme/icon-m-right"
                    color: aboutItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
            }
        }
        VerticalScrollDecorator { }
    }

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

    RemorsePopup { id: signOutRemorse }
    RemorsePopup { id: feedSizesRemorse }
    RemorsePopup { id: clearRemorse }
}
