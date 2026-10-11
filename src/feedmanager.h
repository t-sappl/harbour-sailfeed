// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace Fuoten {
class AbstractStorage;
class Component;
}

// Feed management on the Nextcloud server: add, rename, move and delete
// feeds; add, rename and delete folders. Uses libfuoten's API components with
// the default configuration and storage, so the local database is updated
// together with the server. One operation at a time. QML singleton
// "FeedManager".
class FeedManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit FeedManager(QObject *parent = nullptr);

    bool busy() const { return m_busy; }

    // folderId 0 = no folder. url without scheme gets "https://".
    Q_INVOKABLE bool addFeed(const QString &url, int folderId);
    Q_INVOKABLE bool addFolder(const QString &name);
    Q_INVOKABLE bool renameFeed(int feedId, const QString &name);
    Q_INVOKABLE bool moveFeed(int feedId, int folderId);
    Q_INVOKABLE bool deleteFeed(int feedId);
    Q_INVOKABLE bool renameFolder(int folderId, const QString &name);
    // Deletes the folder with all its feeds (server behaviour).
    Q_INVOKABLE bool deleteFolder(int folderId);

    // Folders of the local database, by name: [{ id, name }, ...]
    Q_INVOKABLE QVariantList folders() const;

    // "example.org/feed" -> "https://example.org/feed"; "" if not usable.
    Q_INVOKABLE static QString normalizeUrl(const QString &input);

    // For tests: storage to read folders from (default: Component's default).
    void setStorage(Fuoten::AbstractStorage *storage) { m_storage = storage; }

Q_SIGNALS:
    void busyChanged(bool busy);
    // action: "addFeed", "addFolder", "renameFeed", "moveFeed", "deleteFeed",
    // "renameFolder", "deleteFolder"
    void succeeded(const QString &action);
    void failed(const QString &action, const QString &message);

private:
    bool run(Fuoten::Component *c, const QString &action);
    void setBusy(bool busy);

    bool m_busy = false;
    Fuoten::AbstractStorage *m_storage = nullptr;
};
