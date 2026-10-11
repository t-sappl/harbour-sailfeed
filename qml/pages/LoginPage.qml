// SPDX-License-Identifier: GPL-3.0-or-later
// Nextcloud account. Recommended: sign in with the browser (Nextcloud Login
// Flow v2 – the server creates the app password and hands it over).
// Alternative: user name and an app password created in Nextcloud under
// Settings > Security > Devices & sessions.
import QtQuick 2.6
import Sailfish.Silica 1.0

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

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !page.manual && !account.browserLoginActive
                text: qsTr("Sign in with the browser")
                enabled: server.text.length > 0 && !account.busy
                onClicked: page.signInWithBrowser()
            }

            Column {
                width: parent.width
                visible: account.browserLoginActive
                spacing: Theme.paddingMedium
                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    size: BusyIndicatorSize.Medium
                    running: account.browserLoginActive
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.highlightColor
                    text: qsTr("Sign in to your Nextcloud in the browser and grant access. Sailfeed continues by itself.")
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    visible: account.browserLoginSecondsLeft > 0
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Waiting another %1 min").arg(Math.ceil(account.browserLoginSecondsLeft / 60))
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
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
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            Item { width: 1; height: Theme.paddingLarge }

            // --- Alternative: app password ----------------------------------
            BackgroundItem {
                width: parent.width
                visible: !account.browserLoginActive
                onClicked: page.manual = !page.manual
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    horizontalAlignment: Text.AlignHCenter
                    text: page.manual ? qsTr("Sign in with the browser instead") : qsTr("Sign in with an app password instead")
                    color: parent.highlighted ? Theme.highlightColor : Theme.secondaryHighlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }
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

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
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
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: account.busy && !account.browserLoginActive
                visible: running
            }
        }
        VerticalScrollDecorator { }
    }
}
