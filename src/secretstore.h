// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Stores the Nextcloud app password with Sailfish Secrets in a collection
// "harbourSailfeed", protected by the device lock and accessible by this
// app only (OwnerOnlyMode). The requests are synchronous; they are only used
// at start-up, on sign-in and on sign-out.
//
// Builds with SAILFEED_NO_SECRETS (desktop tests) keep the password in memory
// only.
class SecretStore
{
public:
    SecretStore();
    ~SecretStore();

    // Returns an empty string if no password is stored or on error.
    QString loadPassword();
    bool savePassword(const QString &password);
    bool deletePassword();

    QString lastError() const { return m_lastError; }

private:
    bool ensureCollection();

    QString m_lastError;
#ifdef SAILFEED_NO_SECRETS
    QString m_memory;
#else
    struct Private;
    Private *d;
#endif
};
