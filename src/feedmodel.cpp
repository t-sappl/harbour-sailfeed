// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "feedmodel.h"

#include <Fuoten/Feed>
#include <Fuoten/FuotenEnums>
#include <Fuoten/Models/FeedListFilterModel>
#include <Fuoten/Storage/AbstractStorage>

#include "configuration.h"

#include <QDateTime>
#include <QVariantMap>

#include <algorithm>
#include <QSettings>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

FeedModel::FeedModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_source(new Fuoten::FeedListFilterModel(this))
{
    m_source->setSortingRole(Fuoten::FuotenEnums::Name);
    m_source->setSortByFolder(true);
    m_source->setSortOrder(Qt::AscendingOrder);
    m_source->setHideRead(false);
    // See ArticleModel: no re-sorting on data changes (Qt 5.6 layoutChanged
    // would make the list jump). Order depends only on folder and title.
    m_source->setDynamicSortFilter(false);

    setSourceModel(m_source);
    setDynamicSortFilter(false);

    const QStringList favs = QSettings(Configuration::settingsFilePath(), QSettings::IniFormat)
            .value(QStringLiteral("feeds/favorites")).toStringList();
    for (const QString &id : favs)
        m_favorites.insert(id.toLongLong());
    connect(m_source, &Fuoten::BaseFilterModel::loadedChanged, this, [this](bool loaded) {
        if (loaded)
            resort();
    });

    connect(m_source, &Fuoten::BaseFilterModel::storageChanged, this, &FeedModel::storageChanged);
    connect(m_source, &Fuoten::BaseFilterModel::loadedChanged, this, &FeedModel::loadedChanged);
    connect(m_source, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        // libfuoten announces Qt::DisplayRole only; re-announce with all roles.
        for (int srow = topLeft.row(); srow <= bottomRight.row(); ++srow) {
            const QModelIndex pidx = mapFromSource(m_source->index(srow, 0));
            if (pidx.isValid())
                Q_EMIT dataChanged(pidx, pidx);
        }
        bump();
    });

    auto changed = [this]() { Q_EMIT countChanged(rowCount()); bump(); };
    connect(this, &QAbstractItemModel::rowsInserted, this, changed);
    connect(this, &QAbstractItemModel::rowsRemoved, this, changed);
    connect(this, &QAbstractItemModel::modelReset, this, changed);
    connect(this, &QAbstractItemModel::layoutChanged, this, changed);
}

FeedModel::~FeedModel() = default;

QString FeedModel::noFolderName()
{
    return tr("Other feeds");
}

QHash<int, QByteArray> FeedModel::roleNames() const
{
    return {
        { FeedIdRole,      QByteArrayLiteral("feedId") },
        { TitleRole,       QByteArrayLiteral("title") },
        { UnreadCountRole, QByteArrayLiteral("unreadCount") },
        { FolderIdRole,    QByteArrayLiteral("folderId") },
        { FolderNameRole,  QByteArrayLiteral("folderName") },
        { FaviconRole,     QByteArrayLiteral("favicon") },
        { FeedRole,        QByteArrayLiteral("feed") },
        { FavoriteRole,    QByteArrayLiteral("favorite") },
        { FolderKeyRole,   QByteArrayLiteral("folderKey") },
    };
}

Fuoten::Feed *FeedModel::feedAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return nullptr;
    return QSortFilterProxyModel::data(index(row, 0), Qt::DisplayRole).value<Fuoten::Feed *>();
}

QVariant FeedModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role < FeedIdRole)
        return QSortFilterProxyModel::data(index, role);

    Fuoten::Feed *f = feedAt(index.row());
    if (!f)
        return QVariant();

    switch (role) {
    case FeedIdRole:      return static_cast<int>(f->id());
    case TitleRole:       return f->title();
    case UnreadCountRole: return static_cast<int>(f->unreadCount());
    case FolderIdRole:    return f->folderId() > 0 ? static_cast<int>(f->folderId()) : -1;
    case FolderNameRole:  return folderDisplayName(f);
    case FolderKeyRole:   return folderKey(f);
    case FaviconRole:     return f->faviconLink().toString();
    case FeedRole:        return QVariant::fromValue<QObject *>(f);
    case FavoriteRole:    return m_favorites.contains(f->id());
    default:              return QVariant();
    }
}

Fuoten::AbstractStorage *FeedModel::storage() const
{
    return m_source->storage();
}

void FeedModel::setStorage(Fuoten::AbstractStorage *storage)
{
    m_source->setStorage(storage);
}

bool FeedModel::loaded() const
{
    return m_source->loaded();
}

void FeedModel::load()
{
    if (storage())
        m_source->load();
}

void FeedModel::reload()
{
    if (!storage())
        return;
    if (m_source->loaded())
        m_source->reload();
    else
        m_source->load();
    resort();       // libfuoten reloads synchronously from the database
}

int FeedModel::folderUnread(const QString &folderName) const
{
    int sum = 0;
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        Fuoten::Feed *f = feedAt(row);
        if (!f)
            continue;
        if (folderDisplayName(f) == folderName)
            sum += static_cast<int>(f->unreadCount());
    }
    return sum;
}

int FeedModel::folderIdForName(const QString &folderName) const
{
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        Fuoten::Feed *f = feedAt(row);
        if (f && !f->folderName().isEmpty() && f->folderName() == folderName)
            return static_cast<int>(f->folderId());
    }
    return -1;
}

bool FeedModel::hasFolder(Fuoten::Feed *f)
{
    return f->folderId() > 0 && !f->folderName().isEmpty();
}

QString FeedModel::folderKey(Fuoten::Feed *f)
{
    return hasFolder(f) ? QStringLiteral("folder:") + QString::number(f->folderId())
                        : QStringLiteral("none");
}

QString FeedModel::folderDisplayName(Fuoten::Feed *f)
{
    return hasFolder(f) ? f->folderName() : noFolderName();
}

QString FeedModel::folderNameForKey(const QString &key) const
{
    if (key == QLatin1String("none"))
        return noFolderName();
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        Fuoten::Feed *f = feedAt(row);
        if (f && folderKey(f) == key)
            return f->folderName();
    }
    return QString();
}

int FeedModel::folderIdForKey(const QString &key) const
{
    if (!key.startsWith(QLatin1String("folder:")))
        return -1;
    bool ok = false;
    const int id = key.mid(7).toInt(&ok);
    return ok && id > 0 ? id : -1;
}

int FeedModel::folderUnreadForKey(const QString &key) const
{
    int sum = 0;
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        Fuoten::Feed *f = feedAt(row);
        if (f && folderKey(f) == key)
            sum += static_cast<int>(f->unreadCount());
    }
    return sum;
}

void FeedModel::setFavorite(int feedId, bool favorite)
{
    if (feedId <= 0 || m_favorites.contains(feedId) == favorite)
        return;
    QSet<qint64> next = m_favorites;
    if (favorite)
        next.insert(feedId);
    else
        next.remove(feedId);
    applyFavorites(next);
}

QVariantList FeedModel::favoriteFeeds() const
{
    QList<qint64> ids = m_favorites.values();
    std::sort(ids.begin(), ids.end());
    QVariantList out;
    for (qint64 id : ids) {
        QString title;
        for (int row = 0; row < rowCount(); ++row) {
            Fuoten::Feed *f = feedAt(row);
            if (f && f->id() == id) {
                title = f->title();
                break;
            }
        }
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("title"), title);
        out << m;
    }
    return out;
}

void FeedModel::setFavoriteIds(const QVariantList &ids)
{
    QSet<qint64> next;
    for (const QVariant &v : ids) {
        const qint64 id = v.toLongLong();
        if (id > 0)
            next.insert(id);
    }
    if (next != m_favorites)
        applyFavorites(next);
}

void FeedModel::applyFavorites(const QSet<qint64> &favorites)
{
    const QSet<qint64> changed = (favorites - m_favorites) + (m_favorites - favorites);
    m_favorites = favorites;
    QStringList ids;
    for (qint64 id : m_favorites)
        ids << QString::number(id);
    ids.sort();
    QSettings settings(Configuration::settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("feeds/favorites"), ids);
    // The rows show the star; the order changes at once (user action).
    for (int row = 0; row < rowCount(); ++row) {
        Fuoten::Feed *f = feedAt(row);
        if (f && changed.contains(f->id()))
            Q_EMIT dataChanged(index(row, 0), index(row, 0));
    }
    resort();
    Q_EMIT favoritesChanged();
}

void FeedModel::setSortMode(int mode)
{
    if (mode < UnreadFirst || mode > UnreadThenActivity || mode == m_sortMode)
        return;
    m_sortMode = mode;
    Q_EMIT sortModeChanged(mode);
    resort();
}

// Article counts per feed from the database (own read-only connection; the
// storage's connection belongs to libfuoten). MostActive counts articles
// published in the last activityDays: after the first sync the database
// holds every new article, read or not (the first sync only fetches unread
// and starred ones).
QHash<qint64, int> FeedModel::loadCounts() const
{
    QHash<qint64, int> counts;
    if (m_sortMode != MostActive && m_sortMode != MostStarred && m_sortMode != UnreadThenActivity)
        return counts;
    const QString path = !m_dbPath.isEmpty() ? m_dbPath
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/database.sqlite");
    const QString conn = QStringLiteral("sailfeed-feedstats");
    {
        QSqlDatabase db = QSqlDatabase::contains(conn) ? QSqlDatabase::database(conn, false)
                                                         : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        if (db.databaseName() != path) {
            db.close();
            db.setDatabaseName(path);
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=500"));
        }
        if (!db.isOpen() && !db.open())
            return counts;
        const qint64 since = QDateTime::currentMSecsSinceEpoch() / 1000 - activityDays * 86400LL;
        int recentWeight = 1, starWeight = 0;
        if (m_sortMode == MostStarred) {
            recentWeight = 0;
            starWeight = 1;
        } else if (m_sortMode == UnreadThenActivity) {
            starWeight = starredWeight;
        }
        // score per feed = recentWeight * recent articles + starWeight * starred articles
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT feedId, SUM(CASE WHEN pubDate >= ? THEN ? ELSE 0 END)"
                                 " + SUM(CASE WHEN starred = 1 THEN ? ELSE 0 END) FROM items GROUP BY feedId"));
        q.addBindValue(since);
        q.addBindValue(recentWeight);
        q.addBindValue(starWeight);
        if (q.exec()) {
            while (q.next())
                counts.insert(q.value(0).toLongLong(), q.value(1).toInt());
        }
    }
    return counts;
}

int FeedModel::feedMetric(Fuoten::Feed *f) const
{
    switch (m_sortMode) {
    case UnreadFirst:  return f->unreadCount() > 0 ? 1 : 0;
    case MostActive:
    case MostStarred:
    case UnreadThenActivity: return m_feedMetric.value(f->id());
    default:           return 0;
    }
}

void FeedModel::resort()
{
    m_feedMetric = loadCounts();
    m_folderUnread.clear();
    m_folderMetric.clear();
    m_folderFavorites.clear();
    const int n = m_source->rowCount();
    for (int row = 0; row < n; ++row) {
        auto *f = m_source->data(m_source->index(row, 0), Qt::DisplayRole).value<Fuoten::Feed *>();
        if (!f)
            continue;
        m_folderUnread[folderKey(f)] += static_cast<int>(f->unreadCount());
        m_folderMetric[folderKey(f)] += feedMetric(f);
        if (m_favorites.contains(f->id()))
            m_folderFavorites[folderKey(f)] += 1;
    }
    invalidate();
    sort(0, Qt::AscendingOrder);
    bump();
}

bool FeedModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    auto *l = sourceModel()->data(left, Qt::DisplayRole).value<Fuoten::Feed *>();
    auto *r = sourceModel()->data(right, Qt::DisplayRole).value<Fuoten::Feed *>();
    if (!l || !r)
        return l < r;

    const QString lf = folderKey(l);
    const QString rf = folderKey(r);
    if (lf != rf) {
        // The user's own folders always come first; feeds without a folder
        // are listed after them, in every sort mode.
        const bool lnone = !hasFolder(l);
        const bool rnone = !hasFolder(r);
        if (lnone != rnone)
            return rnone;
        int lm = m_folderMetric.value(lf);
        int rm = m_folderMetric.value(rf);
        if (m_sortMode == UnreadFirst) {        // any unread counts, not how many
            lm = m_folderUnread.value(lf) > 0;
            rm = m_folderUnread.value(rf) > 0;
        }
        if (favoritesFirst()) {
            const bool lfav = m_folderFavorites.value(lf) > 0;
            const bool rfav = m_folderFavorites.value(rf) > 0;
            if (lfav != rfav)
                return lfav;                    // folders with a favorite first
        }
        if (m_sortMode == UnreadThenActivity) {
            const bool lu = m_folderUnread.value(lf) > 0;
            const bool ru = m_folderUnread.value(rf) > 0;
            if (lu != ru)
                return lu;                      // folders with unread first
        }
        if (lm != rm)
            return lm > rm;                     // higher first
        const int c = folderDisplayName(l).localeAwareCompare(folderDisplayName(r));
        return c != 0 ? c < 0 : lf < rf;
    }
    if (favoritesFirst()) {
        const bool lfav = m_favorites.contains(l->id());
        const bool rfav = m_favorites.contains(r->id());
        if (lfav != rfav)
            return lfav;                        // favorite feeds first
    }
    if (m_sortMode == UnreadThenActivity) {
        const bool lu = l->unreadCount() > 0;
        const bool ru = r->unreadCount() > 0;
        if (lu != ru)
            return lu;                          // feeds with unread first
    }
    const int lm = feedMetric(l);
    const int rm = feedMetric(r);
    if (lm != rm)
        return lm > rm;
    const int c = l->title().localeAwareCompare(r->title());
    return c != 0 ? c < 0 : l->id() < r->id();
}

// libfuoten refuses to queue a "mark read" while its previous worker still
// runs (e.g. feed and then folder marked quickly). Run it once the storage is
// idle instead of losing it.
bool FeedModel::whenStorageIdle(const std::function<bool()> &action)
{
    Fuoten::AbstractStorage *s = storage();
    if (!s)
        return false;
    if (!s->inOperation())
        return action();
    auto conn = QSharedPointer<QMetaObject::Connection>::create();
    *conn = connect(s, &Fuoten::AbstractStorage::inOperationChanged, this, [this, conn, action](bool inOp) {
        if (inOp)
            return;
        disconnect(*conn);
        if (!whenStorageIdle(action))
            qWarning("Marking as read failed");
    });
    return true;
}

bool FeedModel::markFeedRead(int feedId)
{
    Fuoten::AbstractStorage *s = storage();
    if (!s || feedId <= 0)
        return false;
    return whenStorageIdle([s, feedId]() {
        const qint64 newest = s->getNewestItemId(Fuoten::FuotenEnums::Feed, feedId);
        return newest > 0 && s->enqueueMarkFeedRead(feedId, newest);
    });
}

bool FeedModel::markFolderRead(int folderId)
{
    Fuoten::AbstractStorage *s = storage();
    if (!s || folderId < 0)
        return false;
    if (folderId == 0) {
        // "Other feeds": libfuoten has no newest item for folder 0, so each
        // feed without a folder is marked on its own (one after another:
        // the storage takes one job at a time, see whenStorageIdle).
        bool any = false;
        for (int row = 0; row < rowCount(); ++row) {
            Fuoten::Feed *f = feedAt(row);
            if (f && !hasFolder(f) && f->unreadCount() > 0)
                any = markFeedRead(static_cast<int>(f->id())) || any;
        }
        return any;
    }
    return whenStorageIdle([s, folderId]() {
        const qint64 newest = s->getNewestItemId(Fuoten::FuotenEnums::Folder, folderId);
        return newest > 0 && s->enqueueMarkFolderRead(folderId, newest);
    });
}

bool FeedModel::markAllRead()
{
    Fuoten::AbstractStorage *s = storage();
    if (!s)
        return false;
    return whenStorageIdle([s]() { return s->enqueueMarkAllItemsRead(); });
}

void FeedModel::bump()
{
    ++m_revision;
    Q_EMIT revisionChanged(m_revision);
}
