// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "feedrowsmodel.h"

#include "feedmodel.h"

#include <Fuoten/Folder>
#include <Fuoten/FuotenEnums>
#include <Fuoten/Storage/AbstractStorage>

#include <algorithm>

FeedRowsModel::FeedRowsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

FeedRowsModel::~FeedRowsModel() = default;

FeedModel *FeedRowsModel::feeds() const
{
    return m_feeds;
}

QHash<int, QByteArray> FeedRowsModel::roleNames() const
{
    return {
        { RowTypeRole,     QByteArrayLiteral("rowType") },
        { KeyRole,         QByteArrayLiteral("key") },
        { FeedIdRole,      QByteArrayLiteral("feedId") },
        { FolderIdRole,    QByteArrayLiteral("folderId") },
        { TitleRole,       QByteArrayLiteral("title") },
        { UnreadCountRole, QByteArrayLiteral("unreadCount") },
        { FaviconRole,     QByteArrayLiteral("favicon") },
        { FavoriteRole,    QByteArrayLiteral("favorite") },
        { FeedCountRole,   QByteArrayLiteral("feedCount") },
        { IsOtherRole,     QByteArrayLiteral("isOther") },
    };
}

void FeedRowsModel::setFeeds(FeedModel *feeds)
{
    if (m_feeds == feeds)
        return;
    if (m_feeds)
        m_feeds->disconnect(this);
    m_feeds = feeds;
    if (m_feeds) {
        connect(m_feeds, &QAbstractItemModel::dataChanged, this, &FeedRowsModel::onSourceDataChanged);
        connect(m_feeds, &QAbstractItemModel::modelReset, this, &FeedRowsModel::rebuild);
        connect(m_feeds, &QAbstractItemModel::layoutChanged, this, &FeedRowsModel::rebuild);
        connect(m_feeds, &QAbstractItemModel::rowsInserted, this, &FeedRowsModel::rebuild);
        connect(m_feeds, &QAbstractItemModel::rowsRemoved, this, &FeedRowsModel::rebuild);
        connect(m_feeds, &FeedModel::storageChanged, this, [this]() { connectStorage(); rebuild(); });
    }
    connectStorage();
    rebuild();
    Q_EMIT feedsChanged();
}

// Folders without feeds are not in FeedModel; the storage tells when folders
// change (sync, created, renamed, deleted).
void FeedRowsModel::connectStorage()
{
    for (const QMetaObject::Connection &c : m_storageConns)
        disconnect(c);
    m_storageConns.clear();
    Fuoten::AbstractStorage *s = m_feeds ? m_feeds->storage() : nullptr;
    if (!s)
        return;
    m_storageConns << connect(s, &Fuoten::AbstractStorage::requestedFolders, this, &FeedRowsModel::rebuild);
    m_storageConns << connect(s, &Fuoten::AbstractStorage::createdFolder, this, &FeedRowsModel::rebuild);
    m_storageConns << connect(s, &Fuoten::AbstractStorage::renamedFolder, this, &FeedRowsModel::rebuild);
    m_storageConns << connect(s, &Fuoten::AbstractStorage::deletedFolder, this, &FeedRowsModel::rebuild);
    m_storageConns << connect(s, &Fuoten::AbstractStorage::readyChanged, this, &FeedRowsModel::rebuild);
}

QVector<FeedRowsModel::Row> FeedRowsModel::buildRows() const
{
    QVector<Row> rows;
    if (!m_feeds)
        return rows;

    // Groups of feeds in FeedModel's order (folders first, "Other feeds" last)
    QVector<Row> groups;
    QList<qint64> foldersWithFeeds;
    const int n = m_feeds->rowCount();
    for (int src = 0; src < n; ++src) {
        const QModelIndex idx = m_feeds->index(src, 0);
        const QString key = idx.data(FeedModel::FolderKeyRole).toString();
        if (groups.isEmpty() || groups.last().key != key) {
            Row folder;
            folder.folder = true;
            folder.key = key;
            folder.folderId = idx.data(FeedModel::FolderIdRole).toLongLong();
            folder.name = idx.data(FeedModel::FolderNameRole).toString();
            folder.firstSource = src;
            groups << folder;
            if (folder.folderId > 0)
                foldersWithFeeds << folder.folderId;
        }
        ++groups.last().sourceCount;
    }

    // Empty folders, newest first, at the top
    Fuoten::AbstractStorage *s = m_feeds->storage();
    if (s && s->ready()) {
        QList<Fuoten::Folder *> folders = s->getFolders(Fuoten::FuotenEnums::ID, Qt::DescendingOrder);
        for (Fuoten::Folder *f : folders) {
            if (f->id() > 0 && !foldersWithFeeds.contains(f->id())) {
                Row folder;
                folder.folder = true;
                folder.key = QStringLiteral("folder:") + QString::number(f->id());
                folder.folderId = f->id();
                folder.name = f->name();
                rows << folder;
            }
            delete f;
        }
    }

    for (const Row &group : groups) {
        rows << group;
        for (int i = 0; i < group.sourceCount; ++i) {
            const int src = group.firstSource + i;
            Row feed;
            feed.key = QStringLiteral("feed:") + m_feeds->index(src, 0).data(FeedModel::FeedIdRole).toString();
            feed.folderId = group.folderId;
            feed.sourceRow = src;
            rows << feed;
        }
    }
    return rows;
}

void FeedRowsModel::rebuild()
{
    const QVector<Row> rows = buildRows();
    bool same = rows.size() == m_rows.size();
    for (int i = 0; same && i < rows.size(); ++i)
        same = rows.at(i).key == m_rows.at(i).key && rows.at(i).name == m_rows.at(i).name;

    if (same) {
        m_rows = rows;      // source rows may have moved
    } else {
        beginResetModel();
        m_rows = rows;
    }
    m_rowOfSource.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        if (!m_rows.at(i).folder)
            m_rowOfSource.insert(m_rows.at(i).sourceRow, i);
    if (same) {
        if (!m_rows.isEmpty())
            Q_EMIT dataChanged(index(0), index(m_rows.size() - 1));
    } else {
        endResetModel();
        Q_EMIT countChanged(m_rows.size());
    }
}

void FeedRowsModel::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    for (int src = topLeft.row(); src <= bottomRight.row(); ++src) {
        const int row = m_rowOfSource.value(src, -1);
        if (row < 0)
            continue;
        Q_EMIT dataChanged(index(row), index(row));
        // ... and the folder row above it (unread sum)
        for (int r = row - 1; r >= 0; --r) {
            if (m_rows.at(r).folder) {
                Q_EMIT dataChanged(index(r), index(r));
                break;
            }
        }
    }
}

int FeedRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant FeedRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size() || !m_feeds)
        return QVariant();
    const Row &r = m_rows.at(index.row());

    if (r.folder) {
        switch (role) {
        case RowTypeRole:   return QStringLiteral("folder");
        case KeyRole:       return r.key;
        case FeedIdRole:    return -1;
        case FolderIdRole:  return static_cast<int>(r.folderId);
        case Qt::DisplayRole:
        case TitleRole:     return r.name;
        case UnreadCountRole: {
            int sum = 0;
            for (int i = 0; i < r.sourceCount; ++i)
                sum += m_feeds->index(r.firstSource + i, 0).data(FeedModel::UnreadCountRole).toInt();
            return sum;
        }
        case FaviconRole:   return QString();
        case FavoriteRole:  return false;
        case FeedCountRole: return r.sourceCount;
        case IsOtherRole:   return r.folderId < 0;
        default:            return QVariant();
        }
    }

    const QModelIndex src = m_feeds->index(r.sourceRow, 0);
    switch (role) {
    case RowTypeRole:     return QStringLiteral("feed");
    case KeyRole:         return r.key;
    case FeedIdRole:      return src.data(FeedModel::FeedIdRole);
    case FolderIdRole:    return static_cast<int>(r.folderId);
    case Qt::DisplayRole:
    case TitleRole:       return src.data(FeedModel::TitleRole);
    case UnreadCountRole: return src.data(FeedModel::UnreadCountRole);
    case FaviconRole:     return src.data(FeedModel::FaviconRole);
    case FavoriteRole:    return src.data(FeedModel::FavoriteRole);
    case FeedCountRole:   return 0;
    case IsOtherRole:     return false;
    default:              return QVariant();
    }
}
