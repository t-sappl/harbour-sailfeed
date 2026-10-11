// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "configuration.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

namespace {
const QString kUser     = QStringLiteral("account/user");
const QString kPassword = QStringLiteral("account/password");
const QString kHost     = QStringLiteral("account/host");
const QString kPath     = QStringLiteral("account/path");
const QString kPort     = QStringLiteral("account/port");
const QString kSsl      = QStringLiteral("account/ssl");
const QString kValid    = QStringLiteral("account/valid");
const QString kLastSync = QStringLiteral("sync/lastSync");
const QString kIgnoreSsl = QStringLiteral("account/ignoreSslErrors");
}

QString Configuration::settingsFilePath()
{
    // GenericConfigLocation = ~/.config; Sailjail grants ~/.config/<org>/<app>/
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QLatin1Char('/') + QCoreApplication::organizationName()
            + QLatin1Char('/') + QCoreApplication::applicationName();
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + QCoreApplication::applicationName() + QLatin1String(".conf");
}

Configuration::Configuration(QObject *parent)
    : Fuoten::AbstractConfiguration(parent)
    , m_settings(settingsFilePath(), QSettings::IniFormat)
{
}

Configuration::~Configuration() = default;

QString Configuration::getUsername() const
{
    return m_settings.value(kUser).toString();
}

void Configuration::setUsername(const QString &username)
{
    if (username == getUsername())
        return;
    m_settings.setValue(kUser, username);
    Q_EMIT usernameChanged(username);
}

QString Configuration::getPassword() const
{
    return readPassword();
}

void Configuration::setPassword(const QString &password)
{
    writePassword(password);
}

bool Configuration::getUseSSL() const
{
    return m_settings.value(kSsl, true).toBool();
}

void Configuration::setUseSSL(bool useSSL)
{
    m_settings.setValue(kSsl, useSSL);
    Q_EMIT serverUrlChanged(serverUrl());
}

QString Configuration::getHost() const
{
    return m_settings.value(kHost).toString();
}

void Configuration::setHost(const QString &host)
{
    m_settings.setValue(kHost, host);
    Q_EMIT serverUrlChanged(serverUrl());
}

QString Configuration::getInstallPath() const
{
    return m_settings.value(kPath).toString();
}

void Configuration::setInstallPath(const QString &path)
{
    m_settings.setValue(kPath, path);
    Q_EMIT serverUrlChanged(serverUrl());
}

int Configuration::getServerPort() const
{
    return m_settings.value(kPort, 0).toInt();
}

void Configuration::setServerPort(int port)
{
    m_settings.setValue(kPort, port);
    Q_EMIT serverUrlChanged(serverUrl());
}

bool Configuration::isAccountValid() const
{
    return m_settings.value(kValid, false).toBool();
}

void Configuration::setIsAccountValid(bool nIsAccountValid)
{
    if (nIsAccountValid == isAccountValid())
        return;
    m_settings.setValue(kValid, nIsAccountValid);
    m_settings.sync();
    Q_EMIT isAccountValidChanged(nIsAccountValid);
}

QString Configuration::getUserAgent() const
{
    return QStringLiteral("Sailfeed/%1 (Sailfish OS)").arg(QStringLiteral(SAILFEED_VERSION));
}

bool Configuration::getIgnoreSSLErrors() const
{
    return m_settings.value(kIgnoreSsl, false).toBool();
}

void Configuration::setIgnoreSslErrors(bool ignore)
{
    if (ignore == getIgnoreSSLErrors())
        return;
    m_settings.setValue(kIgnoreSsl, ignore);
    m_settings.sync();
    Q_EMIT ignoreSslErrorsChanged(ignore);
}

QDateTime Configuration::getLastSync() const
{
    return m_settings.value(kLastSync).toDateTime();
}

void Configuration::setLastSync(const QDateTime &syncTime)
{
    m_settings.setValue(kLastSync, syncTime);
    m_settings.sync();
    Q_EMIT lastSyncChanged(syncTime);
}

void Configuration::deleteAccount()
{
    for (const QString &key : {kUser, kPassword, kHost, kPath, kPort, kSsl, kValid, kLastSync, kIgnoreSsl,
                               QStringLiteral("backup/userId"), QStringLiteral("backup/userIdAccount"),
                               QStringLiteral("backup/lastBackup"), QStringLiteral("backup/lastFingerprint"),
                               QStringLiteral("backup/linkedAccount")})
        m_settings.remove(key);
    m_settings.sync();
    writePassword(QString());
    Q_EMIT usernameChanged(QString());
    Q_EMIT serverUrlChanged(QString());
    Q_EMIT isAccountValidChanged(false);
    Q_EMIT accountDeleted();
}

QString Configuration::serverUrl() const
{
    if (getHost().isEmpty())
        return QString();
    QUrl url;
    url.setScheme(getUseSSL() ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(getHost());
    if (getServerPort() > 0)
        url.setPort(getServerPort());
    url.setPath(getInstallPath());
    return url.toString();
}

bool Configuration::setServerAddress(const QString &address)
{
    QString a = address.trimmed();
    if (a.isEmpty())
        return false;
    if (!a.contains(QLatin1String("://")))
        a.prepend(QLatin1String("https://"));
    return setServerUrl(QUrl(a, QUrl::StrictMode));
}

QString Configuration::readPassword() const
{
    if (!m_passwordLoaded) {
        m_passwordLoaded = true;
        // Migration: the unpublished 0.1.0 kept the password base64 encoded in the
        // settings file. Move it to Sailfish Secrets and remove it there.
        if (m_settings.contains(kPassword)) {
            const QString legacy = QString::fromUtf8(QByteArray::fromBase64(m_settings.value(kPassword).toByteArray()));
            if (legacy.isEmpty() || m_secrets.savePassword(legacy)) {
                m_settings.remove(kPassword);
                m_settings.sync();
            }
            m_password = legacy;
        } else {
            m_password = m_secrets.loadPassword();
        }
    }
    return m_password;
}

void Configuration::writePassword(const QString &password)
{
    m_password = password;
    m_passwordLoaded = true;
    // On failure the password still works for this session (kept in memory).
    if (password.isEmpty())
        m_secrets.deletePassword();
    else
        m_secrets.savePassword(password);
}
