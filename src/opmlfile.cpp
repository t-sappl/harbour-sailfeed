// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "opmlfile.h"

#include "feedfinder.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

void writeFeed(QXmlStreamWriter &w, const OpmlFile::Feed &f)
{
    w.writeEmptyElement(QStringLiteral("outline"));
    const QString title = f.title.isEmpty() ? f.url : f.title;
    w.writeAttribute(QStringLiteral("type"), QStringLiteral("rss"));
    w.writeAttribute(QStringLiteral("text"), title);
    w.writeAttribute(QStringLiteral("title"), title);
    w.writeAttribute(QStringLiteral("xmlUrl"), f.url);
    if (!f.link.isEmpty())
        w.writeAttribute(QStringLiteral("htmlUrl"), f.link);
}

// The value of an attribute, whatever its case (xmlUrl, xmlurl, XMLURL)
QString attribute(const QXmlStreamAttributes &attrs, const QString &name)
{
    for (const QXmlStreamAttribute &a : attrs) {
        if (a.name().compare(name, Qt::CaseInsensitive) == 0)
            return a.value().toString().trimmed();
    }
    return QString();
}

QString nameOf(const QXmlStreamAttributes &attrs)
{
    QString n = attribute(attrs, QStringLiteral("title"));
    if (n.isEmpty())
        n = attribute(attrs, QStringLiteral("text"));
    return n.simplified();
}

// A usable feed address, or empty: http(s) with a host; "feed://" and
// "feed:https://" as some readers write them
QString feedUrl(const QString &s)
{
    const QUrl url(FeedFinder::fromFeedScheme(s));
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()
            || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
        return QString();
    return url.toString();
}

struct Frame {
    bool feed = false;
    QString name;           // folder name (outlines without xmlUrl)
    bool hasChildren = false;
};

} // namespace

QByteArray OpmlFile::write(const QList<Feed> &feeds, const QStringList &emptyFolders,
                           const QDateTime &created, const QString &title)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(2);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("opml"));
    w.writeAttribute(QStringLiteral("version"), QStringLiteral("2.0"));
    w.writeStartElement(QStringLiteral("head"));
    w.writeTextElement(QStringLiteral("title"), title);
    // RFC 822, as OPML 2.0 asks
    w.writeTextElement(QStringLiteral("dateCreated"),
                       QLocale::c().toString(created.toUTC(), QStringLiteral("ddd, dd MMM yyyy HH:mm:ss")) + QStringLiteral(" GMT"));
    w.writeEndElement();    // head
    w.writeStartElement(QStringLiteral("body"));

    // Folders in the order they first appear (the caller sorts), then the
    // empty ones, then the feeds without a folder
    QStringList folders;
    for (const Feed &f : feeds) {
        if (!f.folder.isEmpty() && !folders.contains(f.folder))
            folders << f.folder;
    }
    for (const QString &folder : folders) {
        w.writeStartElement(QStringLiteral("outline"));
        w.writeAttribute(QStringLiteral("text"), folder);
        w.writeAttribute(QStringLiteral("title"), folder);
        for (const Feed &f : feeds) {
            if (f.folder == folder)
                writeFeed(w, f);
        }
        w.writeEndElement();
    }
    for (const QString &folder : emptyFolders) {
        if (folders.contains(folder) || folder.isEmpty())
            continue;
        w.writeEmptyElement(QStringLiteral("outline"));
        w.writeAttribute(QStringLiteral("text"), folder);
        w.writeAttribute(QStringLiteral("title"), folder);
    }
    for (const Feed &f : feeds) {
        if (f.folder.isEmpty())
            writeFeed(w, f);
    }
    w.writeEndElement();    // body
    w.writeEndElement();    // opml
    w.writeEndDocument();
    return out;
}

OpmlFile::Parsed OpmlFile::parse(const QByteArray &data)
{
    Parsed p;
    QXmlStreamReader r(data);
    QList<Frame> stack;
    QSet<QString> seen;             // compare keys of the feeds taken
    QSet<QString> folderKeys;       // folder names, case folded
    bool rootSeen = false;
    bool inHeadTitle = false;

    auto addFolder = [&p, &folderKeys](const QString &name) {
        if (name.isEmpty() || folderKeys.contains(name.toCaseFolded()))
            return;
        folderKeys.insert(name.toCaseFolded());
        p.folders << name;
    };

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QString el = r.name().toString().toLower();
            if (!rootSeen) {
                rootSeen = true;
                if (el != QLatin1String("opml")) {
                    p.error = QCoreApplication::translate("OpmlFile", "This is not an OPML file.");
                    return p;
                }
                continue;
            }
            if (el == QLatin1String("title") && stack.isEmpty()) {
                inHeadTitle = true;
                continue;
            }
            if (el != QLatin1String("outline"))
                continue;
            if (!stack.isEmpty())
                stack.last().hasChildren = true;
            const QXmlStreamAttributes attrs = r.attributes();
            Frame frame;
            const QString rawUrl = attribute(attrs, QStringLiteral("xmlUrl"));
            if (!rawUrl.isEmpty()) {
                frame.feed = true;
                const QString url = feedUrl(rawUrl);
                if (!url.isEmpty()) {
                    const QString key = FeedFinder::compareKey(url);
                    if (!seen.contains(key)) {
                        seen.insert(key);
                        Feed f;
                        f.url = url;
                        f.title = nameOf(attrs);
                        if (f.title.isEmpty())
                            f.title = QUrl(url).host();
                        const QString link = feedUrl(attribute(attrs, QStringLiteral("htmlUrl")));
                        f.link = link;
                        for (int i = stack.size() - 1; i >= 0; --i) {
                            if (!stack.at(i).feed && !stack.at(i).name.isEmpty()) {
                                f.folder = stack.at(i).name;
                                break;
                            }
                        }
                        addFolder(f.folder);
                        p.feeds << f;
                    }
                }
            } else {
                frame.name = nameOf(attrs);
            }
            stack << frame;
        } else if (r.isEndElement()) {
            const QString el = r.name().toString().toLower();
            if (el == QLatin1String("title"))
                inHeadTitle = false;
            if (el == QLatin1String("outline") && !stack.isEmpty()) {
                const Frame frame = stack.takeLast();
                // A folder without anything inside: an empty folder of the
                // other reader. A wrapper with only sub-folders is not one.
                if (!frame.feed && !frame.hasChildren)
                    addFolder(frame.name);
            }
        } else if (r.isCharacters() && inHeadTitle) {
            p.title += r.text().toString();
        }
    }

    if (r.hasError()) {
        p.error = QCoreApplication::translate("OpmlFile", "The file is damaged (line %1: %2).")
                .arg(r.lineNumber()).arg(r.errorString());
        p.feeds.clear();
        p.folders.clear();
        return p;
    }
    if (!rootSeen) {
        p.error = QCoreApplication::translate("OpmlFile", "This is not an OPML file.");
        return p;
    }
    p.title = p.title.simplified();
    if (p.feeds.isEmpty()) {
        p.error = QCoreApplication::translate("OpmlFile", "There are no feeds in this file.");
        return p;
    }
    p.ok = true;
    return p;
}

bool OpmlFile::looksLikeOpml(const QByteArray &start)
{
    QXmlStreamReader r(start);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement())
            return r.name().compare(QLatin1String("opml"), Qt::CaseInsensitive) == 0;
        if (r.hasError())
            return false;
    }
    return false;
}

QString OpmlFile::exportFileName(const QDate &date, int counter)
{
    QString name = QStringLiteral("sailfeed-feeds-") + date.toString(QStringLiteral("yyyy-MM-dd"));
    if (counter > 1)
        name += QLatin1Char('-') + QString::number(counter);
    return name + QStringLiteral(".opml");
}
