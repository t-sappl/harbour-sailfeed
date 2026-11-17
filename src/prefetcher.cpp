// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prefetcher.h"
#include "geckocookiejar.h"
#include "configuration.h"
#include "htmlutils.h"

#include <QDateTime>
#include <QRunnable>
#include <QSettings>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextCodec>

Prefetcher::Prefetcher(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_jar(new GeckoCookieJar)
{
    m_nam->setCookieJar(m_jar);             // the NAM takes ownership
    m_jar->setShareGeckoCookies(shareBrowserCookies());
    m_pool.setMaxThreadCount(1);
}

Prefetcher::~Prefetcher()
{
    m_pool.clear();
    m_pool.waitForDone();       // a running job posts to this object
}

namespace {
// Makes the lean copy (HtmlUtils::leanPage) off the GUI thread
class LeanJob : public QRunnable
{
public:
    LeanJob(Prefetcher *target, const QString &key, const QUrl &url, const QString &html)
        : m_target(target), m_key(key), m_url(url), m_html(html) {}
    void run() override
    {
        const QString lean = HtmlUtils::leanPage(m_html, m_url);
        QMetaObject::invokeMethod(m_target, "leanReady", Qt::QueuedConnection,
                                  Q_ARG(QString, m_key), Q_ARG(QUrl, m_url),
                                  Q_ARG(QString, m_html), Q_ARG(QString, lean));
    }
private:
    Prefetcher *m_target;
    QString m_key;
    QUrl m_url;
    QString m_html;
};
}

namespace {
const QString kShare = QStringLiteral("privacy/shareBrowserCookies");
const QString kClearAtStart = QStringLiteral("privacy/clearWebDataAtStart");
const QString kClearOnce = QStringLiteral("privacy/clearWebDataOnce");
QString settingsPath() { return Configuration::settingsFilePath(); }
}

bool Prefetcher::shareBrowserCookies() const { return QSettings(settingsPath(), QSettings::IniFormat).value(kShare, true).toBool(); }

void Prefetcher::setShareBrowserCookies(bool share)
{
    if (share == shareBrowserCookies())
        return;
    QSettings(settingsPath(), QSettings::IniFormat).setValue(kShare, share);
    m_jar->setShareGeckoCookies(share);
    clear();                                // pages downloaded with the old setting
    Q_EMIT privacyChanged();
}

bool Prefetcher::clearAtStart() const { return QSettings(settingsPath(), QSettings::IniFormat).value(kClearAtStart, false).toBool(); }

void Prefetcher::setClearAtStart(bool clear)
{
    if (clear == clearAtStart())
        return;
    QSettings(settingsPath(), QSettings::IniFormat).setValue(kClearAtStart, clear);
    Q_EMIT privacyChanged();
}

bool Prefetcher::clearScheduled() const { return QSettings(settingsPath(), QSettings::IniFormat).value(kClearOnce, false).toBool(); }

void Prefetcher::scheduleClearWebData()
{
    QSettings(settingsPath(), QSettings::IniFormat).setValue(kClearOnce, true);
    Q_EMIT privacyChanged();
}

void Prefetcher::cancelClearWebData()
{
    QSettings(settingsPath(), QSettings::IniFormat).remove(kClearOnce);
    Q_EMIT privacyChanged();
}

bool Prefetcher::clearWebDataIfRequested(const QString &cookieDatabasePath)
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    const bool once = s.value(kClearOnce, false).toBool();
    if (!once && !s.value(kClearAtStart, false).toBool())
        return false;
    s.remove(kClearOnce);
    const QString path = !cookieDatabasePath.isEmpty() ? cookieDatabasePath : GeckoCookieJar::findDatabasePath();
    return GeckoCookieJar::clearWebData(path) > 0;
}

void Prefetcher::setCookieDatabase(const QString &path)
{
    m_jar->setDatabasePath(path);
}

QString Prefetcher::cookieInfo() const
{
    const QString path = m_jar->databasePath();
    if (path.isEmpty())
        return tr("No browser cookies yet");
    const int n = m_jar->geckoCookieCount();
    if (n < 0)
        return tr("Browser cookies not readable (%1)").arg(path);
    // Two strings instead of %n: there is no English translation file to
    // resolve plural forms ("1 browser cookie(s)" in English otherwise)
    return n == 1 ? tr("1 browser cookie shared with the reader mode")
                  : tr("%1 browser cookies shared with the reader mode").arg(n);
}

void Prefetcher::clear()
{
    m_cache.clear();
    m_lean.clear();
    m_leaning.clear();          // results still being made are dropped
    m_order.clear();
    m_articles.clear();
    m_articleOrder.clear();
}

QString Prefetcher::key(const QUrl &url)
{
    return url.adjusted(QUrl::RemoveFragment).toString();
}

void Prefetcher::prefetch(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return;
    const QString k = key(url);
    if (m_cache.contains(k) || m_pending.contains(k) || m_leaning.contains(k))
        return;

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    req.setRawHeader("User-Agent", "Mozilla/5.0 (Linux; Sailfish OS; Mobile; rv:91.0) Gecko/20100101 Firefox/91.0");
    req.setRawHeader("Accept", "text/html,application/xhtml+xml;q=0.9,*/*;q=0.5");
    req.setRawHeader("Accept-Language", "de,en;q=0.7");

    QNetworkReply *reply = m_nam->get(req);
    m_pending.insert(k, reply);
    m_lastData.insert(k, QDateTime::currentMSecsSinceEpoch());
    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply, k](qint64 received, qint64) {
        if (received > maxBytes) {
            reply->abort();
            return;
        }
        if (m_pending.value(k) == reply)
            m_lastData.insert(k, QDateTime::currentMSecsSinceEpoch());
    });
    // Headers and redirects are progress, too (before the first byte of the page)
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply, k]() {
        if (m_pending.value(k) == reply)
            m_lastData.insert(k, QDateTime::currentMSecsSinceEpoch());
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() { onFinished(reply, url); });
}

void Prefetcher::onFinished(QNetworkReply *reply, const QUrl &url)
{
    const QString k = key(url);
    m_pending.remove(k);
    m_lastData.remove(k);
    reply->deleteLater();

    const QString type = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool htmlType = type.isEmpty() || type.contains(QLatin1String("html"));
    if (reply->error() != QNetworkReply::NoError || status >= 400 || !htmlType) {
        Q_EMIT finished(url, false);
        return;
    }

    const QByteArray data = reply->readAll();
    // Charset from the HTTP header, else from <meta charset>, else UTF-8.
    QTextCodec *codec = nullptr;
    const int cs = type.indexOf(QLatin1String("charset="));
    if (cs >= 0) {
        QByteArray name = type.mid(cs + 8).toLatin1();
        const int end = name.indexOf(';');
        if (end >= 0)
            name.truncate(end);
        codec = QTextCodec::codecForName(name.trimmed().replace('"', ""));
    }
    if (!codec)
        codec = QTextCodec::codecForHtml(data, QTextCodec::codecForName("UTF-8"));
    const QString html = codec->toUnicode(data);
    if (html.trimmed().isEmpty()) {
        Q_EMIT finished(url, false);
        return;
    }

    m_leaning.insert(k);
    m_pool.start(new LeanJob(this, k, url, html));
}

void Prefetcher::leanReady(const QString &k, const QUrl &url, const QString &html, const QString &lean)
{
    if (!m_leaning.remove(k))
        return;                 // cancelled or cleared meanwhile
    m_cache.insert(k, html);
    m_lean.insert(k, lean);
    m_order.removeAll(k);
    m_order.append(k);
    while (m_order.size() > maxEntries) {
        const QString old = m_order.takeFirst();
        m_cache.remove(old);
        m_lean.remove(old);
    }
    Q_EMIT finished(url, true);
}

QString Prefetcher::html(const QUrl &url) const
{
    return m_cache.value(key(url));
}

QString Prefetcher::leanHtml(const QUrl &url) const
{
    return m_lean.value(key(url));
}

void Prefetcher::setArticle(const QUrl &url, const QString &json)
{
    const QString k = key(url);
    if (k.isEmpty() || json.isEmpty())
        return;
    m_articles.insert(k, json);
    m_articleOrder.removeAll(k);
    m_articleOrder.append(k);
    while (m_articleOrder.size() > 2 * maxEntries)
        m_articles.remove(m_articleOrder.takeFirst());
}

QString Prefetcher::article(const QUrl &url) const
{
    return m_articles.value(key(url));
}

bool Prefetcher::isPending(const QUrl &url) const
{
    const QString k = key(url);
    return m_pending.contains(k) || m_leaning.contains(k);
}

void Prefetcher::cancel(const QUrl &url)
{
    const QString k = key(url);
    m_leaning.remove(k);
    if (QNetworkReply *reply = m_pending.value(k))
        reply->abort();         // finished(url, false) through onFinished()
}

int Prefetcher::idleMs(const QUrl &url) const
{
    const QString k = key(url);
    if (m_leaning.contains(k))
        return 0;               // downloaded, almost ready
    if (!m_pending.contains(k))
        return -1;
    return static_cast<int>(qMin<qint64>(QDateTime::currentMSecsSinceEpoch() - m_lastData.value(k), 1 << 30));
}
