// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "articlecleaner.h"

#include "configuration.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

const QString kMonths = QStringLiteral("storage/deleteReadAfterMonths");
const QString kHint = QStringLiteral("storage/cleanupHintPending");
const QString kLastRun = QStringLiteral("storage/lastCleanup");           // ISO date
const QString kLastCount = QStringLiteral("storage/lastCleanupDeleted");
// Set by a changed setting: the next start cleans up even if it ran today
const QString kForce = QStringLiteral("storage/cleanupAtNextStart");
const QString kImageCache = QStringLiteral("storage/imageCacheMB");

// Files of a QNetworkDiskCache directory, without "prepared" (downloads
// being written right now)
template <typename F>
void forEachCacheFile(const QString &dir, F f)
{
    QDirIterator it(dir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    const QString prepared = QDir(dir).filePath(QStringLiteral("prepared")) + QLatin1Char('/');
    while (it.hasNext()) {
        const QString path = it.next();
        if (!path.startsWith(prepared))
            f(path, it.fileInfo());
    }
}

// A connection of its own, closed again before libfuoten opens the file
class Connection
{
public:
    Connection(const QString &path, const QString &name, bool readOnly)
        : m_name(name)
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        db.setDatabaseName(path);
        if (readOnly)
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=500"));
        m_ok = db.open();
        if (!m_ok)
            qWarning("Cleanup: cannot open %s: %s", qUtf8Printable(path), qUtf8Printable(db.lastError().text()));
    }
    ~Connection()
    {
        {
            QSqlDatabase db = QSqlDatabase::database(m_name, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_name);
    }
    bool ok() const { return m_ok; }
    QSqlDatabase db() const { return QSqlDatabase::database(m_name, false); }

private:
    QString m_name;
    bool m_ok = false;
};

} // namespace

ArticleCleaner::ArticleCleaner(QObject *parent)
    : QObject(parent)
    , m_databasePath(defaultDatabasePath())
    , m_settingsPath(Configuration::settingsFilePath())
    , m_imageCacheDir(defaultImageCacheDir())
{
}

void ArticleCleaner::setPaths(const QString &databasePath, const QString &settingsPath,
                              const QString &imageCacheDir)
{
    m_databasePath = databasePath;
    m_settingsPath = settingsPath;
    if (!imageCacheDir.isEmpty())
        m_imageCacheDir = imageCacheDir;
    Q_EMIT changed();
}

QString ArticleCleaner::defaultImageCacheDir()
{
    // As in main(): ~/.cache/org.sappl/harbour-sailfeed/images
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/images");
}

int ArticleCleaner::imageCacheMB() const
{
    return QSettings(m_settingsPath, QSettings::IniFormat).value(kImageCache, defaultImageCacheMB).toInt();
}

void ArticleCleaner::setImageCacheMB(int mb)
{
    mb = qBound(20, mb, 1000);
    if (mb == imageCacheMB())
        return;
    QSettings(m_settingsPath, QSettings::IniFormat).setValue(kImageCache, mb);
    Q_EMIT changed();
}

qint64 ArticleCleaner::imageCacheLimit(const QString &settingsPath)
{
    const int mb = qBound(20, QSettings(settingsPath, QSettings::IniFormat).value(kImageCache, defaultImageCacheMB).toInt(), 1000);
    return qint64(mb) * 1024 * 1024;
}

double ArticleCleaner::imageCacheBytes() const
{
    qint64 total = 0;
    forEachCacheFile(m_imageCacheDir, [&total](const QString &, const QFileInfo &fi) { total += fi.size(); });
    return static_cast<double>(total);
}

double ArticleCleaner::clearImageCache()
{
    // The cache objects of the running image loaders keep working: a missing
    // file is a cache miss, the image is downloaded again when needed.
    qint64 freed = 0;
    forEachCacheFile(m_imageCacheDir, [&freed](const QString &path, const QFileInfo &fi) {
        const qint64 size = fi.size();
        if (QFile::remove(path))
            freed += size;
    });
    qInfo("Image cache cleared: %lld bytes", static_cast<long long>(freed));
    return static_cast<double>(freed);
}

QString ArticleCleaner::defaultDatabasePath()
{
    // As in main(): ~/.local/share/org.sappl/harbour-sailfeed/database.sqlite
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/database.sqlite");
}

int ArticleCleaner::deleteAfterMonths() const
{
    return QSettings(m_settingsPath, QSettings::IniFormat).value(kMonths, 0).toInt();
}

void ArticleCleaner::setDeleteAfterMonths(int months)
{
    months = qBound(0, months, 12);
    if (months == deleteAfterMonths())
        return;
    QSettings s(m_settingsPath, QSettings::IniFormat);
    s.setValue(kMonths, months);
    // A changed setting answers the hint
    s.remove(kHint);
    if (months > 0)
        s.setValue(kForce, true);
    Q_EMIT changed();
}

bool ArticleCleaner::hintPending() const
{
    return QSettings(m_settingsPath, QSettings::IniFormat).value(kHint, false).toBool();
}

void ArticleCleaner::dismissHint()
{
    if (!hintPending())
        return;
    QSettings(m_settingsPath, QSettings::IniFormat).remove(kHint);
    Q_EMIT changed();
}

QString ArticleCleaner::storageInfo() const
{
    const QLocale locale;
    QStringList lines;

    const QFileInfo file(m_databasePath);
    if (file.exists()) {
        // Shown in MB with one decimal below 10 MB
        const double mb = file.size() / (1024.0 * 1024.0);
        const QString size = locale.toString(mb, 'f', mb < 10 ? 1 : 0);
        qint64 count = -1;
        {
            Connection c(m_databasePath, QStringLiteral("sailfeed-cleanup-info"), true);
            if (c.ok()) {
                QSqlQuery q(c.db());
                if (q.exec(QStringLiteral("SELECT COUNT(*) FROM items")) && q.next())
                    count = q.value(0).toLongLong();
            }
        }
        // Two strings instead of %n: there is no English translation file
        // to resolve plural forms (see Prefetcher::cookieInfo)
        if (count == 1)
            lines << tr("1 article stored (%1 MB)").arg(size);
        else if (count >= 0)
            lines << tr("%1 articles stored (%2 MB)").arg(locale.toString(count), size);
        else
            lines << tr("Database: %1 MB").arg(size);
    }

    QSettings s(m_settingsPath, QSettings::IniFormat);
    const QDate last = QDate::fromString(s.value(kLastRun).toString(), Qt::ISODate);
    if (last.isValid()) {
        const int n = s.value(kLastCount, 0).toInt();
        const QString date = locale.toString(last, QLocale::ShortFormat);
        if (n == 0)
            lines << tr("Last cleanup %1: nothing to delete").arg(date);
        else if (n == 1)
            lines << tr("Last cleanup %1: 1 article deleted").arg(date);
        else
            lines << tr("Last cleanup %1: %2 articles deleted").arg(date, locale.toString(n));
    }
    return lines.join(QLatin1Char('\n'));
}

void ArticleCleaner::initDefault(const QString &settingsPath, bool databaseExists)
{
    QSettings s(settingsPath, QSettings::IniFormat);
    if (s.contains(kMonths))
        return;
    if (databaseExists) {
        // Update: nobody loses articles by surprise (author, 15 Oct)
        s.setValue(kMonths, 0);
        s.setValue(kHint, true);
    } else {
        s.setValue(kMonths, defaultNewInstall);
    }
}

bool ArticleCleaner::isDue(const QString &settingsPath, const QDate &today)
{
    QSettings s(settingsPath, QSettings::IniFormat);
    if (s.value(kMonths, 0).toInt() <= 0)
        return false;
    const QDate last = QDate::fromString(s.value(kLastRun).toString(), Qt::ISODate);
    // A date in the future (clock was wrong) does not block the cleanup
    return !last.isValid() || last != today || s.value(kForce, false).toBool();
}

int ArticleCleaner::runIfDue(const QString &databasePath, const QString &settingsPath, const QDateTime &now)
{
    if (!isDue(settingsPath, now.date()) || !QFileInfo::exists(databasePath))
        return -1;
    const int months = qBound(1, QSettings(settingsPath, QSettings::IniFormat).value(kMonths).toInt(), 12);
    // QDateTime::toSecsSinceEpoch() needs Qt 5.8 (the device has 5.6)
    const qint64 cutoff = now.toUTC().addMonths(-months).toMSecsSinceEpoch() / 1000;
    bool vacuumed = false;
    const int n = deleteOld(databasePath, cutoff, &vacuumed);
    if (n < 0)
        return -1;      // tried again at the next start
    QSettings s(settingsPath, QSettings::IniFormat);
    s.setValue(kLastRun, now.date().toString(Qt::ISODate));
    s.setValue(kLastCount, n);
    s.remove(kForce);
    qInfo("Cleanup: %d read articles older than %d month(s) deleted%s", n, months, vacuumed ? ", database compacted" : "");
    return n;
}

int ArticleCleaner::deleteOld(const QString &databasePath, qint64 cutoff, bool *vacuumed)
{
    if (vacuumed)
        *vacuumed = false;
    Connection c(databasePath, QStringLiteral("sailfeed-cleanup"), false);
    if (!c.ok())
        return -1;
    QSqlQuery q(c.db());
    // queue: a read/star change not yet sent to the server – the item has to
    // stay until the sync took it (libfuoten reads the queue from the items)
    if (!q.prepare(QStringLiteral("DELETE FROM items WHERE unread = 0 AND starred = 0 "
                                  "AND (queue IS NULL OR queue = 0) AND pubDate < ?"))) {
        qWarning("Cleanup: %s", qUtf8Printable(q.lastError().text()));
        return -1;
    }
    q.addBindValue(cutoff);
    if (!q.exec()) {
        qWarning("Cleanup: %s", qUtf8Printable(q.lastError().text()));
        return -1;
    }
    const int n = q.numRowsAffected();
    q.finish();
    // Give the space back to the system after a big cleanup (the first one,
    // or after the setting got shorter); small daily ones reuse free pages.
    if (n > vacuumThreshold) {
        QSqlQuery v(c.db());
        if (v.exec(QStringLiteral("VACUUM"))) {
            if (vacuumed)
                *vacuumed = true;
        } else {
            qWarning("Cleanup: VACUUM failed: %s", qUtf8Printable(v.lastError().text()));
        }
    }
    return n;
}
