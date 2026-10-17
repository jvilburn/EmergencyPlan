#pragma once

#include "Address.h"
#include "Birthday.h"
#include "FieldChange.h"
#include "Gender.h"
#include "Name.h"
#include "Phone.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <optional>

/// JSON for the values commands change. Each value reads back through the reader for its
/// own type, so a change's before and after round-trip exactly.
QJsonValue toJsonValue(const QString& value);
QJsonValue toJsonValue(bool value);
QJsonValue toJsonValue(const QStringList& value);
QJsonValue toJsonValue(const Address& value);
QJsonValue toJsonValue(const Birthday& value);
QJsonValue toJsonValue(const std::optional<Gender>& value);

template<typename T>
T fromJsonValue(const QJsonValue& value);

template<>
QString fromJsonValue<QString>(const QJsonValue& value);
template<>
Name fromJsonValue<Name>(const QJsonValue& value);
template<>
Phone fromJsonValue<Phone>(const QJsonValue& value);
template<>
bool fromJsonValue<bool>(const QJsonValue& value);
template<>
QStringList fromJsonValue<QStringList>(const QJsonValue& value);
template<>
Address fromJsonValue<Address>(const QJsonValue& value);
template<>
Birthday fromJsonValue<Birthday>(const QJsonValue& value);
template<>
std::optional<Gender> fromJsonValue<std::optional<Gender>>(const QJsonValue& value);

template<typename T>
QJsonObject changeToJson(const FieldChange<T>& change)
{
    QJsonObject json;
    json["before"] = toJsonValue(change.before());
    json["after"] = toJsonValue(change.after());
    return json;
}

template<typename T>
FieldChange<T> changeFromJson(const QJsonObject& json)
{
    return FieldChange<T>(fromJsonValue<T>(json["before"]), fromJsonValue<T>(json["after"]));
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
std::optional<FieldChange<T>> readChange(const QJsonObject& json, const QString& key)
{
    if (!json.contains(key))
    {
        return std::nullopt;
    }
    return changeFromJson<T>(json[key].toObject());
}
