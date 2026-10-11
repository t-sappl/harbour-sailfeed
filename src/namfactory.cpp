// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "namfactory.h"

#include <QNetworkDiskCache>
#include <QNetworkRequest>

CachingNam::CachingNam(QObject *parent) : QNetworkAccessManager(parent) {}

QNetworkReply *CachingNam::createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QNetworkRequest req(request);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    if (!req.hasRawHeader("User-Agent"))
        req.setRawHeader("User-Agent", "Mozilla/5.0 (Sailfish OS; Mobile) Sailfeed");
    return QNetworkAccessManager::createRequest(op, req, outgoingData);
}

NamFactory::NamFactory(const QString &cacheDir, qint64 maxCacheBytes)
    : m_cacheDir(cacheDir)
    , m_maxCacheBytes(maxCacheBytes)
{
}

QNetworkAccessManager *NamFactory::create(QObject *parent)
{
    // QML may call this from several threads (e.g. the image loader thread),
    // so every manager gets its own cache object on the shared directory.
    auto *nam = new CachingNam(parent);
    auto *cache = new QNetworkDiskCache(nam);
    cache->setCacheDirectory(m_cacheDir);
    cache->setMaximumCacheSize(m_maxCacheBytes);
    nam->setCache(cache);
    return nam;
}
