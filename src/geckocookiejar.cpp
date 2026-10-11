// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "geckocookiejar.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkCookie>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QVariant>

namespace {
int s_instance = 0;
}

GeckoCookieJar::GeckoCookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
    , m_connection(QStringLiteral("sailfeed-gecko-cookies-%1").arg(++s_instance))
{
}

GeckoCookieJar::~GeckoCookieJar()
{
    if (QSqlDatabase::contains(m_connection)) {
        QSqlDatabase::database(m_connection, false).close();
        QSqlDatabase::removeDatabase(m_connection);
    }
}

void GeckoCookieJar::setDatabasePath(const QString &path)
{
    if (QSqlDatabase::contains(m_connection)) {
        QSqlDatabase::database(m_connection, false).close();
        QSqlDatabase::removeDatabase(m_connection);
    }
    m_path = path;
    m_searched = !path.isEmpty();
}

QString GeckoCookieJar::databasePath() const
{
    if (!m_searched || (!m_path.isEmpty() && !QFileInfo::exists(m_path))) {
        m_searched = true;
        m_path = findDatabase();
    }
    return m_path;
}

// The WebView keeps its Gecko profile (".mozilla") in the app's cache or data
// folder; look for cookies.sqlite there, a few levels deep, skipping the
// image cache.
QString GeckoCookieJar::findDatabase()
{
    const QStringList roots {
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
    };
    QString best;
    QDateTime bestTime;
    for (const QString &root : roots) {
        if (root.isEmpty())
            continue;
        const QString direct = root + QStringLiteral("/.mozilla/cookies.sqlite");
        QStringList found;
        if (QFileInfo::exists(direct)) {
            found << direct;
        } else {
            QDirIterator it(root, QStringList { QStringLiteral("cookies.sqlite") },
                            QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
            int visited = 0;
            while (it.hasNext() && visited++ < 5000) {
                const QString p = it.next();
                if (!p.contains(QLatin1String("/images/")))
                    found << p;
            }
        }
        for (const QString &p : found) {
            const QDateTime t = QFileInfo(p).lastModified();
            if (best.isEmpty() || t > bestTime) {
                best = p;
                bestTime = t;
            }
        }
    }
    return best;
}

bool GeckoCookieJar::openDb() const
{
    const QString path = databasePath();
    if (path.isEmpty())
        return false;
    QSqlDatabase db;
    if (QSqlDatabase::contains(m_connection)) {
        db = QSqlDatabase::database(m_connection, false);
        if (db.databaseName() != path) {
            db.close();
            db.setDatabaseName(path);
        }
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=300"));
    }
    if (db.isOpen())
        return true;
    if (!db.open()) {
        qWarning("Gecko cookies: cannot open %s: %s", qPrintable(path), qPrintable(db.lastError().text()));
        return false;
    }
    return true;
}

int GeckoCookieJar::geckoCookieCount() const
{
    if (!openDb())
        return -1;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM moz_cookies")) || !q.next())
        return -1;
    return q.value(0).toInt();
}

QList<QNetworkCookie> GeckoCookieJar::geckoCookiesForUrl(const QUrl &url) const
{
    QList<QNetworkCookie> result;
    const QString host = url.host().toLower();
    if (host.isEmpty() || !openDb())
        return result;

    // Gecko stores "example.org" for host-only and ".example.org" for domain
    // cookies; collect both forms for the host and all its parent domains.
    QStringList hosts;
    const QStringList labels = host.split(QLatin1Char('.'));
    for (int i = 0; i + 1 < labels.size(); ++i) {
        const QString d = QStringList(labels.mid(i)).join(QLatin1Char('.'));
        if (i == 0)
            hosts << d;
        hosts << QLatin1Char('.') + d;
    }
    if (labels.size() == 1)
        hosts << host;

    QStringList marks;
    for (int i = 0; i < hosts.size(); ++i)
        marks << QStringLiteral("?");
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("SELECT * FROM moz_cookies WHERE host IN (%1)").arg(marks.join(QLatin1Char(','))));
    for (const QString &h : hosts)
        q.addBindValue(h);
    if (!q.exec()) {
        qWarning("Gecko cookies: query failed: %s", qPrintable(q.lastError().text()));
        return result;
    }

    const QSqlRecord rec = q.record();
    const int iName = rec.indexOf(QStringLiteral("name"));
    const int iValue = rec.indexOf(QStringLiteral("value"));
    const int iHost = rec.indexOf(QStringLiteral("host"));
    const int iPath = rec.indexOf(QStringLiteral("path"));
    const int iSecure = rec.indexOf(QStringLiteral("isSecure"));
    const int iExpiry = rec.indexOf(QStringLiteral("expiry"));
    const int iOrigin = rec.indexOf(QStringLiteral("originAttributes"));
    if (iName < 0 || iValue < 0 || iHost < 0)
        return result;

    const bool https = url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0;
    QString reqPath = url.path();
    if (reqPath.isEmpty())
        reqPath = QStringLiteral("/");
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;

    while (q.next()) {
        // Partitioned (third-party, per top-level site) cookies don't belong here.
        if (iOrigin >= 0 && !q.value(iOrigin).toString().isEmpty())
            continue;
        if (iSecure >= 0 && q.value(iSecure).toInt() != 0 && !https)
            continue;
        if (iExpiry >= 0) {
            qint64 exp = q.value(iExpiry).toLongLong();
            if (exp > 100000000000LL)          // newer Gecko: milliseconds
                exp /= 1000;
            if (exp > 0 && exp < now)
                continue;
        }
        const QString path = iPath >= 0 ? q.value(iPath).toString() : QStringLiteral("/");
        if (!path.isEmpty() && !reqPath.startsWith(path))
            continue;
        QNetworkCookie c(q.value(iName).toString().toUtf8(), q.value(iValue).toString().toUtf8());
        c.setDomain(q.value(iHost).toString());
        c.setPath(path.isEmpty() ? QStringLiteral("/") : path);
        result << c;
    }
    // More specific paths first, as browsers do.
    std::stable_sort(result.begin(), result.end(), [](const QNetworkCookie &a, const QNetworkCookie &b) {
        return a.path().size() > b.path().size();
    });
    return result;
}

int GeckoCookieJar::clearWebData(const QString &cookieDatabasePath)
{
    if (cookieDatabasePath.isEmpty())
        return 0;
    QDir profile = QFileInfo(cookieDatabasePath).absoluteDir();
    int removed = 0;
    const QStringList files {
        QStringLiteral("cookies.sqlite"), QStringLiteral("cookies.sqlite-wal"), QStringLiteral("cookies.sqlite-shm"),
        QStringLiteral("webappsstore.sqlite"), QStringLiteral("webappsstore.sqlite-wal"), QStringLiteral("webappsstore.sqlite-shm"),
    };
    for (const QString &f : files) {
        if (profile.exists(f) && profile.remove(f))
            ++removed;
    }
    for (const QString &d : { QStringLiteral("storage"), QStringLiteral("cache2"), QStringLiteral("startupCache") }) {
        QDir sub(profile.filePath(d));
        if (sub.exists() && sub.removeRecursively())
            ++removed;
    }
    return removed;
}

QList<QNetworkCookie> GeckoCookieJar::cookiesForUrl(const QUrl &url) const
{
    if (!m_share)
        return QNetworkCookieJar::cookiesForUrl(url);
    // Gecko's cookies win: they reflect choices made in the web view (consent),
    // while a download may have received a "not decided yet" cookie.
    QList<QNetworkCookie> result = geckoCookiesForUrl(url);
    for (const QNetworkCookie &o : QNetworkCookieJar::cookiesForUrl(url)) {
        bool dup = false;
        for (const QNetworkCookie &g : result)
            dup = dup || g.name() == o.name();
        if (!dup)
            result << o;
    }
    return result;
}
