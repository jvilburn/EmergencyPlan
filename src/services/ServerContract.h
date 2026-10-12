#pragma once

#include "CryptoTypes.h"
#include "DocumentHeader.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <optional>

/// What the Ward Planning server accepts and returns - types only, no transport.
///
/// The server is a versioned store of encrypted documents with an edit lease. It
/// holds no key material, never reads a payload, and knows nothing about people: who
/// may read a ward is whoever its current header seals the key to, and who may write
/// it is whoever can sign as one of them. Its only cryptography is computing a
/// fingerprint from two public keys and checking an Ed25519 signature.
///
/// Confidentiality does not rest on it. Reads are open to anyone with a document id,
/// which is 128 random bits, and give ciphertext.
///
/// FakeServer (tests) implements this in memory and enforces every rule the real
/// server must, so a client written against it will not meet a stricter server.
namespace ServerContract
{

/// Why a call failed. None only on success.
enum class ServerError
{
    None,
    Unreachable,
    NotAKeyHolder,      ///< the signer is not among the current header's recipients
    UnknownDocument,
    VersionConflict,    ///< the upload was not built on the current version
    LeaseHeld,          ///< another device holds the edit lease
    IdTaken,            ///< a document id already registered
    RateLimited,
    InvalidRequest      ///< ill-formed, or a signature that does not verify
};

/// A call's outcome: a value, or the reason there is none.
template<typename T>
struct Result
{
    std::optional<T> value;
    ServerError error = ServerError::None;

    bool ok() const { return value.has_value(); }
    static Result success(const T& result) { return Result{result, ServerError::None}; }
    static Result failure(ServerError reason) { return Result{std::nullopt, reason}; }
};

/// How every call answers. Called exactly once, and never before the call that
/// was given it returns, so a caller may start a request from inside its own
/// state change without being re-entered halfway through it.
template<typename T>
using Reply = std::function<void(const Result<T>&)>;

/// The value of a call that returns nothing but success.
struct Done
{
};

/// Who published a version, and when. The server knows the device, never a name;
/// the roster inside the payload names it.
struct Publication
{
    Fingerprint device;
    QDateTime when;
};

/// POST /documents and PUT /documents/{id}: the plaintext header and the opaque
/// blob. The server checks the signer against the header and never looks inside the
/// blob.
struct Upload
{
    DocumentHeader header;
    QByteArray blob;
};

/// Who signs a write: the device's two public keys, whose fingerprint the current
/// header must name - or the uploaded header, for the first version - and an Ed25519
/// signature over signedBytes.
struct Signer
{
    DeviceKeys keys;
    QByteArray signature;

    Fingerprint fingerprint() const;
};

/// The header as it travels and is signed: its JSON, compact, keys sorted, exactly as
/// headerText gives it. A server verifies the signature over the text it received and
/// parses it afterwards, so it never has to reproduce a serialisation.
QByteArray headerText(const DocumentHeader& header);

/// What a write's signature covers: the call's name, the document id, the base version
/// (0 for the first version), and a hash of the header text and one of the blob. One
/// function builds it on both sides. The lease calls cover the call and the document
/// id only; a captured one replays, which takes or releases a lease the signer could
/// take or release anyway.
QByteArray signedBytes(QLatin1String call, const QString& documentId, qint64 baseVersion, const Upload& upload);
QByteArray signedBytes(QLatin1String call, const QString& documentId);

constexpr QLatin1String CallRegister("register");
constexpr QLatin1String CallUpload("upload");
constexpr QLatin1String CallTakeLease("takeLease");
constexpr QLatin1String CallReleaseLease("releaseLease");

// Reads carry the header as the server stored it. A reader parses it with
// Reconcile::validateDownload, so a header in a later format reaches the client as
// NeedsNewerVersion rather than being lost in transit.

/// GET /documents/{id}/head
struct Head
{
    qint64 version = 0;
    Publication published;
    std::optional<Fingerprint> leaseHolder;
    QJsonObject header;
};

/// GET /documents/{id} - one consistent state, so a publish landing mid-download
/// can never pair a header with another version's blob.
struct Snapshot
{
    qint64 version = 0;
    Publication published;
    QJsonObject header;
    QByteArray blob;
};

/// POST /documents/{id}/lease
struct Lease
{
    Fingerprint holder;
    QDateTime expires;
};

/// The server, as a client calls it. Every call answers through its reply, once.
class ServerApi
{
public:
    virtual ~ServerApi() = default;

    /// Stores version 1 of a document under its client-made id. Anyone may; the real
    /// server limits registrations by address. The header must name the signer.
    /// Answers the version.
    virtual void registerDocument(const Signer& signer, const Upload& upload, Reply<qint64> reply) = 0;

    virtual void head(const QString& documentId, Reply<Head> reply) = 0;
    virtual void download(const QString& documentId, Reply<Snapshot> reply) = 0;

    /// Conditional on baseVersion. The signer must be among the current header's
    /// recipients. Answers the new version.
    virtual void upload(const Signer& signer,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload,
                        Reply<qint64> reply) = 0;

    /// The edit lease: taken or refreshed by a key holder, held two minutes, refused
    /// while another device holds it.
    virtual void takeLease(const Signer& signer, const QString& documentId, Reply<Lease> reply) = 0;
    virtual void releaseLease(const Signer& signer, const QString& documentId, Reply<Done> reply) = 0;

    /// Moves the server's clock forward. The fake always honours it; the real server
    /// only in local development, never in a deployed build. It exists so the
    /// conformance suite can check lease expiry.
    virtual void advanceClock(qint64 seconds, Reply<Done> reply) = 0;
};

} // namespace ServerContract
