// WardPlanningSpecReader <app data folder> <document id>
//
// Reads the ward file of that id, where docs/FORMAT.md's *Where the files live* puts it, with
// the device key file beside it, as docs/FORMAT.md alone states the format, and with nothing
// of the app's own. It prints the payload it read as compact JSON on standard output and exits
// 0; it exits 2 for a newer format, and 1, with the problem on standard error, for anything
// else.

#include "Envelope.h"
#include "Fields.h"
#include "Payload.h"

#include <sodium.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{

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

/// The payload of the ward file named by document in appData, read as this device reads it.
nlohmann::json readWard(const std::string& appData, const std::string& document)
{
    if (!decode(document, IdBytes).has_value())
    {
        throw Refusal(document + " is not a document id");
    }
    DeviceKeys keys = readKeyFile(readFile(appData + "/keys/device.key"));

    std::string payloadBytes;
    Header header = readHeader(readFile(appData + "/wards/" + document + ".wardplanning"), &payloadBytes);
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
    checkAgreement(header, readLocalPayload(payload));
    return payload;
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "usage: WardPlanningSpecReader <app data folder> <document id>\n";
        return 1;
    }
    if (sodium_init() < 0)
    {
        std::cerr << "libsodium did not initialize\n";
        return 1;
    }
    try
    {
        std::cout << readWard(argv[1], argv[2]).dump() << std::flush;
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
