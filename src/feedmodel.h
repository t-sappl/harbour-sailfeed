// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QSet>

#include <functional>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVariantList>

namespace Fuoten {
class AbstractStorage;
class Feed;
class FeedListFilterModel;
}

// Feed list for the feeds page: all feeds, grouped by folder (folder name,
// then feed title), with flat roles for QML (sections need a role).
//
// Wraps libfuoten's FeedListFilterModel, which keeps the unread counts up to
// date when articles are marked or synced.
class FeedModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(Fuoten::AbstractStorage *storage READ storage WRITE setStorage NOTIFY storageChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Incremented on every data change; bind to it to refresh values that
    // come from invokable functions (folderUnread()).
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    // How resort() orders folders and feeds, see SortMode.
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
public:
    // Folders are always sections; the mode orders folders (by the sum of
    // their feeds) and the feeds inside them. Ties: alphabetical.
    enum SortMode {
        UnreadFirst = 0,    // folders/feeds with unread articles first
        Alphabetical = 1,
        MostActive = 2,     // most articles published in the last activityDays
        MostStarred = 3,    // most starred articles
        // Unread first; inside both groups by activity: articles of the last
        // activityDays plus starredWeight per starred article (interest).
        UnreadThenActivity = 4
    };
    // Favorite feeds come first in the activity modes (MostActive,
    // UnreadThenActivity), and folders containing one come first.
    Q_ENUM(SortMode)
    static constexpr int activityDays = 30;
    static constexpr int starredWeight = 3;

    enum Roles {
        FeedIdRole = Qt::UserRole + 200,
        TitleRole,
        UnreadCountRole,
        FolderIdRole,
        FolderNameRole,
        FaviconRole,
        FeedRole,
        FavoriteRole,
        FolderKeyRole       // section key: "folder:<id>", or "none" (no folder)
    };
    Q_ENUM(Roles)

    explicit FeedModel(QObject *parent = nullptr);
    ~FeedModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Fuoten::AbstractStorage *storage() const;
    void setStorage(Fuoten::AbstractStorage *storage);
    bool loaded() const;
    int count() const { return rowCount(); }
    int revision() const { return m_revision; }

    Q_INVOKABLE void load();
    Q_INVOKABLE void reload();

    // Sorts: folders with unread articles first, then the others; inside a
    // folder feeds with unread articles first; each group alphabetically.
    // Not dynamic (the list would jump while counts change): call it when
    // the feed list is shown.
    Q_INVOKABLE void resort();
    int sortMode() const { return m_sortMode; }

    // Favorite feeds: kept on this device (settings file), re-sorts at once.
    Q_INVOKABLE bool isFavorite(int feedId) const { return m_favorites.contains(feedId); }
    Q_INVOKABLE void setFavorite(int feedId, bool favorite);
    // Favorites for the settings backup: [{ id, title }] (title empty for a
    // feed that is not in the list), and replacing them all at once.
    Q_INVOKABLE QVariantList favoriteFeeds() const;
    // Addresses of all subscribed feeds (for "Subscribe to website")
    Q_INVOKABLE QStringList feedUrls() const;
    // The subscribed feeds as {url, link, title} ("Subscribe to website": already subscribed?)
    Q_INVOKABLE QVariantList subscribedFeeds() const;
    // One feed for the feed settings page: id, title, url, link, folderId,
    // folderName, favicon; empty map if unknown
    Q_INVOKABLE QVariantMap feedInfo(int feedId) const;
    // Feeds whose title contains every word of text (search, 1.1):
    // [{ id, title, unread, favicon, folderName }], in list order
    Q_INVOKABLE QVariantList feedsMatching(const QString &text) const;
    Q_INVOKABLE void setFavoriteIds(const QVariantList &ids);
    void setSortMode(int mode);
    // Database file for the article counts of MostActive/MostStarred
    // (default: the app's database.sqlite).
    void setDatabasePath(const QString &path) { m_dbPath = path; }

    // Sum of unread articles of all feeds in the folder with this name
    // (as shown in the section header) and that folder's id (-1: none).
    Q_INVOKABLE int folderUnread(const QString &folderName) const;
    Q_INVOKABLE int folderIdForName(const QString &folderName) const;

    // The same by section key (FolderKeyRole). The feed list groups by key,
    // not by name, so a folder that happens to be called like the group of
    // feeds without folder ("Other feeds") stays a separate section.
    Q_INVOKABLE QString folderNameForKey(const QString &key) const;
    Q_INVOKABLE int folderIdForKey(const QString &key) const;
    Q_INVOKABLE int folderUnreadForKey(const QString &key) const;

    // "Mark all as read" from the feed list's context menu: local at once,
    // queued for the server (feeds/folders/items read). Return false if
    // there was nothing to mark.
    Q_INVOKABLE bool markFeedRead(int feedId);
    Q_INVOKABLE bool markFolderRead(int folderId);
    Q_INVOKABLE bool markAllRead();

    // Section label for feeds without folder.
    static QString noFolderName();

Q_SIGNALS:
    void storageChanged(Fuoten::AbstractStorage *storage);
    void loadedChanged(bool loaded);
    void countChanged(int count);
    void revisionChanged(int revision);
    void sortModeChanged(int sortMode);
    // Favorites changed (for the automatic settings backup)
    void favoritesChanged();

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    static QString folderKey(Fuoten::Feed *f);
    static QString folderDisplayName(Fuoten::Feed *f);
    static bool hasFolder(Fuoten::Feed *f);
    Fuoten::Feed *feedAt(int row) const;
    void bump();

    Fuoten::FeedListFilterModel *m_source;
    int m_revision = 0;
    QHash<QString, int> m_folderUnread;     // per folder key, snapshot taken by resort()
    QHash<QString, int> m_folderMetric;     // sum of m_feedMetric per folder
    QHash<qint64, int> m_feedMetric;        // MostActive/MostStarred counts per feed
    int m_sortMode = UnreadFirst;
    QString m_dbPath;
    QSet<qint64> m_favorites;
    QHash<QString, int> m_folderFavorites;  // favorite feeds per folder
    void applyFavorites(const QSet<qint64> &favorites);
    bool favoritesFirst() const { return m_sortMode == MostActive || m_sortMode == UnreadThenActivity; }
    QHash<qint64, int> loadCounts() const;
    bool whenStorageIdle(const std::function<bool()> &action);
    int feedMetric(Fuoten::Feed *f) const;
};
