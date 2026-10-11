// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QNetworkCookieJar>
#include <QString>

// Cookie jar for the Prefetcher that also offers the cookies the in-app
// browser (Sailfish WebView / Gecko) has stored in its profile
// (cookies.sqlite). Without them, pages behind a consent wall (e.g. golem.de:
// "with ads or Golem pur") would always be downloaded as the wall, even after
// the choice was made in the web view.
//
// Gecko's database is only read, never written. Cookies the downloads set
// themselves are kept in memory as usual.
class GeckoCookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    explicit GeckoCookieJar(QObject *parent = nullptr);
    ~GeckoCookieJar() override;

    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override;

    // Path of cookies.sqlite; empty = search the app's cache and data folders
    // (where the WebView keeps its profile) on first use.
    void setDatabasePath(const QString &path);
    QString databasePath() const;
    // Number of cookies in the Gecko database (-1: not found / not readable).
    int geckoCookieCount() const;

    // false: the downloads don't get Gecko's cookies (privacy setting).
    void setShareGeckoCookies(bool share) { m_share = share; }
    bool shareGeckoCookies() const { return m_share; }

    // Deletes cookies and stored site data of the Gecko profile that holds
    // cookies.sqlite (cookies, local storage, cache). Only safe before the
    // web view engine has started. Returns the number of removed entries.
    static int clearWebData(const QString &cookieDatabasePath);
    // Path of cookies.sqlite as found in the app's folders ("" if none).
    static QString findDatabasePath() { return findDatabase(); }

    // Only the Gecko cookies for url (exposed for tests).
    QList<QNetworkCookie> geckoCookiesForUrl(const QUrl &url) const;

private:
    bool openDb() const;
    static QString findDatabase();

    mutable QString m_path;
    mutable bool m_searched = false;
    bool m_share = true;
    QString m_connection;
};
