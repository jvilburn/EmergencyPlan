#pragma once

#include "Command.h"
#include "Id.h"

/// A test command with no inverse that sets a member's phone whatever it holds, refused
/// with ruleWording when that is not empty. It stands in for a command with a rule, which
/// no content command has, and for a write the undo stack does not hold. It is not in
/// CommandRegistry, so it never takes part in a replay.
class ForcePhoneCommand : public Command
{
public:
    ForcePhoneCommand(const PersonId& personId, const QString& phone, const QString& ruleWording);

    QString type() const override { return "testForcePhone"; }
    CommandOutcome execute(Document& document) const override;
    CommandPtr inverse() const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

private:
    PersonId m_personId;
    QString m_phone;
    QString m_ruleWording;
};
