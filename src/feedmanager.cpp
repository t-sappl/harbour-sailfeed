// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "feedmanager.h"

#include <Fuoten/API/CreateFeed>
#include <Fuoten/API/CreateFolder>
#include <Fuoten/API/DeleteFeed>
#include <Fuoten/API/DeleteFolder>
#include <Fuoten/API/MoveFeed>
#include <Fuoten/API/RenameFeed>
#include <Fuoten/API/RenameFolder>
#include <Fuoten/Error>
#include <Fuoten/Folder>
#include <Fuoten/FuotenEnums>
#include <Fuoten/Storage/AbstractStorage>

#include <QSharedPointer>
#include <QUrl>
#include <QVariantMap>

FeedManager::FeedManager(QObject *parent) : QObject(parent) {}

void FeedManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged(busy);
}

QString FeedManager::normalizeUrl(const QString &input)
{
    QString s = input.trimmed();
    if (s.isEmpty())
        return QString();
    if (!s.contains(QLatin1String("://")))
        s.prepend(QLatin1String("https://"));
    const QUrl url(s, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()
            || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
        return QString();
    return url.toString();
}

// Starts c and reports the result. libfuoten emits failed(Error*) on errors
// (also for invalid input before any request) and otherwise ends the
// operation without error; the storage has been updated by then.
bool FeedManager::run(Fuoten::Component *c, const QString &action)
{
    if (m_busy) {
        delete c;
        return false;
    }
    setBusy(true);
    auto done = QSharedPointer<bool>::create(false);
    auto finish = [this, c, done, action](const QString &error) {
        if (*done)
            return;
        *done = true;
        c->deleteLater();
        setBusy(false);
        if (error.isNull())
            Q_EMIT succeeded(action);
        else
            Q_EMIT failed(action, error);
    };
    connect(c, &Fuoten::Component::failed, this, [finish](Fuoten::Error *e) {
        QString text = e ? e->text() : QString();
        if (e && !e->data().isEmpty())
            text += QStringLiteral(" (") + e->data() + QLatin1Char(')');
        finish(text.isNull() ? QStringLiteral("") : text);
    });
    connect(c, &Fuoten::Component::inOperationChanged, this, [c, finish](bool inOp) {
        if (!inOp && !c->error())
            finish(QString());
    });
    c->execute();
    return true;
}

bool FeedManager::addFeed(const QString &url, int folderId)
{
    const QString u = normalizeUrl(url);
    auto *c = new Fuoten::CreateFeed(this);
    c->setUrl(QUrl(u));
    c->setFolderId(folderId < 0 ? 0 : folderId);
    return run(c, QStringLiteral("addFeed"));
}

bool FeedManager::addFolder(const QString &name)
{
    auto *c = new Fuoten::CreateFolder(this);
    c->setName(name.trimmed());
    return run(c, QStringLiteral("addFolder"));
}

bool FeedManager::renameFeed(int feedId, const QString &name)
{
    auto *c = new Fuoten::RenameFeed(this);
    c->setFeedId(feedId);
    c->setNewName(name.trimmed());
    return run(c, QStringLiteral("renameFeed"));
}

bool FeedManager::moveFeed(int feedId, int folderId)
{
    auto *c = new Fuoten::MoveFeed(this);
    c->setFeedId(feedId);
    c->setFolderId(folderId < 0 ? 0 : folderId);
    return run(c, QStringLiteral("moveFeed"));
}

bool FeedManager::deleteFeed(int feedId)
{
    auto *c = new Fuoten::DeleteFeed(this);
    c->setFeedId(feedId);
    return run(c, QStringLiteral("deleteFeed"));
}

bool FeedManager::renameFolder(int folderId, const QString &name)
{
    auto *c = new Fuoten::RenameFolder(this);
    c->setFolderId(folderId);
    c->setNewName(name.trimmed());
    return run(c, QStringLiteral("renameFolder"));
}

bool FeedManager::deleteFolder(int folderId)
{
    auto *c = new Fuoten::DeleteFolder(this);
    c->setFolderId(folderId);
    return run(c, QStringLiteral("deleteFolder"));
}

QVariantList FeedManager::folders() const
{
    QVariantList list;
    Fuoten::AbstractStorage *s = m_storage ? m_storage : Fuoten::Component::defaultStorage();
    if (!s || !s->ready())
        return list;
    const QList<Fuoten::Folder *> folders = s->getFolders(Fuoten::FuotenEnums::Name, Qt::AscendingOrder);
    for (Fuoten::Folder *f : folders) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), static_cast<int>(f->id()));
        m.insert(QStringLiteral("name"), f->name());
        list << m;
        delete f;
    }
    return list;
}
