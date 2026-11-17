// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QThread>
#include <atomic>

namespace Fuoten {
class SQLiteStorage;
}

class ArticleIndexWorker;

// What the article list and the search need from the article bodies,
// computed once per article and kept in the app's own table
// "sailfeed_extra" of the database (1.4, speed): excerpt, list thumbnail,
// hasContent and the case-folded search text. The list loads its articles
// without bodies (they were all held in memory and parsed row by row on the
// GUI thread), reads these values for the rows it shows and fetches a body
// only when an article is opened.
//
// A worker thread with its own connection fills the table: at start for
// what is missing (newest articles first), after every sync for the new and
// changed articles. Rows are tied to the article by id, guidHash and
// fingerprint: a row left over from another account (ids start again) is
// never used, a changed article (new fingerprint) is indexed again.
// One instance per storage (forStorage()).
class ArticleIndex : public QObject
{
    Q_OBJECT
public:
    struct Extra {
        QString excerpt;
        QString thumb;
        bool hasContent = true;
        bool known = false;     // false: not indexed yet, defaults
    };

    static ArticleIndex *forStorage(Fuoten::SQLiteStorage *storage);
    ~ArticleIndex() override;

    // Values of the indexed articles among ids (GUI thread, read-only
    // connection). Missing ids are not indexed yet: requestIndexing().
    QHash<qint64, Extra> lookup(const QList<qint64> &ids);

    // Index what is missing (queued; several calls make one run)
    void requestIndexing();
    // Full-text search: ids of the articles whose title and text contain
    // every word (words case-folded). Answered by searchFinished(token, …).
    int search(const QStringList &foldedWords);

    // For tests: true while the worker has jobs.
    bool busy() const { return m_pending.load() > 0; }

    static QString tableSql();

Q_SIGNALS:
    void indexed(const QList<qint64> &ids);
    // A run of requestIndexing() is over (also when nothing was missing)
    void indexDone();
    void searchFinished(int token, const QList<qint64> &ids);

private:
    explicit ArticleIndex(Fuoten::SQLiteStorage *storage);

    Fuoten::SQLiteStorage *m_storage;
    QString m_path;
    QThread m_thread;
    ArticleIndexWorker *m_worker = nullptr;
    QSqlDatabase m_readDb;
    QString m_readName;
    std::atomic<int> m_pending{0};
    std::atomic<bool> m_indexQueued{false};
    std::atomic<bool> m_stop{false};
    int m_lastToken = 0;

    friend class ArticleIndexWorker;
};

class ArticleIndexWorker : public QObject
{
    Q_OBJECT
public:
    explicit ArticleIndexWorker(ArticleIndex *index) : m_index(index) {}

public Q_SLOTS:
    void open(const QString &path);
    void close();
    void indexMissing();
    void indexBatch();
    void runSearch(int token, const QStringList &words);

Q_SIGNALS:
    void indexed(const QList<qint64> &ids);
    void indexDone();
    void searchFinished(int token, const QList<qint64> &ids);

private:
    bool ensureOpen();
    ArticleIndex *m_index;
    QSqlDatabase m_db;
    QString m_name;
    QString m_path;
    bool m_tableReady = false;
    bool m_running = false;     // a run of indexBatch() calls is going on
};
