// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Privacy: browser cookies for the reader mode, deleting cookies
// and website data (at the next start, before the web engine runs).
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
            PageHeader { title: qsTr("Privacy") }

            TextSwitch {
                text: qsTr("Use the in-app browser's cookies for the reader mode")
                description: qsTr("Choices on consent banners also apply to articles loaded in advance.")
                checked: Prefetch.shareBrowserCookies
                automaticCheck: false
                onClicked: Prefetch.shareBrowserCookies = !Prefetch.shareBrowserCookies
            }
            TextSwitch {
                text: qsTr("Delete cookies at every start")
                description: qsTr("Websites forget your choices and sign-ins; consent banners appear again.")
                checked: Prefetch.clearAtStart
                automaticCheck: false
                onClicked: Prefetch.clearAtStart = !Prefetch.clearAtStart
            }

            Item { width: 1; height: Theme.paddingLarge }

            ListActionItem {
                visible: !Prefetch.clearScheduled
                text: qsTr("Delete cookies and website data")
                description: Prefetch.shareBrowserCookies ? Prefetch.cookieInfo() : ""
                onClicked: clearRemorse.execute(qsTr("Deleting website data"), function() { Prefetch.scheduleClearWebData() })
            }
            ListActionItem {
                visible: Prefetch.clearScheduled
                text: qsTr("Keep them")
                description: qsTr("Cookies and website data are deleted the next time Sailfeed starts (the in-app browser must not be running).")
                onClicked: Prefetch.cancelClearWebData()
            }
        }
        VerticalScrollDecorator { }
    }

    RemorsePopup { id: clearRemorse }
}
