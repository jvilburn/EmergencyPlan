#include "Command.h"

#include <QJsonArray>
#include <QObject>

Description::Description(const QString& heading, const QStringList& lines)
    : m_heading(heading)
    , m_lines(lines)
{
}

Description Description::undone() const
{
    return Description(QObject::tr("Undo: %1").arg(m_heading), m_lines);
}

QJsonObject Description::toJson() const
{
    QJsonObject json;
    json["heading"] = m_heading;
    if (!m_lines.isEmpty())
    {
        json["lines"] = QJsonArray::fromStringList(m_lines);
    }
    return json;
}

Description Description::fromJson(const QJsonObject& json)
{
    QStringList lines;
    QJsonArray linesArray = json["lines"].toArray();
    for (const QJsonValue& value : linesArray)
    {
        lines.append(value.toString());
    }
    return Description(json["heading"].toString(), lines);
}

bool Description::operator==(const Description& other) const
{
    return m_heading == other.m_heading
        && m_lines == other.m_lines;
}

Command::Command(const Description& description)
    : m_description(description)
{
}

QString Command::refusal(const Document& document) const
{
    Q_UNUSED(document);
    return QString();
}
