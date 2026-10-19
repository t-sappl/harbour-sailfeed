// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

// Pure helper functions for article HTML. Deliberately QtCore-only (no
// QTextDocument) so they are cheap enough to run for every list row and can
// be unit-tested without a GUI.
namespace HtmlUtils {

// Picks a thumbnail URL for an article. Order (see DESIGN.md):
//  1. enclosureLink if enclosureMime starts with "image/"
//  2. first <img src> in the body that is not a tracking pixel
//  3. mediaThumbnail
// Relative and protocol-relative URLs are resolved against articleUrl.
// Returns an empty QUrl if nothing suitable is found.
QUrl thumbnail(const QUrl &enclosureLink, const QString &enclosureMime,
               const QString &body, const QUrl &mediaThumbnail,
               const QUrl &articleUrl);

// Strips tags, decodes entities and collapses whitespace. The result is
// cut at maxChars (on a word boundary if possible) and gets an ellipsis.
QString plainExcerpt(const QString &html, int maxChars = 220);

// Whether the feed sent something to show in the preview (1.2): text of at
// least minTextChars (shorter is a "Read more …" link or nothing), an image
// in the body (not a tracking pixel) or an enclosure (image, podcast, video).
// Articles without it open the full article at once.
bool hasContent(const QString &body, const QUrl &enclosureLink, int minTextChars = 25);

// Decodes the HTML entities that commonly appear in feeds.
QString decodeEntities(const QString &text);

// Prepares an article body for a QML Text item with RichText format:
// removes script/style/iframe/object/form blocks and all <img> tags (the
// lead image is shown separately), keeps links and basic formatting.
QString cleanBody(const QString &html);

// Options for readerHtml(); colors are CSS color strings.
struct ReaderStyle {
    QString background = QStringLiteral("#111418");
    QString text = QStringLiteral("#ffffff");
    QString secondary = QStringLiteral("rgba(255,255,255,0.6)");
    QString link = QStringLiteral("#5ec8ff");
    int fontPx = 20;
    bool serif = false;         // serif font instead of the sans system font
    double lineHeight = 1.55;   // 1.2 … 2.2
    int margin = 1;             // side margins: 0 narrow, 1 normal, 2 wide
    bool justify = false;       // justified text with hyphenation
    bool images = true;         // false: hide images and videos
};

// Site key of a URL for the "always open in the original" list: host in
// lower case without a leading "www." ("" for invalid URLs).
QString siteKey(const QString &url);
// True if url belongs to one of the sites (a site matches its subdomains).
bool isListedSite(const QString &url, const QStringList &sites);

// Builds the reader-mode page from the JSON string the Readability
// extraction script returns ({title, byline, siteName, content, lang, dir}).
// Returns an empty string if the JSON is invalid or has no content.
QString readerHtml(const QString &articleJson, const ReaderStyle &style);

// Escapes text for use in HTML element content and attribute values.
QString escapeHtml(const QString &text);

} // namespace HtmlUtils
