// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class Configuration;
namespace Fuoten {
class AbstractStorage;
class Synchronizer;
class GetVersion;
class LoginFlowV2;
class Error;
}

// Signs in to the Nextcloud News API: stores the account data, verifies it
// with an authenticated version request and starts the first sync.
class AccountManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY serverVersionChanged)
    // Sign-in in the browser (Nextcloud Login Flow v2) is waiting for the user
    Q_PROPERTY(bool browserLoginActive READ browserLoginActive NOTIFY browserLoginActiveChanged)
    Q_PROPERTY(int browserLoginSecondsLeft READ browserLoginSecondsLeft NOTIFY browserLoginSecondsLeftChanged)
public:
    AccountManager(Configuration *config, Fuoten::AbstractStorage *storage,
                   Fuoten::Synchronizer *synchronizer, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString errorString() const { return m_errorString; }
    QString serverVersion() const { return m_serverVersion; }

    // serverAddress may be "host", "host/path" or a full URL.
    Q_INVOKABLE void login(const QString &serverAddress, const QString &user, const QString &appPassword);
    Q_INVOKABLE void logout();

    // Sign-in in the browser: asks the server for a login page (signal
    // loginUrl, to be opened in the browser), then waits until the user has
    // granted access there; the server hands over user name and app password.
    Q_INVOKABLE void loginWithBrowser(const QString &serverAddress);
    Q_INVOKABLE void cancelBrowserLogin();
    bool browserLoginActive() const;
    int browserLoginSecondsLeft() const { return m_secondsLeft; }
    // For tests: seconds between the polling requests (default 2).
    void setBrowserLoginPollingInterval(int seconds) { m_pollingInterval = seconds; }

    // Error text for the user with a hint how to solve it, from a libfuoten
    // error (also used for sync errors).
    static QString friendlyError(Fuoten::Error *error);
    // The same hints for any request to the Nextcloud (also WebDAV):
    // networkError is a QNetworkReply::NetworkError, httpStatus 0 if none.
    // newsApi: a 404 means "no News app at this address".
    static QString describeNetworkError(int networkError, int httpStatus, const QString &details,
                                        bool newsApi = true);
    Q_INVOKABLE QString describeError(Fuoten::Error *error) const { return friendlyError(error); }

Q_SIGNALS:
    void busyChanged(bool busy);
    void errorStringChanged(const QString &errorString);
    void serverVersionChanged(const QString &serverVersion);
    void loginSucceeded();
    void loginFailed(const QString &errorString);
    void loginUrl(const QUrl &url);
    void browserLoginActiveChanged();
    void browserLoginSecondsLeftChanged(int seconds);

private:
    void setBusy(bool busy);
    void setErrorString(const QString &error);
    void fail(const QString &error);
    void verifyAndSync();
    void rememberAccount();
    void forgetOldDataIfOtherAccount();

    Configuration *m_config;
    Fuoten::AbstractStorage *m_storage;
    Fuoten::Synchronizer *m_synchronizer;
    QPointer<Fuoten::GetVersion> m_request;
    QPointer<Fuoten::LoginFlowV2> m_flow;
    QString m_oldServer;
    QString m_oldUser;
    int m_secondsLeft = 0;
    int m_pollingInterval = 2;
    QString m_errorString;
    QString m_serverVersion;
    bool m_busy = false;
};
