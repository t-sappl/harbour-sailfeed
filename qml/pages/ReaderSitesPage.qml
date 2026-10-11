// SPDX-License-Identifier: GPL-3.0-or-later
// Sites that always open in the original instead of the reader mode (for
// pages Readability does not handle well). Added from the web view's pulley
// or here; removed by long press.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0

Page {
    id: page

    SilicaListView {
        id: list
        anchors.fill: parent
        model: app.originalSiteList

        header: PageHeader {
            title: qsTr("Sites in the original")
            description: qsTr("Open without reader mode")
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeSmall
            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Remove")
                    onClicked: {
                        var site = modelData
                        var window = app        // the menu's context is gone after the countdown
                        item.remorseAction(qsTr("Removing"), function() { window.setOriginalSite(site, false) })
                    }
                }
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                text: modelData
                truncationMode: TruncationMode.Fade
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Add site")
                onClicked: {
                    var d = pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                                           { title: qsTr("Add site"), acceptText: qsTr("Add"),
                                             label: qsTr("Website address, e.g. example.org") })
                    d.accepted.connect(function() { app.setOriginalSite(d.value, true) })
                }
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No sites")
            hintText: qsTr("In the web view: pull down, \"Always open this site in the original\"")
        }

        VerticalScrollDecorator { }
    }
}
