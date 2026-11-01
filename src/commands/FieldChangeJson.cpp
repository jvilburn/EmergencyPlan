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

QJsonValue toJsonValue(const std::optional<TaskAssignment>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toJson();
}

QJsonValue toJsonValue(const std::optional<TaskResolution>& value)
{
    if (!value.has_value())
    {
        return QJsonValue(QJsonValue::Null);
    }
    return value->toJson();
}

template<>
QString fromJsonValue<QString>(const QJsonValue& value)
{
    return value.toString();
}

template<>
Name fromJsonValue<Name>(const QJsonValue& value)
{
    return Name(value.toString());
}

template<>
Phone fromJsonValue<Phone>(const QJsonValue& value)
{
    return Phone(value.toString());
}

template<>
bool fromJsonValue<bool>(const QJsonValue& value)
{
    return value.toBool();
}

template<>
QStringList fromJsonValue<QStringList>(const QJsonValue& value)
{
    QStringList strings;
    QJsonArray array = value.toArray();
    for (const QJsonValue& item : array)
    {
        strings.append(item.toString());
    }
    return strings;
}

template<>
Address fromJsonValue<Address>(const QJsonValue& value)
{
    Address address;
    QJsonArray lines = value.toArray();
    for (const QJsonValue& line : lines)
    {
        address.addLine(line.toString());
    }
    return address;
}

template<>
Birthday fromJsonValue<Birthday>(const QJsonValue& value)
{
    return Birthday::fromJson(value.toObject());
}

template<>
std::optional<Gender> fromJsonValue<std::optional<Gender>>(const QJsonValue& value)
{
    return Gender::fromJson(value);
}

template<>
std::optional<PersonId> fromJsonValue<std::optional<PersonId>>(const QJsonValue& value)
{
    if (!value.isString())
    {
        return std::nullopt;
    }
    return PersonId::fromString(value.toString());
}

template<>
std::optional<TaskAssignment> fromJsonValue<std::optional<TaskAssignment>>(const QJsonValue& value)
{
    if (!value.isObject())
    {
        return std::nullopt;
    }
    return TaskAssignment::fromJson(value.toObject());
}

template<>
std::optional<TaskResolution> fromJsonValue<std::optional<TaskResolution>>(const QJsonValue& value)
{
    if (!value.isObject())
    {
        return std::nullopt;
    }
    return TaskResolution::fromJson(value.toObject());
}

namespace
{

QSet<PersonId> idsFromJson(const QJsonValue& value)
{
    QSet<PersonId> ids;
    QJsonArray array = value.toArray();
    for (const QJsonValue& item : array)
    {
        ids.insert(PersonId::fromString(item.toString()));
    }
    return ids;
}

}  // namespace

QJsonObject peopleChangeToJson(const IdSetChange<PersonId>& change)
{
    QJsonObject json;
    json["added"] = sortedIdArray(change.added());
    json["removed"] = sortedIdArray(change.removed());
    return json;
}

IdSetChange<PersonId> peopleChangeFromJson(const QJsonObject& json)
{
    return IdSetChange<PersonId>(idsFromJson(json["added"]), idsFromJson(json["removed"]));
}
