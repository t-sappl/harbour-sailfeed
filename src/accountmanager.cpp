// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "accountmanager.h"
#include "configuration.h"

#include <Fuoten/API/GetVersion>
#include <Fuoten/API/LoginFlowV2>
#include <Fuoten/Error>
#include <Fuoten/Helpers/Synchronizer>
#include <Fuoten/Storage/AbstractStorage>

#include <QNetworkReply>

AccountManager::AccountManager(Configuration *config, Fuoten::AbstractStorage *storage,
                               Fuoten::Synchronizer *synchronizer, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_storage(storage)
    , m_synchronizer(synchronizer)
{
}

void AccountManager::rememberAccount()
{
    m_oldServer = m_config->serverUrl();
    m_oldUser = m_config->getUsername();
}

// A different account must not see the articles of the previous one.
void AccountManager::forgetOldDataIfOtherAccount()
{
    if ((!m_oldServer.isEmpty() && m_oldServer != m_config->serverUrl())
            || (!m_oldUser.isEmpty() && m_oldUser != m_config->getUsername())) {
        m_storage->clearStorage();
        m_config->setLastSync(QDateTime());
    }
}

void AccountManager::login(const QString &serverAddress, const QString &user, const QString &appPassword)
{
    if (m_busy)
        return;

    setErrorString(QString());
    rememberAccount();

    if (!m_config->setServerAddress(serverAddress)) {
        fail(tr("Invalid server address."));
        return;
    }
    m_config->setUsername(user.trimmed());
    m_config->setPassword(appPassword.trimmed());
    forgetOldDataIfOtherAccount();

    m_config->setIsAccountValid(false);
    setBusy(true);
    verifyAndSync();
}

// Authenticated request: fails with an authorization error on a wrong app
// password and with a network error on a wrong server address.
void AccountManager::verifyAndSync()
{
    auto *req = new Fuoten::GetVersion(this);
    req->setConfiguration(m_config);
    m_request = req;

    connect(req, &Fuoten::Component::succeeded, this, [this, req]() {
        m_serverVersion = req->versionString();
        Q_EMIT serverVersionChanged(m_serverVersion);
        m_config->setIsAccountValid(true);
        setBusy(false);
        req->deleteLater();
        Q_EMIT loginSucceeded();
        m_synchronizer->sync();
    });
    connect(req, &Fuoten::Component::failed, this, [this, req](Fuoten::Error *error) {
        const QString text = friendlyError(error);
        req->deleteLater();
        fail(text);
    });

    req->execute();
}

bool AccountManager::browserLoginActive() const
{
    return !m_flow.isNull();
}

void AccountManager::loginWithBrowser(const QString &serverAddress)
{
    if (m_busy)
        return;

    setErrorString(QString());
    rememberAccount();

    if (!m_config->setServerAddress(serverAddress)) {
        fail(tr("Invalid server address."));
        return;
    }
    m_config->setIsAccountValid(false);
    setBusy(true);

    auto *flow = new Fuoten::LoginFlowV2(this);
    flow->setConfiguration(m_config);
    flow->setPollingInterval(m_pollingInterval);
    m_flow = flow;
    Q_EMIT browserLoginActiveChanged();

    connect(flow, &Fuoten::LoginFlowV2::gotLoginUrl, this, &AccountManager::loginUrl);
    connect(flow, &Fuoten::LoginFlowV2::secondsLeftChanged, this, [this](int seconds) {
        m_secondsLeft = seconds;
        Q_EMIT browserLoginSecondsLeftChanged(seconds);
    });
    // The server handed over user name and app password; libfuoten has put
    // them into the configuration (password into Sailfish Secrets).
    connect(flow, &Fuoten::Component::succeeded, this, [this, flow]() {
        flow->deleteLater();
        m_flow.clear();
        Q_EMIT browserLoginActiveChanged();
        forgetOldDataIfOtherAccount();
        m_config->setIsAccountValid(false);
        verifyAndSync();
    });
    connect(flow, &Fuoten::Component::failed, this, [this, flow](Fuoten::Error *error) {
        const QString text = friendlyError(error);
        flow->deleteLater();
        m_flow.clear();
        Q_EMIT browserLoginActiveChanged();
        fail(text);
    });

    flow->execute();
}

void AccountManager::cancelBrowserLogin()
{
    if (!m_flow)
        return;
    m_flow->disconnect(this);
    m_flow->deleteLater();
    m_flow.clear();
    Q_EMIT browserLoginActiveChanged();
    setBusy(false);
}

QString AccountManager::friendlyError(Fuoten::Error *error)
{
    if (!error)
        return tr("Unknown error.");
    int http = error->httpStatus();
    if (error->type() == Fuoten::Error::AuthorizationError && http != 401 && http != 403)
        http = 401;                         // libfuoten's own "not authorized" errors
    return describeNetworkError(error->networkError(), http, error->text());
}

QString AccountManager::describeNetworkError(int networkError, int httpStatus, const QString &details,
                                             bool newsApi)
{
    QString hint;
    switch (networkError) {
    case QNetworkReply::HostNotFoundError:
        hint = tr("Server not found. Check the address and the internet connection.");
        break;
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:     // our own request timeout
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
        hint = tr("Server not reachable. Check the internet connection, or try again later.");
        break;
    case QNetworkReply::SslHandshakeFailedError:
        hint = tr("The server's certificate is not trusted. If your server uses a self-signed certificate, turn on \"Accept self-signed certificates\".");
        break;
    default:
        break;
    }
    if (hint.isEmpty()) {
        if (httpStatus == 401 || httpStatus == 403)
            hint = tr("Sign-in failed: user name or app password wrong. Create a new app password in Nextcloud, or sign in with the browser.");
        else if (httpStatus == 404 && newsApi)
            hint = tr("No Nextcloud News found at this address. Check the address and whether the News app is installed.");
        else if (httpStatus >= 500)
            hint = tr("The server reported an error. Try again later.");
    }
    if (hint.isEmpty())
        return details;
    return details.isEmpty() ? hint : hint + QStringLiteral("\n(") + details + QLatin1Char(')');
}

void AccountManager::logout()
{
    cancelBrowserLogin();
    m_storage->clearStorage();
    m_config->deleteAccount();
}

void AccountManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged(busy);
}

void AccountManager::setErrorString(const QString &error)
{
    if (m_errorString == error)
        return;
    m_errorString = error;
    Q_EMIT errorStringChanged(error);
}

void AccountManager::fail(const QString &error)
{
    m_config->setIsAccountValid(false);
    setBusy(false);
    setErrorString(error);
    Q_EMIT loginFailed(error);
}
