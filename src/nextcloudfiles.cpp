// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "nextcloudfiles.h"

#include "accountmanager.h"
#include "configuration.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>

namespace {
const QString kUserId = QStringLiteral("backup/userId");
const QString kUserIdAccount = QStringLiteral("backup/userIdAccount");

QByteArray propfindBody()
{
    return QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                             "<d:propfind xmlns:d=\"DAV:\"><d:prop>"
                             "<d:getlastmodified/><d:getcontentlength/><d:getetag/><d:resourcetype/>"
                             "</d:prop></d:propfind>");
}

QString cleanEtag(QString etag)
{
    etag = etag.trimmed();
    if (etag.startsWith(QLatin1String("W/")))
        etag = etag.mid(2);
    etag.remove(QLatin1Char('"'));
    return etag;
}

bool isSuccess(int status)
{
    return status >= 200 && status < 300;
}
} // namespace

NextcloudFiles::NextcloudFiles(Configuration *config, QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
}

NextcloudFiles::~NextcloudFiles() = default;

// --- user ID -----------------------------------------------------------------

QString NextcloudFiles::accountTag() const
{
    return m_config->getUsername() + QLatin1Char('@') + m_config->serverUrl();
}

QString NextcloudFiles::userId() const
{
    QSettings s(Configuration::settingsFilePath(), QSettings::IniFormat);
    if (s.value(kUserIdAccount).toString() != accountTag())
        return QString();
    return s.value(kUserId).toString();
}

void NextcloudFiles::clearUserId()
{
    QSettings s(Configuration::settingsFilePath(), QSettings::IniFormat);
    s.remove(kUserId);
    s.remove(kUserIdAccount);
}

void NextcloudFiles::resolveUserId(const Callback &done)
{
    const QString cached = userId();
    if (!cached.isEmpty()) {
        Result r;
        r.ok = true;
        r.data = cached.toUtf8();
        done(r);
        return;
    }
    if (!m_config->isAccountValid() || m_config->serverUrl().isEmpty()) {
        Result r;
        r.errorString = tr("Not signed in.");
        done(r);
        return;
    }

    QUrl url(m_config->serverUrl() + QStringLiteral("/ocs/v2.php/cloud/user"));
    url.setQuery(QStringLiteral("format=json"));
    const QString tag = accountTag();
    send("GET", url, QByteArray(),
         { { "OCS-APIRequest", "true" }, { "Accept", "application/json" } },
         [this, done, tag](QNetworkReply *reply) {
        Result r = resultFrom(reply);
        if (r.ok) {
            const QJsonObject data = QJsonDocument::fromJson(r.data).object()
                    .value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
            const QString id = data.value(QStringLiteral("id")).toString();
            if (id.isEmpty()) {
                r.ok = false;
                r.errorString = tr("The server did not tell the user ID.");
            } else {
                QSettings s(Configuration::settingsFilePath(), QSettings::IniFormat);
                s.setValue(kUserId, id);
                s.setValue(kUserIdAccount, tag);
                r.data = id.toUtf8();
            }
        }
        done(r);
    });
}

void NextcloudFiles::withUserId(const Callback &fail, const std::function<void()> &action)
{
    resolveUserId([fail, action](const Result &r) {
        if (r.ok)
            action();
        else
            fail(r);
    });
}

// --- URLs --------------------------------------------------------------------

QUrl NextcloudFiles::folderUrl() const
{
    const QString id = userId();
    if (id.isEmpty())
        return QUrl();
    QString path = m_config->serverUrl() + QStringLiteral("/remote.php/dav/files/")
            + QString::fromUtf8(QUrl::toPercentEncoding(id)) + QLatin1Char('/');
    if (!m_folder.isEmpty())
        path += QString::fromUtf8(QUrl::toPercentEncoding(m_folder)) + QLatin1Char('/');
    return QUrl(path, QUrl::StrictMode);
}

QUrl NextcloudFiles::fileUrl(const QString &fileName) const
{
    const QUrl folder = folderUrl();
    if (folder.isEmpty())
        return QUrl();
    return QUrl(folder.toString(QUrl::FullyEncoded) + QString::fromUtf8(QUrl::toPercentEncoding(fileName)),
                QUrl::StrictMode);
}

// --- operations --------------------------------------------------------------

void NextcloudFiles::ensureFolder(const Callback &done)
{
    withUserId(done, [this, done]() {
        const QUrl url = folderUrl();
        send("PROPFIND", url, propfindBody(), { { "Depth", "0" } }, [this, done, url](QNetworkReply *reply) {
            const Result found = resultFrom(reply);
            if (found.ok || !found.notFound()) {
                done(found);
                return;
            }
            send("MKCOL", url, QByteArray(), {}, [done](QNetworkReply *reply) {
                Result r = resultFrom(reply);
                if (r.httpStatus == 405) {          // created in the meantime
                    r = Result();
                    r.ok = true;
                    r.httpStatus = 405;
                }
                done(r);
            });
        });
    });
}

void NextcloudFiles::list(const Callback &done)
{
    withUserId(done, [this, done]() {
        send("PROPFIND", folderUrl(), propfindBody(), { { "Depth", "1" } }, [done](QNetworkReply *reply) {
            Result r = resultFrom(reply);
            if (r.ok) {
                r.entries = parseMultistatus(r.data);
            } else if (r.notFound()) {
                r = Result();                       // no folder yet: nothing saved
                r.ok = true;
                r.httpStatus = 404;
            }
            done(r);
        });
    });
}

void NextcloudFiles::download(const QString &fileName, const Callback &done)
{
    withUserId(done, [this, done, fileName]() {
        send("GET", fileUrl(fileName), QByteArray(), {}, [done](QNetworkReply *reply) {
            done(resultFrom(reply));
        });
    });
}

void NextcloudFiles::upload(const QString &fileName, const QByteArray &data, const Callback &done,
                            const QString &contentType)
{
    withUserId(done, [this, done, fileName, data, contentType]() {
        const QList<QPair<QByteArray, QByteArray>> headers{ { "Content-Type", contentType.toUtf8() } };
        auto put = [this, fileName, data, headers](const RawCallback &cb) {
            send("PUT", fileUrl(fileName), data, headers, cb);
        };
        put([this, done, put](QNetworkReply *reply) {
            const Result first = resultFrom(reply);
            // Nextcloud answers 409 (or 404) when the folder is missing
            if (first.ok || (first.httpStatus != 409 && first.httpStatus != 404)) {
                done(first);
                return;
            }
            ensureFolder([done, put](const Result &folder) {
                if (!folder.ok) {
                    done(folder);
                    return;
                }
                put([done](QNetworkReply *reply) { done(resultFrom(reply)); });
            });
        });
    });
}

void NextcloudFiles::remove(const QString &fileName, const Callback &done)
{
    withUserId(done, [this, done, fileName]() {
        send("DELETE", fileUrl(fileName), QByteArray(), {}, [done](QNetworkReply *reply) {
            Result r = resultFrom(reply);
            if (r.notFound()) {
                r = Result();
                r.ok = true;
                r.httpStatus = 404;
            }
            done(r);
        });
    });
}

void NextcloudFiles::copy(const QString &fromName, const QString &toName, const Callback &done)
{
    withUserId(done, [this, done, fromName, toName]() {
        const QByteArray destination = fileUrl(toName).toString(QUrl::FullyEncoded).toUtf8();
        send("COPY", fileUrl(fromName), QByteArray(), { { "Destination", destination }, { "Overwrite", "F" } },
             [done](QNetworkReply *reply) { done(resultFrom(reply)); });
    });
}

// --- network -----------------------------------------------------------------

void NextcloudFiles::send(const QByteArray &verb, const QUrl &url, const QByteArray &body,
                          const QList<QPair<QByteArray, QByteArray>> &headers, const RawCallback &done)
{
    QNetworkRequest req(url);
    const QByteArray credentials = (m_config->getUsername() + QLatin1Char(':') + m_config->getPassword())
            .toUtf8().toBase64();
    req.setRawHeader("Authorization", "Basic " + credentials);
    req.setRawHeader("User-Agent", m_config->getUserAgent().toUtf8());
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    for (const auto &h : headers)
        req.setRawHeader(h.first, h.second);
    if (verb == "PROPFIND")
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));

    auto *buffer = new QBuffer;
    buffer->setData(body);
    buffer->open(QIODevice::ReadOnly);
    QNetworkReply *reply = m_nam->sendCustomRequest(req, verb, buffer);
    buffer->setParent(reply);

#ifndef QT_NO_SSL
    if (m_config->getIgnoreSSLErrors()) {
        connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError> &) {
            reply->ignoreSslErrors();
        });
    }
#endif

    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        done(reply);
    });
}

NextcloudFiles::Result NextcloudFiles::resultFrom(QNetworkReply *reply)
{
    Result r;
    r.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.data = reply->readAll();
    QString etag = QString::fromUtf8(reply->rawHeader("OC-ETag"));
    if (etag.isEmpty())
        etag = QString::fromUtf8(reply->rawHeader("ETag"));
    r.etag = cleanEtag(etag);
    // An HTTP error status also sets a network error; only a missing reply
    // (no status) is a real network problem.
    r.networkError = r.httpStatus > 0 ? 0 : static_cast<int>(reply->error());
    r.ok = isSuccess(r.httpStatus) && reply->error() == QNetworkReply::NoError;
    if (!r.ok)
        r.errorString = describe(r.networkError, r.httpStatus,
                                 r.httpStatus > 0 ? QStringLiteral("HTTP %1").arg(r.httpStatus) : reply->errorString());
    return r;
}

QString NextcloudFiles::describe(int networkError, int httpStatus, const QString &details)
{
    if (httpStatus == 507)
        return tr("Not enough storage space on the Nextcloud.");
    if (httpStatus == 404)
        return tr("Not found on the Nextcloud.") + QStringLiteral(" (") + details + QLatin1Char(')');
    return AccountManager::describeNetworkError(networkError, httpStatus, details, false);
}

// --- WebDAV XML --------------------------------------------------------------

QList<NextcloudFiles::Entry> NextcloudFiles::parseMultistatus(const QByteArray &xml)
{
    QList<Entry> entries;
    QXmlStreamReader x(xml);
    Entry current;
    bool inResponse = false, isCollection = false;
    QString href;
    while (!x.atEnd()) {
        x.readNext();
        const QStringRef name = x.name();
        if (x.isStartElement()) {
            if (x.namespaceUri() != QLatin1String("DAV:"))
                continue;
            if (name == QLatin1String("response")) {
                inResponse = true;
                current = Entry();
                isCollection = false;
                href.clear();
            } else if (!inResponse) {
                continue;
            } else if (name == QLatin1String("href")) {
                href = x.readElementText();
            } else if (name == QLatin1String("collection")) {
                isCollection = true;
            } else if (name == QLatin1String("getlastmodified")) {
                // Properties a resource does not have come back empty (in a
                // propstat with status 404), so empty values are skipped.
                const QString t = x.readElementText().trimmed();
                if (!t.isEmpty()) {
                    QDateTime dt = QLocale::c().toDateTime(t, QStringLiteral("ddd, dd MMM yyyy hh:mm:ss 'GMT'"));
                    if (!dt.isValid())
                        dt = QLocale::c().toDateTime(t, QStringLiteral("ddd, d MMM yyyy hh:mm:ss 'GMT'"));
                    dt.setTimeSpec(Qt::UTC);
                    current.modified = dt;
                }
            } else if (name == QLatin1String("getcontentlength")) {
                const QString t = x.readElementText().trimmed();
                if (!t.isEmpty())
                    current.size = t.toLongLong();
            } else if (name == QLatin1String("getetag")) {
                const QString t = cleanEtag(x.readElementText());
                if (!t.isEmpty())
                    current.etag = t;
            }
        } else if (x.isEndElement() && x.namespaceUri() == QLatin1String("DAV:")
                   && name == QLatin1String("response")) {
            inResponse = false;
            if (isCollection || href.isEmpty() || href.endsWith(QLatin1Char('/')))
                continue;
            const QString path = QUrl::fromPercentEncoding(href.toUtf8());
            current.name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
            if (!current.name.isEmpty())
                entries.append(current);
        }
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        if (a.modified != b.modified)
            return a.modified > b.modified;
        return a.name > b.name;
    });
    return entries;
}
