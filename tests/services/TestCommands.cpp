#include "TestCommands.h"
#include "Document.h"
#include "TestWard.h"

ForcePhoneCommand::ForcePhoneCommand(const PersonId& personId, const QString& phone, const QString& ruleWording)
    : Command(CommandDescription("Force phone", QStringList()))
    , m_personId(personId)
    , m_phone(phone)
    , m_ruleWording(ruleWording)
{
}

CommandOutcome ForcePhoneCommand::execute(Document& document) const
{
    FamilyId familyId = document.familyIdForPerson(m_personId).value();
    Family family = TestWard::family(document, familyId);
    document.updateFamily(TestWard::withPhone(family, m_personId, m_phone));
    return CommandOutcome::Applied;
}

QString ForcePhoneCommand::refusal(const Document& document) const
{
    Q_UNUSED(document);
    return m_ruleWording;
}

QJsonObject ForcePhoneCommand::toJson() const
{
    QJsonObject json;
    json["personId"] = m_personId.toString();
    json["phone"] = m_phone;
    return json;
}

DocumentChange ForcePhoneCommand::documentChange() const
{
    return DocumentChange::full();
}
