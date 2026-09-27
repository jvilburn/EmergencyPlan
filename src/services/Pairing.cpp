#include "Pairing.h"

#include "CanonicalBytes.h"
#include "DocumentCrypto.h"

#include <sodium.h>

namespace
{

constexpr int CommitmentSize = 32;

/// The first eight bytes of a 16-byte digest - BLAKE2b's shortest - read as one
/// unsigned number and reduced to six digits. The bias of a 64-bit value modulo a
/// million is below one part in 10^13.
constexpr int ComparisonHashSize = 16;
constexpr int ComparisonNumberBytes = 8;
constexpr quint64 ComparisonModulus = 1000000;

// DocumentCrypto::hash returns nothing for a size outside BLAKE2b's range, which
// would read as zero and show every pairing the same number.
static_assert(CommitmentSize >= crypto_generichash_blake2b_BYTES_MIN);
static_assert(ComparisonHashSize >= crypto_generichash_blake2b_BYTES_MIN);

constexpr quint32 PairingCodeModulus = 100000000;
constexpr int PairingCodeDigits = 8;

void appendKeys(CanonicalBytes& writer, const DeviceKeys& keys)
{
    writer.appendBytes(keys.publicKey.bytes);
    writer.appendBytes(keys.verifyKey.bytes);
}

QByteArray handoverMessage(const Pairing::Handover& handover, const QByteArray& nonce)
{
    CanonicalBytes writer;
    writer.appendBytes(nonce);
    writer.appendText(handover.member.toHex());
    writer.appendText(handover.displayName);
    writer.appendNumber(qint64(handover.documentIds.size()));
    for (const QString& documentId : handover.documentIds)
    {
        writer.appendText(documentId);
    }
    return writer.bytes();
}

quint64 bigEndian(const QByteArray& bytes)
{
    quint64 value = 0;
    for (const char byte : bytes)
    {
        value = (value << 8) | quint8(byte);
    }
    return value;
}

} // namespace

namespace Pairing
{

QByteArray newNonce()
{
    QByteArray nonce(NonceSize, '\0');
    randombytes_buf(nonce.data(), size_t(nonce.size()));
    return nonce;
}

QByteArray commit(const DeviceKeys& initiator, const QByteArray& nonce)
{
    if (!initiator.isValid() || nonce.size() != NonceSize)
    {
        return QByteArray();
    }

    CanonicalBytes writer;
    appendKeys(writer, initiator);
    writer.appendBytes(nonce);
    return DocumentCrypto::hash(HashDomain::PairingCommit, writer.bytes(), CommitmentSize);
}

bool revealMatches(const QByteArray& commitment,
                   const DeviceKeys& initiator,
                   const QByteArray& nonce)
{
    // commit is empty for invalid keys or a wrong-sized nonce, so those never match.
    const QByteArray expected = commit(initiator, nonce);
    return commitment.size() == CommitmentSize && expected.size() == CommitmentSize
        && sodium_memcmp(commitment.constData(), expected.constData(), CommitmentSize) == 0;
}

QString comparisonNumber(const DeviceKeys& initiator,
                         const DeviceKeys& responder,
                         const QByteArray& nonce)
{
    CanonicalBytes writer;
    appendKeys(writer, initiator);
    appendKeys(writer, responder);
    writer.appendBytes(nonce);

    const QByteArray digest =
        DocumentCrypto::hash(HashDomain::PairingComparison, writer.bytes(), ComparisonHashSize);
    const QString digits =
        QString::number(bigEndian(digest.left(ComparisonNumberBytes)) % ComparisonModulus)
            .rightJustified(6, QLatin1Char('0'));
    return digits.left(3) + QLatin1Char(' ') + digits.mid(3);
}

QString newPairingCode()
{
    const quint32 number = randombytes_uniform(PairingCodeModulus);
    return QString::number(number).rightJustified(PairingCodeDigits, QLatin1Char('0'));
}

QByteArray signHandover(const Handover& handover,
                        const QByteArray& nonce,
                        const SigningKey& responderKey)
{
    return DocumentCrypto::sign(SignatureTag::Pairing, handoverMessage(handover, nonce), responderKey);
}

bool handoverVerifies(const Handover& handover,
                      const QByteArray& nonce,
                      const VerifyKey& responderVerifyKey)
{
    return DocumentCrypto::verify(SignatureTag::Pairing,
                                  handoverMessage(handover, nonce),
                                  handover.signature,
                                  responderVerifyKey);
}

Responder::Responder(const DeviceKeys& self)
    : m_self(self)
{
}

bool Responder::keepCommitment(const QByteArray& commitment)
{
    if (m_commitment.has_value())
    {
        return false;
    }

    m_commitment = commitment;
    return true;
}

std::optional<QString> Responder::acceptReveal(const DeviceKeys& initiator, const QByteArray& nonce) const
{
    if (!m_commitment.has_value() || !revealMatches(m_commitment.value(), initiator, nonce))
    {
        return std::nullopt;
    }

    return comparisonNumber(initiator, m_self, nonce);
}

Initiator::Initiator(const DeviceKeys& self)
    : m_self(self)
    , m_nonce(newNonce())
{
}

QByteArray Initiator::commitment() const
{
    return commit(m_self, m_nonce);
}

bool Initiator::keepResponder(const DeviceKeys& responder)
{
    if (m_responder.has_value())
    {
        return false;
    }

    m_responder = responder;
    return true;
}

std::optional<QByteArray> Initiator::reveal() const
{
    return m_responder.has_value() ? std::optional<QByteArray>(m_nonce) : std::nullopt;
}

std::optional<QString> Initiator::comparisonNumber() const
{
    if (!m_responder.has_value())
    {
        return std::nullopt;
    }

    return Pairing::comparisonNumber(m_self, m_responder.value(), m_nonce);
}

} // namespace Pairing
