#pragma once

#include "Envelope.h"

#include <nlohmann/json.hpp>

#include <vector>

/// The public keys of one device or open invite in an access list.
struct PublicKeys
{
    Bytes sealingPublic;  ///< X25519
    Bytes verifyKey;      ///< Ed25519
};

/// Reads payload strictly as a local file's, every part and field as docs/FORMAT.md's
/// *Payload* states, and answers the public keys of every device and open invite its access
/// list holds. Throws a Refusal for anything the format does not accept.
std::vector<PublicKeys> readLocalPayload(const nlohmann::json& payload);

/// Reads payload strictly as a version's, which carries no device part, as readLocalPayload
/// reads the rest.
std::vector<PublicKeys> readVersionPayload(const nlohmann::json& payload);
