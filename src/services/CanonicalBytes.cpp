#include "CanonicalBytes.h"

CanonicalBytes& CanonicalBytes::appendBytes(const QByteArray& value)
{
    const quint32 length = quint32(value.size());
    m_bytes.append(char((length >> 24) & 0xff));
    m_bytes.append(char((length >> 16) & 0xff));
    m_bytes.append(char((length >> 8) & 0xff));
    m_bytes.append(char(length & 0xff));
    m_bytes.append(value);
    return *this;
}

CanonicalBytes& CanonicalBytes::appendText(const QString& text)
{
    return appendBytes(text.toUtf8());
}

CanonicalBytes& CanonicalBytes::appendNumber(qint64 number)
{
    return appendBytes(QByteArray::number(number));
}
