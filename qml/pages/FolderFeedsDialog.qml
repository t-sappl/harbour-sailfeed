// SPDX-License-Identifier: GPL-3.0-or-later
// Feeds of a folder (1.4): second step of "Add folder" (which feeds go into
// the new folder) and "Choose feeds …" in a folder's menu (existing folder:
// its feeds are ticked; unticking moves a feed out to "no folder").
// All feeds with the folder they are in: feeds without a folder first, then
// grouped by folder, alphabetically (like the folder pages of Sailcast and
// Sailwave). Creating the folder without choosing feeds is fine, too.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Dialog {
    id: dialog

    // Name typed in the name dialog (replaced by this page) and the feed list
    // that creates the folder and moves the feeds (set by FeedsPage.addFolder)
    property string newName
    property var feedsPage
    // Existing folder ("Choose feeds …"); 0 for a new one
    property int folderId: 0
    property string existingName
    readonly property string folderName: folderId > 0 ? existingName : newName
    property var initial: ({})

    property var chosen: ({})
    property int chosenCount: 0
    // Chips "All" / "None" (as Sailcast's folder page)
    function setAll(on) {
        var c = {}, n = 0
        if (on) {
            for (var i = 0; i < feedsModel.count; i++) {
                c[feedsModel.get(i).feedId] = true
                n++
            }
        }
        chosen = c
        chosenCount = n
    }
    function toggle(id) {
        var c = chosen
        if (c[id]) { delete c[id]; chosenCount-- } else { c[id] = true; chosenCount++ }
        chosen = c
    }

    acceptDestination: feedsPage
    acceptDestinationAction: PageStackAction.Pop
    onAccepted: {
        var ids = []
        for (var k in chosen)
            if (!initial[k])
                ids.push(Number(k))
        if (!feedsPage)
            return
        if (folderId <= 0) {
            feedsPage.createFolderWithFeeds(folderName, ids)
            return
        }
        var moves = []
        for (var a = 0; a < ids.length; a++)
            moves.push({ feed: ids[a], folder: folderId })
        for (var r in initial)
            if (!chosen[r])
                moves.push({ feed: Number(r), folder: 0 })
        feedsPage.runMoves(moves, folderId)
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        // A ListModel (not the plain list): sections need roles
        model: ListModel { id: feedsModel }
        Component.onCompleted: {
            var l = app.feeds.feedsByFolder()
            var c = {}, n = 0
            for (var i = 0; i < l.length; i++) {
                // "id" is no good role name in a ListModel
                feedsModel.append({ feedId: l[i].id, title: l[i].title, folderName: l[i].folderName,
                                    favicon: l[i].favicon })
                if (dialog.folderId > 0 && l[i].folderId === dialog.folderId) {
                    c[l[i].id] = true
                    n++
                }
            }
            dialog.initial = c
            var copy = {}
            for (var k in c)
                copy[k] = true
            dialog.chosen = copy
            dialog.chosenCount = n
        }

        header: Column {
            width: list.width
            DialogHeader {
                title: dialog.folderName
                acceptText: dialog.folderId > 0 ? qsTr("Save") : qsTr("Create")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: dialog.folderId > 0 ? qsTr("Switched-on feeds are in this folder. Switch a feed off to move it out (to no folder).")
                                          : qsTr("Choose the feeds to move into the new folder. You can also create it empty.")
            }
            Item { width: 1; height: Theme.paddingMedium }
            Flow {
                visible: feedsModel.count > 1
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: Theme.paddingSmall
                Chip {
                    text: qsTr("All")
                    on: dialog.chosenCount === feedsModel.count
                    onClicked: dialog.setAll(true)
                }
                Chip {
                    text: qsTr("None")
                    on: dialog.chosenCount === 0
                    onClicked: dialog.setAll(false)
                }
            }
            Item { width: 1; height: Theme.paddingMedium }
        }

        section {
            property: "folderName"
            criteria: ViewSection.FullString
            delegate: SectionHeader {
                text: section !== "" ? section : qsTr("No folder")
            }
        }

        // A switch per feed, as on Sailcast's folder page (758; the tick
        // on the right was easy to miss)
        delegate: TextSwitch {
            width: list.width
            text: model.title
            checked: dialog.chosen[model.feedId] === true
            automaticCheck: false
            onClicked: dialog.toggle(model.feedId)
        }

        VerticalScrollDecorator { }
    }
}
