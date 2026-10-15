// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Article list: marking as read, unread only, the mark-all
// button, the feed list's sort order.
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
            PageHeader { title: qsTr("Article list") }

            // A choice of steps instead of a slider (author, 14 Oct): the
            // usual values at a glance, one tap; a value set before that is not
            // in the list shows the nearest step
            ComboBox {
                id: autoReadBox
                readonly property var steps: [0, 2, 3, 4, 5, 7, 10, 15]
                width: parent.width
                label: qsTr("Mark as read after")
                description: app.autoReadDelay === 0
                             ? qsTr("Articles are only marked as read when you open them or mark them yourself.")
                             : qsTr("Articles that are fully visible in the list for this time count as read.")
                currentIndex: {
                    var best = 0
                    for (var i = 1; i < steps.length; i++)
                        if (Math.abs(steps[i] - app.autoReadDelay) < Math.abs(steps[best] - app.autoReadDelay))
                            best = i
                    return best
                }
                // Written on tap only (see "Links in articles" in the reader settings)
                menu: ContextMenu {
                    Repeater {
                        model: autoReadBox.steps
                        MenuItem {
                            text: modelData === 0 ? qsTr("Off") : qsTr("%1 seconds").arg(modelData)
                            onClicked: app.autoReadDelay = modelData
                        }
                    }
                }
            }
            ComboBox {
                width: parent.width
                enabled: app.autoReadDelay > 0
                label: qsTr("Grey out read articles")
                description: app.autoReadMode === 1
                             ? qsTr("Articles seen for the time above are greyed out when you scroll on, not while you are still reading.")
                             : qsTr("Articles are greyed out as soon as the time above is over.")
                currentIndex: app.autoReadMode
                menu: ContextMenu {
                    MenuItem { text: qsTr("At once"); onClicked: app.autoReadMode = 0 }
                    MenuItem { text: qsTr("When scrolling on"); onClicked: app.autoReadMode = 1 }
                }
            }
            TextSwitch {
                text: qsTr("Show unread articles only")
                description: qsTr("Articles read in this session stay visible until the next refresh.")
                checked: app.unreadOnly
                automaticCheck: false
                onClicked: app.unreadOnly = !app.unreadOnly
            }
            TextSwitch {
                text: qsTr("Button \"Mark all as read\"")
                description: qsTr("Push it up to mark all as read, with one hand.")
                checked: app.showMarkAllButton
                automaticCheck: false
                onClicked: app.showMarkAllButton = !app.showMarkAllButton
            }

            SectionHeader { text: qsTr("Feed list") }
            FeedSortComboBox {
                description: app.feedSortMode === 2
                             ? qsTr("Feeds with the most articles in the last 30 days first. Counts articles synced to this device.")
                             : app.feedSortMode === 4
                               ? qsTr("Feeds with unread articles first. Inside both groups by activity: articles of the last 30 days, each bookmarked article counts three times.")
                               : qsTr("Can also be changed at the top of the feed list.")
            }
        }
        VerticalScrollDecorator { }
    }
}
