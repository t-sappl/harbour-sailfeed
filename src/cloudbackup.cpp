// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cloudbackup.h"

#include "configuration.h"
#include "nextcloudfiles.h"
#include "settingsbackup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace {
const QString kLastBackup = QStringLiteral("backup/lastBackup");
const QString kLastFingerprint = QStringLiteral("backup/lastFingerprint");
const QString kLinkedAccount = QStringLiteral("backup/linkedAccount");

// Copies of earlier states: sailfeed-settings-YYYY-MM-DD_HHMMSS.json (the time
// the state was saved); the day-only form of the first 0.x builds is still read.
const QRegularExpression &datedPattern()
{
    static const QRegularExpression re(QStringLiteral(
        "^sailfeed-settings-(\\d{4}-\\d{2}-\\d{2})(?:_(\\d{2})(\\d{2})(\\d{2}))?\\.json$"));
    return re;
}

QDateTime stateTimeOf(const QRegularExpressionMatch &m)
{
    const QDate d = QDate::fromString(m.captured(1), QStringLiteral("yyyy-MM-dd"));
    const QTime t = m.captured(2).isEmpty() ? QTime(0, 0)
                  : QTime(m.captured(2).toInt(), m.captured(3).toInt(), m.captured(4).toInt());
    return QDateTime(d, t, Qt::LocalTime);
}
} // namespace

CloudBackup::CloudBackup(Configuration *config, QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_files(new NextcloudFiles(config, nam, this))
    , m_autoTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
{
    m_autoTimer->setSingleShot(true);
    m_autoTimer->setInterval(10 * 1000);
    connect(m_autoTimer, &QTimer::timeout, this, &CloudBackup::autoTimeout);
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(10 * 60 * 1000);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (!linked())
            checkLink();
        else
            autoTimeout();
    });
    // Signed out or another account: no longer linked
    connect(m_config, &Configuration::isAccountValidChanged, this, [this]() {
        Q_EMIT linkedChanged();
        Q_EMIT pendingChanged();
    });
}

CloudBackup::~CloudBackup() = default;

bool CloudBackup::isBackupFile(const QString &name)
{
    return name == SettingsBackup::currentFileName() || datedPattern().match(name).hasMatch();
}

void CloudBackup::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged(busy);
}

QDateTime CloudBackup::lastBackup() const
{
    return QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).value(kLastBackup).toDateTime();
}

void CloudBackup::setBackups(const QVariantList &entries)
{
    m_backups = entries;
    m_listed = true;
    Q_EMIT backupsChanged();
}

namespace {
QVariantList toBackupList(const QList<NextcloudFiles::Entry> &entries)
{
    QVariantMap current;
    QList<QVariantMap> dated;
    for (const NextcloudFiles::Entry &e : entries) {
        if (!CloudBackup::isBackupFile(e.name))
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("modified"), e.modified.toLocalTime());
        m.insert(QStringLiteral("size"), e.size);
        const QRegularExpressionMatch match = datedPattern().match(e.name);
        m.insert(QStringLiteral("current"), !match.hasMatch());
        m.insert(QStringLiteral("date"), match.hasMatch()
                 ? QDate::fromString(match.captured(1), QStringLiteral("yyyy-MM-dd")) : QDate());
        // When the state in the file was saved (copies: from the name)
        m.insert(QStringLiteral("stateTime"), match.hasMatch() ? stateTimeOf(match) : e.modified.toLocalTime());
        if (match.hasMatch())
            dated << m;
        else
            current = m;
    }
    std::sort(dated.begin(), dated.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("name")).toString() > b.value(QStringLiteral("name")).toString();
    });
    QVariantList out;
    if (!current.isEmpty())
        out << current;
    for (const QVariantMap &m : dated)
        out << m;
    return out;
}
} // namespace

QString CloudBackup::copyName(const QDateTime &stateTime)
{
    return QStringLiteral("sailfeed-settings-%1.json")
            .arg(stateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")));
}

bool CloudBackup::backup(const QString &json, const QString &fingerprint)
{
    if (m_busy)
        return false;
    setBusy(true);
    m_autoTimer->stop();
    const QByteArray data = json.toUtf8();
    auto fail = [this](const NextcloudFiles::Result &r) {
        setLastError(r.errorString);
        setBusy(false);
        if (pending() && canBackUpAutomatically())
            m_retryTimer->start();
        Q_EMIT backupFinished(false, r.errorString);
    };

    // 1. Keep the state now on the server as a copy (at most one per
    //    copyInterval), so a backup never takes away the previous settings.
    m_files->list([this, data, fingerprint, fail](const NextcloudFiles::Result &listed) {
        if (!listed.ok) {
            fail(listed);
            return;
        }
        const QVariantList before = toBackupList(listed.entries);
        QVariantMap current;
        QDateTime newestCopy;
        for (const QVariant &v : before) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("current")).toBool())
                current = m;
            else if (!newestCopy.isValid() || m.value(QStringLiteral("stateTime")).toDateTime() > newestCopy)
                newestCopy = m.value(QStringLiteral("stateTime")).toDateTime();
        }
        const bool keepCopy = !current.isEmpty()
                && (!newestCopy.isValid() || newestCopy.secsTo(QDateTime::currentDateTime()) >= m_copyInterval);

        auto upload = [this, data, fingerprint, fail]() {
            // 2. The new latest backup
            m_files->upload(SettingsBackup::currentFileName(), data, [this, fingerprint, fail](const NextcloudFiles::Result &r) {
                if (!r.ok) {
                    fail(r);
                    return;
                }
                QSettings settings(Configuration::settingsFilePath(), QSettings::IniFormat);
                settings.setValue(kLastBackup, QDateTime::currentDateTimeUtc());
                if (!fingerprint.isEmpty())
                    settings.setValue(kLastFingerprint, fingerprint);
                settings.sync();
                Q_EMIT lastBackupChanged();
                setLastError(QString());
                m_retryTimer->stop();
                setLinked();
                Q_EMIT pendingChanged();
                // 3. Keep the newest copies; a failure here does not make the
                //    backup fail.
                m_files->list([this](const NextcloudFiles::Result &r) {
                    const QVariantList list = r.ok ? toBackupList(r.entries) : QVariantList();
                    QStringList copies;
                    for (const QVariant &v : list) {
                        const QVariantMap m = v.toMap();
                        if (!m.value(QStringLiteral("current")).toBool())
                            copies << m.value(QStringLiteral("name")).toString();
                    }
                    const QStringList surplus = copies.mid(m_maxCopies);
                    prune(surplus, 0, [this, list, surplus]() {
                        if (!list.isEmpty()) {
                            QVariantList kept;
                            for (const QVariant &v : list)
                                if (!surplus.contains(v.toMap().value(QStringLiteral("name")).toString()))
                                    kept << v;
                            setBackups(kept);
                        }
                        setBusy(false);
                        Q_EMIT backupFinished(true, QString());
                        // Changed again while uploading
                        if (pending() && canBackUpAutomatically())
                            m_autoTimer->start();
                    });
                });
            });
        };

        if (!keepCopy) {
            upload();
            return;
        }
        const QString target = copyName(current.value(QStringLiteral("stateTime")).toDateTime());
        m_files->copy(SettingsBackup::currentFileName(), target, [upload, target](const NextcloudFiles::Result &r) {
            if (!r.ok && r.httpStatus != 412)       // 412: that copy exists already
                qWarning("Could not keep the previous settings as %s: %s", qPrintable(target),
                         qPrintable(r.errorString));
            upload();
        });
    });
    return true;
}

// --- automatic backup and linking ---------------------------------------------

void CloudBackup::setAutoDelay(int ms)
{
    m_autoTimer->setInterval(ms);
}

void CloudBackup::setRetryDelay(int ms)
{
    m_retryTimer->setInterval(ms);
}

void CloudBackup::setAutoEnabled(bool enabled)
{
    if (m_autoEnabled == enabled)
        return;
    m_autoEnabled = enabled;
    Q_EMIT autoEnabledChanged(enabled);
    if (!enabled) {
        m_autoTimer->stop();
        m_retryTimer->stop();
    } else if (pending() && canBackUpAutomatically()) {
        m_autoTimer->start();
    }
}

void CloudBackup::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    Q_EMIT lastErrorChanged();
}

bool CloudBackup::linked() const
{
    if (!m_config->isAccountValid())
        return false;
    return QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).value(kLinkedAccount).toString()
            == m_files->accountTag();
}

void CloudBackup::setLinked()
{
    if (!m_config->isAccountValid() || linked())
        return;
    QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).setValue(kLinkedAccount, m_files->accountTag());
    Q_EMIT linkedChanged();
    if (pending() && canBackUpAutomatically() && !m_busy)
        m_autoTimer->start();
}

bool CloudBackup::pending() const
{
    if (m_fingerprint.isEmpty())
        return false;
    return QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).value(kLastFingerprint).toString()
            != m_fingerprint;
}

bool CloudBackup::canBackUpAutomatically() const
{
    return m_autoEnabled && m_config->isAccountValid() && linked();
}

void CloudBackup::noteSettings(const QString &fingerprint)
{
    if (fingerprint == m_fingerprint)
        return;
    m_fingerprint = fingerprint;
    Q_EMIT pendingChanged();
    if (pending() && canBackUpAutomatically())
        m_autoTimer->start();               // restarted by every change
    else
        m_autoTimer->stop();
}

void CloudBackup::flush()
{
    if (!pending() || !canBackUpAutomatically() || m_busy)
        return;
    m_autoTimer->stop();
    Q_EMIT autoBackupRequested();
}

void CloudBackup::autoTimeout()
{
    if (!pending() || !canBackUpAutomatically())
        return;
    if (m_busy) {
        m_autoTimer->start();               // try again once the page is done
        return;
    }
    Q_EMIT autoBackupRequested();
}

void CloudBackup::checkLink()
{
    if (!m_config->isAccountValid() || linked() || m_checkingLink)
        return;
    m_checkingLink = true;
    m_files->list([this](const NextcloudFiles::Result &r) {
        m_checkingLink = false;
        if (!r.ok) {
            m_retryTimer->start();          // e.g. offline at start: look again later
            return;
        }
        setBackups(toBackupList(r.entries));
        bool hasBackup = false;
        for (const NextcloudFiles::Entry &e : r.entries)
            hasBackup = hasBackup || e.name == SettingsBackup::currentFileName();
        if (hasBackup)
            Q_EMIT restoreOffer();
        else
            setLinked();                    // nothing to lose on the server
    });
}

void CloudBackup::prune(const QStringList &names, int index, const std::function<void()> &done)
{
    if (index >= names.size()) {
        done();
        return;
    }
    m_files->remove(names.at(index), [this, names, index, done](const NextcloudFiles::Result &r) {
        if (!r.ok)
            qWarning("Could not delete old settings backup %s: %s", qPrintable(names.at(index)),
                     qPrintable(r.errorString));
        prune(names, index + 1, done);
    });
}

bool CloudBackup::refresh()
{
    if (m_busy)
        return false;
    setBusy(true);
    m_files->list([this](const NextcloudFiles::Result &r) {
        if (r.ok)
            setBackups(toBackupList(r.entries));
        setBusy(false);
        Q_EMIT refreshed(r.ok, r.errorString);
    });
    return true;
}

bool CloudBackup::fetch(const QString &name)
{
    if (m_busy || !isBackupFile(name))
        return false;
    setBusy(true);
    m_files->download(name, [this, name](const NextcloudFiles::Result &r) {
        setBusy(false);
        Q_EMIT fetched(name, r.ok, r.ok ? QString::fromUtf8(r.data) : QString(), r.errorString);
    });
    return true;
}

QString CloudBackup::localCopyPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/settings-before-restore.json");
}

void CloudBackup::saveLocalCopy(const QString &json)
{
    const QString path = localCopyPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(json.toUtf8()) < 0 || !f.commit()) {
        qWarning("Could not save the settings before restoring: %s", qPrintable(f.errorString()));
        return;
    }
    Q_EMIT localCopyChanged();
}

QString CloudBackup::localCopy() const
{
    QFile f(localCopyPath());
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QDateTime CloudBackup::localCopyTime() const
{
    const QFileInfo info(localCopyPath());
    return info.exists() ? info.lastModified() : QDateTime();
}
