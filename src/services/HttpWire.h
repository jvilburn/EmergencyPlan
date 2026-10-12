#pragma once

#include "ServerContract.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>

#include <optional>

/// The JSON the Ward Planning server speaks, one function per request body and per
/// answer, so HttpServerApi holds no field names and a test can check each shape on
/// its own.
///
/// Binary values travel as base64; fingerprints and document ids as lowercase hex;
/// times as ISO 8601 with milliseconds in UTC. Every read is strict: a missing or
/// ill-formed field makes the whole answer unreadable.
///
/// | Call | Route | Request | Answer |
/// |---|---|---|---|
/// | registerDocument | `POST /documents` | `{header, blob}`, signed | `{version}` |
/// | head | `GET /documents/{id}/head` | - | `{version, published, leaseHolder?, header}` |
/// | download | `GET /documents/{id}` | - | `{version, published, header, blob}` |
/// | upload | `PUT /documents/{id}` | `{baseVersion, header, blob}`, signed | `{version}` |
/// | takeLease | `POST /documents/{id}/lease` | -, signed | `{holder, expires}` |
/// | releaseLease | `DELETE /documents/{id}/lease` | -, signed | - |
/// | advanceClock | `POST /dev/clock` | `{seconds}` | - |
///
/// A signed request carries three headers: `X-Signer-Public` and `X-Signer-Verify`, the
/// signer's keys in base64, and `X-Signature`, the Ed25519 signature in base64 over
/// ServerContract::signedBytes. `published` is `{device, when}`.
///
/// A request that fails answers with `{error}`, the ServerError's name with a
/// lowercase first letter, and this status:
///
/// | Status | Errors |
/// |---|---|
/// | 400 | `invalidRequest` |
/// | 403 | `notAKeyHolder` |
/// | 404 | `unknownDocument` |
/// | 409 | `versionConflict`, `idTaken` |
/// | 423 | `leaseHeld` |
/// | 429 | `rateLimited` |
///
/// An answer naming no error this build knows is `invalidRequest`. One with no
/// `{error}` at all - a proxy's page, a wrong address - is Unreachable, except a bare
/// 429, which a front end may send for the server. A redirect, a 5xx, and no answer
/// are Unreachable.
namespace HttpWire
{

// --- Requests the client writes ---

QJsonObject registerRequest(const ServerContract::Upload& upload);
QJsonObject uploadRequest(qint64 baseVersion, const ServerContract::Upload& upload);
QJsonObject advanceClockRequest(qint64 seconds);

/// The three headers a signed request carries, name and value.
QList<QPair<QByteArray, QByteArray>> signerHeaders(const ServerContract::Signer& signer);

// --- Answers the client reads; nullopt for any missing or ill-formed field ---

std::optional<qint64> readVersion(const QJsonObject& json);
std::optional<ServerContract::Head> readHead(const QJsonObject& json);
std::optional<ServerContract::Snapshot> readSnapshot(const QJsonObject& json);
std::optional<ServerContract::Lease> readLease(const QJsonObject& json);

/// The failure a request's answer names; see the table above for an answer that
/// names none.
ServerContract::ServerError readFailure(int httpStatus, const QByteArray& body);

} // namespace HttpWire
