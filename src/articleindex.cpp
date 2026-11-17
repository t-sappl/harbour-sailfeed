// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "articleindex.h"
#include "htmlutils.h"

#include <Fuoten/Storage/SQLiteStorage>

#include <QMetaObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUrl>
#include <QVariant>

namespace {
constexpr int kBatch = 100;

// A null QString is bound as NULL (the columns are NOT NULL)
QString notNull(const QString &s)
{
    return s.isNull() ? QStringLiteral("") : s;
}

QString idList(const QList<qint64> &ids)
{
    QStringList l;
    l.reserve(ids.size());
    for (qint64 id : ids)
        l << QString::number(id);
    return l.join(QLatin1Char(','));
}
} // namespace

QString ArticleIndex::tableSql()
{
    return QStringLiteral("CREATE TABLE IF NOT EXISTS sailfeed_extra "
                          "(id INTEGER PRIMARY KEY NOT NULL, guidHash TEXT NOT NULL, fingerprint TEXT NOT NULL, "
                          "excerpt TEXT NOT NULL, thumb TEXT NOT NULL, hasContent INTEGER NOT NULL, "
                          "search TEXT NOT NULL)");
}

ArticleIndex *ArticleIndex::forStorage(Fuoten::SQLiteStorage *storage)
{
    if (!storage)
        return nullptr;
    if (auto *existing = storage->findChild<ArticleIndex *>(QString(), Qt::FindDirectChildrenOnly))
        return existing;
    return new ArticleIndex(storage);
}

ArticleIndex::ArticleIndex(Fuoten::SQLiteStorage *storage)
    : QObject(storage)
    , m_storage(storage)
    , m_path(storage->databasePath())
    , m_readName(QStringLiteral("sailfeed-extra-read-%1").arg(reinterpret_cast<quintptr>(this)))
{
    qRegisterMetaType<QList<qint64>>("QList<qint64>");
    m_worker = new ArticleIndexWorker(this);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &ArticleIndexWorker::indexed, this, &ArticleIndex::indexed);
    connect(m_worker, &ArticleIndexWorker::searchFinished, this, &ArticleIndex::searchFinished);
    connect(m_worker, &ArticleIndexWorker::indexDone, this, &ArticleIndex::indexDone);
    m_thread.setObjectName(QStringLiteral("sailfeed-index"));
    m_thread.start(QThread::LowPriority);
    QMetaObject::invokeMethod(m_worker, "open", Qt::QueuedConnection, Q_ARG(QString, m_path));

    // New and changed articles of a sync (also "Load older articles"). A
    // changed article has a new fingerprint, so it counts as missing; a
    // read/star change keeps it and costs nothing.
    connect(storage, &Fuoten::AbstractStorage::requestedItems, this, [this]() { requestIndexing(); });
    // Ready again after clearStorage() (signed out) or at start
    connect(storage, &Fuoten::AbstractStorage::readyChanged, this, [this](bool ready) {
        if (ready)
            requestIndexing();
    });
    if (storage->ready())
        requestIndexing();
}

ArticleIndex::~ArticleIndex()
{
    m_stop.store(true);         // a run stops after its batch
    QMetaObject::invokeMethod(m_worker, "close", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
    if (m_readDb.isValid()) {
        m_readDb.close();
        m_readDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_readName);
    }
}

void ArticleIndex::requestIndexing()
{
    if (m_indexQueued.exchange(true))
        return;
    ++m_pending;
    QMetaObject::invokeMethod(m_worker, "indexMissing", Qt::QueuedConnection);
}

int ArticleIndex::search(const QStringList &foldedWords)
{
    const int token = ++m_lastToken;
    ++m_pending;
    QMetaObject::invokeMethod(m_worker, "runSearch", Qt::QueuedConnection,
                              Q_ARG(int, token), Q_ARG(QStringList, foldedWords));
    return token;
}

QHash<qint64, ArticleIndex::Extra> ArticleIndex::lookup(const QList<qint64> &ids)
{
    QHash<qint64, Extra> out;
    if (ids.isEmpty() || m_path.isEmpty())
        return out;
    if (!m_readDb.isValid()) {
        m_readDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_readName);
        m_readDb.setDatabaseName(m_path);
        m_readDb.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=500"));
    }
    if (!m_readDb.isOpen() && !m_readDb.open())
        return out;
    QSqlQuery q(m_readDb);
    q.setForwardOnly(true);
    // Joined with items: only rows of this very article (same guidHash) in
    // its current version (same fingerprint)
    if (!q.exec(QStringLiteral("SELECT e.id, e.excerpt, e.thumb, e.hasContent FROM sailfeed_extra e "
                               "JOIN items i ON i.id = e.id AND i.guidHash = e.guidHash "
                               "AND i.fingerprint = e.fingerprint WHERE e.id IN (%1)")
                .arg(idList(ids))))
        return out;     // table not there yet: nothing indexed
    while (q.next()) {
        Extra e;
        e.excerpt = q.value(1).toString();
        e.thumb = q.value(2).toString();
        e.hasContent = q.value(3).toBool();
        e.known = true;
        out.insert(q.value(0).toLongLong(), e);
    }
    return out;
}

// --- worker ----------------------------------------------------------------

void ArticleIndexWorker::open(const QString &path)
{
    m_path = path;
    ensureOpen();
}

bool ArticleIndexWorker::ensureOpen()
{
    if (m_db.isOpen() && m_tableReady)
        return true;
    if (m_path.isEmpty())
        return false;
    if (m_name.isEmpty()) {
        m_name = QStringLiteral("sailfeed-extra-write-%1").arg(reinterpret_cast<quintptr>(this));
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_db.setDatabaseName(m_path);
        m_db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));
    }
    if (!m_db.isOpen()) {
        if (!m_db.open())
            return false;
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
        q.exec(QStringLiteral("PRAGMA journal_size_limit = 8388608"));
    }
    // Tried again at every call until it worked (the database may be locked
    // by the start's cleanup for a while)
    m_tableReady = QSqlQuery(m_db).exec(ArticleIndex::tableSql());
    return m_tableReady;
}

void ArticleIndexWorker::close()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }
}

void ArticleIndexWorker::indexMissing()
{
    // Requests from now on start a new run – unless one is running: it goes
    // on until nothing is missing, new articles included
    m_index->m_indexQueued.store(false);
    if (!m_running) {
        if (ensureOpen()) {
            // Rows of articles that are gone (deleted, other account)
            QSqlQuery(m_db).exec(QStringLiteral("DELETE FROM sailfeed_extra WHERE id NOT IN (SELECT id FROM items)"));
        }
        m_running = true;
        indexBatch();
    }
    --m_index->m_pending;
}

// One batch per call, the next one queued: a search waiting in the queue
// runs in between, and closing the app does not wait for the whole run
void ArticleIndexWorker::indexBatch()
{
    bool more = false;
    if (!m_index->m_stop.load() && ensureOpen()) {
        QSqlQuery sel(m_db);
        sel.setForwardOnly(true);
        if (sel.exec(QStringLiteral(
                "SELECT i.id, i.guidHash, i.fingerprint, i.title, i.body, i.enclosureMime, i.enclosureLink, "
                "i.mediaThumbnail, i.url FROM items i "
                "LEFT JOIN sailfeed_extra e ON e.id = i.id AND e.guidHash = i.guidHash AND e.fingerprint = i.fingerprint "
                "WHERE e.id IS NULL ORDER BY i.pubDate DESC LIMIT %1").arg(kBatch))) {
            struct Row { qint64 id; QString guidHash; QString fingerprint; HtmlUtils::ArticleExtra extra; };
            QList<Row> rows;
            while (sel.next()) {
                Row r;
                r.id = sel.value(0).toLongLong();
                r.guidHash = sel.value(1).toString();
                r.fingerprint = sel.value(2).toString();
                r.extra = HtmlUtils::articleExtra(sel.value(3).toString(), sel.value(4).toString(),
                                                  QUrl(sel.value(6).toString()), sel.value(5).toString(),
                                                  QUrl(sel.value(7).toString()), QUrl(sel.value(8).toString()));
                rows.append(r);
            }
            sel.finish();
            if (!rows.isEmpty()) {
                bool ok = m_db.transaction();
                QSqlQuery ins(m_db);
                ok = ok && ins.prepare(QStringLiteral("INSERT OR REPLACE INTO sailfeed_extra "
                                                      "(id, guidHash, fingerprint, excerpt, thumb, hasContent, search) "
                                                      "VALUES (?, ?, ?, ?, ?, ?, ?)"));
                QList<qint64> ids;
                for (const Row &r : rows) {
                    if (!ok)
                        break;
                    ins.addBindValue(r.id);
                    ins.addBindValue(notNull(r.guidHash));
                    ins.addBindValue(notNull(r.fingerprint));
                    ins.addBindValue(notNull(r.extra.excerpt));
                    ins.addBindValue(notNull(r.extra.thumb));
                    ins.addBindValue(r.extra.hasContent ? 1 : 0);
                    ins.addBindValue(notNull(r.extra.search));
                    ok = ins.exec();
                    ids.append(r.id);
                }
                if (ok && m_db.commit()) {
                    Q_EMIT indexed(ids);
                    more = rows.size() == kBatch;
                } else {
                    qWarning("Article index: could not store a batch: %s / %s",
                             qUtf8Printable(m_db.lastError().text()), qUtf8Printable(ins.lastError().text()));
                    m_db.rollback();    // tried again at the next request
                }
            }
        }
        // else: items table not there (signed out)
    }
    if (more && !m_index->m_stop.load()) {
        QMetaObject::invokeMethod(this, "indexBatch", Qt::QueuedConnection);
        return;
    }
    m_running = false;
    Q_EMIT indexDone();
}

void ArticleIndexWorker::runSearch(int token, const QStringList &words)
{
    QList<qint64> ids;
    if (ensureOpen() && !words.isEmpty()) {
        QString sql = QStringLiteral("SELECT e.id FROM sailfeed_extra e JOIN items i ON i.id = e.id "
                                     "AND i.guidHash = e.guidHash WHERE 1 = 1");
        for (int i = 0; i < words.size(); ++i)
            sql += QStringLiteral(" AND instr(e.search, ?) > 0");
        QSqlQuery q(m_db);
        q.setForwardOnly(true);
        if (q.prepare(sql)) {
            for (const QString &w : words)
                q.addBindValue(w);
            if (q.exec()) {
                while (q.next())
                    ids.append(q.value(0).toLongLong());
            }
        }
    }
    Q_EMIT searchFinished(token, ids);
    --m_index->m_pending;
}
