// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "opmlfile.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

class Configuration;
class NextcloudFiles;
class QNetworkAccessManager;
namespace Fuoten { class AbstractStorage; }

// Import and export of the feed list as an OPML file (1.4). Available in
// QML as "feedTransfer".
//
// Export: the feeds and folders of the local database (the same as on the
// server after a sync) as OPML (OpmlFile::write)
//   - to the user's Nextcloud: Sailfeed/sailfeed-feeds.opml (WebDAV, like
//     the settings backup; replaced by the next export, Nextcloud keeps
//     versions),
//   - to the Downloads folder: sailfeed-feeds-<date>.opml (a counter when
//     the name exists; Sailjail permission "Downloads"),
//   - shared through the Sailfish share dialog (QML: ShareAction with the
//     text of opmlText()).
// Import: a file from the Nextcloud folder Sailfeed/ or from Downloads is
// parsed into a plan (what is new, what is subscribed already, which folders
// would be created). startImport() then subscribes the new feeds one after
// another on the server (CreateFolder for missing folders, CreateFeed), so
// the server checks every feed itself. Feeds already subscribed – by address
// (FeedFinder::compareKey) or because the server says so (409) – are
// skipped. The import goes on when the page is left; importFinished() tells
// QML to sync and reload the feed list.
class FeedTransfer : public QObject
{
    Q_OBJECT
    // A Nextcloud file operation (list, download, upload) is running
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    Q_PROPERTY(int importDone READ importDone NOTIFY importProgressChanged)
    Q_PROPERTY(int importTotal READ importTotal NOTIFY importProgressChanged)
    Q_PROPERTY(QString importCurrent READ importCurrent NOTIFY importProgressChanged)
    // OPML files in Sailfeed/ on the Nextcloud: [{ name, modified, size }], newest first
    Q_PROPERTY(QVariantList nextcloudFiles READ nextcloudFiles NOTIFY nextcloudFilesChanged)
    // Result of the last import: { added, skipped, failed: [{ title, url, reason }], cancelled, time }
    Q_PROPERTY(QVariantMap lastImport READ lastImport NOTIFY lastImportChanged)
    Q_PROPERTY(QString downloadsPath READ downloadsPath CONSTANT)
public:
    static constexpr qint64 maxFileSize = 5 * 1024 * 1024;

    explicit FeedTransfer(Configuration *config, QNetworkAccessManager *nam = nullptr,
                          QObject *parent = nullptr);
    ~FeedTransfer() override;

    // Tests: another storage than libfuoten's default, another Downloads folder
    void setStorage(Fuoten::AbstractStorage *storage) { m_storage = storage; }
    void setDownloadsPath(const QString &path) { m_downloads = path; }
    NextcloudFiles *files() const { return m_files; }

    bool busy() const { return m_busy; }
    bool importing() const { return m_importing; }
    int importDone() const { return m_done; }
    int importTotal() const { return m_total; }
    QString importCurrent() const { return m_current; }
    QVariantList nextcloudFiles() const { return m_ncFiles; }
    QVariantMap lastImport() const { return m_lastImport; }
    QString downloadsPath() const { return m_downloads; }

    // Number of feeds and folders on the device: { feeds, folders }
    Q_INVOKABLE QVariantMap summary() const;
    // The feed list as OPML text
    Q_INVOKABLE QString opmlText() const;
    // Uploads the OPML file to the Nextcloud. Ends with exported().
    Q_INVOKABLE bool exportToNextcloud();
    // Writes the OPML file to the Downloads folder: { ok, name, error }
    Q_INVOKABLE QVariantMap exportToDownloads();

    // OPML files in the Downloads folder: [{ name, path, modified, size }], newest first
    Q_INVOKABLE QVariantList downloadFiles() const;
    // Lists the OPML files in Sailfeed/ on the Nextcloud. Ends with nextcloudListed().
    Q_INVOKABLE bool listNextcloud();
    // Downloads and reads a file of the Nextcloud folder. Ends with loaded().
    Q_INVOKABLE bool loadNextcloud(const QString &name);
    // Reads a file of the Downloads folder; returns the plan (as loaded())
    Q_INVOKABLE QVariantMap loadFile(const QString &path);

    // The plan for a file's content: { ok, error, title, total, subscribed,
    // newCount, newFolders: [name], feeds: [{ title, url, folder, subscribed }] }.
    // Keeps the new feeds for startImport().
    Q_INVOKABLE QVariantMap plan(const QByteArray &data);
    // Subscribes the new feeds of the last plan. keepFolders: in their
    // folders (created when missing), otherwise without a folder.
    Q_INVOKABLE bool startImport(bool keepFolders);
    // Stops after the feed being subscribed
    Q_INVOKABLE void cancelImport();

    static bool isOpmlName(const QString &name);

Q_SIGNALS:
    void busyChanged(bool busy);
    void importingChanged(bool importing);
    void importProgressChanged();
    void nextcloudFilesChanged();
    void lastImportChanged();
    // Export to the Nextcloud: name of the file, message = error text
    void exported(bool ok, const QString &name, const QString &message);
    void nextcloudListed(bool ok, const QString &message);
    void loaded(const QString &name, bool ok, const QString &message, const QVariantMap &plan);
    void importFinished(int added, int skipped, int failed, bool cancelled);

private:
    Fuoten::AbstractStorage *storage() const;
    QList<OpmlFile::Feed> localFeeds(QStringList *emptyFolders = nullptr) const;
    QHash<QString, qint64> localFolderIds() const;
    QSet<QString> localFeedKeys() const;
    void setBusy(bool busy);
    void step();
    void createFeed(const OpmlFile::Feed &feed, qint64 folderId);
    void finishImport();

    Configuration *m_config;
    NextcloudFiles *m_files;
    Fuoten::AbstractStorage *m_storage = nullptr;
    QString m_downloads;
    bool m_busy = false;
    QVariantList m_ncFiles;

    // Import
    QList<OpmlFile::Feed> m_planned;    // new feeds of the last plan
    QList<OpmlFile::Feed> m_queue;
    QHash<QString, qint64> m_folderIds; // case folded name -> id
    QSet<QString> m_failedFolders;      // case folded names that could not be created
    bool m_keepFolders = true;
    bool m_importing = false;
    bool m_cancel = false;
    int m_done = 0;
    int m_total = 0;
    int m_added = 0;
    int m_skipped = 0;
    QVariantList m_failures;
    QString m_current;
    QVariantMap m_lastImport;
};
