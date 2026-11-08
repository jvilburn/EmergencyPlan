// WardPlanningSpecReader <app data folder> <document id> [<update file>]
//
// Reads the ward file of that id, where docs/FORMAT.md's *Where the files live* puts it, with
// the device key file beside it, as docs/FORMAT.md alone states the format, and with nothing
// of the app's own. It prints the payload it read as compact JSON on standard output and exits
// 0; it exits 2 for a newer format, and 1, with the problem on standard error, for anything
// else. Given an update file, it reads that file as docs/FORMAT.md's *Update files* states,
// with the ward file as the version this device holds, and prints the update's payload.

#include "Envelope.h"
#include "Fields.h"
#include "Payload.h"

#include <sodium.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>

namespace
{

/// Which payload a file carries: a local file's, with this device's own part, or a version's.
enum class PayloadKind
{
    LocalFile,
    Version
};

std::string readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw Refusal("the file " + path + " could not be opened");
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

/// docs/FORMAT.md's *Header and access list agree*.
void checkAgreement(const Header& header, const std::vector<PublicKeys>& keys)
{
    std::vector<Bytes> computed;
    for (const PublicKeys& each : keys)
    {
        computed.push_back(fingerprintOf(each.sealingPublic, each.verifyKey));
    }
    std::vector<Bytes> named;
    for (const RecipientEntry& entry : header.recipients)
    {
        named.push_back(entry.fingerprint);
    }
    std::sort(computed.begin(), computed.end());
    std::sort(named.begin(), named.end());
    if (named != computed || std::adjacent_find(named.begin(), named.end()) != named.end())
    {
        throw Refusal("the header's recipients are not one each for the devices and open invites of the access list");
    }
}

/// The payload of a file of document whose header and payload bytes are given, opened with
/// keys and read as kind, with the public keys of its access list in access.
nlohmann::json openFile(const Header& header, const std::string& payloadBytes, const std::string& document,
                        const DeviceKeys& keys, PayloadKind kind, std::vector<PublicKeys>* access)
{
    if (encode(header.document) != document)
    {
        throw Refusal("the header names another document");
    }
    if (header.recipients.empty())
    {
        throw Refusal("the header names no recipient, as only a deleted ward's final version does");
    }
    Bytes dataKey = unsealDataKey(header, keys);
    nlohmann::json payload = nlohmann::json::parse(openPayload(header, dataKey, payloadBytes), nullptr, false);
    if (payload.is_discarded())
    {
        throw Refusal("the payload is not JSON");
    }
    switch (kind)
    {
    case PayloadKind::LocalFile:
        *access = readLocalPayload(payload);
        break;
    case PayloadKind::Version:
        *access = readVersionPayload(payload);
        break;
    }
    checkAgreement(header, *access);
    return payload;
}

/// The verify key of the device or open invite in access whose fingerprint is fingerprint.
std::optional<Bytes> verifyKeyOf(const std::vector<PublicKeys>& access, const Bytes& fingerprint)
{
    for (const PublicKeys& each : access)
    {
        if (fingerprintOf(each.sealingPublic, each.verifyKey) == fingerprint)
        {
            return each.verifyKey;
        }
    }
    return std::nullopt;
}

/// The payload of the update file text, read with keys against held, the access list of the
/// version this device holds of document: docs/FORMAT.md's *Update files*.
nlohmann::json readUpdate(const std::string& text, const std::string& document, const DeviceKeys& keys,
                          const std::vector<PublicKeys>& held)
{
    std::size_t boundary = text.find('\n');
    if (boundary == std::string::npos)
    {
        throw Refusal("the update has no signature line");
    }
    std::optional<Bytes> signature = decode(text.substr(0, boundary), SignatureBytes);
    if (!signature.has_value())
    {
        throw Refusal("the update's signature line is not base64url of 64 bytes");
    }
    std::string payloadBytes;
    Header header = readHeader(text.substr(boundary + 1), &payloadBytes);
    std::optional<Bytes> verifyKey = verifyKeyOf(held, header.writer);
    if (!verifyKey.has_value())
    {
        throw Refusal("the update's writer is not on the access list this device holds");
    }
    std::string line = "wardplanning-v1 update " + encode(header.document) + " " + encode(hashOf(header.line)) + " "
        + encode(hashOf(payloadBytes));
    if (crypto_sign_verify_detached(signature->data(), reinterpret_cast<const unsigned char*>(line.data()), line.size(),
                                    verifyKey->data())
        != 0)
    {
        throw Refusal("the update's signature does not verify");
    }
    std::vector<PublicKeys> access;
    return openFile(header, payloadBytes, document, keys, PayloadKind::Version, &access);
}

/// The payload the run asks for: of the ward file named by document in appData, read as this
/// device reads it, or, given updatePath, of that update file read against it.
nlohmann::json run(const std::string& appData, const std::string& document, const std::string& updatePath)
{
    if (!decode(document, IdBytes).has_value())
    {
        throw Refusal(document + " is not a document id");
    }
    DeviceKeys keys = readKeyFile(readFile(appData + "/keys/device.key"));

    std::string payloadBytes;
    Header header = readHeader(readFile(appData + "/wards/" + document + ".wardplanning"), &payloadBytes);
    std::vector<PublicKeys> held;
    nlohmann::json payload = openFile(header, payloadBytes, document, keys, PayloadKind::LocalFile, &held);
    if (updatePath.empty())
    {
        return payload;
    }
    return readUpdate(readFile(updatePath), document, keys, held);
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "usage: WardPlanningSpecReader <app data folder> <document id> [<update file>]\n";
        return 1;
    }
    if (sodium_init() < 0)
    {
        std::cerr << "libsodium did not initialize\n";
        return 1;
    }
    try
    {
        std::cout << run(argv[1], argv[2], argc == 4 ? argv[3] : std::string()).dump() << std::flush;
        return 0;
    }
    catch (const NewerFormat& newer)
    {
        std::cerr << newer.what() << "\n";
        return 2;
    }
    catch (const std::exception& problem)
    {
        std::cerr << problem.what() << "\n";
        return 1;
    }
}
