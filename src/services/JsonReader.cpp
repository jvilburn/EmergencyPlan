#include "JsonReader.h"
#include "Base64Url.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <cmath>

namespace
{

/// 2^53: every integer up to this size is exact in a double.
constexpr double LargestExactInteger = 9007199254740992.0;

QString elementOf(const QString& key, int index)
{
    return QString("%1[%2]").arg(key).arg(index);
}

}  // namespace

JsonReader::JsonReader(const QJsonObject& object)
    : JsonReader(object, QString(), std::make_shared<Status>())
{
}

JsonReader::JsonReader(const QJsonObject& object, const QString& path, const std::shared_ptr<Status>& status)
    : m_object(object)
    , m_path(path)
    , m_status(status)
{
}

std::optional<QJsonObject> JsonReader::parseObject(const QByteArray& bytes)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }
    return document.object();
}

bool JsonReader::isInteger(const QJsonValue& value)
{
    if (!value.isDouble())
    {
        return false;
    }
    double number = value.toDouble();
    return std::floor(number) == number
        && std::fabs(number) <= LargestExactInteger;
}

QString JsonReader::string(const QString& key)
{
    QJsonValue value = take(key);
    if (!value.isUndefined() && !value.isString())
    {
        fail(key, "is not a string");
    }
    return value.toString();
}

QString JsonReader::optionalString(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return QString();
    }
    if (!value.isString())
    {
        fail(key, "is not a string");
        return QString();
    }
    if (value.toString().isEmpty())
    {
        fail(key, "is written out empty");
    }
    return value.toString();
}

qint64 JsonReader::integer(const QString& key)
{
    QJsonValue value = take(key);
    if (value.isUndefined())
    {
        return 0;
    }
    if (!isInteger(value))
    {
        fail(key, "is not an integer");
        return 0;
    }
    return static_cast<qint64>(value.toDouble());
}

std::optional<qint64> JsonReader::optionalInteger(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return std::nullopt;
    }
    if (!isInteger(value))
    {
        fail(key, "is not an integer");
        return std::nullopt;
    }
    return static_cast<qint64>(value.toDouble());
}

double JsonReader::number(const QString& key)
{
    QJsonValue value = take(key);
    if (!value.isUndefined() && !value.isDouble())
    {
        fail(key, "is not a number");
    }
    return value.toDouble();
}

std::optional<double> JsonReader::optionalNumber(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return std::nullopt;
    }
    if (!value.isDouble())
    {
        fail(key, "is not a number");
        return std::nullopt;
    }
    return value.toDouble();
}

bool JsonReader::boolean(const QString& key)
{
    QJsonValue value = take(key);
    if (!value.isUndefined() && !value.isBool())
    {
        fail(key, "is not a boolean");
    }
    return value.toBool();
}

bool JsonReader::optionalFlag(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return false;
    }
    if (!value.isBool())
    {
        fail(key, "is not a boolean");
        return false;
    }
    if (!value.toBool())
    {
        fail(key, "is written out empty");
    }
    return value.toBool();
}

bool JsonReader::readNull(const QString& key)
{
    if (!m_object.value(key).isNull())
    {
        return false;
    }
    m_taken.insert(key);
    return true;
}

QString JsonReader::word(const QString& key, const QStringList& words)
{
    return checkedWord(key, string(key), words);
}

QString JsonReader::optionalWord(const QString& key, const QStringList& words)
{
    QString text = optionalString(key);
    if (text.isEmpty())
    {
        return QString();
    }
    return checkedWord(key, text, words);
}

QDate JsonReader::date(const QString& key)
{
    QString text = string(key);
    QDate date = QDate::fromString(text, Qt::ISODate);
    if (!date.isValid() || date.toString(Qt::ISODate) != text)
    {
        fail(key, "is not a date");
        return QDate();
    }
    return date;
}

std::optional<QDate> JsonReader::optionalDate(const QString& key)
{
    if (!m_object.contains(key))
    {
        m_taken.insert(key);
        return std::nullopt;
    }
    return date(key);
}

QDateTime JsonReader::time(const QString& key)
{
    QString text = string(key);
    QDateTime time = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!time.isValid() || time.toString(Qt::ISODateWithMs) != text)
    {
        fail(key, "is not a time");
        return QDateTime();
    }
    return time;
}

QByteArray JsonReader::bytes(const QString& key, int length)
{
    QJsonValue value = take(key);
    if (value.isUndefined())
    {
        return QByteArray();
    }
    std::optional<QByteArray> decoded;
    if (value.isString())
    {
        decoded = Base64Url::decode(value.toString(), length);
    }
    if (!decoded.has_value())
    {
        fail(key, QString("is not base64url of %1 bytes").arg(length));
        return QByteArray();
    }
    return *decoded;
}

JsonReader JsonReader::object(const QString& key)
{
    QJsonValue value = take(key);
    if (!value.isUndefined() && !value.isObject())
    {
        fail(key, "is not an object");
    }
    return JsonReader(value.toObject(), pathOf(key), m_status);
}

std::optional<JsonReader> JsonReader::optionalObject(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return std::nullopt;
    }
    if (!value.isObject())
    {
        fail(key, "is not an object");
        return std::nullopt;
    }
    if (value.toObject().isEmpty())
    {
        fail(key, "is written out empty");
        return std::nullopt;
    }
    return JsonReader(value.toObject(), pathOf(key), m_status);
}

QList<JsonReader> JsonReader::objects(const QString& key)
{
    QJsonValue value = take(key);
    if (value.isUndefined())
    {
        return {};
    }
    if (!value.isArray())
    {
        fail(key, "is not an array");
        return {};
    }
    return readObjects(key, value.toArray());
}

QList<JsonReader> JsonReader::optionalObjects(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return {};
    }
    if (!value.isArray())
    {
        fail(key, "is not an array");
        return {};
    }
    if (value.toArray().isEmpty())
    {
        fail(key, "is written out empty");
        return {};
    }
    return readObjects(key, value.toArray());
}

QStringList JsonReader::strings(const QString& key)
{
    QJsonValue value = take(key);
    if (value.isUndefined())
    {
        return {};
    }
    if (!value.isArray())
    {
        fail(key, "is not an array");
        return {};
    }
    return readStrings(key, value.toArray());
}

QStringList JsonReader::optionalStrings(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        return {};
    }
    if (!value.isArray())
    {
        fail(key, "is not an array");
        return {};
    }
    if (value.toArray().isEmpty())
    {
        fail(key, "is written out empty");
        return {};
    }
    return readStrings(key, value.toArray());
}

QList<QByteArray> JsonReader::optionalBytesList(const QString& key, int length)
{
    QStringList texts = optionalStrings(key);
    QList<QByteArray> values;
    for (int i = 0; i < texts.size(); ++i)
    {
        std::optional<QByteArray> decoded = Base64Url::decode(texts[i], length);
        if (!decoded.has_value())
        {
            fail(elementOf(key, i), QString("is not base64url of %1 bytes").arg(length));
            return {};
        }
        values.append(*decoded);
    }
    return values;
}

void JsonReader::fail(const QString& key, const QString& what)
{
    if (m_status->problem.isEmpty())
    {
        m_status->problem = pathOf(key) + ": " + what;
    }
}

void JsonReader::finish()
{
    const QStringList keys = m_object.keys();
    for (const QString& key : keys)
    {
        if (!m_taken.contains(key))
        {
            fail(key, "is not expected");
            return;
        }
    }
}

QJsonValue JsonReader::take(const QString& key)
{
    QJsonValue value = takeOptional(key);
    if (value.isUndefined())
    {
        fail(key, "missing");
    }
    return value;
}

QJsonValue JsonReader::takeOptional(const QString& key)
{
    m_taken.insert(key);
    return m_object.value(key);
}

QString JsonReader::pathOf(const QString& key) const
{
    if (m_path.isEmpty())
    {
        return key;
    }
    return m_path + "." + key;
}

QList<JsonReader> JsonReader::readObjects(const QString& key, const QJsonArray& array)
{
    QList<JsonReader> readers;
    for (int i = 0; i < array.size(); ++i)
    {
        if (!array[i].isObject())
        {
            fail(elementOf(key, i), "is not an object");
            return {};
        }
        readers.append(JsonReader(array[i].toObject(), pathOf(elementOf(key, i)), m_status));
    }
    return readers;
}

QStringList JsonReader::readStrings(const QString& key, const QJsonArray& array)
{
    QStringList strings;
    for (int i = 0; i < array.size(); ++i)
    {
        if (!array[i].isString())
        {
            fail(elementOf(key, i), "is not a string");
            return {};
        }
        strings.append(array[i].toString());
    }
    return strings;
}

QString JsonReader::checkedWord(const QString& key, const QString& text, const QStringList& words)
{
    if (!words.contains(text))
    {
        fail(key, "is not one of " + words.join(", "));
        return QString();
    }
    return text;
}

QString JsonReader::idText(const QString& key, const QString& text)
{
    if (text.isEmpty())
    {
        fail(key, "is an empty id");
    }
    return text;
}

QStringList JsonReader::idTexts(const QString& key, const QStringList& texts)
{
    for (int i = 0; i < texts.size(); ++i)
    {
        if (texts[i].isEmpty())
        {
            fail(elementOf(key, i), "is an empty id");
            return {};
        }
    }
    return texts;
}
