#include "Address.h"

void Address::addLine(const QString& line)
{
    QString sanitized = sanitizeLine(line);
    if (!sanitized.isEmpty())
    {
        m_lines.append(sanitized);
    }
}

QString Address::full() const
{
    return m_lines.join(", ");
}

QString Address::multiLine() const
{
    return m_lines.join('\n');
}

bool Address::isEmpty() const
{
    return m_lines.isEmpty();
}

int Address::size() const
{
    return m_lines.size();
}

const QStringList& Address::lines() const
{
    return m_lines;
}

bool Address::operator==(const Address& other) const
{
    return m_lines == other.m_lines;
}

bool Address::operator!=(const Address& other) const
{
    return m_lines != other.m_lines;
}

QString Address::sanitizeLine(const QString& line)
{
    QString result;
    result.reserve(line.size());
    for (const QChar& ch : line)
    {
        if (ch.isPrint())
        {
            result.append(ch);
        }
        else if (ch.isSpace())
        {
            result.append(' ');
        }
    }
    return result.simplified();
}
