// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
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

    // Starts downloading url (http/https only); no-op if cached or pending.
    Q_INVOKABLE void prefetch(const QUrl &url);
    // Decoded HTML of a finished download, or "" if none.
    Q_INVOKABLE QString html(const QUrl &url) const;
    Q_INVOKABLE bool isPending(const QUrl &url) const;
    // Drops downloaded pages, e.g. after a consent wall was found: the other
    // pages from that site were most likely downloaded as the wall, too.
    Q_INVOKABLE void clear();

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
    static QString key(const QUrl &url);

    QNetworkAccessManager *m_nam;
    GeckoCookieJar *m_jar;
    QHash<QString, QString> m_cache;
    QStringList m_order;                    // oldest first
    QHash<QString, QNetworkReply *> m_pending;
};
