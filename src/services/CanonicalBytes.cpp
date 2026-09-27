#include "CanonicalBytes.h"

CanonicalBytes& CanonicalBytes::append(const QByteArray& value)
{
    const quint32 length = quint32(value.size());
    m_bytes.append(char((length >> 24) & 0xff));
    m_bytes.append(char((length >> 16) & 0xff));
    m_bytes.append(char((length >> 8) & 0xff));
    m_bytes.append(char(length & 0xff));
    m_bytes.append(value);
    return *this;
}

CanonicalBytes& CanonicalBytes::append(const QString& text)
{
    return append(text.toUtf8());
}

CanonicalBytes& CanonicalBytes::append(qint64 number)
{
    return append(QByteArray::number(number));
}
