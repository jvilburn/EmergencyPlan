#pragma once

#include "ServerContract.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

/// The Ward Planning server's wire format: the JSON body of every request the
/// client sends and every answer it reads. Part of the protocol - the companion
/// server reads and writes exactly these shapes.
///
/// Every body is a JSON object. Raw bytes travel as standard base64 with padding;
/// fingerprints and member ids as lowercase hex, as in a header; times as ISO 8601
/// UTC with milliseconds; a header as the JSON object of its file's header line.
/// A request needing a session carries `Authorization: Bearer <token>`, and a signed
/// one `X-Request-Signature: <base64>` over the canonical body ServerContract names.
///
/// | Call | Route | Request | Answer |
/// |---|---|---|---|
/// | challenge | `POST /auth/challenge` | - | `{challenge}` |
/// | authenticate | `POST /auth/session` | `{device, challenge, signature}` | `{token}` |
/// | registerDocument | `POST /documents` | `{header, blob}` | `{version}` |
/// | head | `GET /documents/{id}/head` | - | `{version, leaseHolder?}` |
/// | headers | `GET /documents/{id}/headers?after={n}` | - | `{headers}` |
/// | download | `GET /documents/{id}?after={n}` | - | `{version, headers, blob}` |
/// | upload | `PUT /documents/{id}` | `{baseVersion, header, blob}` | `{version}` |
/// | takeLease | `POST /documents/{id}/lease` | - | `{holder, expires}` |
/// | releaseLease | `DELETE /documents/{id}/lease` | - | - |
/// | requestAccess | `POST /documents/{id}/requests` | an access request | - |
/// | accessRequests | `GET /documents/{id}/requests` | - | `{requests}` |
/// | clearAccessRequest | `DELETE /documents/{id}/requests?device={fingerprint}` | - | - |
/// | myRequestStatus | `GET /documents/{id}/requests/mine` | - | `{status}` |
/// | myDocuments | `GET /me/documents` | - | `{documents}` |
/// | startPairing | `POST /pairings` | `{deviceName, commitment}` | `{code}` |
/// | pairing | `GET /pairings/{code}` | - | a pairing state |
/// | respondToPairing | `PUT /pairings/{code}/responder` | `{publicKey, verifyKey}` | - |
/// | revealPairing | `PUT /pairings/{code}/reveal` | `{publicKey, verifyKey, nonce}` | - |
/// | handOver | `PUT /pairings/{code}/handover` | `{handover}` | - |
/// | advanceClock | `POST /dev/clock` | `{seconds}` | - |
///
/// A request that fails answers a non-2xx status with `{error}`, the ServerError's
/// name with a lowercase first letter - `versionConflict`, `leaseHeld`. A reply
/// that does not arrive, or a 5xx, is Unreachable.
namespace HttpWire
{

// --- Requests the client writes ---

QJsonObject authenticateRequest(const VerifyKey& device, const QByteArray& challenge, const QByteArray& signature);
QJsonObject registerRequest(const ServerContract::Upload& upload);
QJsonObject uploadRequest(qint64 baseVersion, const ServerContract::Upload& upload);
QJsonObject accessRequestRequest(const ServerContract::AccessRequest& request);
QJsonObject startPairingRequest(const QString& deviceName, const QByteArray& commitment);
QJsonObject deviceKeysRequest(const DeviceKeys& keys);
QJsonObject revealPairingRequest(const DeviceKeys& initiator, const QByteArray& nonce);
QJsonObject handOverRequest(const QByteArray& handover);
QJsonObject advanceClockRequest(qint64 seconds);

// --- Answers the client reads; nullopt for any missing or ill-formed field ---

std::optional<QByteArray> readChallenge(const QJsonObject& json);
std::optional<ServerContract::Session> readSession(const QJsonObject& json);
std::optional<qint64> readVersion(const QJsonObject& json);
std::optional<ServerContract::Head> readHead(const QJsonObject& json);
std::optional<ServerContract::HeaderChain> readHeaderChain(const QJsonObject& json);
std::optional<ServerContract::Snapshot> readSnapshot(const QJsonObject& json);
std::optional<ServerContract::Lease> readLease(const QJsonObject& json);
std::optional<QList<ServerContract::AccessRequest>> readAccessRequests(const QJsonObject& json);
std::optional<ServerContract::RequestStatus> readRequestStatus(const QJsonObject& json);
std::optional<QStringList> readDocumentIds(const QJsonObject& json);
std::optional<QString> readPairingCode(const QJsonObject& json);
std::optional<ServerContract::PairingState> readPairingState(const QJsonObject& json);

/// The error a failed request names, or by its status when it names none this
/// build knows. 5xx is Unreachable: the server is there but not answering.
ServerContract::ServerError readError(int httpStatus, const QByteArray& body);

} // namespace HttpWire
