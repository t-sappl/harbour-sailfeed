// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// OPML 2.0, the common file format for feed lists (1.4): writing the feeds
// and folders of the account, reading a file from another feed reader.
// QtCore only, tested on the desktop.
//
// Nextcloud News has one folder level. Writing: one <outline> per folder
// with its feeds inside, feeds without a folder directly in <body>.
// Reading: a feed is an <outline> with an xmlUrl attribute; its folder is the
// nearest enclosing <outline> without xmlUrl (deeper folder trees of other
// readers end up in their innermost folder). Feeds appearing twice (by
// address, see FeedFinder::compareKey) are listed once.
class OpmlFile
{
public:
    struct Feed {
        QString title;
        QString url;        // the feed's address (xmlUrl)
        QString link;       // the website (htmlUrl), may be empty
        QString folder;     // empty: no folder
    };

    struct Parsed {
        bool ok = false;
        QString error;      // understandable text when !ok
        QString title;      // <head><title>, may be empty
        QList<Feed> feeds;  // in file order
        QStringList folders;   // folder names in file order (also empty folders)
    };

    // The file content (UTF-8). emptyFolders: folders without feeds, written
    // as empty outlines so a later import creates them too.
    static QByteArray write(const QList<Feed> &feeds, const QStringList &emptyFolders,
                            const QDateTime &created, const QString &title = QStringLiteral("Sailfeed"));

    static Parsed parse(const QByteArray &data);

    // Looks like OPML (for listing files in Downloads): an <opml> element
    // near the start
    static bool looksLikeOpml(const QByteArray &start);

    // File name of an export to the Downloads folder: sailfeed-feeds-<date>.opml
    static QString exportFileName(const QDate &date, int counter = 1);
    // File name of the export to the Nextcloud (replaced by the next export)
    static QString nextcloudFileName() { return QStringLiteral("sailfeed-feeds.opml"); }
};
