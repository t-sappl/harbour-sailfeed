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

// Thumbnail for the article list (1.4): small images first, to save data
// (the preview keeps thumbnail()'s big image). Order: media:thumbnail of the
// feed, an image enclosure, then the first usable <img> of the body – from
// its srcset the smallest candidate at least minWidth pixels wide (else the
// widest), otherwise its src.
QUrl listThumbnail(const QUrl &enclosureLink, const QString &enclosureMime,
                   const QString &body, const QUrl &mediaThumbnail,
                   const QUrl &articleUrl, int minWidth = 320);

// Everything the article list and the search need from an article's body,
// computed once (ArticleIndex, 1.4): the plain text is made once for the
// excerpt, hasContent and the search text.
struct ArticleExtra {
    QString excerpt;            // plainExcerpt(body, 220)
    QString thumb;              // listThumbnail()
    bool hasContent = true;     // hasContent()
    QString search;             // case-folded "title \n plain text"
};
ArticleExtra articleExtra(const QString &title, const QString &body, const QUrl &enclosureLink,
                          const QString &enclosureMime, const QUrl &mediaThumbnail,
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

// Lean copy of a downloaded page for the reader mode's extraction (1.4,
// speed): the web view only has to build the DOM for Readability, so
// nothing is left that loads or runs –
//  - <script> blocks are removed (JSON-LD data stays: Readability reads
//    title and author from it, it does not run), so are <link> (style
//    sheets, preloads, icons), <iframe>/<frame> blocks, <embed> and
//    <meta http-equiv="refresh"> and inline event handlers (on…, 1.4);
//    inline <style> stays (the extraction
//    script drops fixed/sticky overlays by their computed position)
//  - src, srcset, poster, background and <object data> are renamed to
//    data-sf-…, relative addresses resolved against <base href> or pageUrl;
//    an <img> that had an address gets src="data:," as well, so
//    Readability keeps it as an image with an address (it drops images
//    without one) and does not treat it as lazy
// Comments are dropped; <meta name="sailfeed-lean"> is added after <head>
// (or in front). Everything else stays as it is; restoreLeanResources()
// turns Readability's output back into normal HTML.
QString leanPage(const QString &html, const QUrl &pageUrl);

// Reverses leanPage()'s renaming in the article HTML (src, srcset, poster;
// the placeholder src="data:," goes). images = false leaves the addresses
// renamed, so hidden images are not downloaded either. Images get
// loading="lazy" in both cases: the reader page is ready without waiting
// for pictures further down.
QString restoreLeanResources(const QString &content, bool images);

} // namespace HtmlUtils
