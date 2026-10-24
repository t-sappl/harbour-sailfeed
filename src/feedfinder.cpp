// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "feedfinder.h"
#include "htmlutils.h"

#include <algorithm>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QXmlStreamReader>

namespace {
const int kMaxRequests = 30;            // per search
const qint64 kMaxBytes = 3 * 1024 * 1024;
const int kTimeoutMs = 12000;           // per request

QString attr(const QString &tag, const QString &name)
{
    static const QRegularExpression re(QStringLiteral(
        "([a-zA-Z_:][-a-zA-Z0-9_:.]*)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s\"'>]+))"));
    auto it = re.globalMatch(tag);
    while (it.hasNext()) {
        const auto m = it.next();
        if (m.captured(1).compare(name, Qt::CaseInsensitive) == 0) {
            for (int i = 2; i <= 4; ++i)
                if (m.capturedStart(i) >= 0)
                    return m.captured(i).trimmed();
        }
    }
    return QString();
}

QString decodeAmp(QString s)
{
    s.replace(QLatin1String("&amp;"), QLatin1String("&"));
    s.replace(QLatin1String("&#38;"), QLatin1String("&"));
    return s;
}

QDateTime parseDate(const QString &text)
{
    const QString s = text.trimmed();
    if (s.isEmpty())
        return QDateTime();
    QDateTime d = QDateTime::fromString(s, Qt::RFC2822Date);
    if (!d.isValid())
        d = QDateTime::fromString(s, Qt::ISODate);
    if (!d.isValid()) {
        // "Mon, 12 Oct 2026 10:00:00 GMT" with a day name Qt 5.6 does not take
        QString t = s;
        const int comma = t.indexOf(QLatin1Char(','));
        if (comma > 0 && comma < 5)
            t = t.mid(comma + 1).trimmed();
        t.replace(QLatin1String(" GMT"), QLatin1String(" +0000"));
        t.replace(QLatin1String(" UTC"), QLatin1String(" +0000"));
        t.replace(QLatin1String(" Z"), QLatin1String(" +0000"));
        d = QDateTime::fromString(t, Qt::RFC2822Date);
    }
    return d;
}

bool isMedia(const QString &type)
{
    return type.startsWith(QLatin1String("audio/"), Qt::CaseInsensitive)
            || type.startsWith(QLatin1String("video/"), Qt::CaseInsensitive);
}
} // namespace

FeedFinder::FeedFinder(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

FeedFinder::~FeedFinder()
{
    cancel();
}

void FeedFinder::setKnownUrls(const QStringList &urls)
{
    if (urls == m_knownUrls)
        return;
    m_knownUrls = urls;
    rebuildKnown();
}

void FeedFinder::setKnownFeeds(const QVariantList &feeds)
{
    if (feeds == m_knownFeeds)
        return;
    m_knownFeeds = feeds;
    rebuildKnown();
}

void FeedFinder::rebuildKnown()
{
    m_knownKeys.clear();
    m_knownTitleSite.clear();
    m_knownArticle.clear();
    for (const QString &u : m_knownUrls)
        m_knownKeys.insert(compareKey(u));
    for (const QVariant &v : m_knownFeeds) {
        const QVariantMap f = v.toMap();
        m_knownKeys.insert(compareKey(f.value(QStringLiteral("url")).toString()));
        const QString title = f.value(QStringLiteral("title")).toString();
        const QString ts = titleSiteKey(title, f.value(QStringLiteral("link")).toString());
        if (!ts.isEmpty())
            m_knownTitleSite.insert(ts, title);
        for (const QVariant &t : f.value(QStringLiteral("articleTitles")).toList()) {
            const QString k = articleKey(t.toString());
            if (!k.isEmpty())
                m_knownArticle.insert(k, title);
        }
    }
    m_knownKeys.remove(QString());
    // results already shown: update the marks
    bool changed = false;
    for (QVariant &v : m_feeds) {
        QVariantMap m = v.toMap();
        const QVariantMap before = m;
        markKnown(m);
        if (m != before) {
            v = m;
            changed = true;
        }
    }
    Q_EMIT knownUrlsChanged();
    if (changed)
        Q_EMIT feedsChanged();
}

void FeedFinder::markKnown(QVariantMap &feed) const
{
    bool sub = false;
    const QStringList aliases = feed.value(QStringLiteral("aliases")).toStringList();
    for (const QString &a : aliases) {
        if (m_knownKeys.contains(a)) {
            sub = true;
            break;
        }
    }
    feed.insert(QStringLiteral("subscribed"), sub);
    QString likely;
    if (!sub) {
        likely = m_knownTitleSite.value(titleSiteKey(feed.value(QStringLiteral("title")).toString(),
                                                     feed.value(QStringLiteral("siteLink")).toString()));
    }
    // Same articles as a subscribed feed (another address of the same feed,
    // also when renamed): at least two of its newest titles, or its only one
    if (!sub && likely.isEmpty()) {
        QHash<QString, int> hits;
        const QStringList titles = feed.value(QStringLiteral("itemTitles")).toStringList();
        for (const QString &t : titles) {
            const QString owner = m_knownArticle.value(articleKey(t));
            if (!owner.isEmpty())
                ++hits[owner];
        }
        for (auto it = hits.constBegin(); it != hits.constEnd(); ++it) {
            if (it.value() >= 2 || (titles.size() == 1 && it.value() == 1)) {
                likely = it.key();
                break;
            }
        }
    }
    feed.insert(QStringLiteral("likelySubscribedAs"), likely);
}

QString FeedFinder::articleKey(const QString &title)
{
    QString t = HtmlUtils::decodeEntities(title).toCaseFolded().simplified();
    return t.size() >= 12 ? t : QString();      // too short to say anything
}

// ---------------------------------------------------------------------------
// Pure helpers

QVariantMap FeedFinder::parseFeed(const QByteArray &data)
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), false);
    QXmlStreamReader xml(data);
    // root element
    while (!xml.atEnd() && !xml.isStartElement())
        xml.readNext();
    if (!xml.isStartElement())
        return out;
    const QString root = xml.name().toString();
    QString format;
    if (root == QLatin1String("rss"))
        format = QStringLiteral("RSS");
    else if (root == QLatin1String("feed"))
        format = QStringLiteral("Atom");
    else if (root == QLatin1String("RDF"))
        format = QStringLiteral("RDF");
    else
        return out;

    const bool atom = format == QLatin1String("Atom");
    const QString itemName = atom ? QStringLiteral("entry") : QStringLiteral("item");
    QString title, description, selfUrl, siteLink;
    QStringList recent;
    QStringList itemTitles;                     // the newest 20, for markKnown
    QDateTime latest;
    int items = 0, media = 0;
    bool inItem = false;
    QString itemTitle;
    bool itemMedia = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString n = xml.name().toString();
            if (n == itemName) {
                inItem = true;
                itemTitle.clear();
                itemMedia = false;
                continue;
            }
            if (inItem) {
                if (n == QLatin1String("title") && itemTitle.isEmpty()) {
                    itemTitle = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                } else if (n == QLatin1String("pubDate") || n == QLatin1String("published")
                           || n == QLatin1String("updated") || n == QLatin1String("date")) {
                    const QDateTime d = parseDate(xml.readElementText(QXmlStreamReader::IncludeChildElements));
                    if (d.isValid() && (!latest.isValid() || d > latest))
                        latest = d;
                } else if (n == QLatin1String("enclosure")) {
                    if (isMedia(xml.attributes().value(QStringLiteral("type")).toString()))
                        itemMedia = true;
                } else if (atom && n == QLatin1String("link")
                           && xml.attributes().value(QStringLiteral("rel")) == QLatin1String("enclosure")
                           && isMedia(xml.attributes().value(QStringLiteral("type")).toString())) {
                    itemMedia = true;
                } else if (n == QLatin1String("content")
                           && isMedia(xml.attributes().value(QStringLiteral("type")).toString())) {
                    itemMedia = true;               // media:content
                }
            } else {
                // channel / feed level
                if (n == QLatin1String("link")) {
                    const QString rel = xml.attributes().value(QStringLiteral("rel")).toString();
                    const QString href = xml.attributes().value(QStringLiteral("href")).toString().trimmed();
                    if (rel == QLatin1String("self")) {
                        if (selfUrl.isEmpty())
                            selfUrl = href;         // Atom, or atom:link in RSS
                    } else if (!href.isEmpty()) {
                        if (siteLink.isEmpty() && (rel.isEmpty() || rel == QLatin1String("alternate")))
                            siteLink = href;        // Atom
                    } else if (siteLink.isEmpty() && xml.attributes().isEmpty()) {
                        siteLink = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();   // RSS
                    }
                    continue;
                }
                if (n == QLatin1String("title") && title.isEmpty()) {
                    title = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                } else if ((n == QLatin1String("description") || n == QLatin1String("subtitle"))
                           && description.isEmpty()) {
                    description = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                }
            }
        } else if (xml.isEndElement()) {
            if (inItem && xml.name() == itemName) {
                inItem = false;
                ++items;
                if (itemMedia)
                    ++media;
                if (recent.size() < 3 && !itemTitle.isEmpty())
                    recent << itemTitle;
                if (itemTitles.size() < 20 && !itemTitle.isEmpty())
                    itemTitles << itemTitle;
            }
        }
    }
    if (xml.hasError() && items == 0 && title.isEmpty())
        return out;                 // broken beyond use

    // Plain text for title/description: drop tags left by HTML content
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    description.remove(tags);
    if (description.size() > 300)
        description = description.left(299) + QChar(0x2026);

    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("format"), format);
    out.insert(QStringLiteral("title"), title);
    out.insert(QStringLiteral("description"), description.simplified());
    out.insert(QStringLiteral("itemCount"), items);
    out.insert(QStringLiteral("latest"), latest);
    out.insert(QStringLiteral("recent"), recent);
    out.insert(QStringLiteral("itemTitles"), itemTitles);
    out.insert(QStringLiteral("podcast"), items > 0 && media * 2 >= items);
    out.insert(QStringLiteral("selfUrl"), selfUrl);
    out.insert(QStringLiteral("siteLink"), siteLink);
    return out;
}

QList<QUrl> FeedFinder::discoverLinks(const QByteArray &html, const QUrl &pageUrl)
{
    const QString text = QString::fromUtf8(html);
    QUrl base = pageUrl;
    static const QRegularExpression baseRe(QStringLiteral("<base\\b[^>]*>"),
                                           QRegularExpression::CaseInsensitiveOption);
    const auto bm = baseRe.match(text);
    if (bm.hasMatch()) {
        const QString href = attr(bm.captured(0), QStringLiteral("href"));
        if (!href.isEmpty())
            base = pageUrl.resolved(QUrl(decodeAmp(href)));
    }

    QList<QUrl> out;
    QSet<QString> seen;
    auto add = [&](const QString &href) {
        const QString h = decodeAmp(href.trimmed());
        if (h.isEmpty() || h.startsWith(QLatin1Char('#')) || h.startsWith(QLatin1String("javascript:"), Qt::CaseInsensitive)
                || h.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive))
            return;
        QUrl u = base.resolved(QUrl(h));
        if (u.scheme() == QLatin1String("feed"))            // feed://example.org/rss
            u.setScheme(QStringLiteral("https"));
        if (u.scheme() != QLatin1String("http") && u.scheme() != QLatin1String("https"))
            return;
        u.setFragment(QString());
        const QString key = compareKey(u.toString());
        if (seen.contains(key))
            return;
        seen.insert(key);
        out << u;
    };

    // 1. announced feeds
    static const QRegularExpression linkRe(QStringLiteral("<link\\b[^>]*>"),
                                           QRegularExpression::CaseInsensitiveOption);
    auto it = linkRe.globalMatch(text);
    while (it.hasNext()) {
        const QString tag = it.next().captured(0);
        const QString rel = attr(tag, QStringLiteral("rel")).toLower();
        const QString type = attr(tag, QStringLiteral("type")).toLower();
        if (!rel.contains(QLatin1String("alternate")) && !rel.contains(QLatin1String("feed")))
            continue;
        if (type.contains(QLatin1String("rss")) || type.contains(QLatin1String("atom"))
                || type.contains(QLatin1String("rdf")) || type == QLatin1String("text/xml")
                || type == QLatin1String("application/xml"))
            add(attr(tag, QStringLiteral("href")));
    }

    // 2. links that look like feeds ("RSS" buttons, "/feed", "rss.xml")
    static const QRegularExpression aRe(QStringLiteral("<a\\b([^>]*)>(.*?)</a>"),
                                        QRegularExpression::CaseInsensitiveOption
                                        | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression hrefFeed(QStringLiteral(
        "(^|[/.?=&_-])(rss2?|atom|feeds?)([/.?#_-]|\\.xml|\\.rss|$)|\\.(rss|atom)$|/rss\\.xml$|/feed\\.xml$|/index\\.xml$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression textFeed(QStringLiteral("\\b(rss|atom|feed)\\b"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stripTags(QStringLiteral("<[^>]*>"));
    int anchors = 0;
    auto ait = aRe.globalMatch(text);
    while (ait.hasNext() && anchors < 10) {
        const auto m = ait.next();
        const QString attrs = QStringLiteral("<a ") + m.captured(1) + QLatin1Char('>');
        const QString href = attr(attrs, QStringLiteral("href"));
        if (href.isEmpty())
            continue;
        QString path = QUrl(decodeAmp(href)).path();
        const QString query = QUrl(decodeAmp(href)).query();
        if (!query.isEmpty())
            path += QLatin1Char('?') + query;
        QString inner = m.captured(2);
        // image buttons: alt/src/title count as the link's text
        QString innerText = QString(inner).remove(stripTags).simplified();
        if (innerText.isEmpty())
            innerText = attr(inner, QStringLiteral("alt")) + QLatin1Char(' ') + attr(inner, QStringLiteral("src"));
        innerText += QLatin1Char(' ') + attr(attrs, QStringLiteral("title"));
        const bool byHref = hrefFeed.match(path).hasMatch();
        const bool byText = textFeed.match(innerText).hasMatch() && innerText.size() < 60;
        if (byHref || byText) {
            const int before = out.size();
            add(href);
            if (out.size() > before)
                ++anchors;
        }
    }
    return out;
}

QList<QUrl> FeedFinder::parentPages(const QUrl &url)
{
    QList<QUrl> out;
    QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/"))
        return out;
    if (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    while (out.size() < 3) {
        const int slash = path.lastIndexOf(QLatin1Char('/'));
        if (slash < 0)
            break;
        path = path.left(slash);
        QUrl p = url;
        p.setPath(path + QLatin1Char('/'));
        p.setQuery(QString());
        p.setFragment(QString());
        out << p;
        if (path.isEmpty())
            break;
    }
    return out;
}

QList<QUrl> FeedFinder::wellKnownCandidates(const QUrl &url, const QStringList &extraNames)
{
    QUrl root = url;
    root.setPath(QStringLiteral("/"));
    root.setQuery(QString());
    root.setFragment(QString());

    // Common to most sites, then CMS: WordPress, Blogger, Ghost, Drupal, Hugo, Jekyll
    QStringList names = {
        QStringLiteral("feed"), QStringLiteral("feed/"), QStringLiteral("rss"), QStringLiteral("rss.xml"),
        QStringLiteral("feed.xml"), QStringLiteral("atom.xml"), QStringLiteral("index.xml"),
        QStringLiteral("index.rss"), QStringLiteral("feed.rss"), QStringLiteral("rss/"),
        QStringLiteral("feeds/posts/default"), QStringLiteral("?feed=rss2"),
    };
    for (const QString &n : extraNames)
        if (!names.contains(n))
            names << n;

    QList<QUrl> out;
    for (const QString &n : names)
        out << root.resolved(QUrl(n));

    // A blog in a sub-folder ("example.org/blog/2026/post"): its first folder too
    QStringList parts = url.path().split(QLatin1Char('/'));
    parts.removeAll(QString());
    if (parts.size() >= 2) {
        QUrl sub = root;
        sub.setPath(QLatin1Char('/') + parts.first() + QLatin1Char('/'));
        for (const QString &n : { QStringLiteral("feed/"), QStringLiteral("rss.xml"), QStringLiteral("index.xml"),
                                  QStringLiteral("feed.xml"), QStringLiteral("atom.xml") })
            out << sub.resolved(QUrl(n));
    }
    return out;
}

QStringList FeedFinder::fileNamesOf(const QStringList &feedUrls, int max)
{
    QHash<QString, int> counts;
    static const QRegularExpression feedish(QStringLiteral("rss|atom|feed|\\.xml$"),
                                            QRegularExpression::CaseInsensitiveOption);
    for (const QString &u : feedUrls) {
        const QString path = QUrl(u).path();
        const QString last = path.section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty);
        if (!last.isEmpty() && last.size() < 30 && feedish.match(last).hasMatch())
            counts[path.endsWith(QLatin1Char('/')) ? last + QLatin1Char('/') : last]++;
    }
    QStringList names = counts.keys();
    std::sort(names.begin(), names.end(), [&](const QString &a, const QString &b) {
        return counts.value(a) != counts.value(b) ? counts.value(a) > counts.value(b) : a < b;
    });
    return names.mid(0, max);
}

QString FeedFinder::compareKey(const QString &url)
{
    QString s = url.trimmed().toLower();
    const int hash = s.indexOf(QLatin1Char('#'));
    if (hash >= 0)
        s.truncate(hash);
    const int scheme = s.indexOf(QLatin1String("://"));
    if (scheme >= 0)
        s = s.mid(scheme + 3);
    if (s.startsWith(QLatin1String("www.")))
        s = s.mid(4);
    // Tracking parameters say nothing about the feed (e.g. heise's
    // "?wt_mc=rss.red…"); the others stay (Blogger "?alt=rss", "?feed=rss2")
    const int q = s.indexOf(QLatin1Char('?'));
    if (q >= 0) {
        static const QRegularExpression tracking(
            QStringLiteral("^(utm_[a-z_]*|wt_mc|wt_zmc|at_[a-z_]*|fbclid|gclid|mc_[a-z]+|ref)(=|$)"));
        QStringList kept;
        const QStringList params = s.mid(q + 1).split(QLatin1Char('&'), QString::SkipEmptyParts);
        for (const QString &p : params) {
            if (!tracking.match(p).hasMatch())
                kept << p;
        }
        s.truncate(q);
        while (s.endsWith(QLatin1Char('/')))
            s.chop(1);
        if (!kept.isEmpty())
            s += QLatin1Char('?') + kept.join(QLatin1Char('&'));
        return s;
    }
    while (s.endsWith(QLatin1Char('/')))
        s.chop(1);
    return s;
}

QString FeedFinder::titleSiteKey(const QString &title, const QString &siteLink)
{
    const QString t = title.simplified().toLower();
    QString host = QUrl(siteLink.trimmed()).host().toLower();
    if (host.startsWith(QLatin1String("www.")))
        host = host.mid(4);
    if (t.isEmpty() || host.isEmpty())
        return QString();
    return t + QLatin1Char('|') + host;
}

bool FeedFinder::looksLikeComments(const QString &url, const QString &title)
{
    static const QRegularExpression re(QStringLiteral("comment|kommentar"), QRegularExpression::CaseInsensitiveOption);
    return re.match(QUrl(url).path()).hasMatch() || re.match(title).hasMatch();
}

// ---------------------------------------------------------------------------
// Search

void FeedFinder::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged(busy);
}

void FeedFinder::cancel()
{
    const auto replies = m_replies;
    m_replies.clear();
    for (QNetworkReply *r : replies) {
        r->disconnect(this);
        r->abort();
        r->deleteLater();
    }
    setBusy(false);
}

void FeedFinder::find(const QString &address)
{
    cancel();
    m_feeds.clear();
    m_found.clear();
    m_tried.clear();
    m_error.clear();
    m_startError.clear();
    m_searched = false;
    m_stage = 0;
    m_requests = 0;
    Q_EMIT feedsChanged();

    QString s = address.trimmed();
    if (s.startsWith(QLatin1String("feed://"), Qt::CaseInsensitive))
        s = QStringLiteral("https://") + s.mid(7);
    if (!s.contains(QLatin1String("://")))
        s.prepend(QLatin1String("https://"));
    const QUrl url(s, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()
            || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        finish(tr("This is not a web address."));
        return;
    }
    m_start = url;
    setBusy(true);
    get(url, Kind::Start);
}

void FeedFinder::get(const QUrl &url, Kind kind)
{
    const QString key = compareKey(url.toString());
    if (m_tried.contains(key) || m_requests >= kMaxRequests)
        return;
    m_tried.insert(key);
    ++m_requests;

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    req.setRawHeader("User-Agent", "Mozilla/5.0 (Linux; Sailfish OS; Mobile; rv:91.0) Gecko/20100101 Firefox/91.0");
    req.setRawHeader("Accept", "application/rss+xml, application/atom+xml, application/xml;q=0.9, text/xml;q=0.9, text/html;q=0.8, */*;q=0.5");
    QNetworkReply *reply = m_nam->get(req);
    m_replies << reply;
    // Every address on the way counts for "already subscribed"
    reply->setProperty("sfAliases", QStringList{ url.toString() });
    connect(reply, &QNetworkReply::redirected, reply, [reply](const QUrl &to) {
        QStringList a = reply->property("sfAliases").toStringList();
        a << to.toString();
        reply->setProperty("sfAliases", a);
    });

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > kMaxBytes)
            reply->abort();
    });
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(kTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind]() { onReply(reply, kind); });
}

void FeedFinder::onReply(QNetworkReply *reply, Kind kind)
{
    m_replies.removeAll(reply);
    reply->deleteLater();
    const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
    const QUrl finalUrl = reply->url();

    if (!data.isEmpty()) {
        const QVariantMap info = parseFeed(data);
        if (info.value(QStringLiteral("ok")).toBool()) {
            QStringList aliases = reply->property("sfAliases").toStringList();
            const QString self = info.value(QStringLiteral("selfUrl")).toString();
            if (!self.isEmpty())
                aliases << finalUrl.resolved(QUrl(self)).toString();
            addFeed(finalUrl, info, aliases);
        } else if (kind != Kind::Candidate) {
            addCandidates(discoverLinks(data, finalUrl));
        }
    } else if (kind == Kind::Start) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_startError = status >= 400 ? tr("The page answered with error %1.").arg(status)
                                     : tr("The page could not be loaded (%1).").arg(reply->errorString());
    }
    if (m_replies.isEmpty())
        stepDone();
}

void FeedFinder::addCandidates(const QList<QUrl> &urls)
{
    for (const QUrl &u : urls)
        get(u, Kind::Candidate);
}

void FeedFinder::stepDone()
{
    if (!m_feeds.isEmpty() || !m_startError.isEmpty()) {
        finish(m_feeds.isEmpty() ? m_startError : QString());
        return;
    }
    if (m_stage == 0) {
        m_stage = 1;
        for (const QUrl &p : parentPages(m_start))
            get(p, Kind::Page);
        if (!m_replies.isEmpty())
            return;
    }
    if (m_stage == 1) {
        m_stage = 2;
        addCandidates(wellKnownCandidates(m_start, fileNamesOf(m_knownUrls)));
        if (!m_replies.isEmpty())
            return;
    }
    finish(m_feeds.isEmpty() ? tr("No feed found on this website.") : QString());
}

void FeedFinder::addFeed(const QUrl &url, const QVariantMap &info, const QStringList &aliases)
{
    const QString key = compareKey(url.toString());
    QStringList keys{ key };
    for (const QString &a : aliases) {
        const QString k = compareKey(a);
        if (!k.isEmpty() && !keys.contains(k))
            keys << k;
    }
    // The same feed reached through another address (redirect, self link)
    for (const QString &k : keys) {
        if (m_found.contains(k))
            return;
    }
    for (const QString &k : keys)
        m_found.insert(k);
    QVariantMap m = info;
    m.remove(QStringLiteral("ok"));
    m.insert(QStringLiteral("url"), url.toString());
    m.insert(QStringLiteral("aliases"), keys);
    m.insert(QStringLiteral("comments"), looksLikeComments(url.toString(), info.value(QStringLiteral("title")).toString()));
    markKnown(m);
    m_feeds << m;
    // Order: normal feeds, then comments, then podcasts (stable)
    auto rank = [](const QVariant &v) {
        const QVariantMap f = v.toMap();
        return f.value(QStringLiteral("podcast")).toBool() ? 2 : (f.value(QStringLiteral("comments")).toBool() ? 1 : 0);
    };
    std::stable_sort(m_feeds.begin(), m_feeds.end(), [&](const QVariant &a, const QVariant &b) { return rank(a) < rank(b); });
    Q_EMIT feedsChanged();
}

void FeedFinder::finish(const QString &error)
{
    cancel();
    m_error = error;
    m_searched = true;
    Q_EMIT finished();
}
