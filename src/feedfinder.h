// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

// "Subscribe to website" (release 1.1): finds the RSS/Atom feeds of a web
// address. QML type "FeedFinder", one per page.
//
// Steps (stops widening the search as soon as feeds are found):
//   0. the address itself – if it is a feed, that is the result
//   1. its HTML: <link rel="alternate" type="application/rss+xml|atom+xml">
//      and links that look like feeds (href "…/feed", "rss.xml", text "RSS")
//   2. nothing found: the parent pages ("/blog/2026/post" -> "/blog/2026/",
//      "/blog/", "/"), steps 1 again
//   3. still nothing: well-known paths of the site and of common CMS
//      (/feed, /rss.xml, /index.xml, Blogger, WordPress …) plus the file
//      names the user's subscribed feeds use
// Every candidate is downloaded and checked (<rss>, Atom <feed>, <rdf:RDF>).
// Result per feed: url, title, description, format, itemCount, latest,
// recent (3 titles), podcast, comments, subscribed, likelySubscribedAs.
//
// "Already subscribed" (15 Oct, device test: thenextweb.com and heise.de
// offered feeds that were subscribed): a result counts as subscribed when any
// of its addresses matches a subscribed feed's address – the address asked
// for, every redirect, the final address and the feed's own "self" link –
// compared without scheme, "www.", trailing "/" and tracking parameters.
// Same title and same website but another address only gives
// "likelySubscribedAs" (the subscribed feed's title): the button stays, a
// false "subscribed" would make subscribing impossible.
class FeedFinder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList feeds READ feeds NOTIFY feedsChanged)
    // After a search without result: why (page not reachable, no feed …)
    Q_PROPERTY(QString error READ error NOTIFY finished)
    Q_PROPERTY(bool searched READ searched NOTIFY finished)
    // Addresses of the subscribed feeds: marks results as "subscribed" and
    // adds their file names to the well-known paths.
    Q_PROPERTY(QStringList knownUrls READ knownUrls WRITE setKnownUrls NOTIFY knownUrlsChanged)
    // The subscribed feeds as maps {url, link, title} (FeedModel.subscribedFeeds())
    Q_PROPERTY(QVariantList knownFeeds READ knownFeeds WRITE setKnownFeeds NOTIFY knownUrlsChanged)
public:
    explicit FeedFinder(QObject *parent = nullptr);
    ~FeedFinder() override;

    bool busy() const { return m_busy; }
    QVariantList feeds() const { return m_feeds; }
    QString error() const { return m_error; }
    bool searched() const { return m_searched; }
    QStringList knownUrls() const { return m_knownUrls; }
    void setKnownUrls(const QStringList &urls);
    QVariantList knownFeeds() const { return m_knownFeeds; }
    void setKnownFeeds(const QVariantList &feeds);

    Q_INVOKABLE void find(const QString &address);
    Q_INVOKABLE void cancel();

    // --- pure helpers (desktop test) ---
    // Checks downloaded data: ok (is a feed), format ("RSS", "Atom", "RDF"),
    // title, description, itemCount, latest (QDateTime), recent (up to 3
    // titles), podcast (most items have audio/video enclosures), selfUrl (the
    // feed's own address, <link rel="self">), siteLink (the website).
    static QVariantMap parseFeed(const QByteArray &data);
    // Feed addresses announced or linked in an HTML page, in page order
    static QList<QUrl> discoverLinks(const QByteArray &html, const QUrl &pageUrl);
    // "/a/b/c" -> "/a/b/", "/a/", "/" (at most 3)
    static QList<QUrl> parentPages(const QUrl &url);
    // Well-known feed addresses of the site of url; extraNames: further
    // file names ("index.xml"), tried at the site root
    static QList<QUrl> wellKnownCandidates(const QUrl &url, const QStringList &extraNames);
    // Feed-like last path segments of the given feed addresses, most used first
    static QStringList fileNamesOf(const QStringList &feedUrls, int max = 5);
    // For comparing addresses: without scheme, "www.", trailing "/",
    // fragment and tracking parameters (utm_*, wt_mc, at_*, fbclid …)
    static QString compareKey(const QString &url);
    // For "likely subscribed": "title|site host" (lower case, simplified)
    static QString titleSiteKey(const QString &title, const QString &siteLink);
    // "comments" feed by address or title
    static bool looksLikeComments(const QString &url, const QString &title);

Q_SIGNALS:
    void busyChanged(bool busy);
    void feedsChanged();
    void knownUrlsChanged();
    void finished();

private:
    enum class Kind { Start, Page, Candidate };
    void get(const QUrl &url, Kind kind);
    void onReply(QNetworkReply *reply, Kind kind);
    void addCandidates(const QList<QUrl> &urls);
    void stepDone();
    void addFeed(const QUrl &url, const QVariantMap &info, const QStringList &aliases);
    void markKnown(QVariantMap &feed) const;
    void rebuildKnown();
    void finish(const QString &error);
    void setBusy(bool busy);

    QNetworkAccessManager *m_nam;
    QList<QNetworkReply *> m_replies;
    QUrl m_start;
    int m_stage = 0;            // 0 start page, 1 parent pages, 2 well-known paths
    int m_requests = 0;
    QSet<QString> m_tried;      // compare keys of requested addresses
    QSet<QString> m_found;      // compare keys of found feeds
    QVariantList m_feeds;
    QString m_error;
    QString m_startError;
    bool m_busy = false;
    bool m_searched = false;
    QStringList m_knownUrls;
    QVariantList m_knownFeeds;
    QSet<QString> m_knownKeys;
    QHash<QString, QString> m_knownTitleSite;    // titleSiteKey -> title
};
