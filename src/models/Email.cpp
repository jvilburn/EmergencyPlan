#include "Email.h"

bool Email::isEmail(const QString& text)
{
    return text.contains('@');
}
