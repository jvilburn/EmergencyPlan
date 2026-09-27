#pragma once

#include <QJsonObject>

/// Payload JSON for every model type, each with every field it can carry, spelled
/// out as a writer produces it. The strict readers are tested against these, so a
/// field the writer and reader spell differently shows up as a failed round trip.
namespace ModelSamples
{

/// "2026-09-27T12:34:56.789Z": UTC with milliseconds, as the writer spells a time.
extern const char* const Time;

QJsonObject person();           ///< id "p1", every optional field present
QJsonObject secondPerson();     ///< id "p2", required fields only
QJsonObject family();           ///< id "f1", members p1 and p2
QJsonObject team();             ///< led by p1, members p1 and p2
QJsonObject tag();              ///< names p1 and f1
QJsonObject emergencyAsset();
QJsonObject district(const char* id, const char* groupId);
QJsonObject group(const char* id, bool reliefSociety);
QJsonObject ward();
QJsonObject stake();
QJsonObject notification();
QJsonObject attempt();
QJsonObject task();             ///< assigned to p1, notified, resolved
QJsonObject familyRecord();
QJsonObject emergencyResponse();

/// A whole payload using every sample above, with a one-person roster.
QJsonObject document();

} // namespace ModelSamples
