// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.6
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingSmall

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: localstorage.totalUnread
            font.pixelSize: Theme.fontSizeHuge
            color: Theme.highlightColor
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("unread")
            color: Theme.secondaryColor
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: synchronizer.inOperation ? qsTr("Updating …")
                  : (synchronizer.error ? qsTr("Refresh failed") : "Sailfeed")
            color: synchronizer.error && !synchronizer.inOperation ? Theme.errorColor : Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
        }
    }

    CoverActionList {
        enabled: config.isAccountValid
        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: if (!synchronizer.inOperation) synchronizer.sync()
        }
    }
}
