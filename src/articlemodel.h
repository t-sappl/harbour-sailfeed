// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace Fuoten {
class AbstractStorage;
class Article;
class ArticleListFilterModel;
}

// Article list for the UI.
//
// Wraps libfuoten's ArticleListFilterModel (which loads articles from the
// local SQLite storage and keeps itself up to date during syncs) and adds
// flat roles for the delegate: title, feed, relative age, day section,
// plain-text excerpt and thumbnail URL.
//
// "Unread only" is implemented here instead of with libfuoten's hideRead,
// because hideRead filters dynamically: an article marked as read while the
// user is looking at it would vanish and the list would jump. Here, articles
// marked read in this session stay visible (greyed out) until the next
// reload().
class ArticleModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(Fuoten::AbstractStorage *storage READ storage WRITE setStorage NOTIFY storageChanged)
    Q_PROPERTY(int folderId READ folderId NOTIFY selectionChanged)
    Q_PROPERTY(int feedId READ feedId NOTIFY selectionChanged)
    Q_PROPERTY(bool starredOnly READ starredOnly NOTIFY selectionChanged)
    Q_PROPERTY(bool unreadOnly READ unreadOnly WRITE setUnreadOnly NOTIFY unreadOnlyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool inOperation READ inOperation NOTIFY inOperationChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Loading older (already read) articles of the selection from the server.
    Q_PROPERTY(bool fetchingOlder READ fetchingOlder NOTIFY fetchingOlderChanged)
    Q_PROPERTY(bool canFetchOlder READ canFetchOlder NOTIFY fetchingOlderChanged)
    // With "unread only": show the read articles of this selection anyway
    // (reset when the selection changes), and how many the filter hides.
    Q_PROPERTY(bool showRead READ showRead WRITE setShowRead NOTIFY showReadChanged)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY hiddenCountChanged)
    // Unread articles among the listed ones (greys out "mark all as read")
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
public:
    enum Roles {
        ItemIdRole = Qt::UserRole + 100,
        TitleRole,
        FeedTitleRole,
        AgeRole,
        DayRole,
        ExcerptRole,
        ThumbRole,
        UrlRole,
        UnreadRole,
        StarredRole,
        PubDateRole,
        ArticleRole
    };
    Q_ENUM(Roles)

    explicit ArticleModel(QObject *parent = nullptr);
    ~ArticleModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Fuoten::AbstractStorage *storage() const;
    void setStorage(Fuoten::AbstractStorage *storage);

    // Selection: feedId >= 0 → one feed, else folderId >= 0 → one folder,
    // else all articles.
    int folderId() const { return m_folderId; }
    int feedId() const { return m_feedId; }
    Q_INVOKABLE void setSelection(int folderId, int feedId);
    // All starred articles (any feed, read or unread; "unread only" does
    // not apply here). setSelection() leaves this view.
    Q_INVOKABLE void selectStarred();
    bool starredOnly() const { return m_starredOnly; }

    bool unreadOnly() const { return m_unreadOnly; }
    bool showRead() const { return m_showRead; }
    void setShowRead(bool showRead);
    int hiddenCount() const;
    int unreadCount() const { return m_unreadCount; }
    void setUnreadOnly(bool unreadOnly);

    bool loaded() const;
    bool inOperation() const;
    int count() const { return rowCount(); }

    Q_INVOKABLE void load();
    Q_INVOKABLE void reload();

    // Marks the article in this proxy row as read (locally at once, queued
    // for the server; sent with the next sync via items/read/multiple).
    Q_INVOKABLE void markRead(int row);
    Q_INVOKABLE void markRowsRead(const QVariantList &rows);
    // Marks every article of the current selection (all / folder) as read.
    Q_INVOKABLE void markAllRead();

    Q_INVOKABLE int rowOfId(double itemId) const;
    // Item id of a row (-1 if invalid); cheap, unlike get().
    Q_INVOKABLE double idAt(int row) const;
    // Marks articles by id (ids stay valid when rows move, e.g. for
    // "mark when scrolling on", where rows are collected before marking).
    Q_INVOKABLE void markIdsRead(const QVariantList &ids);
    Q_INVOKABLE void setUnreadById(double itemId, bool unread);
    Q_INVOKABLE void setStarredById(double itemId, bool starred);

    // Snapshot of one article for the article page (title, feedTitle,
    // pubDate, body (cleaned HTML), url, thumb, unread, starred, itemId).
    Q_INVOKABLE QVariantMap get(int row) const;

    // The first sync only fetches unread and starred articles, so feeds whose
    // articles were all read elsewhere look empty. This fetches the next
    // olderBatch articles of the selection (feed, folder or all; read ones
    // too) older than the oldest one in the list. canFetchOlder turns false
    // when the server has no more.
    Q_INVOKABLE void fetchOlder();
    bool fetchingOlder() const { return m_fetchingOlder; }
    bool canFetchOlder() const { return m_canFetchOlder && !m_starredOnly; }
    static constexpr int olderBatch = 40;

    // Re-evaluates the relative age and day section of all rows.
    Q_INVOKABLE void refreshAges();

    // Exposed for unit tests.
    static QString ageText(const QDateTime &pubDate, const QDateTime &now);
    static QString dayText(const QDateTime &pubDate, const QDateTime &now);

Q_SIGNALS:
    void storageChanged(Fuoten::AbstractStorage *storage);
    void selectionChanged();
    void unreadOnlyChanged(bool unreadOnly);
    void loadedChanged(bool loaded);
    void inOperationChanged(bool inOperation);
    void countChanged(int count);
    // Emitted whenever articles were queued for the server.
    void queueChanged();
    void fetchingOlderChanged();
    void showReadChanged(bool showRead);
    void hiddenCountChanged();
    void unreadCountChanged(int unreadCount);
    // newArticles: how many articles the server sent that were not there yet
    void olderFetched(int newArticles);
    void fetchOlderFailed(const QString &message);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void setFetchingOlder(bool fetching);
    bool m_fetchingOlder = false;
    bool m_canFetchOlder = true;
    QMetaObject::Connection m_olderConn;

    struct Extra {
        QString excerpt;
        QString thumb;
    };

    Fuoten::Article *articleAt(int row) const;
    Fuoten::Article *articleById(qint64 id) const;
    const Extra &extraFor(Fuoten::Article *article) const;
    void applySelection();
    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    Fuoten::ArticleListFilterModel *m_source;
    int m_folderId = -1;
    int m_feedId = -1;
    bool m_starredOnly = false;
    bool m_showRead = false;
    bool m_unreadOnly = false;
    QSet<qint64> m_keep;                    // read in this session, keep visible
    void scheduleUnreadCount();
    void updateUnreadCount();
    int m_unreadCount = 0;
    QTimer *m_unreadTimer = nullptr;
    QMetaObject::Connection m_totalUnreadConn;

    mutable QHash<qint64, Extra> m_extra;   // per-article excerpt/thumbnail cache
};
