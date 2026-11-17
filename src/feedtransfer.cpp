// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "feedtransfer.h"

#include "configuration.h"
#include "feedfinder.h"
#include "nextcloudfiles.h"

#include <Fuoten/API/Component>
#include <Fuoten/API/CreateFeed>
#include <Fuoten/API/CreateFolder>
#include <Fuoten/Error>
#include <Fuoten/Feed>
#include <Fuoten/Folder>
#include <Fuoten/FuotenEnums>
#include <Fuoten/Storage/AbstractStorage>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <functional>

namespace {

// libfuoten's error for 409 on CreateFeed (same catalog, same text). The id
// in a variable, so lupdate does not copy libfuoten's message into the
// app's translation file.
const char *const feedExistsId = "libfuoten-err-feed-exists";

bool isFeedExists(const Fuoten::Error *e)
{
    return e && e->text() == qtTrId(feedExistsId);
}

QString errorText(const Fuoten::Error *e)
{
    if (!e)
        return QString();
    QString text = e->text();
    if (!e->data().isEmpty())
        text += QStringLiteral(" (") + e->data() + QLatin1Char(')');
    return text;
}

// Runs a libfuoten component once and calls done(error, result); error is
// null on success. CreateFeed and CreateFolder end with succeeded(json)
// (after the storage was updated) or failed(error) – also for invalid
// input before any request.
void runComponent(Fuoten::Component *c, QObject *context,
                  const std::function<void(const Fuoten::Error *error, const QJsonDocument &json)> &done)
{
    auto finished = QSharedPointer<bool>::create(false);
    QObject::connect(c, &Fuoten::Component::succeeded, context, [c, finished, done](const QJsonDocument &result) {
        if (*finished)
            return;
        *finished = true;
        c->deleteLater();
        done(nullptr, result);
    });
    QObject::connect(c, &Fuoten::Component::failed, context, [c, finished, done](Fuoten::Error *e) {
        if (*finished)
            return;
        *finished = true;
        c->deleteLater();
        if (e) {
            done(e, QJsonDocument());
        } else {
            Fuoten::Error unknown(Fuoten::Error::RequestError, Fuoten::Error::Critical, QString(), QString());
            done(&unknown, QJsonDocument());
        }
    });
    c->execute();
}

} // namespace

FeedTransfer::FeedTransfer(Configuration *config, QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_files(new NextcloudFiles(config, nam, this))
    , m_downloads(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
{
}

FeedTransfer::~FeedTransfer() = default;

Fuoten::AbstractStorage *FeedTransfer::storage() const
{
    Fuoten::AbstractStorage *s = m_storage ? m_storage : Fuoten::Component::defaultStorage();
    return (s && s->ready()) ? s : nullptr;
}

void FeedTransfer::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged(busy);
}

bool FeedTransfer::isOpmlName(const QString &name)
{
    return name.endsWith(QLatin1String(".opml"), Qt::CaseInsensitive)
        || name.endsWith(QLatin1String(".xml"), Qt::CaseInsensitive);
}

// Folders by name, feeds by folder and title (case folded); emptyFolders:
// the folders without feeds
QList<OpmlFile::Feed> FeedTransfer::localFeeds(QStringList *emptyFolders) const
{
    QList<OpmlFile::Feed> out;
    Fuoten::AbstractStorage *s = storage();
    if (!s)
        return out;
    QHash<qint64, QString> folderNames;
    const QList<Fuoten::Folder *> folders = s->getFolders(Fuoten::FuotenEnums::Name, Qt::AscendingOrder);
    for (Fuoten::Folder *f : folders)
        folderNames.insert(f->id(), f->name());
    qDeleteAll(folders);

    Fuoten::QueryArgs args;
    args.sortingRole = Fuoten::FuotenEnums::Name;
    const QList<Fuoten::Feed *> feeds = s->getFeeds(args);
    QSet<qint64> used;
    for (Fuoten::Feed *f : feeds) {
        OpmlFile::Feed o;
        o.title = f->title();
        o.url = f->url().toString();
        o.link = f->link().toString();
        o.folder = folderNames.value(f->folderId());
        used.insert(f->folderId());
        if (!o.url.isEmpty())
            out << o;
    }
    qDeleteAll(feeds);

    std::sort(out.begin(), out.end(), [](const OpmlFile::Feed &a, const OpmlFile::Feed &b) {
        const int c = a.folder.compare(b.folder, Qt::CaseInsensitive);
        if (c != 0) {
            // feeds without a folder last
            if (a.folder.isEmpty() || b.folder.isEmpty())
                return !a.folder.isEmpty();
            return c < 0;
        }
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });
    if (emptyFolders) {
        emptyFolders->clear();
        for (auto it = folderNames.cbegin(); it != folderNames.cend(); ++it) {
            if (!used.contains(it.key()))
                *emptyFolders << it.value();
        }
        std::sort(emptyFolders->begin(), emptyFolders->end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
    }
    return out;
}

QHash<QString, qint64> FeedTransfer::localFolderIds() const
{
    QHash<QString, qint64> ids;
    Fuoten::AbstractStorage *s = storage();
    if (!s)
        return ids;
    const QList<Fuoten::Folder *> folders = s->getFolders(Fuoten::FuotenEnums::Name, Qt::AscendingOrder);
    for (Fuoten::Folder *f : folders)
        ids.insert(f->name().toCaseFolded(), f->id());
    qDeleteAll(folders);
    return ids;
}

QSet<QString> FeedTransfer::localFeedKeys() const
{
    QSet<QString> keys;
    for (const OpmlFile::Feed &f : localFeeds())
        keys.insert(FeedFinder::compareKey(f.url));
    return keys;
}

QVariantMap FeedTransfer::summary() const
{
    QStringList empty;
    const QList<OpmlFile::Feed> feeds = localFeeds(&empty);
    QSet<QString> folders;
    for (const OpmlFile::Feed &f : feeds) {
        if (!f.folder.isEmpty())
            folders.insert(f.folder);
    }
    QVariantMap m;
    m.insert(QStringLiteral("feeds"), feeds.size());
    m.insert(QStringLiteral("folders"), folders.size() + empty.size());
    return m;
}

QString FeedTransfer::opmlText() const
{
    QStringList empty;
    const QList<OpmlFile::Feed> feeds = localFeeds(&empty);
    return QString::fromUtf8(OpmlFile::write(feeds, empty, QDateTime::currentDateTimeUtc()));
}

bool FeedTransfer::exportToNextcloud()
{
    if (m_busy)
        return false;
    QStringList empty;
    const QList<OpmlFile::Feed> feeds = localFeeds(&empty);
    const QByteArray data = OpmlFile::write(feeds, empty, QDateTime::currentDateTimeUtc());
    setBusy(true);
    const QString name = OpmlFile::nextcloudFileName();
    m_files->upload(name, data, [this, name](const NextcloudFiles::Result &r) {
        setBusy(false);
        Q_EMIT exported(r.ok, name, r.errorString);
    }, QStringLiteral("text/x-opml; charset=utf-8"));
    return true;
}

QVariantMap FeedTransfer::exportToDownloads()
{
    QVariantMap result;
    if (m_downloads.isEmpty() || !QDir().mkpath(m_downloads)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), tr("The Downloads folder is not available."));
        return result;
    }
    const QDate today = QDate::currentDate();
    QString name;
    for (int i = 1; i < 100; ++i) {
        name = OpmlFile::exportFileName(today, i);
        if (!QFileInfo::exists(m_downloads + QLatin1Char('/') + name))
            break;
    }
    QStringList empty;
    const QList<OpmlFile::Feed> feeds = localFeeds(&empty);
    QSaveFile file(m_downloads + QLatin1Char('/') + name);
    if (!file.open(QIODevice::WriteOnly)
            || file.write(OpmlFile::write(feeds, empty, QDateTime::currentDateTimeUtc())) < 0
            || !file.commit()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), tr("Could not write the file: %1").arg(file.errorString()));
        return result;
    }
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("name"), name);
    return result;
}

QVariantList FeedTransfer::downloadFiles() const
{
    QVariantList out;
    if (m_downloads.isEmpty())
        return out;
    const QFileInfoList files = QDir(m_downloads).entryInfoList(QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo &fi : files) {
        if (!isOpmlName(fi.fileName()) || fi.size() > maxFileSize)
            continue;
        // .xml only when it is OPML (feeds and other XML files land here too)
        if (!fi.fileName().endsWith(QLatin1String(".opml"), Qt::CaseInsensitive)) {
            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly) || !OpmlFile::looksLikeOpml(f.read(4096)))
                continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("name"), fi.fileName());
        m.insert(QStringLiteral("path"), fi.absoluteFilePath());
        m.insert(QStringLiteral("modified"), fi.lastModified());
        m.insert(QStringLiteral("size"), fi.size());
        out << m;
        if (out.size() >= 50)
            break;
    }
    return out;
}

bool FeedTransfer::listNextcloud()
{
    if (m_busy)
        return false;
    setBusy(true);
    m_files->list([this](const NextcloudFiles::Result &r) {
        setBusy(false);
        if (r.ok) {
            QVariantList list;
            for (const NextcloudFiles::Entry &e : r.entries) {
                if (!isOpmlName(e.name) || e.size > maxFileSize)
                    continue;
                QVariantMap m;
                m.insert(QStringLiteral("name"), e.name);
                m.insert(QStringLiteral("modified"), e.modified.toLocalTime());
                m.insert(QStringLiteral("size"), e.size);
                list << m;
            }
            m_ncFiles = list;
            Q_EMIT nextcloudFilesChanged();
        }
        Q_EMIT nextcloudListed(r.ok, r.errorString);
    });
    return true;
}

bool FeedTransfer::loadNextcloud(const QString &name)
{
    if (m_busy)
        return false;
    setBusy(true);
    m_files->download(name, [this, name](const NextcloudFiles::Result &r) {
        setBusy(false);
        if (!r.ok) {
            Q_EMIT loaded(name, false, r.errorString, QVariantMap());
            return;
        }
        const QVariantMap p = plan(r.data);
        Q_EMIT loaded(name, p.value(QStringLiteral("ok")).toBool(), p.value(QStringLiteral("error")).toString(), p);
    });
    return true;
}

QVariantMap FeedTransfer::loadFile(const QString &path)
{
    QVariantMap failed;
    failed.insert(QStringLiteral("ok"), false);
    // Only files of the Downloads folder (the page lists them)
    const QFileInfo fi(path);
    if (m_downloads.isEmpty() || QFileInfo(m_downloads).canonicalFilePath() != fi.absoluteDir().canonicalPath()) {
        failed.insert(QStringLiteral("error"), tr("The file is not in the Downloads folder."));
        return failed;
    }
    QFile f(path);
    if (fi.size() > maxFileSize) {
        failed.insert(QStringLiteral("error"), tr("The file is too big."));
        return failed;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        failed.insert(QStringLiteral("error"), tr("Could not read the file: %1").arg(f.errorString()));
        return failed;
    }
    return plan(f.readAll());
}

QVariantMap FeedTransfer::plan(const QByteArray &data)
{
    QVariantMap m;
    const OpmlFile::Parsed parsed = OpmlFile::parse(data);
    m.insert(QStringLiteral("ok"), parsed.ok);
    m.insert(QStringLiteral("error"), parsed.error);
    m.insert(QStringLiteral("title"), parsed.title);
    m_planned.clear();
    if (!parsed.ok)
        return m;

    const QSet<QString> subscribed = localFeedKeys();
    const QHash<QString, qint64> folderIds = localFolderIds();
    QVariantList feeds;
    QStringList newFolders;
    int subscribedCount = 0;
    for (const OpmlFile::Feed &f : parsed.feeds) {
        const bool known = subscribed.contains(FeedFinder::compareKey(f.url));
        QVariantMap e;
        e.insert(QStringLiteral("title"), f.title);
        e.insert(QStringLiteral("url"), f.url);
        e.insert(QStringLiteral("folder"), f.folder);
        e.insert(QStringLiteral("subscribed"), known);
        feeds << e;
        if (known) {
            ++subscribedCount;
            continue;
        }
        m_planned << f;
        if (!f.folder.isEmpty() && !folderIds.contains(f.folder.toCaseFolded())
                && !newFolders.contains(f.folder, Qt::CaseInsensitive))
            newFolders << f.folder;
    }
    // Empty folders of the file are created as well (only with "keep folders")
    for (const QString &folder : parsed.folders) {
        if (!folderIds.contains(folder.toCaseFolded()) && !newFolders.contains(folder, Qt::CaseInsensitive)) {
            bool hasNewFeed = false;
            for (const OpmlFile::Feed &f : parsed.feeds)
                hasNewFeed = hasNewFeed || f.folder.compare(folder, Qt::CaseInsensitive) == 0;
            if (!hasNewFeed) {
                // A folder whose feeds are all subscribed already elsewhere
                // is not created (it would stay empty); an empty one is
                bool hasAnyFeed = false;
                for (const OpmlFile::Feed &f : parsed.feeds)
                    hasAnyFeed = hasAnyFeed || f.folder.compare(folder, Qt::CaseInsensitive) == 0;
                if (!hasAnyFeed) {
                    newFolders << folder;
                    OpmlFile::Feed marker;     // folder only, no address
                    marker.folder = folder;
                    m_planned << marker;
                }
            }
        }
    }
    int newCount = 0;
    for (const OpmlFile::Feed &f : m_planned)
        newCount += f.url.isEmpty() ? 0 : 1;
    m.insert(QStringLiteral("total"), parsed.feeds.size());
    m.insert(QStringLiteral("subscribed"), subscribedCount);
    m.insert(QStringLiteral("newCount"), newCount);
    m.insert(QStringLiteral("newFolders"), newFolders);
    m.insert(QStringLiteral("feeds"), feeds);
    return m;
}

bool FeedTransfer::startImport(bool keepFolders)
{
    if (m_importing || m_planned.isEmpty() || !storage())
        return false;
    m_queue.clear();
    for (const OpmlFile::Feed &f : m_planned) {
        // Without folders the empty folders of the file are left out
        if (!f.url.isEmpty() || keepFolders)
            m_queue << f;
    }
    if (m_queue.isEmpty())
        return false;
    m_keepFolders = keepFolders;
    m_folderIds = localFolderIds();
    m_failedFolders.clear();
    m_cancel = false;
    m_done = 0;
    m_total = 0;
    for (const OpmlFile::Feed &f : m_queue)
        m_total += f.url.isEmpty() ? 0 : 1;
    m_added = 0;
    m_skipped = 0;
    m_failures.clear();
    m_current.clear();
    m_importing = true;
    Q_EMIT importingChanged(true);
    Q_EMIT importProgressChanged();
    QTimer::singleShot(0, this, &FeedTransfer::step);
    return true;
}

void FeedTransfer::cancelImport()
{
    if (m_importing)
        m_cancel = true;
}

void FeedTransfer::step()
{
    if (m_cancel || m_queue.isEmpty()) {
        finishImport();
        return;
    }
    const OpmlFile::Feed feed = m_queue.first();
    m_current = feed.url.isEmpty() ? feed.folder : feed.title;
    Q_EMIT importProgressChanged();

    if (!m_keepFolders || feed.folder.isEmpty()) {
        createFeed(feed, 0);
        return;
    }
    const QString key = feed.folder.toCaseFolded();
    if (m_folderIds.contains(key) || m_failedFolders.contains(key)) {
        createFeed(feed, m_folderIds.value(key, 0));
        return;
    }
    auto *c = new Fuoten::CreateFolder(this);
    c->setName(feed.folder);
    runComponent(c, this, [this, feed, key](const Fuoten::Error *e, const QJsonDocument &json) {
        if (!e) {
            const QJsonArray a = json.object().value(QStringLiteral("folders")).toArray();
            const qint64 id = a.isEmpty() ? 0 : static_cast<qint64>(a.first().toObject().value(QStringLiteral("id")).toDouble());
            if (id > 0)
                m_folderIds.insert(key, id);
            else
                m_failedFolders.insert(key);
        } else {
            // e.g. the name exists on the server but not on the device yet:
            // the feeds of this folder go in without a folder
            m_failedFolders.insert(key);
            QVariantMap f;
            f.insert(QStringLiteral("title"), tr("Folder \"%1\"").arg(feed.folder));
            f.insert(QStringLiteral("url"), QString());
            f.insert(QStringLiteral("reason"), errorText(e));
            m_failures << f;
        }
        createFeed(feed, m_folderIds.value(key, 0));
    });
}

void FeedTransfer::createFeed(const OpmlFile::Feed &feed, qint64 folderId)
{
    if (feed.url.isEmpty()) {           // an empty folder of the file: created above
        m_queue.removeFirst();
        QTimer::singleShot(0, this, &FeedTransfer::step);
        return;
    }
    auto *c = new Fuoten::CreateFeed(this);
    c->setUrl(QUrl(feed.url));
    c->setFolderId(folderId);
    runComponent(c, this, [this, feed](const Fuoten::Error *e, const QJsonDocument &) {
        if (!e) {
            ++m_added;
        } else if (isFeedExists(e)) {
            ++m_skipped;
        } else {
            QVariantMap f;
            f.insert(QStringLiteral("title"), feed.title);
            f.insert(QStringLiteral("url"), feed.url);
            f.insert(QStringLiteral("reason"), errorText(e));
            m_failures << f;
        }
        ++m_done;
        m_queue.removeFirst();
        Q_EMIT importProgressChanged();
        QTimer::singleShot(0, this, &FeedTransfer::step);
    });
}

void FeedTransfer::finishImport()
{
    const bool cancelled = m_cancel && !m_queue.isEmpty();
    int failedFeeds = 0;
    for (const QVariant &v : m_failures)
        failedFeeds += v.toMap().value(QStringLiteral("url")).toString().isEmpty() ? 0 : 1;
    m_importing = false;
    m_queue.clear();
    m_planned.clear();          // a new import needs a new plan
    m_current.clear();
    m_lastImport.clear();
    m_lastImport.insert(QStringLiteral("added"), m_added);
    m_lastImport.insert(QStringLiteral("skipped"), m_skipped);
    m_lastImport.insert(QStringLiteral("failed"), m_failures);
    m_lastImport.insert(QStringLiteral("cancelled"), cancelled);
    m_lastImport.insert(QStringLiteral("time"), QDateTime::currentDateTime());
    Q_EMIT importingChanged(false);
    Q_EMIT importProgressChanged();
    Q_EMIT lastImportChanged();
    Q_EMIT importFinished(m_added, m_skipped, failedFeeds, cancelled);
}
