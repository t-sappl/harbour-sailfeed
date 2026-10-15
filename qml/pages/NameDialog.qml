// SPDX-License-Identifier: GPL-3.0-or-later
// Asks for a name (new folder, renaming a feed or folder). The caller reads
// "value" in its accepted handler.
import QtQuick 2.6
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string title
    property string acceptText: qsTr("Save")
    property string label: qsTr("Name")
    property string value
    property string initialValue

    canAccept: nameField.text.trim() !== "" && nameField.text.trim() !== initialValue
    onAccepted: value = nameField.text.trim()

    Column {
        width: parent.width

        DialogHeader {
            title: dialog.title
            acceptText: dialog.acceptText
        }

        TextField {
            id: nameField
            width: parent.width
            focus: true
            label: dialog.label
            text: dialog.initialValue
            // Without word prediction every letter is in "text" at once: with
            // it the word being typed only arrives when the field loses focus,
            // so "Add" and the swipe stayed blocked (device test 14 Oct)
            inputMethodHints: Qt.ImhNoPredictiveText
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.enabled: dialog.canAccept
            EnterKey.onClicked: dialog.accept()
        }
    }
}
