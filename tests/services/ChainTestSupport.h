#pragma once

#include "AccessRoster.h"
#include "CryptoTypes.h"
#include "DocumentCrypto.h"
#include "DocumentHeader.h"

#include <QList>
#include <QString>

#include <optional>

/// Building well-formed signed headers by hand, for the chain tests.
///
/// Shared with the access-control, reconciliation, and server suites, which need
/// the same scaffolding: people with devices, and headers sealed to them and signed
/// by one of them.
namespace ChainTestSupport
{

/// One device belonging to one person. Several devices may carry the same
/// member id, which is how a person's phone and laptop are expressed.
struct TestDevice
{
    MemberId member;
    Keypair sealing;
    SigningKeypair signing;

    Fingerprint fingerprint() const;
};

MemberId newMember();
QString newDocumentId();

/// A device belonging to a brand new person.
TestDevice newPerson();

/// Another device belonging to an existing person.
TestDevice newDeviceFor(const MemberId& member);

/// Seals dataKey to every device, sets the writer's keys and a fresh nonce, and
/// signs as writer. The payload hash is a stand-in: these tests are about headers,
/// not payloads.
///
/// The first device's person is the only admin - a stable choice that does not
/// move when the writer does, so consecutive headers built this way have the same
/// admin list unless a test says otherwise.
DocumentHeader buildHeader(const QString& documentId,
                           int keyGeneration,
                           qint64 saveCounter,
                           const QList<TestDevice>& devices,
                           const TestDevice& writer,
                           const DataKey& dataKey);

/// The same, with the admins named explicitly. They are put in canonical order.
DocumentHeader buildHeader(const QString& documentId,
                           int keyGeneration,
                           qint64 saveCounter,
                           const QList<TestDevice>& devices,
                           const TestDevice& writer,
                           const DataKey& dataKey,
                           const QList<MemberId>& admins);

/// Re-signs a header after a test has changed one of its fields, so the test
/// exercises the rule it means to rather than failing on the signature.
void resign(DocumentHeader& header, const TestDevice& writer);

/// Completes a header an operation produced, the way the save path will: the
/// writer and its keys, the next save counter, the time, a fresh nonce, a stand-in
/// payload hash, and the signature.
void finishWrite(DocumentHeader& header, const TestDevice& writer);

/// The data key this device can unseal from the header, if any.
std::optional<DataKey> unsealFor(const DocumentHeader& header, const TestDevice& device);

RosterDevice rosterDevice(const TestDevice& device, const QString& name);

/// A roster holding these devices, grouped into people by member id. As with
/// buildHeader, the first device's person is the only admin.
AccessRoster rosterOf(const QList<TestDevice>& devices);

/// The same, with the admins named explicitly.
AccessRoster rosterOf(const QList<TestDevice>& devices, const QList<MemberId>& admins);

/// Member ids in the order a header lists admins.
QList<MemberId> canonicalAdmins(const QList<MemberId>& admins);

} // namespace ChainTestSupport
