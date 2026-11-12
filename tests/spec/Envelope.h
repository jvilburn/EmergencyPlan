#pragma once

#include "Fields.h"

#include <stdexcept>
#include <string>
#include <vector>

/// A file whose format is above every version this reader knows.
class NewerFormat : public std::runtime_error
{
public:
    NewerFormat() : std::runtime_error("the file is in a newer format") {}
};

/// A device's keys, from its key file, or an invite's, from its seed.
struct DeviceKeys
{
    Bytes sealingPublic;  ///< X25519
    Bytes sealingSecret;  ///< X25519
    Bytes verifyKey;      ///< Ed25519
};

/// One recipient entry of a header.
struct RecipientEntry
{
    Bytes fingerprint;
    Bytes sealedKey;
};

/// A header as the reader read it, with the exact bytes of its line.
struct Header
{
    std::string line;
    Bytes document;
    std::int64_t number = 0;
    Bytes writer;
    Bytes nonce;
    std::vector<RecipientEntry> recipients;
};

/// The keys the key file text holds, as docs/FORMAT.md's *The key file* states.
DeviceKeys readKeyFile(const std::string& text);

/// The fingerprint of a device or invite: docs/FORMAT.md's *Fingerprint*.
Bytes fingerprintOf(const Bytes& sealingPublic, const Bytes& verifyKey);

/// Unkeyed BLAKE2b-256 of text, with a zero salt and no personalization.
Bytes hashOf(const std::string& text);

/// The fields of line, a JSON object found at path whose format is read as docs/FORMAT.md's
/// *Format versions* states, with format taken. Throws NewerFormat for a format above 1.
Fields readFormatLine(const std::string& line, const std::string& path);

/// The header line of file, read strictly, and the payload bytes after its newline. Throws
/// NewerFormat for a format above 1.
Header readHeader(const std::string& file, std::string* payloadBytes);

/// The data key sealed to keys in header's entry for them.
Bytes unsealDataKey(const Header& header, const DeviceKeys& keys);

/// The payload bytes decrypted under dataKey against the header line, then inflated: the
/// compact payload JSON.
std::string openPayload(const Header& header, const Bytes& dataKey, const std::string& payloadBytes);
