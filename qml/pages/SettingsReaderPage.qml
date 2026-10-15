// SPDX-License-Identifier: GPL-3.0-or-later
// Settings → Reader mode: look of the reader page (with a sample that stays
// at the top while the options below are changed), what "full article"
// opens, links in articles, sites in the original.
import QtQuick 2.6
import Sailfish.Silica 1.0
import harbour.sailfeed 1.0
import "../components"

Page {
    id: page

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            PageHeader { id: header; title: qsTr("Reader mode") }

            // Room for the sample (drawn above the column, see below)
            Item { id: samplePlace; width: parent.width; height: sample.height }

            Item { width: 1; height: Theme.paddingMedium }

            LabeledSlider {
                id: readerSizeSlider
                minimumValue: 14; maximumValue: 32; stepSize: 1
                value: app.readerFontPx
                label: qsTr("Text size")
                valueText: Math.round(sliderValue)
                onReleased: app.readerFontPx = Math.round(sliderValue)
            }
            // All combo boxes write on tap only: onCurrentIndexChanged can
            // fire while the page is built and change a setting by itself
            // (device test 13 Oct evening, "Links in articles")
            ComboBox {
                width: parent.width
                label: qsTr("Font")
                currentIndex: app.readerSerif ? 1 : 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("Sans serif"); onClicked: app.readerSerif = false }
                    MenuItem { text: qsTr("Serif"); onClicked: app.readerSerif = true }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Colors")
                description: qsTr("Also switchable while reading, with the half-filled circle at the bottom.")
                currentIndex: app.readerColors
                menu: ContextMenu {
                    MenuItem { text: qsTr("Like the ambience"); onClicked: app.readerColors = 0 }
                    MenuItem { text: qsTr("Light"); onClicked: app.readerColors = 1 }
                    MenuItem { text: qsTr("Dark"); onClicked: app.readerColors = 2 }
                    MenuItem { text: qsTr("Sepia"); onClicked: app.readerColors = 3 }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Line spacing")
                currentIndex: app.readerLineSpacing
                menu: ContextMenu {
                    MenuItem { text: qsTr("Compact"); onClicked: app.readerLineSpacing = 0 }
                    MenuItem { text: qsTr("Normal"); onClicked: app.readerLineSpacing = 1 }
                    MenuItem { text: qsTr("Wide"); onClicked: app.readerLineSpacing = 2 }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Margins")
                currentIndex: app.readerMargin
                menu: ContextMenu {
                    MenuItem { text: qsTr("Narrow"); onClicked: app.readerMargin = 0 }
                    MenuItem { text: qsTr("Normal"); onClicked: app.readerMargin = 1 }
                    MenuItem { text: qsTr("Wide"); onClicked: app.readerMargin = 2 }
                }
            }
            TextSwitch {
                text: qsTr("Justified text")
                description: qsTr("With hyphenation where the page states its language.")
                checked: app.readerJustify
                automaticCheck: false
                onClicked: app.readerJustify = !app.readerJustify
            }
            TextSwitch {
                text: qsTr("Show images")
                checked: app.readerImages
                automaticCheck: false
                onClicked: app.readerImages = !app.readerImages
            }

            SectionHeader { text: qsTr("Opening") }
            ComboBox {
                width: parent.width
                label: qsTr("Full article")
                description: qsTr("What the button below the preview and a swipe to the left open.")
                currentIndex: app.openMode
                menu: ContextMenu {
                    MenuItem { text: qsTr("Reader mode"); onClicked: app.openMode = 0 }
                    MenuItem { text: qsTr("Original website"); onClicked: app.openMode = 1 }
                    MenuItem { text: qsTr("External browser"); onClicked: app.openMode = 2 }
                }
            }
            ComboBox {
                width: parent.width
                label: qsTr("Links in articles")
                description: qsTr("A long press on a link opens it the other way.")
                currentIndex: app.linksInApp ? 0 : 1
                menu: ContextMenu {
                    MenuItem { text: qsTr("Reader mode in the app"); onClicked: app.linksInApp = true }
                    MenuItem { text: qsTr("External browser"); onClicked: app.linksInApp = false }
                }
            }
            SettingsEntry {
                title: qsTr("Sites in the original")
                summary: app.originalSiteList.length === 1
                         ? qsTr("1 site always opens without reader mode")
                         : qsTr("%1 sites always open without reader mode").arg(app.originalSiteList.length)
                target: Qt.resolvedUrl("ReaderSitesPage.qml")
            }
        }

        // Sample of the reader page, inside the flickable: scrolls up with the page
        // until it reaches the top, then stays there, so every change below shows at once. Size under
        // the finger, font, line spacing, margins, justification and colors;
        // CSS px -> screen px with the web view's measured pixel ratio; full
        // width like the reader page, whose margins are font size × 0.6/1/1.6.
        Rectangle {
            id: sample
            z: 1
            y: Math.max(samplePlace.y, flick.contentY)
            width: flick.width
            height: Math.min(readerSample.height + 2 * Theme.paddingLarge, page.height / 3)
            clip: true
            color: app.readerPalette.background
            // Swallows taps (they would reach the row hidden underneath); a
            // swipe starting here still scrolls the page (the flickable takes it)
            MouseArea { anchors.fill: parent }
            Text {
                id: readerSample
                readonly property real em: Math.round(readerSizeSlider.sliderValue) * app.readerPixelRatio
                x: em * [0.6, 1, 1.6][Math.max(0, Math.min(2, app.readerMargin))]
                y: Theme.paddingLarge
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: app.readerPalette.text
                font.pixelSize: Math.round(em)
                font.family: app.readerSerif ? "serif" : Theme.fontFamily
                lineHeightMode: Text.ProportionalHeight
                lineHeight: app.readerLineHeights[Math.max(0, Math.min(2, app.readerLineSpacing))]
                horizontalAlignment: app.readerJustify ? Text.AlignJustify : Text.AlignLeft
                text: qsTr("This is how the reader mode shows an article: calm text without ads, in the size, font and colors chosen here.")
            }
            // Thin line where the page scrolls under the sample
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: Math.max(1, Math.round(Theme.pixelRatio))
                color: Theme.rgba(Theme.highlightColor, 0.3)
                visible: flick.contentY > samplePlace.y
            }
        }

        VerticalScrollDecorator { }
    }
}
