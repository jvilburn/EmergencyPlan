#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1String>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <initializer_list>
#include <optional>

/// Reading the document format strictly: a reader accepts an object only when it
/// understands all of it, and each value only in exactly the form the writer
/// produces.
///
/// Every read* returns false when the key is absent or its value is the wrong type
/// or form, leaving out unchanged. The readOptional* functions accept an absent key;
/// a field the writer omits when empty must not be empty when present, since the
/// writer never writes it so.
namespace StrictJson
{

/// True when every key in json is one of allowed. A field a newer writer added
/// without a version bump is refused rather than silently dropped.
bool hasOnlyKeys(const QJsonObject& json, std::initializer_list<QLatin1String> allowed);

/// UTC, with milliseconds. Qt::ISODate drops them, and a value that loses them on
/// the way through a file is no longer equal to itself.
QString writeTime(const QDateTime& time);

/// yyyy-MM-dd.
QString writeDate(const QDate& date);

bool readString(const QJsonObject& json, QLatin1String key, QString& out);

/// A string the writer never leaves empty: an id, or an optional field it omits
/// when empty.
bool readNonEmptyString(const QJsonObject& json, QLatin1String key, QString& out);

bool readBool(const QJsonObject& json, QLatin1String key, bool& out);

/// A whole number within int's range. The range is checked while the value is
/// still a double, because converting one outside it is undefined behaviour.
bool readInt(const QJsonObject& json, QLatin1String key, int& out);

bool readInt64(const QJsonObject& json, QLatin1String key, qint64& out);

/// Any finite number.
bool readDouble(const QJsonObject& json, QLatin1String key, double& out);

/// Exactly as writeTime writes it, so the value survives a round trip unchanged.
bool readTime(const QJsonObject& json, QLatin1String key, QDateTime& out);

/// Exactly as writeDate writes it.
bool readDate(const QJsonObject& json, QLatin1String key, QDate& out);

// --- Optional fields: true when the key is absent, or present and well formed ---

/// A string the writer omits when empty, so present means not empty.
bool readOptionalNonEmptyString(const QJsonObject& json, QLatin1String key, QString& out);

/// A string the writer writes whenever it is set, empty or not.
bool readOptionalString(const QJsonObject& json, QLatin1String key, std::optional<QString>& out);

bool readOptionalInt(const QJsonObject& json, QLatin1String key, std::optional<int>& out);
bool readOptionalDouble(const QJsonObject& json, QLatin1String key, std::optional<double>& out);

bool readObject(const QJsonObject& json, QLatin1String key, QJsonObject& out);

/// An array whose every element is an object.
bool readObjectArray(const QJsonObject& json, QLatin1String key, QList<QJsonObject>& out);

/// An array whose every element is a string.
bool readStringArray(const QJsonObject& json, QLatin1String key, QStringList& out);

/// An array of ids: non-empty strings, none repeated.
template<typename Id>
bool readIdSet(const QJsonObject& json, QLatin1String key, QSet<Id>& out)
{
    QStringList texts;
    if (!readStringArray(json, key, texts))
    {
        return false;
    }

    QSet<Id> ids;
    for (const QString& text : texts)
    {
        const Id id = Id::fromString(text);
        if (text.isEmpty() || ids.contains(id))
        {
            return false;
        }
        ids.insert(id);
    }

    out = ids;
    return true;
}

/// One id: a non-empty string.
template<typename Id>
bool readId(const QJsonObject& json, QLatin1String key, Id& out)
{
    QString text;
    if (!readNonEmptyString(json, key, text))
    {
        return false;
    }

    out = Id::fromString(text);
    return true;
}

} // namespace StrictJson
