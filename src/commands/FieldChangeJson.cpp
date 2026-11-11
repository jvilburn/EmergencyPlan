#include "FieldChangeJson.h"

#include <QJsonArray>

QJsonValue toJsonValue(const QString& value)
{
    return QJsonValue(value);
}

QJsonValue toJsonValue(bool value)
{
    return QJsonValue(value);
}

QJsonValue toJsonValue(const QStringList& value)
{
    return QJsonArray::fromStringList(value);
}

QJsonValue toJsonValue(const Address& value)
{
    return QJsonArray::fromStringList(value.lines());
}

QJsonValue toJsonValue(const Birthday& value)
{
    return value.toJson();
}

QJsonValue toJsonValue(const std::optional<Gender>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toJson();
}

QJsonValue toJsonValue(const std::optional<PersonId>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toString();
}

QJsonValue toJsonValue(const std::optional<QDate>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toString(Qt::ISODate);
}

template<>
QString readValue<QString>(JsonReader& json, const QString& key)
{
    return json.string(key);
}

template<>
Name readValue<Name>(JsonReader& json, const QString& key)
{
    return Name(json.string(key));
}

template<>
Phone readValue<Phone>(JsonReader& json, const QString& key)
{
    return Phone(json.string(key));
}

template<>
bool readValue<bool>(JsonReader& json, const QString& key)
{
    return json.boolean(key);
}

template<>
QStringList readValue<QStringList>(JsonReader& json, const QString& key)
{
    return json.strings(key);
}

template<>
Address readValue<Address>(JsonReader& json, const QString& key)
{
    return Address::fromLines(json.strings(key));
}

template<>
Birthday readValue<Birthday>(JsonReader& json, const QString& key)
{
    return Birthday::fromJson(json.object(key));
}

template<>
std::optional<Gender> readValue<std::optional<Gender>>(JsonReader& json, const QString& key)
{
    if (json.readNull(key))
    {
        return std::nullopt;
    }
    return json.word(key, Gender::values(), &Gender::toString);
}

template<>
std::optional<PersonId> readValue<std::optional<PersonId>>(JsonReader& json, const QString& key)
{
    if (json.readNull(key))
    {
        return std::nullopt;
    }
    return json.id<PersonId>(key);
}

template<>
std::optional<QDate> readValue<std::optional<QDate>>(JsonReader& json, const QString& key)
{
    if (json.readNull(key))
    {
        return std::nullopt;
    }
    return json.date(key);
}

QJsonValue toJsonValue(AccessRole value)
{
    return accessRoleName(value);
}

template<>
AccessRole readValue<AccessRole>(JsonReader& json, const QString& key)
{
    return readAccessRole(json, key);
}

QJsonObject peopleChangeToJson(const IdSetChange<PersonId>& change)
{
    QJsonObject json;
    json["added"] = sortedIdArray(change.added());
    json["removed"] = sortedIdArray(change.removed());
    return json;
}

IdSetChange<PersonId> peopleChangeFromJson(JsonReader json)
{
    QSet<PersonId> added = json.ids<PersonId>("added");
    QSet<PersonId> removed = json.ids<PersonId>("removed");
    json.finish();
    return IdSetChange<PersonId>(added, removed);
}
