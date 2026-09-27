#include "AccessControl.h"

#include "DocumentCrypto.h"

#include <QDateTime>
#include <QHash>

namespace
{

/// The writer, as both the header and the roster know it. Every operation starts
/// here: the two must agree before either can be trusted, and the writer must be
/// in both.
struct Writer
{
    RecipientEntry entry;
    RosterPerson person;
};

std::optional<Writer> writerOf(const DocumentHeader& header,
                               const AccessRoster& roster,
                               const Fingerprint& writer)
{
    if (!AccessControl::headerMatchesRoster(header, roster))
    {
        return std::nullopt;
    }

    const std::optional<RecipientEntry> entry = header.entryFor(writer);
    const std::optional<RosterPerson> person = roster.personFor(writer);
    if (!entry.has_value() || !person.has_value())
    {
        return std::nullopt;
    }

    return Writer{entry.value(), person.value()};
}

/// What goes into the roster has to come back out of it. A person or device the
/// payload reader would refuse makes the whole document unreadable for every
/// member, and it would arrive through a grant from an unverified request.
bool survivesARoundTrip(const RosterPerson& person)
{
    const std::optional<RosterPerson> back = RosterPerson::fromJson(person.toJson());
    return back.has_value() && back.value() == person;
}

bool survivesARoundTrip(const RosterDevice& device)
{
    const std::optional<RosterDevice> back = RosterDevice::fromJson(device.toJson());
    return back.has_value() && back.value() == device;
}

/// Everything an operation changes, applied the same way every time.
///
/// The writer's keys come from its entry in the header the change starts from, so
/// they are right even when the writer is leaving. The fields a write must produce
/// afresh are cleared, so a save path that forgets one fails rather than reusing
/// the last write's nonce.
std::optional<AccessChange> buildChange(const DocumentHeader& header,
                                        const AccessRoster& roster,
                                        const DataKey& dataKey,
                                        int keyGeneration,
                                        const RecipientEntry& writer)
{
    DocumentHeader next = header;
    next.keyGeneration = keyGeneration;
    next.lastWriter = writer.fingerprint;
    next.writerPublicKey = writer.publicKey;
    next.writerVerifyKey = writer.verifyKey;
    next.lastWriteTime = QDateTime();
    next.nonce.clear();
    next.payloadHash.clear();
    next.writerSignature.clear();

    const std::optional<DocumentHeader> resealed = AccessControl::resealAll(next, roster, dataKey);
    if (!resealed.has_value())
    {
        return std::nullopt;
    }

    AccessChange change;
    change.header = resealed.value();
    change.roster = roster;
    change.dataKey = dataKey;
    return change;
}

} // namespace

std::optional<AccessChange> AccessControl::grant(const DocumentHeader& header,
                                                 const AccessRoster& roster,
                                                 const DataKey& dataKey,
                                                 const Fingerprint& writer,
                                                 const RosterPerson& newPerson)
{
    const std::optional<Writer> author = writerOf(header, roster, writer);
    if (!author.has_value() || !author.value().person.isAdmin() || !dataKey.isValid())
    {
        return std::nullopt;
    }

    // Further devices arrive by linking, which the chain only accepts from the
    // person's own device. A grant carrying several would add them on someone
    // else's word.
    RosterPerson person = newPerson;
    person.setRole(RosterPerson::Role::Member);
    if (person.deviceCount() != 1 || !survivesARoundTrip(person))
    {
        return std::nullopt;
    }

    AccessRoster next = roster;
    if (!next.addPerson(person))
    {
        return std::nullopt;
    }

    return buildChange(header, next, dataKey, header.keyGeneration, author.value().entry);
}

std::optional<AccessChange> AccessControl::linkDevice(const DocumentHeader& header,
                                                      const AccessRoster& roster,
                                                      const DataKey& dataKey,
                                                      const Fingerprint& writer,
                                                      const RosterDevice& newDevice)
{
    const std::optional<Writer> author = writerOf(header, roster, writer);
    if (!author.has_value() || !dataKey.isValid() || !survivesARoundTrip(newDevice)
        || roster.personFor(newDevice.fingerprint()).has_value())
    {
        return std::nullopt;
    }

    AccessRoster next = roster;
    if (!next.addDevice(author.value().person.member(), newDevice))
    {
        return std::nullopt;
    }

    return buildChange(header, next, dataKey, header.keyGeneration, author.value().entry);
}

std::optional<AccessChange> AccessControl::unlinkDevice(const DocumentHeader& header,
                                                        const AccessRoster& roster,
                                                        const Fingerprint& writer,
                                                        const Fingerprint& device)
{
    const std::optional<Writer> author = writerOf(header, roster, writer);
    const std::optional<RosterPerson> owner = roster.personFor(device);
    if (!author.has_value() || !owner.has_value() || device == writer)
    {
        return std::nullopt;
    }

    const bool ownDevice = owner.value().member() == author.value().person.member();
    if (!ownDevice && !author.value().person.isAdmin())
    {
        return std::nullopt;
    }

    // Refuses the last device: losing your last device is leaving or being removed.
    AccessRoster next = roster;
    if (!next.removeDevice(owner.value().member(), device))
    {
        return std::nullopt;
    }

    return buildChange(header,
                       next,
                       DocumentCrypto::generateDataKey(),
                       header.keyGeneration + 1,
                       author.value().entry);
}

std::optional<AccessChange> AccessControl::remove(const DocumentHeader& header,
                                                  const AccessRoster& roster,
                                                  const Fingerprint& writer,
                                                  const MemberId& target)
{
    const std::optional<Writer> author = writerOf(header, roster, writer);
    if (!author.has_value() || !author.value().person.isAdmin()
        || author.value().person.member() == target || !roster.contains(target))
    {
        return std::nullopt;
    }

    AccessRoster next = roster;
    next.removePerson(target);

    return buildChange(header,
                       next,
                       DocumentCrypto::generateDataKey(),
                       header.keyGeneration + 1,
                       author.value().entry);
}

std::optional<AccessChange> AccessControl::setRole(const DocumentHeader& header,
                                                   const AccessRoster& roster,
                                                   const DataKey& dataKey,
                                                   const Fingerprint& writer,
                                                   const MemberId& person,
                                                   RosterPerson::Role role)
{
    const std::optional<Writer> author = writerOf(header, roster, writer);
    if (!author.has_value() || !author.value().person.isAdmin() || !dataKey.isValid())
    {
        return std::nullopt;
    }

    AccessRoster next = roster;
    if (!next.setRole(person, role) || next.admins().isEmpty())
    {
        return std::nullopt;
    }

    return buildChange(header, next, dataKey, header.keyGeneration, author.value().entry);
}

std::optional<AccessChange> AccessControl::leave(const DocumentHeader& header,
                                                 const AccessRoster& roster,
                                                 const DataKey& dataKey,
                                                 const Fingerprint& writer)
{
    const std::optional<Writer> author = writerOf(header, roster, writer);
    if (!author.has_value() || !dataKey.isValid())
    {
        return std::nullopt;
    }

    // A ward with people and no admin could never grant or remove anyone again, so
    // the last admin names a successor first. The last person of all may always go.
    AccessRoster next = roster;
    next.removePerson(author.value().person.member());
    if (!next.isEmpty() && next.admins().isEmpty())
    {
        return std::nullopt;
    }

    return buildChange(header, next, dataKey, header.keyGeneration, author.value().entry);
}

std::optional<DocumentHeader> AccessControl::resealAll(const DocumentHeader& header,
                                                       const AccessRoster& roster,
                                                       const DataKey& dataKey)
{
    if (!dataKey.isValid())
    {
        return std::nullopt;
    }

    DocumentHeader next = header;
    next.recipients.clear();
    next.admins = roster.admins();

    // people() and devices() are both sorted, so the same roster always produces
    // its entries in the same order.
    for (const RosterPerson& person : roster.people())
    {
        for (const RosterDevice& device : person.devices())
        {
            RecipientEntry entry;
            entry.member = person.member();
            entry.fingerprint = device.fingerprint();
            entry.publicKey = device.publicKey();
            entry.verifyKey = device.verifyKey();
            entry.sealedKey = DocumentCrypto::sealDataKey(dataKey, device.publicKey());

            if (entry.sealedKey.isEmpty())
            {
                return std::nullopt;
            }

            next.recipients.append(entry);
        }
    }

    return next;
}

bool AccessControl::headerMatchesRoster(const DocumentHeader& header, const AccessRoster& roster)
{
    if (header.admins != roster.admins())
    {
        return false;
    }

    QHash<QString, RecipientEntry> entries;
    for (const RecipientEntry& entry : header.recipients)
    {
        if (!entry.fingerprint.isValid())
        {
            return false;
        }

        const QString key = entry.fingerprint.toHex();
        if (entries.contains(key))
        {
            return false;
        }
        entries.insert(key, entry);
    }

    qsizetype deviceCount = 0;
    for (const RosterPerson& person : roster.people())
    {
        for (const RosterDevice& device : person.devices())
        {
            ++deviceCount;

            const auto entry = entries.constFind(device.fingerprint().toHex());
            if (entry == entries.constEnd())
            {
                return false;
            }

            if (entry.value().member != person.member()
                || entry.value().publicKey != device.publicKey()
                || entry.value().verifyKey != device.verifyKey())
            {
                return false;
            }
        }
    }

    // Every roster device found its entry; equal counts means no entry is left
    // over without a device behind it.
    return deviceCount == entries.size();
}
