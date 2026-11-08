#include "Envelope.h"
#include "Fields.h"

#include <sodium.h>
#include <zlib.h>

#include <cstring>

namespace
{

nlohmann::json parse(const std::string& text, const std::string& what)
{
    nlohmann::json value = nlohmann::json::parse(text, nullptr, false);
    if (value.is_discarded() || !value.is_object())
    {
        throw Refusal(what + " is not a JSON object");
    }
    return value;
}

}  // namespace

DeviceKeys readKeyFile(const std::string& text)
{
    Fields fields(parse(text, "the key file"), "keyFile");
    if (fields.integer("format") != 1)
    {
        throw Refusal("keyFile.format is not 1");
    }
    Bytes sealingSecret = fields.bytes("sealingSecret", KeyBytes);
    Bytes signingSeed = fields.bytes("signingSeed", KeyBytes);
    fields.done();

    DeviceKeys keys;
    keys.sealingSecret = sealingSecret;
    keys.sealingPublic = Bytes(KeyBytes);
    crypto_scalarmult_base(keys.sealingPublic.data(), sealingSecret.data());
    keys.verifyKey = Bytes(KeyBytes);
    Bytes signingSecret(crypto_sign_SECRETKEYBYTES);
    crypto_sign_seed_keypair(keys.verifyKey.data(), signingSecret.data(), signingSeed.data());
    return keys;
}

Bytes fingerprintOf(const Bytes& sealingPublic, const Bytes& verifyKey)
{
    Bytes in(sealingPublic);
    in.insert(in.end(), verifyKey.begin(), verifyKey.end());
    unsigned char salt[crypto_generichash_blake2b_SALTBYTES] = {};
    unsigned char personal[crypto_generichash_blake2b_PERSONALBYTES] = {};
    std::memcpy(personal, "wp-fingerprint", std::strlen("wp-fingerprint"));
    Bytes fingerprint(FingerprintBytes);
    crypto_generichash_blake2b_salt_personal(fingerprint.data(), fingerprint.size(), in.data(), in.size(),
                                             nullptr, 0, salt, personal);
    return fingerprint;
}

Bytes hashOf(const std::string& text)
{
    Bytes hash(HashBytes);
    crypto_generichash(hash.data(), hash.size(), reinterpret_cast<const unsigned char*>(text.data()), text.size(),
                       nullptr, 0);
    return hash;
}

Header readHeader(const std::string& file, std::string* payloadBytes)
{
    std::size_t boundary = file.find('\n');
    if (boundary == std::string::npos)
    {
        throw Refusal("the file has no header line");
    }
    Header header;
    header.line = file.substr(0, boundary);
    *payloadBytes = file.substr(boundary + 1);

    nlohmann::json value = parse(header.line, "the header");
    nlohmann::json::const_iterator format = value.find("format");
    if (format == value.end() || !isInteger(*format))
    {
        throw Refusal("header.format is not an integer");
    }
    if (*format > 1)
    {
        throw NewerFormat();
    }
    Fields fields(value, "header");
    if (fields.integer("format") != 1)
    {
        throw Refusal("header.format is not a version this reader knows");
    }
    header.document = fields.bytes("document", IdBytes);
    header.number = fields.integer("number");
    if (header.number < 1)
    {
        throw Refusal(fields.pathOf("number") + " is below 1");
    }
    header.writer = fields.bytes("writer", FingerprintBytes);
    header.nonce = fields.bytes("nonce", NonceBytes);
    for (Fields& recipient : fields.objects("recipients"))
    {
        RecipientEntry entry;
        entry.fingerprint = recipient.bytes("fingerprint", FingerprintBytes);
        entry.sealedKey = recipient.bytes("sealedKey", SealedKeyBytes);
        recipient.done();
        header.recipients.push_back(entry);
    }
    fields.done();
    return header;
}

Bytes unsealDataKey(const Header& header, const DeviceKeys& keys)
{
    Bytes fingerprint = fingerprintOf(keys.sealingPublic, keys.verifyKey);
    for (const RecipientEntry& entry : header.recipients)
    {
        if (entry.fingerprint != fingerprint)
        {
            continue;
        }
        Bytes dataKey(KeyBytes);
        if (crypto_box_seal_open(dataKey.data(), entry.sealedKey.data(), entry.sealedKey.size(),
                                 keys.sealingPublic.data(), keys.sealingSecret.data())
            != 0)
        {
            throw Refusal("the data key does not unseal");
        }
        return dataKey;
    }
    throw Refusal("the header names no recipient with this device's fingerprint");
}

std::string openPayload(const Header& header, const Bytes& dataKey, const std::string& payloadBytes)
{
    if (payloadBytes.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES)
    {
        throw Refusal("the payload is shorter than its tag");
    }
    Bytes compressed(payloadBytes.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long compressedSize = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            compressed.data(), &compressedSize, nullptr,
            reinterpret_cast<const unsigned char*>(payloadBytes.data()), payloadBytes.size(),
            reinterpret_cast<const unsigned char*>(header.line.data()), header.line.size(),
            header.nonce.data(), dataKey.data())
        != 0)
    {
        throw Refusal("the payload does not decrypt");
    }

    if (compressedSize < 4)
    {
        throw Refusal("the payload has no length before its zlib stream");
    }
    uLongf size = (static_cast<uLongf>(compressed[0]) << 24)
        | (static_cast<uLongf>(compressed[1]) << 16)
        | (static_cast<uLongf>(compressed[2]) << 8)
        | static_cast<uLongf>(compressed[3]);
    std::string json(size, '\0');
    uLongf inflated = size;
    if (uncompress(reinterpret_cast<Bytef*>(&json[0]), &inflated, compressed.data() + 4,
                   static_cast<uLong>(compressedSize - 4))
            != Z_OK
        || inflated != size)
    {
        throw Refusal("the payload does not inflate to the length it states");
    }
    return json;
}
