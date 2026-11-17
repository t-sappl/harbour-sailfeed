// SPDX-License-Identifier: GPL-3.0-or-later
// Nextcloud account. Recommended: sign in with the browser (Nextcloud Login
// Flow v2 – the server creates the app password and hands it over).
// Alternative: user name and an app password created in Nextcloud under
// Settings > Security > Devices & sessions.
import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    property bool manual: false

    readonly property bool canSignIn: server.text.length > 0 && user.text.length > 0
                                      && password.text.length > 0 && !account.busy

    function signIn() {
        if (canSignIn)
            account.login(server.text, user.text, password.text)
    }

    function signInWithBrowser() {
        if (server.text.length > 0 && !account.busy)
            account.loginWithBrowser(server.text)
    }

    Connections {
        target: account
        onLoginSucceeded: pageStack.pop()
        onLoginUrl: Qt.openUrlExternally(url)
    }

    // Leaving the page while waiting for the browser stops the sign-in.
    onStatusChanged: if (status === PageStatus.Deactivating && account.browserLoginActive) account.cancelBrowserLogin()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Nextcloud account") }

            // First start: what Sailfeed needs (15 Oct; the details and links
            // to Nextcloud News are at the end of the page)
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Sailfeed shows the feeds of the News app on your Nextcloud. Enter your server's address and sign in.")
            }
            Item { width: 1; height: Theme.paddingLarge }

            TextField {
                id: server
                width: parent.width
                label: qsTr("Server address")
                placeholderText: "https://example.your-storageshare.de"
                text: config.serverUrl
                enabled: !account.busy
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: page.manual ? "image://theme/icon-m-enter-next" : "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.manual ? user.focus = true : page.signInWithBrowser()
            }

            TextSwitch {
                text: qsTr("Accept self-signed certificates")
                description: qsTr("Only for your own server with a certificate you trust. The connection is still encrypted, but the server's identity is not checked.")
                checked: config.ignoreSslErrors
                enabled: !account.busy
                onCheckedChanged: config.ignoreSslErrors = checked
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: account.errorString !== "" && !account.busy
                text: account.errorString
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
            }

            // --- Sign in with the browser (recommended) ---------------------
            Item { width: 1; height: Theme.paddingLarge }

            ActionPill {
                kind: "primary"
                x: Theme.horizontalPageMargin
                visible: !page.manual && !account.browserLoginActive
                text: qsTr("Sign in with the browser")
                // Always enabled (blind test 11 Nov: greyed out before the
                // address was typed, it looked broken); without an address it
                // leads to the address field
                enabled: !account.busy
                onClicked: {
                    if (server.text.trim().length === 0)
                        server.forceActiveFocus()
                    else
                        page.signInWithBrowser()
                }
            }

            Column {
                width: parent.width
                visible: account.browserLoginActive
                spacing: Theme.paddingSmall
                // As Sailcast: small busy indicator and the text beside it
                Row {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    spacing: Theme.paddingMedium
                    BusyIndicator {
                        id: browserBusy
                        anchors.verticalCenter: parent.verticalCenter
                        size: BusyIndicatorSize.Small
                        running: account.browserLoginActive
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - browserBusy.width - parent.spacing
                        wrapMode: Text.Wrap
                        color: Theme.secondaryHighlightColor
                        font.pixelSize: Theme.fontSizeSmall
                        text: qsTr("Sign in to your Nextcloud in the browser and grant access. Sailfeed continues by itself.")
                    }
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    visible: account.browserLoginSecondsLeft > 0
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Waiting another %1 min").arg(Math.ceil(account.browserLoginSecondsLeft / 60))
                }
                ActionPill {
                    kind: "secondary"
                    x: Theme.horizontalPageMargin
                    text: qsTr("Cancel")
                    onClicked: account.cancelBrowserLogin()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !page.manual && !account.browserLoginActive
                topPadding: Theme.paddingMedium
                text: qsTr("Opens your Nextcloud's sign-in page in the browser. After you grant access there, Sailfeed receives its own app password.")
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            Item { width: 1; height: Theme.paddingLarge }

            // --- Alternative: app password ----------------------------------
            // A side action like on Sailcast's pages (blind test: the
            // underlined text before read as body text)
            ActionPill {
                kind: "secondary"
                x: Theme.horizontalPageMargin
                visible: !account.browserLoginActive
                text: page.manual ? qsTr("Sign in with the browser instead") : qsTr("Sign in with an app password instead")
                onClicked: page.manual = !page.manual
            }

            Column {
                width: parent.width
                visible: page.manual

                TextField {
                    id: user
                    width: parent.width
                    label: qsTr("User name")
                    placeholderText: label
                    text: config.username
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    EnterKey.iconSource: "image://theme/icon-m-enter-next"
                    EnterKey.onClicked: password.focus = true
                }
                PasswordField {
                    id: password
                    width: parent.width
                    label: qsTr("App password")
                    placeholderText: label
                    EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                    EnterKey.enabled: page.canSignIn
                    EnterKey.onClicked: page.signIn()
                }

                Item { width: 1; height: Theme.paddingLarge }

                ActionPill {
                    kind: "primary"
                    x: Theme.horizontalPageMargin
                    text: qsTr("Sign in")
                    enabled: page.canSignIn
                    onClicked: page.signIn()
                }

                Item { width: 1; height: Theme.paddingMedium }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    text: qsTr("Use an app password, created in Nextcloud under Settings › Security › Devices & sessions.")
                    wrapMode: Text.Wrap
                    color: Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: account.busy && !account.browserLoginActive
                visible: running
            }

            // --- No Nextcloud News yet (same facts as the store text) -------
            SectionHeader {
                visible: !account.browserLoginActive
                text: qsTr("No Nextcloud News yet?")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !account.browserLoginActive
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Sailfeed is only the reader and fetches no feeds itself. As an admin, enable the app \"News\" in your Nextcloud under Apps (section Multimedia); on a hosted Nextcloud the same, if the provider allows apps. News needs Nextcloud's background jobs via cron to update the feeds.")
            }
            Item { width: 1; height: Theme.paddingMedium; visible: !account.browserLoginActive }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !account.browserLoginActive
                wrapMode: Text.Wrap
                textFormat: Text.StyledText
                linkColor: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: "<a href=\"https://apps.nextcloud.com/apps/news\">" + qsTr("News in the Nextcloud app store") + "</a><br><br>"
                      + "<a href=\"https://nextcloud.github.io/news/install/\">" + qsTr("Installation guide (English)") + "</a>"
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        VerticalScrollDecorator { }
    }
}
