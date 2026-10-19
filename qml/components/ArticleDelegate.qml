// SPDX-License-Identifier: GPL-3.0-or-later
// One row: title (up to 3 lines), "feed / age", excerpt (3 lines), square
// thumbnail on the right. Unread articles have a dot in the left margin, read
// ones are greyed out.
// Compact (setting "List view", 1.2): title up to 2 lines and "feed / age",
// no excerpt, a smaller thumbnail as high as the text, less padding – about
// twice as many articles per page.
import QtQuick 2.6
import Sailfish.Silica 1.0

ListItem {
    id: root
    // General setting, or the feed's own (feed settings)
    property bool compact: app.listViewFor(model.article ? model.article.feedId : -1) === 1
    readonly property real vPad: compact ? Theme.paddingMedium : Theme.paddingLarge
    contentHeight: Math.max(textColumn.height, hasThumb ? thumbnail.height : 0)
                   + 2 * vPad

    readonly property bool hasThumb: model.thumb !== "" && thumbnail.status !== Image.Error
    // Read state straight from the libfuoten Article object (NOTIFY unreadChanged),
    // so the row greys out as soon as the article is marked, independent of the
    // model's dataChanged propagation.
    readonly property bool isUnread: model.article ? model.article.unread : model.unread
    readonly property bool isStarred: model.article ? model.article.starred : model.starred
    readonly property real readOpacity: 0.5

    // Unread marker in the left margin, centered on the first title line
    Rectangle {
        id: unreadDot
        width: Math.round(Theme.paddingSmall * 1.6)
        height: width
        radius: width / 2
        x: Math.round((Theme.horizontalPageMargin - width) / 2)
        y: root.vPad + Math.round(titleLabel.font.pixelSize * 0.65 - height / 2)
        color: Theme.highlightColor
        opacity: root.isUnread ? 1 : 0
        Behavior on opacity { FadeAnimator { duration: 200 } }
    }

    Column {
        id: textColumn
        anchors {
            left: parent.left; leftMargin: Theme.horizontalPageMargin
            right: hasThumb ? thumbnail.left : parent.right
            rightMargin: hasThumb ? Theme.paddingLarge : Theme.horizontalPageMargin
            top: parent.top; topMargin: root.vPad
        }
        spacing: Theme.paddingSmall
        opacity: root.isUnread ? 1.0 : root.readOpacity
        Behavior on opacity { FadeAnimator { duration: 200 } }

        // Sizes follow the Silica scale: title small (bold), meta and excerpt
        // extra small, so text and the thumbnail have matching weight.
        Label {
            id: titleLabel
            width: parent.width
            text: model.title
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
            maximumLineCount: root.compact ? 2 : 3
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            textFormat: Text.PlainText
            color: root.highlighted ? Theme.highlightColor
                                    : Theme.primaryColor
        }
        Label {
            width: parent.width
            text: (root.isStarred ? "★ " : "") + model.feedTitle + " / " + model.age
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
            truncationMode: TruncationMode.Fade
        }
        Label {
            width: parent.width
            visible: !root.compact && text !== ""
            text: root.compact ? "" : model.excerpt
            font.pixelSize: Theme.fontSizeExtraSmall
            maximumLineCount: 3
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            textFormat: Text.PlainText
            color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }

    Image {
        id: thumbnail
        visible: hasThumb
        anchors {
            right: parent.right; rightMargin: Theme.horizontalPageMargin
            top: parent.top; topMargin: root.vPad
        }
        // Square, a fifth of the portrait width (same size in landscape and on
        // tablets); compact: about as high as two title lines and the source
        width: root.compact ? Math.round(Math.min(root.width, Screen.width) / 8)
                            : Math.round(Math.min(root.width, Screen.width) / 5)
        height: width
        sourceSize.width: thumbnail.width
        sourceSize.height: thumbnail.height
        source: model.thumb
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        clip: true
        opacity: root.isUnread ? 1.0 : root.readOpacity

        // Placeholder while loading
        Rectangle {
            anchors.fill: parent
            visible: thumbnail.status !== Image.Ready
            color: Theme.rgba(Theme.highlightBackgroundColor, Theme.highlightBackgroundOpacity)
        }
    }
}
