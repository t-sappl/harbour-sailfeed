// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The settings backup file (release 1.0, package 5b): turns the app's
// settings into JSON and back, and works out what a restore changes.
// Pure data, no storage access, so it can be tested on the desktop; the
// QML side (harbour-sailfeed.qml: settingsState(), applySettingsPlan())
// reads the values from dconf/QSettings and writes the result back.
//
// A "state" is a flat map: every key of generalKeys() and textSizeKeys()
// (the per-feed text sizes as maps feed ID -> value), plus
//   favoriteFeeds: [{ id, title }]   (favorites are stored by feed ID; the
//                                     title is only for showing changes)
//   originalSites: [ "golem.de", … ]
//
// File format "sailfeed-settings", version 1:
//   { format, version, savedAt (UTC, ISO), device, appVersion,
//     settings: { general keys }, favoriteFeeds, originalSites,
//     textSizes: { text size keys } }      // only if chosen
class SettingsBackup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList generalKeys READ generalKeys CONSTANT)
    Q_PROPERTY(QStringList textSizeKeys READ textSizeKeys CONSTANT)
    Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
    Q_PROPERTY(QString currentFileName READ currentFileName CONSTANT)
public:
    enum RestoreMode {
        Replace = 0,        // settings exactly as in the backup
        AddMissing = 1      // only add favorites, sites and per-feed sizes; nothing else changes
    };
    Q_ENUM(RestoreMode)

    static constexpr int formatVersion = 1;

    explicit SettingsBackup(QObject *parent = nullptr);

    static QStringList generalKeys();
    static QStringList textSizeKeys();
    // Keys whose value is a map feed ID -> number
    static bool isPerFeedKey(const QString &key);

    // "Jolla C2", from /etc/hw-release (or the OS name)
    QString deviceName() const;

    // Backup file for the given state. includeTextSizes: add the text sizes
    // group. now: for tests (default: current time).
    Q_INVOKABLE QString serialize(const QVariantMap &state, bool includeTextSizes,
                                  const QDateTime &now = QDateTime()) const;

    // Short hash of what a backup of this state would contain (without
    // date, device and feed titles): changes when a backed-up setting
    // changes, used by the automatic backup.
    Q_INVOKABLE QString fingerprint(const QVariantMap &state, bool includeTextSizes) const;

    // Reads a backup file. Result: ok, error (text for the user), savedAt,
    // device, appVersion, hasTextSizes, state (as above, only valid values:
    // unknown keys and values out of range are dropped).
    Q_INVOKABLE QVariantMap parse(const QString &json) const;

    // What restoring `backup` (parse()'s state) over `current` changes.
    // Result:
    //   changes:        map key -> new value (general and text size keys)
    //   favoriteFeeds:  new list of favorite feed IDs (only if it changes)
    //   originalSites:  new list of sites (only if it changes)
    //   favoritesAdded / favoritesRemoved: titles (ID if unknown)
    //   sitesAdded / sitesRemoved
    //   settingsChanged: number of changed general settings
    //   textSizesChanged: bool
    //   hasChanges: bool
    Q_INVOKABLE QVariantMap plan(const QVariantMap &current, const QVariantMap &backup,
                                 int mode, bool includeTextSizes) const;

    static QString currentFileName() { return QStringLiteral("sailfeed-settings.json"); }

private:
    static bool validValue(const QString &key, const QVariant &value, QVariant *normalized);
};
