// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsbackup.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSysInfo>

#include <algorithm>
#include <cmath>

namespace {
enum class Type { Bool, Int, PerFeed };

struct Spec {
    const char *key;
    Type type;
    int min;
    int max;
};

// Ranges as in the settings page; anything else in a file is dropped.
const Spec kGeneral[] = {
    { "autoReadDelay",       Type::Int,  0, 15 },
    { "autoReadMode",        Type::Int,  0, 1 },
    { "showMarkAllButton",   Type::Bool, 0, 1 },
    { "feedSortMode",        Type::Int,  0, 4 },
    { "openMode",            Type::Int,  0, 2 },
    { "skipPreview",         Type::Bool, 0, 1 },
    { "linksInApp",          Type::Bool, 0, 1 },
    { "unreadOnly",          Type::Bool, 0, 1 },
    { "readerSerif",         Type::Bool, 0, 1 },
    { "readerLineSpacing",   Type::Int,  0, 2 },
    { "readerMargin",        Type::Int,  0, 2 },
    { "readerColors",        Type::Int,  0, 3 },
    { "readerJustify",       Type::Bool, 0, 1 },
    { "readerImages",        Type::Bool, 0, 1 },
    { "shareBrowserCookies", Type::Bool, 0, 1 },
    { "clearCookiesAtStart", Type::Bool, 0, 1 },
};

const Spec kTextSizes[] = {
    { "readerFontPx",        Type::Int,  14, 32 },
    { "previewFontStep",     Type::Int,  -2, 6 },
    { "textSizePerFeed",     Type::Bool, 0, 1 },
    { "previewFontPerFeed",  Type::PerFeed, -2, 6 },
    { "readerFontPerFeed",   Type::PerFeed, 14, 32 },
};

const Spec *findSpec(const QString &key)
{
    for (const Spec &s : kGeneral)
        if (key == QLatin1String(s.key))
            return &s;
    for (const Spec &s : kTextSizes)
        if (key == QLatin1String(s.key))
            return &s;
    return nullptr;
}

bool toInt(const QVariant &v, int min, int max, int *out)
{
    bool ok = false;
    const double d = v.toDouble(&ok);
    if (!ok || v.type() == QVariant::Bool || v.type() == QVariant::String || std::floor(d) != d
            || d < min || d > max)
        return false;
    *out = static_cast<int>(d);
    return true;
}

QList<qint64> favoriteIds(const QVariant &list, QHash<qint64, QString> *titles = nullptr)
{
    QList<qint64> ids;
    for (const QVariant &v : list.toList()) {
        qint64 id = 0;
        QString title;
        if (v.type() == QVariant::Map) {
            const QVariantMap m = v.toMap();
            id = m.value(QStringLiteral("id")).toLongLong();
            title = m.value(QStringLiteral("title")).toString();
        } else {
            id = v.toLongLong();
        }
        if (id <= 0 || ids.contains(id))
            continue;
        ids << id;
        if (titles && !title.isEmpty())
            titles->insert(id, title);
    }
    return ids;
}

QStringList siteList(const QVariant &list)
{
    QStringList sites;
    for (const QVariant &v : list.toList()) {
        const QString s = v.toString().trimmed().toLower();
        if (!s.isEmpty() && !s.contains(QLatin1Char('\n')) && !sites.contains(s))
            sites << s;
    }
    return sites;
}

QVariantList favoritesOut(const QVariant &list)
{
    QHash<qint64, QString> titles;
    QVariantList out;
    for (qint64 id : favoriteIds(list, &titles)) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("title"), titles.value(id));
        out << m;
    }
    return out;
}
} // namespace

SettingsBackup::SettingsBackup(QObject *parent)
    : QObject(parent)
{
}

QStringList SettingsBackup::generalKeys()
{
    QStringList keys;
    for (const Spec &s : kGeneral)
        keys << QLatin1String(s.key);
    return keys;
}

QStringList SettingsBackup::textSizeKeys()
{
    QStringList keys;
    for (const Spec &s : kTextSizes)
        keys << QLatin1String(s.key);
    return keys;
}

bool SettingsBackup::isPerFeedKey(const QString &key)
{
    const Spec *s = findSpec(key);
    return s && s->type == Type::PerFeed;
}

bool SettingsBackup::validValue(const QString &key, const QVariant &value, QVariant *normalized)
{
    const Spec *s = findSpec(key);
    if (!s || !value.isValid())
        return false;
    switch (s->type) {
    case Type::Bool:
        if (value.type() != QVariant::Bool)
            return false;
        *normalized = value.toBool();
        return true;
    case Type::Int: {
        int i = 0;
        if (!toInt(value, s->min, s->max, &i))
            return false;
        *normalized = i;
        return true;
    }
    case Type::PerFeed: {
        if (value.type() != QVariant::Map)
            return false;
        QVariantMap out;
        const QVariantMap in = value.toMap();
        for (auto it = in.cbegin(); it != in.cend(); ++it) {
            bool ok = false;
            const qint64 feedId = it.key().toLongLong(&ok);
            int i = 0;
            if (ok && feedId > 0 && toInt(it.value(), s->min, s->max, &i))
                out.insert(QString::number(feedId), i);
        }
        *normalized = out;
        return true;
    }
    }
    return false;
}

QString SettingsBackup::deviceName() const
{
    QFile f(QStringLiteral("/etc/hw-release"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.startsWith(QLatin1String("NAME="))) {
                QString name = line.mid(5);
                name.remove(QLatin1Char('"'));
                if (!name.isEmpty())
                    return name;
            }
        }
    }
    return QSysInfo::prettyProductName();
}

QString SettingsBackup::serialize(const QVariantMap &state, bool includeTextSizes, const QDateTime &now) const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("sailfeed-settings"));
    root.insert(QStringLiteral("version"), formatVersion);
    const QDateTime when = (now.isValid() ? now : QDateTime::currentDateTime()).toUTC();
    root.insert(QStringLiteral("savedAt"), when.toString(Qt::ISODate));
    root.insert(QStringLiteral("device"), deviceName());
    root.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());

    QJsonObject general;
    for (const QString &key : generalKeys()) {
        QVariant v;
        if (validValue(key, state.value(key), &v))
            general.insert(key, QJsonValue::fromVariant(v));
    }
    root.insert(QStringLiteral("settings"), general);
    root.insert(QStringLiteral("favoriteFeeds"),
                QJsonArray::fromVariantList(favoritesOut(state.value(QStringLiteral("favoriteFeeds")))));
    root.insert(QStringLiteral("originalSites"),
                QJsonArray::fromStringList(siteList(state.value(QStringLiteral("originalSites")))));

    if (includeTextSizes) {
        QJsonObject sizes;
        for (const QString &key : textSizeKeys()) {
            QVariant v;
            if (validValue(key, state.value(key), &v))
                sizes.insert(key, QJsonValue::fromVariant(v));
        }
        root.insert(QStringLiteral("textSizes"), sizes);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString SettingsBackup::fingerprint(const QVariantMap &state, bool includeTextSizes) const
{
    QJsonObject root;
    QJsonObject general;
    for (const QString &key : generalKeys()) {
        QVariant v;
        if (validValue(key, state.value(key), &v))
            general.insert(key, QJsonValue::fromVariant(v));
    }
    root.insert(QStringLiteral("settings"), general);
    QJsonArray favs;
    QList<qint64> ids = favoriteIds(state.value(QStringLiteral("favoriteFeeds")));
    std::sort(ids.begin(), ids.end());
    for (qint64 id : ids)
        favs.append(static_cast<double>(id));
    root.insert(QStringLiteral("favoriteFeeds"), favs);
    QStringList sites = siteList(state.value(QStringLiteral("originalSites")));
    sites.sort();
    root.insert(QStringLiteral("originalSites"), QJsonArray::fromStringList(sites));
    if (includeTextSizes) {
        QJsonObject sizes;
        for (const QString &key : textSizeKeys()) {
            QVariant v;
            if (validValue(key, state.value(key), &v))
                sizes.insert(key, QJsonValue::fromVariant(v));
        }
        root.insert(QStringLiteral("textSizes"), sizes);
    }
    const QByteArray canonical = QJsonDocument(root).toJson(QJsonDocument::Compact);   // keys sorted
    return QString::fromLatin1(QCryptographicHash::hash(canonical, QCryptographicHash::Sha1).toHex().left(16));
}

QVariantMap SettingsBackup::parse(const QString &json) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    const QJsonObject root = doc.object();
    if (err.error != QJsonParseError::NoError || !doc.isObject()
            || root.value(QStringLiteral("format")).toString() != QLatin1String("sailfeed-settings")) {
        result.insert(QStringLiteral("error"), tr("This is not a Sailfeed settings backup."));
        return result;
    }
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version > formatVersion) {
        result.insert(QStringLiteral("error"), tr("This backup was made by a newer Sailfeed version. Please update the app."));
        return result;
    }

    QVariantMap state;
    const QVariantMap general = root.value(QStringLiteral("settings")).toObject().toVariantMap();
    for (const QString &key : generalKeys()) {
        QVariant v;
        if (validValue(key, general.value(key), &v))
            state.insert(key, v);
    }
    const bool hasSizes = root.value(QStringLiteral("textSizes")).isObject();
    if (hasSizes) {
        const QVariantMap sizes = root.value(QStringLiteral("textSizes")).toObject().toVariantMap();
        for (const QString &key : textSizeKeys()) {
            QVariant v;
            if (validValue(key, sizes.value(key), &v))
                state.insert(key, v);
        }
    }
    state.insert(QStringLiteral("favoriteFeeds"),
                 favoritesOut(root.value(QStringLiteral("favoriteFeeds")).toArray().toVariantList()));
    state.insert(QStringLiteral("originalSites"),
                 siteList(root.value(QStringLiteral("originalSites")).toArray().toVariantList()));

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("savedAt"),
                  QDateTime::fromString(root.value(QStringLiteral("savedAt")).toString(), Qt::ISODate));
    result.insert(QStringLiteral("device"), root.value(QStringLiteral("device")).toString());
    result.insert(QStringLiteral("appVersion"), root.value(QStringLiteral("appVersion")).toString());
    result.insert(QStringLiteral("hasTextSizes"), hasSizes);
    result.insert(QStringLiteral("state"), state);
    return result;
}

QVariantMap SettingsBackup::plan(const QVariantMap &current, const QVariantMap &backup,
                                 int mode, bool includeTextSizes) const
{
    const bool replace = mode == Replace;
    QVariantMap changes;
    int settingsChanged = 0;
    bool textSizesChanged = false;

    if (replace) {
        for (const QString &key : generalKeys()) {
            QVariant b, c;
            if (!validValue(key, backup.value(key), &b))
                continue;
            if (!validValue(key, current.value(key), &c) || c != b) {
                changes.insert(key, b);
                ++settingsChanged;
            }
        }
    }

    if (includeTextSizes) {
        for (const QString &key : textSizeKeys()) {
            QVariant b, c;
            if (!validValue(key, backup.value(key), &b))
                continue;               // not in the backup
            const bool hasCurrent = validValue(key, current.value(key), &c);
            QVariant next;
            if (isPerFeedKey(key)) {
                QVariantMap merged = hasCurrent ? c.toMap() : QVariantMap();
                const QVariantMap fromBackup = b.toMap();
                if (replace) {
                    merged = fromBackup;
                } else {
                    for (auto it = fromBackup.cbegin(); it != fromBackup.cend(); ++it)
                        if (!merged.contains(it.key()))
                            merged.insert(it.key(), it.value());
                }
                next = merged;
            } else if (replace) {
                next = b;
            } else {
                continue;               // "add missing" keeps the general sizes
            }
            if (!hasCurrent || next != c) {
                changes.insert(key, next);
                textSizesChanged = true;
            }
        }
    }

    // Favorite feeds
    QHash<qint64, QString> curTitles, backupTitles;
    const QList<qint64> curFav = favoriteIds(current.value(QStringLiteral("favoriteFeeds")), &curTitles);
    const QList<qint64> backupFav = favoriteIds(backup.value(QStringLiteral("favoriteFeeds")), &backupTitles);
    QList<qint64> newFav = replace ? backupFav : curFav;
    if (!replace) {
        for (qint64 id : backupFav)
            if (!newFav.contains(id))
                newFav << id;
    }
    auto title = [&](qint64 id) {
        QString t = curTitles.value(id);
        if (t.isEmpty())
            t = backupTitles.value(id);
        return t.isEmpty() ? QString::number(id) : t;
    };
    QStringList favAdded, favRemoved;
    for (qint64 id : newFav)
        if (!curFav.contains(id))
            favAdded << title(id);
    for (qint64 id : curFav)
        if (!newFav.contains(id))
            favRemoved << title(id);

    // Sites in the original
    const QStringList curSites = siteList(current.value(QStringLiteral("originalSites")));
    const QStringList backupSites = siteList(backup.value(QStringLiteral("originalSites")));
    QStringList newSites = replace ? backupSites : curSites;
    if (!replace) {
        for (const QString &s : backupSites)
            if (!newSites.contains(s))
                newSites << s;
    }
    QStringList sitesAdded, sitesRemoved;
    for (const QString &s : newSites)
        if (!curSites.contains(s))
            sitesAdded << s;
    for (const QString &s : curSites)
        if (!newSites.contains(s))
            sitesRemoved << s;

    QVariantMap result;
    result.insert(QStringLiteral("changes"), changes);
    if (!favAdded.isEmpty() || !favRemoved.isEmpty()) {
        QVariantList ids;
        for (qint64 id : newFav)
            ids << id;
        result.insert(QStringLiteral("favoriteFeeds"), ids);
    }
    if (!sitesAdded.isEmpty() || !sitesRemoved.isEmpty())
        result.insert(QStringLiteral("originalSites"), newSites);
    result.insert(QStringLiteral("favoritesAdded"), favAdded);
    result.insert(QStringLiteral("favoritesRemoved"), favRemoved);
    result.insert(QStringLiteral("sitesAdded"), sitesAdded);
    result.insert(QStringLiteral("sitesRemoved"), sitesRemoved);
    result.insert(QStringLiteral("settingsChanged"), settingsChanged);
    result.insert(QStringLiteral("textSizesChanged"), textSizesChanged);
    result.insert(QStringLiteral("hasChanges"), !changes.isEmpty() || !favAdded.isEmpty() || !favRemoved.isEmpty()
                                                || !sitesAdded.isEmpty() || !sitesRemoved.isEmpty());
    return result;
}
