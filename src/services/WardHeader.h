#pragma once

#include "AccessList.h"
#include "Id.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <optional>

/// One recipient entry: a fingerprint, and the data key sealed to that recipient's X25519 key.
struct Recipient
{
    QByteArray fingerprint;
    QByteArray sealedKey;
};

/// Which version a header is: its number, and the fingerprint of the device or open invite
/// that wrote it. Versions rank by number, then by writer.
struct VersionRank
{
    qint64 number = 0;
    QByteArray writer;

    /// True when this version's number is higher, or the numbers are equal and its writer's
    /// fingerprint is higher, compared byte by byte.
    bool ranksAbove(const VersionRank& other) const;

    bool operator==(const VersionRank& other) const { return number == other.number && writer == other.writer; }
};

struct HeaderRead;

/// The plaintext header line that opens every ward file and version. Its exact bytes, without
/// the newline that ends them, are the associated data of the payload's encryption, so a
/// header is never rebuilt from what was parsed.
class WardHeader
{
public:
    /// The format version this app writes, and the only one it reads.
    static constexpr int Format = 1;

    /// The header of a new write of version of document: a nonce drawn fresh from
    /// randombytes_buf, and dataKey sealed to each of recipients, in their order. Its line is
    /// built here, once.
    static WardHeader build(const DocumentId& document, const VersionRank& version, const QByteArray& dataKey,
                            const QList<RecipientKeys>& recipients);

    /// The header line holds. Its format key is read first: an integer above Format is Newer,
    /// and anything but Format is Unreadable; the rest is then read strictly.
    static HeaderRead parse(const QByteArray& line);

    const DocumentId& document() const { return m_document; }
    const VersionRank& version() const { return m_version; }
    const QByteArray& nonce() const { return m_nonce; }
    const QList<Recipient>& recipients() const { return m_recipients; }
    const QByteArray& line() const { return m_line; }

    /// The data key sealed to the recipient with that fingerprint, or none when the header
    /// does not name it.
    std::optional<QByteArray> sealedKeyFor(const QByteArray& fingerprint) const;

private:
    WardHeader(const DocumentId& document, const VersionRank& version, const QByteArray& nonce,
               const QList<Recipient>& recipients, const QByteArray& line);

    DocumentId m_document;
    VersionRank m_version;
    QByteArray m_nonce;
    QList<Recipient> m_recipients;
    QByteArray m_line;
};

enum class HeaderStatus
{
    Read,
    Newer,      ///< the format is above every version this app reads
    Unreadable  ///< fails validation; problem says where
};

struct HeaderRead
{
    HeaderStatus status;
    QString problem;                   ///< when Unreadable
    std::optional<WardHeader> header;  ///< when Read
};
