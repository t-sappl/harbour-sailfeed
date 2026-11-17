// SPDX-License-Identifier: GPL-3.0-or-later
// Deleting a folder that holds feeds (as Sailcast's folder dialog, 758): one
// entry "Delete folder …" in the folder menu; this dialog asks whether its
// feeds are unsubscribed too (default: no, they move to "Other feeds").
// The countdown follows on the feed list (FeedsPage.deleteFolderWith).
import QtQuick 2.6
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string folderName: ""
    property int feedCount: 0
    property alias withFeeds: feedSwitch.checked

    Column {
        width: parent.width
        spacing: Theme.paddingMedium

        DialogHeader {
            title: qsTr("Delete folder %1?").arg(dialog.folderName)
            acceptText: qsTr("Delete")
        }

        TextSwitch {
            id: feedSwitch
            visible: dialog.feedCount > 0
            text: dialog.feedCount === 1 ? qsTr("Also unsubscribe its feed")
                                         : qsTr("Also unsubscribe its %1 feeds").arg(dialog.feedCount)
            description: checked ? qsTr("The feeds and their articles are removed.")
                                 : qsTr("The feeds are kept and move to \"Other feeds\".")
        }
    }
}
