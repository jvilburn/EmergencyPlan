#include "IdentityService.h"

#include <QObject>
#include <QSysInfo>

std::unique_ptr<IdentityService> IdentityService::open(const QString& path, QString* errorMessage)
{
    IdentityRecord record;
    switch (IdentityFile::load(path, record, errorMessage))
    {
    case IdentityFile::Load::Loaded:
        break;
    case IdentityFile::Load::NotFound:
        record.sealing = DocumentCrypto::generateKeypair();
        record.signing = DocumentCrypto::generateSigningKeypair();
        if (!IdentityFile::store(path, record, errorMessage))
        {
            return nullptr;
        }
        break;
    case IdentityFile::Load::Failed:
        return nullptr;
    }

    return std::unique_ptr<IdentityService>(new IdentityService(path, record));
}

IdentityService::IdentityService(const QString& path, const IdentityRecord& record)
    : m_path(path)
    , m_record(record)
{
}

std::optional<DeviceIdentity> IdentityService::identity() const
{
    if (!m_record.person.has_value())
    {
        return std::nullopt;
    }

    DeviceIdentity identity;
    identity.sealing = m_record.sealing;
    identity.signing = m_record.signing;
    identity.member = m_record.person.value().member;
    identity.displayName = m_record.person.value().displayName;
    identity.deviceName = m_record.person.value().deviceName;
    return identity;
}

DeviceKeys IdentityService::deviceKeys() const
{
    return DeviceKeys{m_record.sealing.publicKey, m_record.signing.verifyKey};
}

QString IdentityService::defaultDeviceName()
{
    return QSysInfo::machineHostName();
}

bool IdentityService::createPerson(const QString& displayName, const QString& deviceName, QString* errorMessage)
{
    MemberId member;
    member.bytes = DocumentCrypto::randomBytes(MemberId::Size);
    return setPerson(IdentityRecord::Person{member, displayName.trimmed(), deviceName.trimmed()}, errorMessage);
}

bool IdentityService::joinPerson(const MemberId& member,
                                 const QString& displayName,
                                 const QString& deviceName,
                                 QString* errorMessage)
{
    return setPerson(IdentityRecord::Person{member, displayName.trimmed(), deviceName.trimmed()}, errorMessage);
}

bool IdentityService::setPerson(const IdentityRecord::Person& person, QString* errorMessage)
{
    QString problem;
    if (m_record.person.has_value())
    {
        problem = QObject::tr("This device already belongs to a person.");
    }
    else if (!person.member.isValid() || person.displayName.isEmpty() || person.deviceName.isEmpty())
    {
        problem = QObject::tr("A name for you and for this device are both needed.");
    }

    if (!problem.isEmpty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = problem;
        }
        return false;
    }

    // Stored before it is taken, so the person in memory is always the one on disk.
    IdentityRecord record = m_record;
    record.person = person;
    if (!IdentityFile::store(m_path, record, errorMessage))
    {
        return false;
    }

    m_record = record;
    return true;
}
