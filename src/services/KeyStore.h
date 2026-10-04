#pragma once

#include "CryptoTypes.h"
#include "DocumentCrypto.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <optional>

namespace QKeychain
{
class Job;
}

/// This device's secret identity, as one keychain entry: both keypairs, and the
/// person they belong to once there is one. One entry, so the sealing key and the
/// signing key can never get out of step; the member id with them, so a reinstall
/// or cleared application data keeps the device attached to its person.
struct IdentityRecord
{
    Keypair sealing;
    SigningKeypair signing;
    std::optional<MemberId> member;

    QByteArray toBytes() const;

    /// Exactly what toBytes writes, with every key the right size; nullopt otherwise.
    static std::optional<IdentityRecord> fromBytes(const QByteArray& bytes);

    bool operator==(const IdentityRecord& other) const;
    bool operator!=(const IdentityRecord& other) const { return !(*this == other); }
};

/// One secret in the OS keychain, under the service "Ward Planning" and an account
/// the caller names, so tests keep to their own.
///
/// Every call answers through exactly one signal. A missing entry is notFound -
/// a first run - and anything else that goes wrong is failed: a locked keychain,
/// or a Linux machine with no Secret Service. Keeping them apart is what stops a
/// locked keychain from looking like a first run and minting a second identity.
class KeyStore : public QObject
{
    Q_OBJECT

public:
    KeyStore(const QString& account, QObject* parent);

    void store(const QByteArray& secret);
    void load();

    /// Answers removed, also when there was nothing to remove.
    void remove();

signals:
    void stored();
    void loaded(const QByteArray& secret);
    void notFound();
    void removed();
    void failed(const QString& errorMessage);

private:
    void onStored(QKeychain::Job* job);
    void onLoaded(QKeychain::Job* job);
    void onRemoved(QKeychain::Job* job);

    QString m_account;
};
