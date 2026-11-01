#include "Command.h"
#include "Document.h"

#include <QJsonArray>
#include <QObject>

namespace
{

/// The names of the people with those ids, in name order.
QStringList namesOf(const QSet<PersonId>& ids, const Document& document)
{
    QStringList names;
    for (const PersonId& id : ids)
    {
        names.append(personName(id, document));
    }
    names.sort();
    return names;
}

}  // namespace

QString personName(const PersonId& id, const Document& document)
{
    std::optional<Person> person = document.findPersonById(id);
    return person.has_value() ? person->displayName() : QObject::tr("(removed)");
}

QStringList describePeopleChange(const IdSetChange<PersonId>& change, const Document& document)
{
    QStringList lines;
    for (const QString& name : namesOf(change.added(), document))
    {
        lines.append(QObject::tr("added: %1").arg(name));
    }
    for (const QString& name : namesOf(change.removed(), document))
    {
        lines.append(QObject::tr("removed: %1").arg(name));
    }
    return lines;
}

bool everyPersonExists(const QSet<PersonId>& ids, const Document& document)
{
    for (const PersonId& id : ids)
    {
        if (!document.findPersonById(id).has_value())
        {
            return false;
        }
    }
    return true;
}

CommandDescription::CommandDescription(const QString& heading, const QStringList& lines)
    : m_heading(heading)
    , m_lines(lines)
{
}

CommandDescription CommandDescription::undone() const
{
    return CommandDescription(QObject::tr("Undo: %1").arg(m_heading), m_lines);
}

QJsonObject CommandDescription::toJson() const
{
    QJsonObject json;
    json["heading"] = m_heading;
    if (!m_lines.isEmpty())
    {
        json["lines"] = QJsonArray::fromStringList(m_lines);
    }
    return json;
}

CommandDescription CommandDescription::fromJson(const QJsonObject& json)
{
    QStringList lines;
    QJsonArray linesArray = json["lines"].toArray();
    for (const QJsonValue& value : linesArray)
    {
        lines.append(value.toString());
    }
    return CommandDescription(json["heading"].toString(), lines);
}

bool CommandDescription::operator==(const CommandDescription& other) const
{
    return m_heading == other.m_heading
        && m_lines == other.m_lines;
}

Command::Command(const CommandDescription& description)
    : m_description(description)
{
}

QString Command::refusal(const Document& document) const
{
    Q_UNUSED(document);
    return QString();
}

QList<Address> Command::addressesGiven() const
{
    return {};
}

QStringList Command::unitsGiven() const
{
    return {};
}

UndoableCommand::UndoableCommand(const CommandDescription& description)
    : Command(description)
{
}
