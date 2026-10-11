// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QQmlNetworkAccessManagerFactory>
#include <QString>

// Network access for QML (thumbnails and article images) with an on-disk
// HTTP cache, so images are not downloaded again on every start.
class CachingNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit CachingNam(QObject *parent = nullptr);

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData = nullptr) override;
};

class NamFactory : public QQmlNetworkAccessManagerFactory
{
public:
    // cacheDir: e.g. ~/.cache/org.sappl/harbour-sailfeed/images
    NamFactory(const QString &cacheDir, qint64 maxCacheBytes);
    QNetworkAccessManager *create(QObject *parent) override;

private:
    QString m_cacheDir;
    qint64 m_maxCacheBytes;
};
