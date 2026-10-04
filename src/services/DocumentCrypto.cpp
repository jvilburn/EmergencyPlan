#include "DocumentCrypto.h"

#include "AccessRoster.h"
#include "DocumentHeader.h"
#include "HexCodec.h"

#include <QHash>
#include <QSet>

#include <sodium.h>

static_assert(DocumentCrypto::SignatureSize == crypto_sign_BYTES);
static_assert(DocumentCrypto::SealedDataKeySize == DataKey::Size + crypto_box_SEALBYTES);
static_assert(DocumentCrypto::NonceSize == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(DocumentCrypto::SigningSeedSize == crypto_sign_SEEDBYTES);
static_assert(DocumentCrypto::PayloadHashSize >= crypto_generichash_blake2b_BYTES_MIN);
static_assert(DocumentCrypto::PayloadHashSize <= crypto_generichash_blake2b_BYTES_MAX);

namespace
{

/// The ASCII bytes prepended to a message before signing. Changing one of these
/// invalidates every existing signature of that kind, so they are part of the
/// document format.
const char* tagText(SignatureTag tag)
{
    switch (tag)
    {
    case SignatureTag::Auth:
        return "auth";
    case SignatureTag::Request:
        return "request";
    case SignatureTag::Header:
        return "header";
    case SignatureTag::Pairing:
        return "pairing";
    }

    // Unreachable for a value of the enumeration. Returning nothing would let a
    // new tag added without a case here sign under the same bytes as another.
    Q_ASSERT(false);
    return nullptr;
}

/// tag || 0x00 || message. The 0x00 is what keeps one tag from being a prefix of
/// another plus the start of a message.
QByteArray taggedMessage(SignatureTag tag, const QByteArray& message)
{
    const char* text = tagText(tag);
    if (text == nullptr)
    {
        return QByteArray();
    }

    QByteArray tagged(text);
    tagged.append('\0');
    tagged.append(message);
    return tagged;
}

/// The personalisation string for each domain, zero-padded to
/// crypto_generichash_blake2b_PERSONALBYTES.
///
/// The static_asserts are what keep them short enough. A runtime check would
/// compile out of a release build, which is exactly where a silently truncated
/// personalisation would do its damage.
constexpr char FingerprintDomain[] = "wp-fingerprint";
constexpr char PayloadDomain[] = "wp-payload";
constexpr char PairingCommitDomain[] = "wp-pair-commit";
constexpr char PairingComparisonDomain[] = "wp-pair-number";
constexpr char RequestDomain[] = "wp-request";

static_assert(sizeof(FingerprintDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);
static_assert(sizeof(PayloadDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);
static_assert(sizeof(PairingCommitDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);
static_assert(sizeof(PairingComparisonDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);
static_assert(sizeof(RequestDomain) - 1 <= crypto_generichash_blake2b_PERSONALBYTES);

const char* domainText(HashDomain domain)
{
    switch (domain)
    {
    case HashDomain::Fingerprint:
        return FingerprintDomain;
    case HashDomain::Payload:
        return PayloadDomain;
    case HashDomain::PairingCommit:
        return PairingCommitDomain;
    case HashDomain::PairingComparison:
        return PairingComparisonDomain;
    case HashDomain::Request:
        return RequestDomain;
    }

    // Unreachable for a value of the enumeration. A new domain added without a
    // case here must not quietly share another's parameter block.
    Q_ASSERT(false);
    return nullptr;
}

const unsigned char* asUnsigned(const QByteArray& bytes)
{
    return reinterpret_cast<const unsigned char*>(bytes.constData());
}

unsigned char* asUnsigned(QByteArray& bytes)
{
    return reinterpret_cast<unsigned char*>(bytes.data());
}

QHash<QString, RecipientEntry> byFingerprint(const DocumentHeader& header)
{
    QHash<QString, RecipientEntry> entries;
    for (const RecipientEntry& entry : header.recipients)
    {
        entries.insert(entry.fingerprint.toHex(), entry);
    }
    return entries;
}

QSet<QString> peopleOf(const DocumentHeader& header)
{
    QSet<QString> people;
    for (const RecipientEntry& entry : header.recipients)
    {
        people.insert(entry.member.toHex());
    }
    return people;
}

/// Rule 4, within one header. Checked on both headers before they are compared,
/// because a comparison against an incoherent header proves nothing.
bool headerIsCoherent(const DocumentHeader& header)
{
    // What parse enforces, held here too, so a header built in memory - one the
    // server receives, or a test builds - is judged by the same rules as one read.
    if (header.formatVersion != DocumentHeader::CurrentFormatVersion
        || !HexCodec::isCanonical(header.documentId, DocumentHeader::DocumentIdLength)
        || (!header.originDocumentId.isEmpty()
            && !HexCodec::isCanonical(header.originDocumentId, DocumentHeader::DocumentIdLength))
        || header.keyGeneration < 1)
    {
        return false;
    }

    if (!header.lastWriter.isValid() || !header.writerPublicKey.isValid()
        || !header.writerVerifyKey.isValid()
        || DocumentCrypto::fingerprintOf(header.writerPublicKey, header.writerVerifyKey)
               != header.lastWriter
        || !header.lastWriteTime.isValid()
        || header.saveCounter < 0 || header.saveCounter > DocumentHeader::MaxSaveCounter
        || header.payloadHash.size() != DocumentCrypto::PayloadHashSize
        || header.nonce.size() != DocumentCrypto::NonceSize)
    {
        return false;
    }

    QSet<QString> fingerprints;
    QSet<QByteArray> publicKeys;
    QSet<QByteArray> verifyKeys;

    for (const RecipientEntry& entry : header.recipients)
    {
        if (!entry.member.isValid() || !entry.publicKey.isValid() || !entry.verifyKey.isValid()
            || entry.sealedKey.size() != DocumentCrypto::SealedDataKeySize)
        {
            return false;
        }

        // A fingerprint that does not match its own keys would let one device's
        // identity be pinned to another device's keys.
        if (entry.fingerprint != DocumentCrypto::fingerprintOf(entry.publicKey, entry.verifyKey))
        {
            return false;
        }

        const QString fingerprint = entry.fingerprint.toHex();
        if (fingerprints.contains(fingerprint) || publicKeys.contains(entry.publicKey.bytes)
            || verifyKeys.contains(entry.verifyKey.bytes))
        {
            return false;
        }

        fingerprints.insert(fingerprint);
        publicKeys.insert(entry.publicKey.bytes);
        verifyKeys.insert(entry.verifyKey.bytes);
    }

    // Admins are people in this header, listed once each in ascending order, and a
    // header that lists anyone lists at least one - so a ward can never be left
    // with people and nobody able to change who they are.
    const QSet<QString> people = peopleOf(header);
    QString previousAdmin;
    for (const MemberId& admin : header.admins)
    {
        const QString hex = admin.toHex();
        if (!admin.isValid() || hex <= previousAdmin || !people.contains(hex))
        {
            return false;
        }
        previousAdmin = hex;
    }

    return header.recipients.isEmpty() || !header.admins.isEmpty();
}

} // namespace

bool DocumentCrypto::initialise()
{
    // 0 on success, 1 when another caller already initialised it, -1 on failure.
    return sodium_init() >= 0;
}

Keypair DocumentCrypto::generateKeypair()
{
    QByteArray publicBytes(PublicKey::Size, '\0');
    QByteArray secretBytes(SecretKey::Size, '\0');

    crypto_box_keypair(asUnsigned(publicBytes), asUnsigned(secretBytes));

    Keypair keys;
    keys.publicKey.bytes = publicBytes;
    keys.secretKey.bytes = secretBytes;
    return keys;
}

SigningKeypair DocumentCrypto::generateSigningKeypair()
{
    QByteArray verifyBytes(VerifyKey::Size, '\0');
    QByteArray signingBytes(SigningKey::Size, '\0');

    crypto_sign_keypair(asUnsigned(verifyBytes), asUnsigned(signingBytes));

    SigningKeypair keys;
    keys.verifyKey.bytes = verifyBytes;
    keys.signingKey.bytes = signingBytes;
    return keys;
}

Keypair DocumentCrypto::keypairFromSecret(const SecretKey& secretKey)
{
    Keypair keys;
    if (!secretKey.isValid())
    {
        return keys;
    }

    QByteArray publicBytes(PublicKey::Size, '\0');
    crypto_scalarmult_base(asUnsigned(publicBytes), asUnsigned(secretKey.bytes));
    keys.publicKey.bytes = publicBytes;
    keys.secretKey = secretKey;
    return keys;
}

QByteArray DocumentCrypto::seedOf(const SigningKey& signingKey)
{
    if (!signingKey.isValid())
    {
        return QByteArray();
    }

    QByteArray seed(SigningSeedSize, '\0');
    crypto_sign_ed25519_sk_to_seed(asUnsigned(seed), asUnsigned(signingKey.bytes));
    return seed;
}

SigningKeypair DocumentCrypto::signingKeypairFromSeed(const QByteArray& seed)
{
    SigningKeypair keys;
    if (seed.size() != SigningSeedSize)
    {
        return keys;
    }

    QByteArray verifyBytes(VerifyKey::Size, '\0');
    QByteArray signingBytes(SigningKey::Size, '\0');
    crypto_sign_seed_keypair(asUnsigned(verifyBytes), asUnsigned(signingBytes), asUnsigned(seed));
    keys.verifyKey.bytes = verifyBytes;
    keys.signingKey.bytes = signingBytes;
    return keys;
}

DataKey DocumentCrypto::generateDataKey()
{
    QByteArray bytes(DataKey::Size, '\0');
    randombytes_buf(asUnsigned(bytes), bytes.size());

    DataKey key;
    key.bytes = bytes;
    return key;
}

QByteArray DocumentCrypto::hash(HashDomain domain, const QByteArray& material, int outputSize)
{
    if (outputSize < crypto_generichash_blake2b_BYTES_MIN
        || outputSize > crypto_generichash_blake2b_BYTES_MAX)
    {
        return QByteArray();
    }

    const char* text = domainText(domain);
    if (text == nullptr)
    {
        return QByteArray();
    }

    QByteArray personal(crypto_generichash_blake2b_PERSONALBYTES, '\0');
    const QByteArray domainBytes(text);
    personal.replace(0, domainBytes.size(), domainBytes);

    // The salt is a second parameter-block field this design has no use for.
    // Passing an explicit zero-filled salt says so, and behaves the same as the
    // null libsodium also accepts.
    const QByteArray salt(crypto_generichash_blake2b_SALTBYTES, '\0');

    QByteArray digest(outputSize, '\0');
    if (crypto_generichash_blake2b_salt_personal(asUnsigned(digest),
                                                 digest.size(),
                                                 asUnsigned(material),
                                                 material.size(),
                                                 nullptr,
                                                 0,
                                                 asUnsigned(salt),
                                                 asUnsigned(personal))
        != 0)
    {
        return QByteArray();
    }

    return digest;
}

Fingerprint DocumentCrypto::fingerprintOf(const PublicKey& publicKey, const VerifyKey& verifyKey)
{
    Fingerprint fingerprint;
    if (!publicKey.isValid() || !verifyKey.isValid())
    {
        return fingerprint;
    }

    fingerprint.bytes = hash(HashDomain::Fingerprint,
                             publicKey.bytes + verifyKey.bytes,
                             Fingerprint::Size);
    return fingerprint;
}

QByteArray DocumentCrypto::sealDataKey(const DataKey& dataKey, const PublicKey& recipient)
{
    if (!dataKey.isValid() || !recipient.isValid())
    {
        return QByteArray();
    }

    // A sealed box is XSalsa20-Poly1305 internally, not the XChaCha20-Poly1305
    // the payload uses. That difference is immaterial for 32 bytes of key
    // material, and it is what crypto_box_seal provides - do not "fix" it.
    QByteArray sealed(SealedDataKeySize, '\0');
    if (crypto_box_seal(asUnsigned(sealed),
                        asUnsigned(dataKey.bytes),
                        dataKey.bytes.size(),
                        asUnsigned(recipient.bytes))
        != 0)
    {
        return QByteArray();
    }

    return sealed;
}

std::optional<DataKey> DocumentCrypto::unsealDataKey(const QByteArray& sealed, const Keypair& keys)
{
    if (sealed.size() != SealedDataKeySize || !keys.publicKey.isValid() || !keys.secretKey.isValid())
    {
        return std::nullopt;
    }

    QByteArray opened(DataKey::Size, '\0');
    if (crypto_box_seal_open(asUnsigned(opened),
                             asUnsigned(sealed),
                             sealed.size(),
                             asUnsigned(keys.publicKey.bytes),
                             asUnsigned(keys.secretKey.bytes))
        != 0)
    {
        return std::nullopt;
    }

    DataKey key;
    key.bytes = opened;
    return key;
}

QByteArray DocumentCrypto::sign(SignatureTag tag, const QByteArray& message, const SigningKey& key)
{
    if (!key.isValid())
    {
        return QByteArray();
    }

    const QByteArray tagged = taggedMessage(tag, message);
    if (tagged.isEmpty())
    {
        return QByteArray();
    }

    QByteArray signature(SignatureSize, '\0');
    if (crypto_sign_detached(asUnsigned(signature),
                             nullptr,
                             asUnsigned(tagged),
                             tagged.size(),
                             asUnsigned(key.bytes))
        != 0)
    {
        return QByteArray();
    }

    return signature;
}

bool DocumentCrypto::verify(SignatureTag tag,
                            const QByteArray& message,
                            const QByteArray& signature,
                            const VerifyKey& key)
{
    if (!key.isValid() || signature.size() != SignatureSize)
    {
        return false;
    }

    const QByteArray tagged = taggedMessage(tag, message);
    if (tagged.isEmpty())
    {
        return false;
    }

    return crypto_sign_verify_detached(asUnsigned(signature),
                                       asUnsigned(tagged),
                                       tagged.size(),
                                       asUnsigned(key.bytes))
        == 0;
}

// ---------------------------------------------------------------------------
// Payload
// ---------------------------------------------------------------------------

QByteArray DocumentCrypto::generateNonce()
{
    QByteArray nonce(NonceSize, '\0');
    randombytes_buf(asUnsigned(nonce), nonce.size());
    return nonce;
}

QByteArray DocumentCrypto::encryptPayload(const QByteArray& plaintext,
                                          const DataKey& key,
                                          const DocumentHeader& header)
{
    // decryptPayload cannot tell an empty document from a decompression failure,
    // so an empty one is refused here rather than written and then unreadable.
    if (!key.isValid() || header.nonce.size() != NonceSize || plaintext.isEmpty())
    {
        return QByteArray();
    }

    const QByteArray compressed = qCompress(plaintext);
    const QByteArray associated = header.associatedData();

    QByteArray sealed(compressed.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
    unsigned long long sealedLength = 0;

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(asUnsigned(sealed),
                                                   &sealedLength,
                                                   asUnsigned(compressed),
                                                   compressed.size(),
                                                   asUnsigned(associated),
                                                   associated.size(),
                                                   nullptr,
                                                   asUnsigned(header.nonce),
                                                   asUnsigned(key.bytes))
        != 0)
    {
        return QByteArray();
    }

    sealed.resize(qsizetype(sealedLength));
    return sealed;
}

std::optional<QByteArray> DocumentCrypto::decryptPayload(const QByteArray& sealed,
                                                         const DataKey& key,
                                                         const DocumentHeader& header)
{
    if (!key.isValid() || header.nonce.size() != NonceSize
        || sealed.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES)
    {
        return std::nullopt;
    }

    const QByteArray associated = header.associatedData();

    QByteArray compressed(sealed.size(), '\0');
    unsigned long long compressedLength = 0;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(asUnsigned(compressed),
                                                   &compressedLength,
                                                   nullptr,
                                                   asUnsigned(sealed),
                                                   sealed.size(),
                                                   asUnsigned(associated),
                                                   associated.size(),
                                                   asUnsigned(header.nonce),
                                                   asUnsigned(key.bytes))
        != 0)
    {
        return std::nullopt;
    }

    compressed.resize(qsizetype(compressedLength));

    const QByteArray plaintext = qUncompress(compressed);
    if (plaintext.isEmpty())
    {
        // qUncompress reports every failure as an empty result, so an empty
        // result is a failure. A document is never empty.
        return std::nullopt;
    }

    return plaintext;
}

QByteArray DocumentCrypto::payloadHash(const QByteArray& ciphertext)
{
    return hash(HashDomain::Payload, ciphertext, PayloadHashSize);
}

// ---------------------------------------------------------------------------
// Header signing
// ---------------------------------------------------------------------------

QByteArray DocumentCrypto::signHeader(const DocumentHeader& header, const SigningKey& key)
{
    return sign(SignatureTag::Header, header.signedBytes(), key);
}

bool DocumentCrypto::verifyHeader(const DocumentHeader& header)
{
    return verify(SignatureTag::Header,
                  header.signedBytes(),
                  header.writerSignature,
                  header.writerVerifyKey);
}

bool DocumentCrypto::writerIsAccountedFor(const DocumentHeader& header, const AccessRoster& roster)
{
    // A writer in the roster is in the header too, with the keys the header
    // carries for it.
    const std::optional<RosterDevice> device = roster.deviceFor(header.lastWriter);
    if (device.has_value())
    {
        return header.entryFor(header.lastWriter).has_value()
            && device.value().publicKey() == header.writerPublicKey
            && device.value().verifyKey() == header.writerVerifyKey;
    }

    // Not in the roster: acceptable only as a leave, whose header no longer lists
    // the writer either.
    return !header.entryFor(header.lastWriter).has_value();
}

// ---------------------------------------------------------------------------
// The membership chain
// ---------------------------------------------------------------------------

bool DocumentCrypto::isCoherent(const DocumentHeader& header)
{
    return headerIsCoherent(header);
}

bool DocumentCrypto::verifyLink(const DocumentHeader& previous, const DocumentHeader& next)
{
    // Rule 4 within each header, before any comparison between them means anything.
    if (!headerIsCoherent(previous) || !headerIsCoherent(next))
    {
        return false;
    }

    // One document. A chain is only ever assembled for one, but nothing else checks
    // it, and a server splicing another ward's headers in would otherwise pass. Its
    // origin and format are set when it is created and never change either; an
    // origin added later would change how Open-a-copy treats it.
    if (previous.documentId != next.documentId
        || previous.originDocumentId != next.originDocumentId
        || previous.formatVersion != next.formatVersion)
    {
        return false;
    }

    // Rule 1: the writer is a device of the PREVIOUS header, with the same keys, and
    // signed. Looking the writer up in next would let a server mint a header listing
    // a key of its own and sign with it.
    const std::optional<RecipientEntry> writerBefore = previous.entryFor(next.lastWriter);
    if (!writerBefore.has_value() || writerBefore.value().publicKey != next.writerPublicKey
        || writerBefore.value().verifyKey != next.writerVerifyKey || !verifyHeader(next))
    {
        return false;
    }

    // Rule 2, first half: the save counter rises, so versions of one key generation
    // are ordered and a device's high-water mark can refuse an older one. How far
    // it may rise in one upload is the server's check, since a chain with headers
    // missing sees larger gaps.
    if (next.saveCounter <= previous.saveCounter)
    {
        return false;
    }

    const QHash<QString, RecipientEntry> before = byFingerprint(previous);
    const QHash<QString, RecipientEntry> after = byFingerprint(next);

    // Rule 4 across the two: a device in both is the same device, of the same person.
    for (auto entry = after.constBegin(); entry != after.constEnd(); ++entry)
    {
        const auto existing = before.constFind(entry.key());
        if (existing != before.constEnd()
            && (existing.value().member != entry.value().member
                || existing.value().publicKey != entry.value().publicKey
                || existing.value().verifyKey != entry.value().verifyKey))
        {
            return false;
        }
    }

    QList<RecipientEntry> added;
    for (auto entry = after.constBegin(); entry != after.constEnd(); ++entry)
    {
        if (!before.contains(entry.key()))
        {
            added.append(entry.value());
        }
    }

    QList<RecipientEntry> dropped;
    for (auto entry = before.constBegin(); entry != before.constEnd(); ++entry)
    {
        if (!after.contains(entry.key()))
        {
            dropped.append(entry.value());
        }
    }

    const MemberId writerMember = writerBefore.value().member;
    const QSet<QString> peopleBefore = peopleOf(previous);
    const QSet<QString> peopleAfter = peopleOf(next);
    const int generationStep = next.keyGeneration - previous.keyGeneration;

    // "An admin" is always an admin of PREVIOUS, so nobody can promote themselves and
    // use the role in the same version.
    const bool writerIsAdmin = previous.isAdmin(writerMember);

    // Rules 3 and 7: a writer missing from its own version is leaving. A leave drops
    // the writer's whole person and nothing else, and does not rotate the key - the
    // leaver would generate the new key, so rotating would protect nothing.
    if (!after.contains(next.lastWriter.toHex()))
    {
        if (peopleBefore - peopleAfter != QSet<QString>{writerMember.toHex()} || !added.isEmpty())
        {
            return false;
        }

        for (const RecipientEntry& entry : dropped)
        {
            if (entry.member != writerMember)
            {
                return false;
            }
        }

        QList<MemberId> remainingAdmins = previous.admins;
        remainingAdmins.removeAll(writerMember);

        return next.admins == remainingAdmins && generationStep == 0;
    }

    // Rules 5 and 8: a device added to an existing person is added by that person; a
    // new person is added by an admin.
    for (const RecipientEntry& entry : added)
    {
        if (peopleBefore.contains(entry.member.toHex()))
        {
            if (entry.member != writerMember)
            {
                return false;
            }
        }
        else if (!writerIsAdmin)
        {
            return false;
        }
    }

    // Rules 6 and 7: a device dropped while its person remains is dropped by that
    // person or an admin; a person dropped entirely is dropped by an admin.
    for (const RecipientEntry& entry : dropped)
    {
        const bool personRemains = peopleAfter.contains(entry.member.toHex());
        if (personRemains && entry.member == writerMember)
        {
            continue;
        }

        if (!writerIsAdmin)
        {
            return false;
        }
    }

    // Rule 9.
    if (previous.admins != next.admins && !writerIsAdmin)
    {
        return false;
    }

    // Rule 2, second half: the key rotates exactly when someone loses access, and
    // only then - so a header skipped for leaving membership unchanged can never
    // have hidden a rotation, and the generation can never jump.
    return generationStep == (dropped.isEmpty() ? 0 : 1);
}

ChainResult DocumentCrypto::verifyChain(const DocumentHeader& anchor,
                                        const QList<DocumentHeader>& later,
                                        const Fingerprint& self)
{
    if (!self.isValid() || later.isEmpty())
    {
        return ChainResult::Broken;
    }

    // Removed is the one result that deletes a file. Concluding it from an anchor
    // that never listed this device would turn a caller's mistake into data loss,
    // so the anchor has to be one this device was actually in.
    if (!anchor.entryFor(self).has_value())
    {
        return ChainResult::Broken;
    }

    const DocumentHeader* previous = &anchor;
    for (const DocumentHeader& next : later)
    {
        if (!verifyLink(*previous, next))
        {
            return ChainResult::Broken;
        }

        previous = &next;
    }

    return later.last().entryFor(self).has_value() ? ChainResult::StillMember
                                                   : ChainResult::Removed;
}
