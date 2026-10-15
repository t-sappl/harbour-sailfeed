// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Article preview: skip it, its text size (with a sample line),
// text sizes per feed (preview and reader mode).
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
            PageHeader { title: qsTr("Article preview") }

            TextSwitch {
                text: qsTr("Skip the preview")
                description: qsTr("Tapping an article opens the full article right away.")
                checked: app.skipPreview
                automaticCheck: false
                onClicked: app.skipPreview = !app.skipPreview
            }

            Item { width: 1; height: Theme.paddingLarge }

            LabeledSlider {
                id: previewSizeSlider
                minimumValue: -2; maximumValue: 6; stepSize: 1
                value: app.previewFontStep
                label: qsTr("Text size")
                valueText: Math.round(100 * (1 + 0.12 * Math.round(sliderValue))) + " %"
                onReleased: app.previewFontStep = Math.round(sliderValue)
            }
            // Sample: the preview's body text at the size under the finger
            // (same formula as ArticlePage.bodyFontPx)
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Math.round(Theme.fontSizeSmall * (1 + 0.12 * Math.round(previewSizeSlider.sliderValue)))
                text: qsTr("This is how the text of the article preview looks at this size.")
            }

            Item { width: 1; height: Theme.paddingLarge }

            TextSwitch {
                text: qsTr("Remember text size per feed")
                description: qsTr("A size changed while reading (A− / A+) applies to that feed only, in the preview and in the reader mode.")
                checked: app.textSizePerFeed
                automaticCheck: false
                onClicked: app.textSizePerFeed = !app.textSizePerFeed
            }
            ListActionItem {
                visible: app.hasFeedTextSizes
                text: qsTr("Reset text sizes of all feeds")
                description: qsTr("All feeds use the sizes set here again.")
                onClicked: remorse.execute(qsTr("Resetting text sizes"), function() { app.resetFeedTextSizes() })
            }
        }
        VerticalScrollDecorator { }
    }

    RemorsePopup { id: remorse }
}
