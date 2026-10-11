// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Fuoten/Helpers/AbstractConfiguration>
#include <QSettings>

#include "secretstore.h"

// Account and sync configuration for libfuoten, persisted with QSettings in
// ~/.config/org.sappl/harbour-sailfeed/ (the directory Sailjail grants).
//
// The app password is not stored here but with Sailfish Secrets (see
// SecretStore). It is read once and then kept in memory, because libfuoten
// asks for it synchronously on every request.
class Configuration : public Fuoten::AbstractConfiguration
{
    Q_OBJECT
    Q_PROPERTY(bool isAccountValid READ isAccountValid NOTIFY isAccountValidChanged)
    Q_PROPERTY(QString username READ getUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QDateTime lastSync READ getLastSync NOTIFY lastSyncChanged)
    // Accept invalid (e.g. self-signed) certificates of the Nextcloud server.
    Q_PROPERTY(bool ignoreSslErrors READ getIgnoreSSLErrors WRITE setIgnoreSslErrors NOTIFY ignoreSslErrorsChanged)
public:
    explicit Configuration(QObject *parent = nullptr);
    ~Configuration() override;

    QString getUsername() const override;
    void setUsername(const QString &username) override;
    QString getPassword() const override;
    void setPassword(const QString &password) override;
    bool getUseSSL() const override;
    void setUseSSL(bool useSSL) override;
    QString getHost() const override;
    void setHost(const QString &host) override;
    QString getInstallPath() const override;
    void setInstallPath(const QString &path) override;
    int getServerPort() const override;
    void setServerPort(int port) override;
    bool isAccountValid() const override;
    QString getUserAgent() const override;
    bool getIgnoreSSLErrors() const override;
    void setIgnoreSslErrors(bool ignore);
    QDateTime getLastSync() const override;
    void setLastSync(const QDateTime &syncTime) override;

    Q_INVOKABLE void deleteAccount() override;

    // Full server URL rebuilt from scheme, host, port and path (for display).
    QString serverUrl() const;

    // Accepts "host", "host/path" or a full URL; https is assumed when no
    // scheme is given. Returns false if the URL can not be parsed.
    Q_INVOKABLE bool setServerAddress(const QString &address);

    void setIsAccountValid(bool nIsAccountValid) override;

    static QString settingsFilePath();

Q_SIGNALS:
    void isAccountValidChanged(bool isAccountValid);
    void usernameChanged(const QString &username);
    void serverUrlChanged(const QString &serverUrl);
    void lastSyncChanged(const QDateTime &lastSync);
    void ignoreSslErrorsChanged(bool ignore);

private:
    QString readPassword() const;
    void writePassword(const QString &password);

    mutable QSettings m_settings;
    mutable SecretStore m_secrets;
    mutable QString m_password;
    mutable bool m_passwordLoaded = false;
};
