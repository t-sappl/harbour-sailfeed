/*
 * SPDX-FileCopyrightText: (C) 2016-2022 Matthias Fehring <https://www.huessenbergnetz.de>
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Modified 2026 by Thomas Sappl for harbour-sailfeed, see 3rdparty/README.md
 * of harbour-sailfeed and patches-libfuoten.diff.
 */

#ifndef SQLITESTORAGE_P
#define SQLITESTORAGE_P

#include "sqlitestorage.h"
#include "abstractstorage_p.h"
#include "../Helpers/abstractconfiguration.h"
#include "../Helpers/abstractnotificator.h"
#include "../article.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QStringList>
#include <QThread>
#include <QJsonDocument>
#include <QSqlQuery>

namespace Fuoten {

// sailfeed patch: one database connection per worker thread. QSqlDatabase
// connections must only be used in the thread that created them; all
// workers used to share the main thread's "fuotendb" connection, which
// crashed when a worker and the main thread queried at the same time.
class WorkerDbConnection
{
public:
    WorkerDbConnection(QSqlDatabase &db, const QString &path, const void *owner)
        : m_db(db)
        , m_name(QStringLiteral("fuotendb_worker_%1").arg(reinterpret_cast<quintptr>(owner)))
    {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_db.setDatabaseName(path);
        m_db.open();
    }
    ~WorkerDbConnection()
    {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }
private:
    QSqlDatabase &m_db;
    QString m_name;
};

class SQLiteStorageManager : public QThread {
    Q_OBJECT
public:
    explicit SQLiteStorageManager(const QString &dbpath, QObject *parent = nullptr);
    ~SQLiteStorageManager() override;

private:
    QSqlDatabase m_db;
    QString m_dbpath;
    quint16 m_currentDbVersion;
    void setFailed(const QSqlError &sqlError, const QString &text);

protected:
    void run() override;

Q_SIGNALS:
    void succeeded();
    void failed(Fuoten::Error *error);
};


class SQLiteStoragePrivate : public AbstractStoragePrivate {
public:
    SQLiteStoragePrivate(const QString &_dbpath);
    ~SQLiteStoragePrivate() override;

    QStringList intListToStringList(const IdList &ints) const;
    QString intListToString(const IdList &ints) const;
    QSqlQuery getQuery() const;

    QSqlDatabase db;
    QThread worker;
};


class ItemsRequestedWorker : public QThread
{
    Q_OBJECT
public:
    ItemsRequestedWorker(const QString &dbpath, const QJsonDocument &json, AbstractConfiguration *config = nullptr, AbstractNotificator *notificator = nullptr, QObject *parent = nullptr);
    ~ItemsRequestedWorker() override;

Q_SIGNALS:
    void requestedItems(const Fuoten::IdList &updatedItems, const Fuoten::IdList &newItems, const Fuoten::IdList &deletedItems);
    void gotTotalUnread(int tu);
    void gotStarred(int st);
    void failed(Fuoten::Error *e);

protected:
    void run() override;

private:
    QSqlDatabase m_db;
    QString m_dbpath;
    QJsonDocument m_json;
    AbstractConfiguration *m_config;
    AbstractNotificator *m_notificator;
};


class GetArticlesAsyncWorker : public QThread
{
    Q_OBJECT
public:
    GetArticlesAsyncWorker(const QString &dbpath, const QueryArgs &args, QObject *parent = nullptr);
    ~GetArticlesAsyncWorker() override;

Q_SIGNALS:
    void gotArticles(const Fuoten::ArticleList &articles);
    void failed(Fuoten::Error *e);

protected:
    void run() override;

private:
    QSqlDatabase m_db;
    QString m_dbpath;
    QueryArgs m_args;
};


class EnqueueMarkReadWorker : public QThread
{
    Q_OBJECT
public:
    EnqueueMarkReadWorker(const QString &dbpath, qint64 id, FuotenEnums::Type idType, qint64 newestItemId = -1, QObject *parent = nullptr);
    ~EnqueueMarkReadWorker() override;

Q_SIGNALS:
    void failed(Fuoten::Error *e);
    void markedReadFeedInQueue(qint64 feedId, qint64 newestItemId);
    void markedReadFolderInQueue(qint64 folderId, qint64 newestItemId);
    void markedAllItemsReadInQueue();
    void gotTotalUnread(int tu);

protected:
    void run() override;

private:
    qint64 m_id;
    qint64 m_newestItemId;
    QSqlDatabase m_db;
    QString m_dbpath;
    FuotenEnums::Type m_idType;
};



class ClearQueueWorker : public QThread
{
    Q_OBJECT
public:
    ClearQueueWorker(const QString &dbpath, QObject *parent = nullptr);
    ~ClearQueueWorker() override;

Q_SIGNALS:
    void failed(Fuoten::Error *e);
    void queueCleared();

protected:
    void run() override;

private:
    QSqlDatabase m_db;
    QString m_dbpath;
};


}

#endif // SQLITESTORAGE_P

