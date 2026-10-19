// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "articlemodel.h"

#include <Fuoten/API/GetItems>
#include <Fuoten/Error>
#include "htmlutils.h"

#include <Fuoten/Article>
#include <Fuoten/FuotenEnums>
#include <Fuoten/Models/ArticleListFilterModel>
#include <Fuoten/Storage/AbstractStorage>

#include <QDateTime>
#include <QLocale>

namespace {
constexpr int kExcerptChars = 220;
}

ArticleModel::ArticleModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_source(new Fuoten::ArticleListFilterModel(this))
{
    m_source->setBodyLimit(0);                      // full HTML body: needed for <img> thumbnails
    m_source->setSortOrder(Qt::DescendingOrder);    // newest first (sorted by pubDate)
    m_source->setHideRead(false);                   // see class comment
    m_source->setParentIdType(Fuoten::FuotenEnums::All);
    // No re-sorting on data changes: with Qt 5.6 (Sailfish OS) every change of
    // an article (read, starred, sync update) made QSortFilterProxyModel emit
    // layoutChanged, which the QML ListView treats like a reset and jumps to
    // the top. Order only depends on pubDate, so sorting once on load (and
    // sorted insertion of new rows) is enough.
    m_source->setDynamicSortFilter(false);

    setSourceModel(m_source);
    setDynamicSortFilter(true);

    connect(m_source, &Fuoten::BaseFilterModel::storageChanged, this, &ArticleModel::storageChanged);
    connect(m_source, &Fuoten::BaseFilterModel::loadedChanged, this, &ArticleModel::loadedChanged);
    connect(m_source, &Fuoten::BaseFilterModel::inOperationChanged, this, &ArticleModel::inOperationChanged);
    connect(m_source, &QAbstractItemModel::dataChanged, this, &ArticleModel::onSourceDataChanged);
    connect(m_source, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
        m_keep.clear();
        m_extra.clear();
    });

    auto emitCount = [this]() { Q_EMIT countChanged(rowCount()); };
    connect(this, &QAbstractItemModel::rowsInserted, this, emitCount);
    connect(this, &QAbstractItemModel::rowsRemoved, this, emitCount);
    connect(this, &QAbstractItemModel::modelReset, this, emitCount);
    connect(this, &QAbstractItemModel::layoutChanged, this, emitCount);
    // hiddenCount changes with the source, also when the proxy hides the rows
    auto emitHidden = [this]() { Q_EMIT hiddenCountChanged(); };
    connect(m_source, &QAbstractItemModel::rowsInserted, this, emitHidden);
    connect(m_source, &QAbstractItemModel::rowsRemoved, this, emitHidden);
    connect(m_source, &QAbstractItemModel::modelReset, this, emitHidden);
    connect(this, &ArticleModel::countChanged, this, emitHidden);

    // Unread count of the listed articles. Marking runs in libfuoten's
    // storage worker and updates the Article objects later (not always with
    // a dataChanged), so recount after every change that can affect it,
    // compressed into one pass per event loop turn.
    m_unreadTimer = new QTimer(this);
    m_unreadTimer->setSingleShot(true);
    m_unreadTimer->setInterval(0);
    connect(m_unreadTimer, &QTimer::timeout, this, &ArticleModel::updateUnreadCount);
    connect(this, &ArticleModel::countChanged, this, &ArticleModel::scheduleUnreadCount);
    connect(this, &QAbstractItemModel::dataChanged, this, &ArticleModel::scheduleUnreadCount);
    connect(this, &ArticleModel::queueChanged, this, &ArticleModel::scheduleUnreadCount);
}

void ArticleModel::scheduleUnreadCount()
{
    m_unreadTimer->start();
}

void ArticleModel::updateUnreadCount()
{
    int n = 0;
    const int rows = rowCount();
    for (int row = 0; row < rows; ++row) {
        Fuoten::Article *a = articleAt(row);
        if (a && a->unread())
            ++n;
    }
    if (n == m_unreadCount)
        return;
    m_unreadCount = n;
    Q_EMIT unreadCountChanged(n);
}

int ArticleModel::hiddenCount() const
{
    return m_source->rowCount() - rowCount();
}

void ArticleModel::setShowRead(bool showRead)
{
    if (m_showRead == showRead)
        return;
    m_showRead = showRead;
    invalidateFilter();
    Q_EMIT showReadChanged(showRead);
}

ArticleModel::~ArticleModel() = default;

QHash<int, QByteArray> ArticleModel::roleNames() const
{
    return {
        { ItemIdRole,    QByteArrayLiteral("itemId") },
        { TitleRole,     QByteArrayLiteral("title") },
        { FeedTitleRole, QByteArrayLiteral("feedTitle") },
        { AgeRole,       QByteArrayLiteral("age") },
        { DayRole,       QByteArrayLiteral("day") },
        { ExcerptRole,   QByteArrayLiteral("excerpt") },
        { ThumbRole,     QByteArrayLiteral("thumb") },
        { UrlRole,       QByteArrayLiteral("url") },
        { UnreadRole,    QByteArrayLiteral("unread") },
        { StarredRole,   QByteArrayLiteral("starred") },
        { PubDateRole,   QByteArrayLiteral("pubDate") },
        { ArticleRole,   QByteArrayLiteral("article") },
        { HasContentRole, QByteArrayLiteral("hasContent") },
    };
}

QVariant ArticleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role < ItemIdRole)
        return QSortFilterProxyModel::data(index, role);

    Fuoten::Article *a = articleAt(index.row());
    if (!a)
        return QVariant();

    switch (role) {
    case ItemIdRole:    return static_cast<double>(a->id());
    case TitleRole:     return HtmlUtils::decodeEntities(a->title());
    case FeedTitleRole: return a->feedTitle();
    case AgeRole:       return ageText(a->pubDate(), QDateTime::currentDateTime());
    case DayRole:       return dayText(a->pubDate(), QDateTime::currentDateTime());
    case ExcerptRole:   return extraFor(a).excerpt;
    case ThumbRole:     return extraFor(a).thumb;
    case UrlRole:       return a->url();
    case UnreadRole:    return a->unread();
    case StarredRole:   return a->starred();
    case PubDateRole:   return a->pubDate();
    case ArticleRole:   return QVariant::fromValue<QObject *>(a);
    case HasContentRole: return extraFor(a).hasContent;
    default:            return QVariant();
    }
}

Fuoten::AbstractStorage *ArticleModel::storage() const
{
    return m_source->storage();
}

void ArticleModel::setStorage(Fuoten::AbstractStorage *storage)
{
    m_source->setStorage(storage);
    // Every read/unread change (local, queued or from a sync) changes the
    // total unread count; the Article objects are updated around it.
    disconnect(m_totalUnreadConn);
    if (storage)
        m_totalUnreadConn = connect(storage, &Fuoten::AbstractStorage::totalUnreadChanged,
                                    this, &ArticleModel::scheduleUnreadCount);
    scheduleUnreadCount();
}

void ArticleModel::setSelection(int folderId, int feedId)
{
    if (folderId < 0)
        folderId = -1;
    if (feedId < 0)
        feedId = -1;
    if (m_folderId == folderId && m_feedId == feedId && !m_starredOnly)
        return;
    m_starredOnly = false;
    m_showRead = false;
    Q_EMIT showReadChanged(false);
    m_folderId = folderId;
    m_feedId = feedId;
    applySelection();
    Q_EMIT selectionChanged();
    reload();
}

void ArticleModel::selectStarred()
{
    if (m_starredOnly)
        return;
    m_starredOnly = true;
    m_showRead = false;
    Q_EMIT showReadChanged(false);
    m_folderId = -1;
    m_feedId = -1;
    applySelection();
    invalidateFilter();
    Q_EMIT selectionChanged();
    reload();
}

bool ArticleModel::oldestFirst() const
{
    return m_source->sortOrder() == Qt::AscendingOrder;
}

void ArticleModel::setOldestFirst(bool oldestFirst)
{
    if (oldestFirst == this->oldestFirst())
        return;
    // Sorted anew once (dynamic sorting is off, see the constructor); the
    // list starts at the top again
    m_source->setSortOrder(oldestFirst ? Qt::AscendingOrder : Qt::DescendingOrder);
    m_source->sort(0);
    Q_EMIT oldestFirstChanged();
}

void ArticleModel::setUnreadOnly(bool unreadOnly)
{
    if (m_unreadOnly == unreadOnly)
        return;
    m_unreadOnly = unreadOnly;
    m_keep.clear();
    invalidateFilter();
    Q_EMIT unreadOnlyChanged(unreadOnly);
}

bool ArticleModel::loaded() const
{
    return m_source->loaded();
}

bool ArticleModel::inOperation() const
{
    return m_source->inOperation();
}

void ArticleModel::applySelection()
{
    if (m_starredOnly) {
        m_source->setParentIdType(Fuoten::FuotenEnums::Starred);
        m_source->setParentId(-1);
    } else if (m_feedId >= 0) {
        m_source->setParentIdType(Fuoten::FuotenEnums::Feed);
        m_source->setParentId(m_feedId);
    } else if (m_folderId < 0) {
        m_source->setParentIdType(Fuoten::FuotenEnums::All);
        m_source->setParentId(-1);
    } else {
        m_source->setParentIdType(Fuoten::FuotenEnums::Folder);
        m_source->setParentId(m_folderId);
    }
}

void ArticleModel::load()
{
    if (!storage())
        return;
    applySelection();
    m_source->load();
}

void ArticleModel::reload()
{
    if (!storage())
        return;
    // libfuoten reloads by removing and re-inserting rows (no model reset),
    // so forget the session state here; re-inserted rows are filtered anew.
    m_keep.clear();
    m_extra.clear();
    m_searchCache.clear();
    m_canFetchOlder = true;
    Q_EMIT fetchingOlderChanged();
    applySelection();
    if (m_source->loaded())
        m_source->reload();
    else
        m_source->load();
}

QStringList ArticleModel::searchWords(const QString &text)
{
    QStringList words = text.toCaseFolded().split(QLatin1Char(' '));
    QStringList out;
    for (const QString &w : words) {
        const QString t = w.trimmed();
        if (!t.isEmpty())
            out << t;
    }
    return out;
}

bool ArticleModel::matchesSearch(const QStringList &words, const QString &foldedHaystack)
{
    for (const QString &w : words)
        if (!foldedHaystack.contains(w))
            return false;
    return true;
}

void ArticleModel::setSearchText(const QString &text)
{
    if (text == m_searchText)
        return;
    m_searchText = text;
    m_searchWords = searchWords(text);
    invalidateFilter();
    Q_EMIT searchChanged();
}

void ArticleModel::setSearchOnly(bool only)
{
    if (only == m_searchOnly)
        return;
    m_searchOnly = only;
    invalidateFilter();
    Q_EMIT searchChanged();
}

void ArticleModel::setSearchFullText(bool fullText)
{
    if (fullText == m_searchFullText)
        return;
    m_searchFullText = fullText;
    m_searchCache.clear();
    invalidateFilter();
    Q_EMIT searchChanged();
}

bool ArticleModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = m_source->index(sourceRow, 0, sourceParent);
    auto *a = m_source->data(idx, Qt::DisplayRole).value<Fuoten::Article *>();
    if (m_searchOnly && m_searchWords.isEmpty())
        return false;
    if (!m_searchWords.isEmpty()) {
        // Search: read articles too; the plain text is built once per article
        if (!a)
            return false;
        auto it = m_searchCache.constFind(a->id());
        if (it == m_searchCache.constEnd()) {
            QString hay = a->title();
            if (m_searchFullText)
                hay += QLatin1Char('\n') + HtmlUtils::plainExcerpt(a->body(), 1000000);
            it = m_searchCache.insert(a->id(), hay.toCaseFolded());
        }
        return matchesSearch(m_searchWords, it.value());
    }
    if (!m_unreadOnly || m_starredOnly || m_showRead)
        return true;
    return a && (a->unread() || m_keep.contains(a->id()));
}

Fuoten::Article *ArticleModel::articleAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return nullptr;
    return QSortFilterProxyModel::data(index(row, 0), Qt::DisplayRole).value<Fuoten::Article *>();
}

Fuoten::Article *ArticleModel::articleById(qint64 id) const
{
    const int row = rowOfId(static_cast<double>(id));
    return row < 0 ? nullptr : articleAt(row);
}

int ArticleModel::rowOfId(double itemId) const
{
    const qint64 id = static_cast<qint64>(itemId);
    const int n = rowCount();
    for (int row = 0; row < n; ++row) {
        Fuoten::Article *a = articleAt(row);
        if (a && a->id() == id)
            return row;
    }
    return -1;
}

const ArticleModel::Extra &ArticleModel::extraFor(Fuoten::Article *a) const
{
    auto it = m_extra.find(a->id());
    if (it == m_extra.end()) {
        Extra e;
        e.excerpt = HtmlUtils::plainExcerpt(a->body(), kExcerptChars);
        e.thumb = HtmlUtils::thumbnail(a->enclosureLink(), a->enclosureMime(), a->body(),
                                       a->mediaThumbnail(), a->url()).toString();
        e.hasContent = HtmlUtils::hasContent(a->body(), a->enclosureLink());
        it = m_extra.insert(a->id(), e);
    }
    return it.value();
}

void ArticleModel::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    // libfuoten only announces Qt::DisplayRole changes; our flat roles are
    // derived from it, so re-announce the affected rows with all roles.
    for (int srow = topLeft.row(); srow <= bottomRight.row(); ++srow) {
        const QModelIndex sidx = m_source->index(srow, 0);
        if (auto *a = m_source->data(sidx, Qt::DisplayRole).value<Fuoten::Article *>())
            m_extra.remove(a->id());        // body may have changed during sync
        const QModelIndex pidx = mapFromSource(sidx);
        if (pidx.isValid())
            Q_EMIT dataChanged(pidx, pidx);
    }
}

void ArticleModel::markRead(int row)
{
    Fuoten::Article *a = articleAt(row);
    if (!a || !a->unread() || a->inOperation())
        return;
    m_keep.insert(a->id());
    a->mark(false, true);               // enqueue = true: local now, server on next sync
    Q_EMIT queueChanged();
}

void ArticleModel::markRowsRead(const QVariantList &rows)
{
    // Resolve all articles first: marking may re-filter and shift rows.
    QList<Fuoten::Article *> articles;
    for (const QVariant &v : rows) {
        if (Fuoten::Article *a = articleAt(v.toInt()))
            articles.append(a);
    }
    bool any = false;
    for (Fuoten::Article *a : articles) {
        if (a->unread() && !a->inOperation()) {
            m_keep.insert(a->id());
            a->mark(false, true);
            any = true;
        }
    }
    if (any)
        Q_EMIT queueChanged();
}

double ArticleModel::idAt(int row) const
{
    Fuoten::Article *a = articleAt(row);
    return a ? static_cast<double>(a->id()) : -1.0;
}

void ArticleModel::markIdsRead(const QVariantList &ids)
{
    bool any = false;
    for (const QVariant &v : ids) {
        Fuoten::Article *a = articleById(static_cast<qint64>(v.toDouble()));
        if (a && a->unread() && !a->inOperation()) {
            m_keep.insert(a->id());
            a->mark(false, true);
            any = true;
        }
    }
    if (any)
        Q_EMIT queueChanged();
}

void ArticleModel::setFetchingOlder(bool fetching)
{
    if (m_fetchingOlder == fetching)
        return;
    m_fetchingOlder = fetching;
    Q_EMIT fetchingOlderChanged();
}

void ArticleModel::fetchOlder()
{
    Fuoten::AbstractStorage *s = storage();
    if (!s || m_fetchingOlder || !canFetchOlder())
        return;

    qint64 oldest = 0;
    const int n = m_source->rowCount();
    for (int row = 0; row < n; ++row) {
        auto *a = m_source->data(m_source->index(row, 0), Qt::DisplayRole).value<Fuoten::Article *>();
        if (a && (oldest == 0 || a->id() < oldest))
            oldest = a->id();
    }

    auto *g = new Fuoten::GetItems(this);
    g->setStorage(s);
    g->setBatchSize(olderBatch);
    g->setOffset(oldest);           // only items with a lower ID (0: newest)
    g->setGetRead(true);
    g->setOldestFirst(false);
    if (m_feedId >= 0) {
        g->setType(Fuoten::FuotenEnums::Feed);
        g->setParentId(m_feedId);
    } else if (m_folderId > 0) {
        g->setType(Fuoten::FuotenEnums::Folder);
        g->setParentId(m_folderId);
    } else {
        // All feeds, also for "Other feeds" (folder 0: feeds without a folder,
        // not a folder the News API knows); the list only takes the items of
        // its own feeds.
        g->setType(Fuoten::FuotenEnums::All);
        g->setParentId(0);
    }

    setFetchingOlder(true);
    connect(g, &Fuoten::Component::failed, this, [this, g](Fuoten::Error *e) {
        g->deleteLater();
        disconnect(m_olderConn);
        setFetchingOlder(false);
        Q_EMIT fetchOlderFailed(e ? e->text() : QString());
    });
    // The storage stores the items (in a worker) and then announces them;
    // the list inserts the new ones by itself.
    disconnect(m_olderConn);
    m_olderConn = connect(s, &Fuoten::AbstractStorage::requestedItems, this,
                          [this](const Fuoten::IdList &, const Fuoten::IdList &newItems, const Fuoten::IdList &) {
        disconnect(m_olderConn);
        if (newItems.isEmpty())
            m_canFetchOlder = false;
        setFetchingOlder(false);
        Q_EMIT olderFetched(newItems.size());
    });
    connect(g, &Fuoten::Component::inOperationChanged, g, [g](bool inOp) {
        if (!inOp)
            g->deleteLater();
    });
    g->execute();
}

void ArticleModel::markAllRead()
{
    Fuoten::AbstractStorage *s = storage();
    if (!s)
        return;

    if (m_starredOnly) {
        // No server call for "starred": mark the listed articles one by one.
        QVariantList rows;
        for (int row = 0; row < rowCount(); ++row)
            rows << row;
        markRowsRead(rows);
        return;
    }

    bool ok = false;
    if (m_feedId < 0 && m_folderId < 0) {
        ok = s->enqueueMarkAllItemsRead();
    } else {
        qint64 newest = -1;
        const int n = m_source->rowCount();
        for (int row = 0; row < n; ++row) {
            auto *a = m_source->data(m_source->index(row, 0), Qt::DisplayRole).value<Fuoten::Article *>();
            if (a && a->id() > newest)
                newest = a->id();
        }
        if (newest > 0)
            ok = m_feedId >= 0 ? s->enqueueMarkFeedRead(m_feedId, newest)
                               : s->enqueueMarkFolderRead(m_folderId, newest);
    }
    if (ok)
        Q_EMIT queueChanged();
}

void ArticleModel::setUnreadById(double itemId, bool unread)
{
    Fuoten::Article *a = articleById(static_cast<qint64>(itemId));
    if (!a || a->unread() == unread || a->inOperation())
        return;
    if (!unread)
        m_keep.insert(a->id());
    a->mark(unread, true);
    Q_EMIT queueChanged();
}

void ArticleModel::setStarredById(double itemId, bool starred)
{
    Fuoten::Article *a = articleById(static_cast<qint64>(itemId));
    if (!a || a->starred() == starred || a->inOperation())
        return;
    a->star(starred, true);
    Q_EMIT queueChanged();
}

QVariantMap ArticleModel::get(int row) const
{
    QVariantMap m;
    Fuoten::Article *a = articleAt(row);
    if (!a)
        return m;
    const QDateTime pub = a->pubDate().toLocalTime();
    m.insert(QStringLiteral("itemId"), static_cast<double>(a->id()));
    m.insert(QStringLiteral("title"), HtmlUtils::decodeEntities(a->title()));
    m.insert(QStringLiteral("feedTitle"), a->feedTitle());
    m.insert(QStringLiteral("feedId"), static_cast<int>(a->feedId()));
    m.insert(QStringLiteral("author"), a->author());
    m.insert(QStringLiteral("pubDate"), pub);
    m.insert(QStringLiteral("dateText"), QLocale().toString(pub, QLocale::ShortFormat));
    m.insert(QStringLiteral("body"), HtmlUtils::cleanBody(a->body()));
    m.insert(QStringLiteral("url"), a->url());
    m.insert(QStringLiteral("thumb"), extraFor(a).thumb);
    m.insert(QStringLiteral("hasContent"), extraFor(a).hasContent);
    m.insert(QStringLiteral("unread"), a->unread());
    m.insert(QStringLiteral("starred"), a->starred());
    return m;
}

void ArticleModel::refreshAges()
{
    const int n = rowCount();
    if (n > 0)
        Q_EMIT dataChanged(index(0, 0), index(n - 1, 0), QVector<int>{AgeRole, DayRole});
}

QString ArticleModel::ageText(const QDateTime &pubDate, const QDateTime &now)
{
    if (!pubDate.isValid())
        return QString();
    const qint64 secs = pubDate.secsTo(now);
    if (secs < 60)
        return tr("now");
    if (secs < 3600)
        return tr("%1 min").arg(secs / 60);
    if (secs < 86400)
        return tr("%1 h").arg(secs / 3600);
    if (secs < 7 * 86400)
        return tr("%1 d").arg(secs / 86400);
    return QLocale().toString(pubDate.toLocalTime().date(), QLocale::ShortFormat);
}

QString ArticleModel::dayText(const QDateTime &pubDate, const QDateTime &now)
{
    if (!pubDate.isValid())
        return QString();
    const QDate day = pubDate.toLocalTime().date();
    const QDate today = now.toLocalTime().date();
    if (day >= today)
        return tr("Today");
    if (day == today.addDays(-1))
        return tr("Yesterday");
    return QLocale().toString(day, QLocale::LongFormat);
}
