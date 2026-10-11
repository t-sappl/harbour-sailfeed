// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

class Configuration;
class QNetworkAccessManager;
class QNetworkReply;

// WebDAV access to one folder in the user's Nextcloud files (default
// "Sailfeed/"), with the account of the News app: server address, user name,
// app password and the "accept self-signed certificates" option. Used for
// the settings backup (release 1.0, package 5).
//
// The WebDAV path contains the internal user ID
// (remote.php/dav/files/<user id>/), which can differ from the login name
// (e.g. signing in with the e-mail address or an LDAP name). It is asked
// from the server once (OCS API cloud/user) and kept in the settings file
// for this account.
//
// All operations are asynchronous and end with exactly one callback call.
// Requests are sent with Basic authentication right away and time out after
// 20 s. C++ instead of QML's XMLHttpRequest, which does not reliably
// support the WebDAV methods in Qt 5.6 (same reason as in Sailwave).
class NextcloudFiles : public QObject
{
    Q_OBJECT
public:
    struct Entry {
        QString name;           // file name in the folder
        QDateTime modified;     // last modification (UTC)
        qint64 size = 0;
        QString etag;
    };

    struct Result {
        bool ok = false;
        int httpStatus = 0;     // 0: no reply (network error, timeout)
        int networkError = 0;   // QNetworkReply::NetworkError
        QByteArray data;        // download: file content
        QString etag;           // upload/download: the file's ETag
        QList<Entry> entries;   // list: files of the folder, newest first
        QString errorString;    // understandable text for the user (empty if ok)
        bool notFound() const { return httpStatus == 404; }
    };
    using Callback = std::function<void(const Result &)>;

    explicit NextcloudFiles(Configuration *config, QNetworkAccessManager *nam = nullptr,
                            QObject *parent = nullptr);
    ~NextcloudFiles() override;

    // Folder below the user's files root, without slashes (default "Sailfeed")
    QString folderName() const { return m_folder; }
    void setFolderName(const QString &name) { m_folder = name; }

    // Request timeout in ms (default 20000; shorter in tests)
    void setTimeout(int ms) { m_timeoutMs = ms; }

    // Internal user ID of the current account, empty until resolved
    QString userId() const;
    // Asks the server for the user ID (OCS cloud/user), or answers from the
    // cache when it belongs to the current server and user name.
    void resolveUserId(const Callback &done);
    // Forgets the cached user ID (e.g. after an account change)
    void clearUserId();

    // "user@server": identifies the account (cache of the user ID, link of
    // the settings backup)
    QString accountTag() const;

    // https://server/remote.php/dav/files/<user id>/<folder>/ (empty URL
    // while the user ID is unknown)
    QUrl folderUrl() const;

    // Creates the folder if it does not exist (PROPFIND, then MKCOL)
    void ensureFolder(const Callback &done);
    // Files of the folder (PROPFIND depth 1), newest first. A missing folder
    // is an empty list, not an error.
    void list(const Callback &done);
    // Downloads a file; a missing file ends with ok = false and notFound().
    void download(const QString &fileName, const Callback &done);
    // Uploads (creates or replaces) a file; creates the folder if needed.
    void upload(const QString &fileName, const QByteArray &data, const Callback &done,
                const QString &contentType = QStringLiteral("application/json; charset=utf-8"));
    // Deletes a file; a file that is already gone counts as success.
    void remove(const QString &fileName, const Callback &done);
    // Copies a file inside the folder on the server (WebDAV COPY, nothing is
    // downloaded); an existing target is not overwritten (412 → ok = false).
    void copy(const QString &fromName, const QString &toName, const Callback &done);

    // Parses a WebDAV multistatus reply: the files (not folders) directly
    // inside the requested folder, newest first. Public for the tests.
    static QList<Entry> parseMultistatus(const QByteArray &xml);
    // Turns a failed request into a text for the user.
    static QString describe(int networkError, int httpStatus, const QString &details);

private:
    using RawCallback = std::function<void(QNetworkReply *reply)>;
    void send(const QByteArray &verb, const QUrl &url, const QByteArray &body,
              const QList<QPair<QByteArray, QByteArray>> &headers, const RawCallback &done);
    // Runs action once the user ID is known (or reports the failure)
    void withUserId(const Callback &fail, const std::function<void()> &action);
    QUrl fileUrl(const QString &fileName) const;
    static Result resultFrom(QNetworkReply *reply);

    Configuration *m_config;
    QNetworkAccessManager *m_nam;
    QString m_folder = QStringLiteral("Sailfeed");
    int m_timeoutMs = 20000;
};
