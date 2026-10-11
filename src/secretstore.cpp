// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "secretstore.h"

#ifdef SAILFEED_NO_SECRETS

SecretStore::SecretStore() = default;
SecretStore::~SecretStore() = default;
QString SecretStore::loadPassword() { return m_memory; }
bool SecretStore::savePassword(const QString &password) { m_memory = password; return true; }
bool SecretStore::deletePassword() { m_memory.clear(); return true; }
bool SecretStore::ensureCollection() { return true; }

#else

#include <Sailfish/Secrets/createcollectionrequest.h>
#include <Sailfish/Secrets/deletesecretrequest.h>
#include <Sailfish/Secrets/result.h>
#include <Sailfish/Secrets/secret.h>
#include <Sailfish/Secrets/secretmanager.h>
#include <Sailfish/Secrets/storedsecretrequest.h>
#include <Sailfish/Secrets/storesecretrequest.h>

using namespace Sailfish::Secrets;

namespace {
const QString kCollection = QStringLiteral("harbourSailfeed");
const QString kSecretName = QStringLiteral("appPassword");

QString describe(const Result &r)
{
    return QStringLiteral("%1 (%2)").arg(r.errorMessage()).arg(static_cast<int>(r.errorCode()));
}

bool isNotFound(const Result &r)
{
    return r.errorCode() == Result::InvalidCollectionError
        || r.errorCode() == Result::InvalidSecretError
        || r.errorCode() == Result::InvalidSecretIdentifierError;
}
} // namespace

struct SecretStore::Private {
    SecretManager manager;
    Secret::Identifier ident{kSecretName, kCollection, SecretManager::DefaultEncryptedStoragePluginName};
};

SecretStore::SecretStore() : d(new Private) {}

SecretStore::~SecretStore()
{
    delete d;
}

bool SecretStore::ensureCollection()
{
    CreateCollectionRequest req;
    req.setManager(&d->manager);
    req.setCollectionName(kCollection);
    req.setCollectionLockType(CreateCollectionRequest::DeviceLock);
    req.setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
    req.setAccessControlMode(SecretManager::OwnerOnlyMode);
    req.setUserInteractionMode(SecretManager::SystemInteraction);
    req.setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    req.setEncryptionPluginName(SecretManager::DefaultEncryptedStoragePluginName);
    req.startRequest();
    req.waitForFinished();

    const Result r = req.result();
    if (r.code() == Result::Succeeded || r.errorCode() == Result::CollectionAlreadyExistsError)
        return true;
    m_lastError = describe(r);
    qWarning("Secrets: failed to create collection: %s", qUtf8Printable(m_lastError));
    return false;
}

QString SecretStore::loadPassword()
{
    StoredSecretRequest req;
    req.setManager(&d->manager);
    req.setIdentifier(d->ident);
    req.setUserInteractionMode(SecretManager::SystemInteraction);
    req.startRequest();
    req.waitForFinished();

    const Result r = req.result();
    if (r.code() == Result::Succeeded)
        return QString::fromUtf8(req.secret().data());
    if (!isNotFound(r)) {
        m_lastError = describe(r);
        qWarning("Secrets: failed to read password: %s", qUtf8Printable(m_lastError));
    }
    return QString();
}

bool SecretStore::savePassword(const QString &password)
{
    if (!ensureCollection())
        return false;

    // Replace an existing secret: delete first (ignore "not found").
    deletePassword();

    Secret secret(d->ident);
    secret.setType(Secret::TypeBlob);
    secret.setData(password.toUtf8());

    StoreSecretRequest req;
    req.setManager(&d->manager);
    req.setSecretStorageType(StoreSecretRequest::CollectionSecret);
    req.setUserInteractionMode(SecretManager::SystemInteraction);
    req.setSecret(secret);
    req.startRequest();
    req.waitForFinished();

    const Result r = req.result();
    if (r.code() == Result::Succeeded)
        return true;
    m_lastError = describe(r);
    qWarning("Secrets: failed to store password: %s", qUtf8Printable(m_lastError));
    return false;
}

bool SecretStore::deletePassword()
{
    DeleteSecretRequest req;
    req.setManager(&d->manager);
    req.setIdentifier(d->ident);
    req.setUserInteractionMode(SecretManager::SystemInteraction);
    req.startRequest();
    req.waitForFinished();

    const Result r = req.result();
    if (r.code() == Result::Succeeded || isNotFound(r))
        return true;
    m_lastError = describe(r);
    qWarning("Secrets: failed to delete password: %s", qUtf8Printable(m_lastError));
    return false;
}

#endif
