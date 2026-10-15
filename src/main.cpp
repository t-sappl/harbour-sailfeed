// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include <sailfishapp.h>

#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTranslator>
#include <QtQml>

#include <Fuoten/API/Component>
#include <Fuoten/Article>
#include <Fuoten/Error>
#include <Fuoten/Folder>
#include <Fuoten/FuotenEnums>
#include <Fuoten/Helpers/Synchronizer>
#include <Fuoten/Models/FolderListFilterModel>
#include <Fuoten/Storage/SQLiteStorage>

#include "accountmanager.h"
#include "articlemodel.h"
#include "cloudbackup.h"
#include "feedmanager.h"
#include "feedfinder.h"
#include "feedmodel.h"
#include "feedrowsmodel.h"
#include "configuration.h"
#include "htmlhelper.h"
#include "namfactory.h"
#include "prefetcher.h"
#include "settingsbackup.h"

namespace {

void loadTranslations(QGuiApplication *app)
{
    const QLocale locale;
    // libfuoten uses id based translations ("libfuoten_de.qm"), the app
    // uses "harbour-sailfeed-de.qm".
    const QList<QPair<QString, QString>> catalogs = {
        { QStringLiteral("libfuoten"), QStringLiteral("_") },
        { QStringLiteral("harbour-sailfeed"), QStringLiteral("-") },
    };
    for (const auto &c : catalogs) {
        auto *t = new QTranslator(app);
        if (t->load(locale, c.first, c.second, QStringLiteral(SAILFEED_I18NDIR), QStringLiteral(".qm")))
            app->installTranslator(t);
        else
            delete t;
    }
}

QString ensureDir(QStandardPaths::StandardLocation location, const QString &sub = QString())
{
    // With organization and application name set, these resolve to
    // ~/.local/share/org.sappl/harbour-sailfeed and
    // ~/.cache/org.sappl/harbour-sailfeed, the directories Sailjail grants.
    QString path = QStandardPaths::writableLocation(location);
    if (!sub.isEmpty())
        path += QLatin1Char('/') + sub;
    if (!QDir().mkpath(path))
        qWarning("Failed to create directory %s", qUtf8Printable(path));
    return path;
}

} // namespace

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QStringLiteral("org.sappl"));
    app->setApplicationName(QStringLiteral("harbour-sailfeed"));
    app->setApplicationVersion(QStringLiteral(SAILFEED_VERSION));

    loadTranslations(app.data());

    qRegisterMetaType<Fuoten::IdList>("Fuoten::IdList");
    qRegisterMetaType<Fuoten::ArticleList>("Fuoten::ArticleList");

    // --- libfuoten backend -----------------------------------------------
    auto *config = new Configuration(app.data());

    const QString dataDir = ensureDir(QStandardPaths::AppDataLocation);
    auto *storage = new Fuoten::SQLiteStorage(dataDir + QStringLiteral("/database.sqlite"), app.data());
    storage->setConfiguration(config);
    storage->init();    // asynchronous, "ready" becomes true when done

    Fuoten::Component::setDefaultConfiguration(config);
    Fuoten::Component::setDefaultStorage(storage);
    Fuoten::Component::setDefaultNam(new QNetworkAccessManager(app.data()));

    auto *synchronizer = new Fuoten::Synchronizer(app.data());
    synchronizer->setConfiguration(config);
    synchronizer->setStorage(storage);

    auto *account = new AccountManager(config, storage, synchronizer, app.data());
    // Settings backup in the user's Nextcloud files (WebDAV)
    auto *cloudBackup = new CloudBackup(config, nullptr, app.data());

    // Sync once at start, as soon as the database is open.
    QObject::connect(storage, &Fuoten::AbstractStorage::readyChanged, synchronizer, [config, synchronizer](bool ready) {
        if (ready && config->isAccountValid() && !synchronizer->inOperation())
            synchronizer->sync();
    });

    // Read/star changes are queued locally. Send them when the app goes to
    // the background, so other devices see them without a manual refresh.
    auto pendingQueue = QSharedPointer<bool>::create(false);
    auto setPending = [pendingQueue]() { *pendingQueue = true; };
    QObject::connect(storage, &Fuoten::AbstractStorage::markedItem, app.data(), setPending);
    QObject::connect(storage, &Fuoten::AbstractStorage::starredItem, app.data(), setPending);
    QObject::connect(storage, &Fuoten::AbstractStorage::markedReadFolderInQueue, app.data(), setPending);
    QObject::connect(storage, &Fuoten::AbstractStorage::markedReadFeedInQueue, app.data(), setPending);
    QObject::connect(storage, &Fuoten::AbstractStorage::markedAllItemsReadInQueue, app.data(), setPending);
    QObject::connect(synchronizer, &Fuoten::Synchronizer::succeeded, app.data(), [pendingQueue]() { *pendingQueue = false; });
    QObject::connect(app.data(), &QGuiApplication::applicationStateChanged, app.data(),
                     [pendingQueue, config, synchronizer](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive && *pendingQueue
                && config->isAccountValid() && !synchronizer->inOperation())
            synchronizer->sync();
    });

    // --- QML types -------------------------------------------------------
    const char *uri = "harbour.sailfeed";
    qmlRegisterUncreatableType<Fuoten::FuotenEnums>(uri, 1, 0, "Fuoten", QStringLiteral("enums only"));
    qmlRegisterUncreatableType<Fuoten::AbstractStorage>(uri, 1, 0, "AbstractStorage", QStringLiteral("use localstorage"));
    qmlRegisterUncreatableType<Fuoten::AbstractConfiguration>(uri, 1, 0, "AbstractConfiguration", QStringLiteral("use config"));
    qmlRegisterType<Fuoten::Error>(uri, 1, 0, "FuotenError");
    qmlRegisterType<Fuoten::Folder>(uri, 1, 0, "Folder");
    qmlRegisterType<Fuoten::Article>(uri, 1, 0, "Article");
    qmlRegisterType<Fuoten::FolderListFilterModel>(uri, 1, 0, "FolderListFilterModel");
    qmlRegisterType<ArticleModel>(uri, 1, 0, "ArticleModel");
    qmlRegisterType<FeedModel>(uri, 1, 0, "FeedModel");
    qmlRegisterType<FeedRowsModel>(uri, 1, 0, "FeedRowsModel");
    qmlRegisterType<FeedFinder>(uri, 1, 0, "FeedFinder");
    qmlRegisterSingletonType<Prefetcher>(uri, 1, 0, "Prefetch",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new Prefetcher; });
    qmlRegisterSingletonType<FeedManager>(uri, 1, 0, "FeedManager",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new FeedManager; });
    qmlRegisterSingletonType<HtmlHelper>(uri, 1, 0, "Html",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new HtmlHelper; });
    qmlRegisterSingletonType<SettingsBackup>(uri, 1, 0, "SettingsBackup",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SettingsBackup; });

    // --- View ------------------------------------------------------------
    // Privacy: delete the in-app browser's cookies and site data before its
    // engine starts (scheduled in the settings, or at every start).
    if (Prefetcher::clearWebDataIfRequested())
        qInfo("Cookies and website data of the in-app browser deleted");

    QScopedPointer<QQuickView> view(SailfishApp::createView());

    NamFactory namFactory(ensureDir(QStandardPaths::CacheLocation, QStringLiteral("images")),
                          100 * 1024 * 1024);
    view->engine()->setNetworkAccessManagerFactory(&namFactory);

    QQmlContext *ctx = view->rootContext();
    ctx->setContextProperty(QStringLiteral("config"), config);
    ctx->setContextProperty(QStringLiteral("localstorage"), storage);
    ctx->setContextProperty(QStringLiteral("synchronizer"), synchronizer);
    ctx->setContextProperty(QStringLiteral("account"), account);
    ctx->setContextProperty(QStringLiteral("cloudBackup"), cloudBackup);

    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    const int ret = app->exec();
    view.reset();       // destroy the engine before the NAM factory
    return ret;
}
