// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QVector>

class FeedModel;

// The rows of the feed list: a row per folder, followed by its feeds, in
// FeedModel's order (sort mode, favorites), then "Other feeds" (feeds
// without a folder). Empty folders are listed too, newest first, at the top
// (a folder just created appears right below "Starred", ready for feeds).
//
// Folder headers used to be ListView sections. Sections are not list rows:
// tapping them did not work reliably, their context menus were drawn over
// the following rows (the view does not make room for a growing section),
// and a remorse countdown on them stayed. Real rows behave like every other
// list item (device test 11 Oct).
//
// Counts and favorites change with dataChanged only (no reset, the list
// does not jump); a different set or order of rows (sorting, sync, feed or
// folder management) resets the model.
class FeedListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(FeedModel *feeds READ feeds WRITE setFeeds NOTIFY feedsChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        RowTypeRole = Qt::UserRole + 300,   // "folder" or "feed"
        KeyRole,            // "folder:<id>", "none" or "feed:<id>"
        FeedIdRole,         // feed rows; -1 for folders
        FolderIdRole,       // folder of the row; -1 for "Other feeds"
        TitleRole,          // feed title or folder name
        UnreadCountRole,    // feed, or sum over the folder's feeds
        FaviconRole,
        FavoriteRole,
        FeedCountRole,      // folder rows: number of feeds
        IsOtherRole         // the "Other feeds" header
    };
    Q_ENUM(Roles)

    explicit FeedListModel(QObject *parent = nullptr);
    ~FeedListModel() override;

    FeedModel *feeds() const { return m_feeds; }
    void setFeeds(FeedModel *feeds);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_rows.size(); }

    // Builds the rows again (also called by the storage and FeedModel signals)
    Q_INVOKABLE void rebuild();

Q_SIGNALS:
    void feedsChanged();
    void countChanged(int count);

private:
    struct Row {
        bool folder = false;
        QString key;
        qint64 folderId = -1;
        QString name;           // folder rows
        int sourceRow = -1;     // feed rows: row in FeedModel
        int firstSource = -1;   // folder rows: their feeds in FeedModel
        int sourceCount = 0;
    };
    QVector<Row> buildRows() const;
    void connectStorage();
    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    QPointer<FeedModel> m_feeds;
    QVector<Row> m_rows;
    QHash<int, int> m_rowOfSource;      // FeedModel row -> our row
    QList<QMetaObject::Connection> m_storageConns;
};
