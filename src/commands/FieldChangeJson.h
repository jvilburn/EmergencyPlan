#pragma once

#include "AccessList.h"
#include "Address.h"
#include "Birthday.h"
#include "FieldChange.h"
#include "Gender.h"
#include "Id.h"
#include "JsonReader.h"
#include "Name.h"
#include "Phone.h"

#include <QDate>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <optional>

/// JSON for the values commands change. Each value reads back through readValue for its own
/// type, so a change's before and after round-trip exactly. An optional value is written as
/// null when there is none.
QJsonValue toJsonValue(const QString& value);
QJsonValue toJsonValue(bool value);
QJsonValue toJsonValue(const QStringList& value);
QJsonValue toJsonValue(const Address& value);
QJsonValue toJsonValue(const Birthday& value);
QJsonValue toJsonValue(const std::optional<Gender>& value);
QJsonValue toJsonValue(const std::optional<PersonId>& value);
QJsonValue toJsonValue(const std::optional<QDate>& value);
QJsonValue toJsonValue(AccessRole value);

/// The value of type T under key, read strictly.
template<typename T>
T readValue(JsonReader& json, const QString& key);

template<>
QString readValue<QString>(JsonReader& json, const QString& key);
template<>
Name readValue<Name>(JsonReader& json, const QString& key);
template<>
Phone readValue<Phone>(JsonReader& json, const QString& key);
template<>
bool readValue<bool>(JsonReader& json, const QString& key);
template<>
QStringList readValue<QStringList>(JsonReader& json, const QString& key);
template<>
Address readValue<Address>(JsonReader& json, const QString& key);
template<>
Birthday readValue<Birthday>(JsonReader& json, const QString& key);
template<>
std::optional<Gender> readValue<std::optional<Gender>>(JsonReader& json, const QString& key);
template<>
std::optional<PersonId> readValue<std::optional<PersonId>>(JsonReader& json, const QString& key);
template<>
std::optional<QDate> readValue<std::optional<QDate>>(JsonReader& json, const QString& key);
template<>
AccessRole readValue<AccessRole>(JsonReader& json, const QString& key);

/// The ids added and the ids removed, each as a sorted array of id strings.
QJsonObject peopleChangeToJson(const IdSetChange<PersonId>& change);
IdSetChange<PersonId> peopleChangeFromJson(JsonReader json);

template<typename T>
QJsonObject changeToJson(const FieldChange<T>& change)
{
    QJsonObject json;
    json["before"] = toJsonValue(change.before());
    json["after"] = toJsonValue(change.after());
    return json;
}

template<typename T>
FieldChange<T> changeFromJson(JsonReader json)
{
    T before = readValue<T>(json, "before");
    T after = readValue<T>(json, "after");
    json.finish();
    return FieldChange<T>(before, after);
}

/// Writes the change under key, or nothing when there is no change.
template<typename T>
void writeChange(QJsonObject& json, const QString& key, const std::optional<FieldChange<T>>& change)
{
    if (change.has_value())
    {
        json[key] = changeToJson(*change);
    }
}

/// The change under key, or none when the key is absent.
template<typename T>
std::optional<FieldChange<T>> readChange(JsonReader& json, const QString& key)
{
    std::optional<JsonReader> change = json.optionalObject(key);
    if (!change.has_value())
    {
        return std::nullopt;
    }
    return changeFromJson<T>(*change);
}
