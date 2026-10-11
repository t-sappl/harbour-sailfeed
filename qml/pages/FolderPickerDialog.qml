// SPDX-License-Identifier: GPL-3.0-or-later
// Choose the folder a feed is moved to ("No folder" or one of the folders).
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0

Dialog {
    id: dialog

    property string feedTitle
    property int currentFolderId: 0
    property int folderId: currentFolderId

    readonly property var folders: [{ id: 0, name: qsTr("No folder") }].concat(FeedManager.folders())

    canAccept: folderId !== currentFolderId

    SilicaListView {
        anchors.fill: parent
        model: dialog.folders

        header: DialogHeader {
            title: qsTr("Move \"%1\"").arg(dialog.feedTitle)
            acceptText: qsTr("Move")
        }

        delegate: BackgroundItem {
            readonly property bool selected: modelData.id === dialog.folderId
            highlighted: down || selected
            onClicked: dialog.folderId = modelData.id

            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                text: modelData.name
                truncationMode: TruncationMode.Fade
                color: highlighted ? Theme.highlightColor : Theme.primaryColor
                font.italic: modelData.id === dialog.currentFolderId
            }
        }

        VerticalScrollDecorator { }
    }
}
