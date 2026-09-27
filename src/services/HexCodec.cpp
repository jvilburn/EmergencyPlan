#include "HexCodec.h"

namespace HexCodec
{

QString encode(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toHex());
}

bool isCanonical(const QString& text, int characters)
{
    if (text.size() != characters)
    {
        return false;
    }

    for (const QChar character : text)
    {
        if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9'))
              || (character >= QLatin1Char('a') && character <= QLatin1Char('f'))))
        {
            return false;
        }
    }

    return true;
}

QByteArray decode(const QString& text, int expectedSize)
{
    if (!isCanonical(text, expectedSize * 2))
    {
        return QByteArray();
    }

    const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
    if (bytes.size() != expectedSize)
    {
        return QByteArray();
    }

    return bytes;
}

} // namespace HexCodec
