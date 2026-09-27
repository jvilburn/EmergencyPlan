#include "StrictJson.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace StrictJson
{

bool hasOnlyKeys(const QJsonObject& json, std::initializer_list<QLatin1String> allowed)
{
    for (auto key = json.constBegin(); key != json.constEnd(); ++key)
    {
        if (std::find(allowed.begin(), allowed.end(), key.key()) == allowed.end())
        {
            return false;
        }
    }
    return true;
}

QString writeTime(const QDateTime& time)
{
    return time.toUTC().toString(Qt::ISODateWithMs);
}

QString writeDate(const QDate& date)
{
    return date.toString(Qt::ISODate);
}

bool readString(const QJsonObject& json, QLatin1String key, QString& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isString())
    {
        return false;
    }

    out = value.toString();
    return true;
}

bool readNonEmptyString(const QJsonObject& json, QLatin1String key, QString& out)
{
    QString text;
    if (!readString(json, key, text) || text.isEmpty())
    {
        return false;
    }

    out = text;
    return true;
}

bool readBool(const QJsonObject& json, QLatin1String key, bool& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isBool())
    {
        return false;
    }

    out = value.toBool();
    return true;
}

bool readInt(const QJsonObject& json, QLatin1String key, int& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number)
        || number < double(std::numeric_limits<int>::min())
        || number > double(std::numeric_limits<int>::max())
        || number != std::trunc(number))
    {
        return false;
    }

    out = int(number);
    return true;
}

bool readInt64(const QJsonObject& json, QLatin1String key, qint64& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    // Both bounds are exact as doubles: -2^63 is representable, and 2^63 is the
    // first value above the range, so the comparison is >= rather than >. The
    // largest qint64 itself rounds up to 2^63 and is refused, which no field read
    // this way comes near.
    const double lowest = -9223372036854775808.0;
    const double aboveHighest = 9223372036854775808.0;

    const double number = value.toDouble();
    if (!std::isfinite(number) || number < lowest || number >= aboveHighest
        || number != std::trunc(number))
    {
        return false;
    }

    out = value.toInteger();
    return true;
}

bool readDouble(const QJsonObject& json, QLatin1String key, double& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble()))
    {
        return false;
    }

    out = value.toDouble();
    return true;
}

bool readTime(const QJsonObject& json, QLatin1String key, QDateTime& out)
{
    QString text;
    if (!readString(json, key, text))
    {
        return false;
    }

    const QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid() || writeTime(parsed) != text)
    {
        return false;
    }

    out = parsed.toUTC();
    return true;
}

bool readDate(const QJsonObject& json, QLatin1String key, QDate& out)
{
    QString text;
    if (!readString(json, key, text))
    {
        return false;
    }

    const QDate parsed = QDate::fromString(text, Qt::ISODate);
    if (!parsed.isValid() || writeDate(parsed) != text)
    {
        return false;
    }

    out = parsed;
    return true;
}

bool readOptionalNonEmptyString(const QJsonObject& json, QLatin1String key, QString& out)
{
    return !json.contains(key) || readNonEmptyString(json, key, out);
}

bool readOptionalString(const QJsonObject& json, QLatin1String key, std::optional<QString>& out)
{
    if (!json.contains(key))
    {
        return true;
    }

    QString text;
    if (!readString(json, key, text))
    {
        return false;
    }
    out = text;
    return true;
}

bool readOptionalInt(const QJsonObject& json, QLatin1String key, std::optional<int>& out)
{
    if (!json.contains(key))
    {
        return true;
    }

    int value = 0;
    if (!readInt(json, key, value))
    {
        return false;
    }
    out = value;
    return true;
}

bool readOptionalDouble(const QJsonObject& json, QLatin1String key, std::optional<double>& out)
{
    if (!json.contains(key))
    {
        return true;
    }

    double value = 0.0;
    if (!readDouble(json, key, value))
    {
        return false;
    }
    out = value;
    return true;
}

bool readObject(const QJsonObject& json, QLatin1String key, QJsonObject& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isObject())
    {
        return false;
    }

    out = value.toObject();
    return true;
}

bool readObjectArray(const QJsonObject& json, QLatin1String key, QList<QJsonObject>& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isArray())
    {
        return false;
    }

    QList<QJsonObject> objects;
    for (const QJsonValue& element : value.toArray())
    {
        if (!element.isObject())
        {
            return false;
        }
        objects.append(element.toObject());
    }

    out = objects;
    return true;
}

bool readStringArray(const QJsonObject& json, QLatin1String key, QStringList& out)
{
    const QJsonValue value = json.value(key);
    if (!value.isArray())
    {
        return false;
    }

    QStringList texts;
    for (const QJsonValue& element : value.toArray())
    {
        if (!element.isString())
        {
            return false;
        }
        texts.append(element.toString());
    }

    out = texts;
    return true;
}

} // namespace StrictJson
