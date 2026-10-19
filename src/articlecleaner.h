// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>

// Deletes old read articles from the local database (DESIGN "Deleting old
// read articles"). Runs at the start in main(), before libfuoten opens the
// database, at most once a day: no concurrent access, no rows vanishing
// under the finger. Bookmarked articles, unread articles and articles with a
// change not yet sent to the server (queue) always stay.
// The setting lives in the settings file (main() reads it before the start).
// QML singleton "Cleanup" for the settings page and the one-time hint.
class ArticleCleaner : public QObject
{
    Q_OBJECT
    // 0 = never, otherwise months (1, 3, 6, 12 in the settings)
    Q_PROPERTY(int deleteAfterMonths READ deleteAfterMonths WRITE setDeleteAfterMonths NOTIFY changed)
    // Update from a version without the cleanup: show the hint once
    Q_PROPERTY(bool hintPending READ hintPending NOTIFY changed)
    // Image cache (thumbnails, article images, favicons): maximum size in
    // MB (50, 100, 250, 500 in the settings; read by main() at the start)
    Q_PROPERTY(int imageCacheMB READ imageCacheMB WRITE setImageCacheMB NOTIFY changed)
public:
    explicit ArticleCleaner(QObject *parent = nullptr);

    int deleteAfterMonths() const;
    void setDeleteAfterMonths(int months);
    bool hintPending() const;
    Q_INVOKABLE void dismissHint();
    // "1,234 articles stored (48 MB)" and the last cleanup, for the settings page
    Q_INVOKABLE QString storageInfo() const;

    int imageCacheMB() const;
    void setImageCacheMB(int mb);
    // Bytes in the image cache now
    Q_INVOKABLE double imageCacheBytes() const;
    // Deletes the cached images now (not the files being written). Returns
    // the bytes freed.
    Q_INVOKABLE double clearImageCache();
    static QString defaultImageCacheDir();
    static qint64 imageCacheLimit(const QString &settingsPath);   // bytes, for main()
    static constexpr int defaultImageCacheMB = 100;

    // --- used by main() (and the tests) ----------------------------------
    static QString defaultDatabasePath();
    // While the setting is unset: a database already there means an update
    // (default "never" and the hint), otherwise a new installation (3 months).
    static void initDefault(const QString &settingsPath, bool databaseExists);
    // The cleanup is switched on and did not run today
    static bool isDue(const QString &settingsPath, const QDate &today);
    // Runs the cleanup if due and notes the date and the count. Returns the
    // number of deleted articles, -1 if it did not run or failed.
    static int runIfDue(const QString &databasePath, const QString &settingsPath, const QDateTime &now);
    // The cleanup itself: deletes read, not starred, not queued articles
    // published before cutoff (seconds since the epoch). VACUUM after more
    // than vacuumThreshold deletions. Returns the count, -1 on errors.
    static int deleteOld(const QString &databasePath, qint64 cutoff, bool *vacuumed = nullptr);

    static constexpr int defaultNewInstall = 3;
    static constexpr int vacuumThreshold = 300;

    // For tests: other files than the app's
    void setPaths(const QString &databasePath, const QString &settingsPath,
                  const QString &imageCacheDir = QString());

Q_SIGNALS:
    void changed();

private:
    QString m_databasePath;
    QString m_settingsPath;
    QString m_imageCacheDir;
};
