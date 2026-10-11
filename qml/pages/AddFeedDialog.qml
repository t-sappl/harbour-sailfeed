// SPDX-License-Identifier: GPL-3.0-or-later
// Add a feed on the Nextcloud server: address (taken from the clipboard if
// it holds one) and folder.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0

Dialog {
    id: dialog

    // Folder preselected in the combo box (0 = no folder)
    property int preselectedFolderId: 0
    readonly property var folders: FeedManager.folders()
    readonly property string feedUrl: FeedManager.normalizeUrl(urlField.text)
    readonly property int folderId: folderBox.currentIndex <= 0 ? 0 : folders[folderBox.currentIndex - 1].id

    canAccept: feedUrl !== "" && !FeedManager.busy
    onAccepted: FeedManager.addFeed(feedUrl, folderId)

    Component.onCompleted: {
        var clip = Clipboard.hasText ? Clipboard.text.trim() : ""
        if (clip.length < 1000 && /^https?:\/\/\S+$/i.test(clip))
            urlField.text = clip
        for (var i = 0; i < folders.length; i++) {
            if (folders[i].id === preselectedFolderId)
                folderBox.currentIndex = i + 1
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            DialogHeader {
                title: qsTr("Add feed")
                acceptText: qsTr("Add")
            }

            TextField {
                id: urlField
                width: parent.width
                focus: text === ""
                label: qsTr("Feed address")
                placeholderText: "https://example.org/feed"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("The address of the RSS or Atom feed. The address of a website often works too, if it announces its feed.")
            }

            ComboBox {
                id: folderBox
                width: parent.width
                label: qsTr("Folder")
                menu: ContextMenu {
                    MenuItem { text: qsTr("No folder") }
                    Repeater {
                        model: dialog.folders
                        MenuItem { text: modelData.name }
                    }
                }
            }
        }
        VerticalScrollDecorator { }
    }
}
