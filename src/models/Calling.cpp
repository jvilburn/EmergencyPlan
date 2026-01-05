#include "Calling.h"

bool Calling::isCalling(const QString& text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    for (const QChar& ch : trimmed)
    {
        if (!ch.isLetter() && !ch.isSpace())
        {
            return false;
        }
    }
    return true;
}
