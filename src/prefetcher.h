// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>

class GeckoCookieJar;
class QNetworkAccessManager;
class QNetworkReply;

// Downloads article web pages in the background (e.g. while the article
// preview is open), so the reader mode can start from HTML that is already
// there instead of waiting for the page to load. Keeps the last few pages in
// memory. QML singleton "Prefetch".
class Prefetcher : public QObject
{
    Q_OBJECT
    // Privacy: send the in-app browser's cookies with the downloads
    Q_PROPERTY(bool shareBrowserCookies READ shareBrowserCookies WRITE setShareBrowserCookies NOTIFY privacyChanged)
    // Privacy: delete cookies and site data at every start
    Q_PROPERTY(bool clearAtStart READ clearAtStart WRITE setClearAtStart NOTIFY privacyChanged)
    // Deleting is scheduled for the next start
    Q_PROPERTY(bool clearScheduled READ clearScheduled NOTIFY privacyChanged)
public:
    explicit Prefetcher(QObject *parent = nullptr);
    ~Prefetcher() override;

    // Starts downloading url (http/https only); no-op if cached or pending.
    Q_INVOKABLE void prefetch(const QUrl &url);
    // Decoded HTML of a finished download, or "" if none.
    Q_INVOKABLE QString html(const QUrl &url) const;
    // The same page without scripts, style sheets, frames and with images
    // that do not load (HtmlUtils::leanPage), for the reader mode's
    // extraction; "" if none. Made once when the download ends (1.4).
    Q_INVOKABLE QString leanHtml(const QUrl &url) const;
    // Reader mode result (the JSON of the extraction script) of a page, so
    // going back to an article shows it at once without loading and
    // extracting again (1.4). Kept for the same pages as the downloads,
    // plus pages loaded without a download; dropped by clear().
    Q_INVOKABLE void setArticle(const QUrl &url, const QString &json);
    Q_INVOKABLE QString article(const QUrl &url) const;
    Q_INVOKABLE bool isPending(const QUrl &url) const;
    // Milliseconds since the download of url last received data (or
    // started), -1 if it is not running. Slow connections: the reader mode
    // keeps waiting while data still comes in instead of starting over.
    Q_INVOKABLE int idleMs(const QUrl &url) const;
    // Drops downloaded pages, e.g. after a consent wall was found: the other
    // pages from that site were most likely downloaded as the wall, too.
    Q_INVOKABLE void clear();
    // Stops the download of url if it is still running (1.4: the preview
    // was left; nothing waits for the page any more).
    Q_INVOKABLE void cancel(const QUrl &url);

    // Cookies of the in-app browser are sent with the downloads (see
    // GeckoCookieJar). Short status for the settings page.
    Q_INVOKABLE QString cookieInfo() const;
    bool shareBrowserCookies() const;
    void setShareBrowserCookies(bool share);
    bool clearAtStart() const;
    void setClearAtStart(bool clear);
    bool clearScheduled() const;
    // Cookies and site data of the in-app browser can only be deleted while
    // its engine is not running: done at the next start of the app.
    Q_INVOKABLE void scheduleClearWebData();
    Q_INVOKABLE void cancelClearWebData();
    // Called by main() before the QML is loaded. Returns true if data was deleted.
    static bool clearWebDataIfRequested(const QString &cookieDatabasePath = QString());

    // For tests: use this cookies.sqlite instead of searching for it.
    void setCookieDatabase(const QString &path);

    static constexpr int maxEntries = 8;
    static constexpr qint64 maxBytes = 4 * 1024 * 1024;

Q_SIGNALS:
    // Emitted when a download ends; ok = HTML is available via html().
    void finished(const QUrl &url, bool ok);
    void privacyChanged();

private:
    void onFinished(QNetworkReply *reply, const QUrl &key);
    Q_INVOKABLE void leanReady(const QString &key, const QUrl &url, const QString &html, const QString &lean);
    static QString key(const QUrl &url);

    QNetworkAccessManager *m_nam;
    GeckoCookieJar *m_jar;
    QHash<QString, QString> m_cache;
    QHash<QString, QString> m_lean;         // same keys as m_cache
    QStringList m_order;                    // oldest first
    QHash<QString, QString> m_articles;     // reader mode results
    QStringList m_articleOrder;             // oldest first
    QHash<QString, QNetworkReply *> m_pending;
    QHash<QString, qint64> m_lastData;      // pending key -> ms since epoch
    // Downloaded, the lean copy is being made in m_pool (1.4: it ran on the
    // GUI thread, up to 4 MB, while the preview was being scrolled)
    QSet<QString> m_leaning;
    QThreadPool m_pool;
};
