// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QVariantList>

#include <functional>

class Configuration;
class NextcloudFiles;
class QNetworkAccessManager;
class QTimer;

// Settings backup on the user's Nextcloud (release 1.0, package 5c): the
// file operations behind the backup and restore pages. Available in QML as
// "cloudBackup". The content (JSON) comes from SettingsBackup.
//
// Files in Sailfeed/ on the Nextcloud:
//   sailfeed-settings.json                    the latest backup
//   sailfeed-settings-YYYY-MM-DD_HHMMSS.json  earlier states: before every
//       backup, the state on the server is copied (WebDAV COPY, named after
//       the time it was saved) unless the newest copy is younger than
//       copyInterval (1 h); the newest maxCopies (10) are kept. So a backup
//       never takes away the settings before a change (like Sailwave's
//       backup states; device test 11 Oct: with one copy per day the copy
//       always equalled the latest backup).
// Before restoring, the current settings are kept on the device
// (settings-before-restore.json in the app's data folder), so a restore can
// be undone.
//
// Automatic backup (5d): QML reports a fingerprint of the backed-up settings
// (SettingsBackup.fingerprint) whenever it changes (noteSettings). If it
// differs from the last backup, autoBackupRequested() follows after
// autoDelay (default 10 s, restarted by every change), or at once with
// flush() (app goes to the background). A failed backup is retried after
// retryDelay (default 10 min), at the next change, start or flush.
//
// Linking (5e): a device only backs up automatically once it is "linked" to
// the account: after a backup or a restore made by hand, after the restore
// offer was answered, or when the server has no backup yet. Otherwise a new
// device would overwrite the backup on the server with its default settings
// before the user could restore it. checkLink() (at start and after
// signing in) looks at the server and emits restoreOffer() when there is a
// backup to offer.
class CloudBackup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Files on the server: [{ name, modified, size, current, date, stateTime }],
    // the latest backup first, then the copies of earlier states, newest first
    Q_PROPERTY(QVariantList backups READ backups NOTIFY backupsChanged)
    Q_PROPERTY(bool listed READ listed NOTIFY backupsChanged)
    Q_PROPERTY(QDateTime lastBackup READ lastBackup NOTIFY lastBackupChanged)
    Q_PROPERTY(QDateTime localCopyTime READ localCopyTime NOTIFY localCopyChanged)
    // Automatic backup (setting) and its state
    Q_PROPERTY(bool autoEnabled READ autoEnabled WRITE setAutoEnabled NOTIFY autoEnabledChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
    // Error of the last backup (empty after a successful one)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool linked READ linked NOTIFY linkedChanged)
public:
    static constexpr int defaultMaxCopies = 10;
    static constexpr int defaultCopyInterval = 3600;     // seconds

    explicit CloudBackup(Configuration *config, QNetworkAccessManager *nam = nullptr,
                         QObject *parent = nullptr);
    ~CloudBackup() override;

    NextcloudFiles *files() const { return m_files; }

    bool busy() const { return m_busy; }
    QVariantList backups() const { return m_backups; }
    bool listed() const { return m_listed; }
    QDateTime lastBackup() const;
    QDateTime localCopyTime() const;

    // Keeps the state on the server as a copy (see above), uploads the new
    // latest backup, then removes copies beyond maxCopies. Ends with
    // backupFinished(). Returns false (and does nothing) while another
    // operation runs. fingerprint: of the settings in json (the automatic
    // backup compares it). A successful backup links the device.
    Q_INVOKABLE bool backup(const QString &json, const QString &fingerprint = QString());

    // For tests: how many copies to keep, and the minimum age (s) of the
    // newest copy before another one is made
    void setMaxCopies(int count) { m_maxCopies = count; }
    void setCopyInterval(int seconds) { m_copyInterval = seconds; }
    static QString copyName(const QDateTime &stateTime);

    bool autoEnabled() const { return m_autoEnabled; }
    void setAutoEnabled(bool enabled);
    bool pending() const;
    QString lastError() const { return m_lastError; }
    bool linked() const;
    void setAutoDelay(int ms);
    void setRetryDelay(int ms);

    // The backed-up settings now have this fingerprint
    Q_INVOKABLE void noteSettings(const QString &fingerprint);
    // Back up now if something is pending (app goes to the background)
    Q_INVOKABLE void flush();
    // This device's settings belong to the account now (backed up, restored,
    // or the restore offer was declined)
    Q_INVOKABLE void setLinked();
    // Not linked yet: is there a backup on the server to offer?
    Q_INVOKABLE void checkLink();
    // Lists the backups on the server. Ends with refreshed().
    Q_INVOKABLE bool refresh();
    // Downloads one backup. Ends with fetched().
    Q_INVOKABLE bool fetch(const QString &name);

    // Local copy of the settings before a restore
    Q_INVOKABLE void saveLocalCopy(const QString &json);
    Q_INVOKABLE QString localCopy() const;

    static bool isBackupFile(const QString &name);

Q_SIGNALS:
    void busyChanged(bool busy);
    void backupsChanged();
    void lastBackupChanged();
    void localCopyChanged();
    // message: understandable error text (empty if ok)
    void backupFinished(bool ok, const QString &message);
    void refreshed(bool ok, const QString &message);
    void fetched(const QString &name, bool ok, const QString &content, const QString &message);
    void autoEnabledChanged(bool enabled);
    void pendingChanged();
    void lastErrorChanged();
    void linkedChanged();
    // Time for an automatic backup: QML calls backup() with the current settings
    void autoBackupRequested();
    // The server has a backup and this device is not linked yet: offer to
    // restore it (latest backup: SettingsBackup::currentFileName())
    void restoreOffer();

private:
    void setBusy(bool busy);
    void setBackups(const QVariantList &entries);
    void prune(const QStringList &dated, int index, const std::function<void()> &done);
    QString localCopyPath() const;
    bool canBackUpAutomatically() const;
    void autoTimeout();
    void setLastError(const QString &error);

    Configuration *m_config;
    NextcloudFiles *m_files;
    bool m_busy = false;
    bool m_listed = false;
    QVariantList m_backups;
    int m_maxCopies = defaultMaxCopies;
    int m_copyInterval = defaultCopyInterval;

    bool m_autoEnabled = true;
    QString m_fingerprint;          // of the current settings
    bool m_checkingLink = false;
    QString m_lastError;
    QTimer *m_autoTimer;
    QTimer *m_retryTimer;
};
