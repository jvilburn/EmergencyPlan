#include "Gender.h"

const Gender Gender::Male{0};
const Gender Gender::Female{1};

bool Gender::isGender(const QString& text)
{
    QString trimmed = text.trimmed();
    return trimmed == "Male" || trimmed == "Female";
}

QString Gender::toString() const
{
    if (m_value == 0)
    {
        return "male";
    }
    return "female";
}

std::optional<Gender> Gender::fromString(const QString& str)
{
    if (str.compare("male", Qt::CaseInsensitive) == 0)
    {
        return Male;
    }
    if (str.compare("female", Qt::CaseInsensitive) == 0)
    {
        return Female;
    }
    return std::nullopt;
}

QJsonValue Gender::toJson() const
{
    return QJsonValue(toString());
}

std::optional<Gender> Gender::fromJson(const QJsonValue& value)
{
    if (value.isNull() || !value.isString())
    {
        return std::nullopt;
    }
    return fromString(value.toString());
}
