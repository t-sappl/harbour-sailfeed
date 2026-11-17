// SPDX-License-Identifier: GPL-3.0-or-later
// "Sorted: newest first ›" – the sort order on the list itself, small and
// right-aligned, a tap opens the choice as a menu (as Sailcast's sort line,
// 758). In a ListView header put it into a ListHeaderColumn and give that
// "menuOpen: sortLine.menuOpen", so the menu opens downwards.
//
// options: [{ name: "Newest first", short: "newest first" }, ...], the index
// in it is the value; "chosen(index)" is emitted on a tap on an entry only.
import QtQuick 2.6
import Sailfish.Silica 1.0

ListItem {
    id: root

    property var options: []
    property int currentIndex: 0
    signal chosen(int index)

    width: parent ? parent.width : Screen.width
    contentHeight: Theme.itemSizeExtraSmall
    onClicked: openMenu()

    Label {
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: Theme.fontSizeExtraSmall
        color: root.highlighted ? Theme.highlightColor : Theme.secondaryHighlightColor
        text: qsTr("Sorted: %1").arg(root.options.length > root.currentIndex
                                      ? root.options[root.currentIndex].short : "") + " ›"
    }

    menu: ContextMenu {
        Repeater {
            model: root.options
            MenuItem {
                text: modelData.name
                font.bold: index === root.currentIndex
                onClicked: root.chosen(index)
            }
        }
    }
}
