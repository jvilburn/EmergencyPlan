#include "Command.h"

#include <QJsonArray>
#include <QObject>

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
