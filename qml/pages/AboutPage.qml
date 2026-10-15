// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: aboutPage

    // Empty entries are not shown.
    readonly property string licenseName: "GPL 3.0 or later"
    readonly property string sourceCodeUrl: "https://github.com/t-sappl/harbour-sailfeed/"
    readonly property string privacyPolicyUrl: "https://github.com/t-sappl/harbour-sailfeed/blob/main/PRIVACY.md"
    readonly property string issuesUrl: "https://github.com/t-sappl/harbour-sailfeed/issues"
    // Not translated (name)
    readonly property string copyright: "© 2026 Thomas Sappl"

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge * 2

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("About Sailfeed") }

            // App icon (standard path of installed Sailfish apps); if it is
            // missing, the space simply collapses.
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "/usr/share/icons/hicolor/172x172/apps/harbour-sailfeed.png"
                width: Theme.iconSizeExtraLarge
                height: width
                sourceSize.width: width
                sourceSize.height: height
                visible: status === Image.Ready

                // Hidden: press and hold shows all first-run tips again, like
                // Sailwave (15 Oct: replaces "Show tips again" in the settings)
                MouseArea {
                    anchors.fill: parent
                    onPressAndHold: {
                        app.resetHints()
                        app.showMessage(qsTr("The tips appear again where they belong"))
                    }
                }
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Sailfeed"
                font.pixelSize: Theme.fontSizeExtraLarge
                color: Theme.highlightColor
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Version %1").arg(Qt.application.version)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: aboutPage.copyright
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                text: qsTr("Feed reader for Nextcloud News on Sailfish OS, with a clean reader mode.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("A personal project: developed for my own use and the features I need most. "
                           + "Shared on OpenRepos and GitHub; the source code is free for anyone to build and adapt.")
            }

            DetailItem {
                label: qsTr("License")
                value: aboutPage.licenseName
            }

            SectionHeader { text: qsTr("Data and services") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Folders, feeds and articles come from your own Nextcloud server (News app). "
                           + "Images and the pages for the reader mode are loaded directly from the websites of the articles. "
                           + "The settings backup is stored in the folder \"Sailfeed\" of your Nextcloud files. "
                           + "No tracking, no ads; no data reaches the developer.")
            }

            SectionHeader { text: qsTr("Components") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                linkColor: Theme.highlightColor
                textFormat: Text.StyledText
                text: qsTr("%1 by Matthias Fehring (LGPL-3.0-or-later): Nextcloud News API and local storage. "
                           + "%2 (Apache-2.0): reader mode. "
                           + "Many thanks to %3, the original Nextcloud News client for Sailfish OS.")
                      .arg("<a href=\"https://github.com/Huessenbergnetz/libfuoten\">libfuoten</a>")
                      .arg("<a href=\"https://github.com/mozilla/readability\">Mozilla Readability</a>")
                      .arg("<a href=\"https://github.com/Huessenbergnetz/Fuoten\">Fuoten</a>")
                onLinkActivated: Qt.openUrlExternally(link)
            }

            SectionHeader { text: qsTr("Development") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                // Same wording in README and on the OpenRepos page (author, 15 Oct)
                text: qsTr("Developed with the assistance of an AI model (Claude by Anthropic). "
                           + "The developer specifies all features, makes all design and technical decisions, tests every change on a Jolla Phone (2026) and uses the app every day.")
            }

            Item { width: 1; height: Theme.paddingMedium }

            ButtonLayout {
                Button {
                    text: qsTr("Source code")
                    onClicked: Qt.openUrlExternally(aboutPage.sourceCodeUrl)
                }
                Button {
                    text: qsTr("Privacy policy")
                    onClicked: Qt.openUrlExternally(aboutPage.privacyPolicyUrl)
                }
                Button {
                    text: qsTr("Report a problem")
                    onClicked: Qt.openUrlExternally(aboutPage.issuesUrl)
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
