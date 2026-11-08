#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>

/// Reads one JSON object exactly as its writer wrote it. Every field is taken by its type: a
/// field missing, of the wrong type or malformed fails the read, and finish() fails it for
/// any field left untaken. A field its writer leaves out when empty is taken with an
/// optional... call, which reads its absence as empty and fails on an empty value written
/// out, since the writer never writes one. A value its writer writes as null when there is
/// none is taken with readNull first.
///
/// Every fromJson that takes a JsonReader ends with finish(). A failure is shared with every
/// reader made from this one and the first one found is kept, with the path of the field it
/// names. A call that fails returns an empty value (for a word, the first of its values), so
/// whoever reads a whole tree asks failed() once, at the top.
class JsonReader
{
public:
    explicit JsonReader(const QJsonObject& object);

    /// The object bytes hold, or none when they are not one JSON object.
    static std::optional<QJsonObject> parseObject(const QByteArray& bytes);

    /// True for a JSON number with no fractional part, within the range a double holds exactly.
    static bool isInteger(const QJsonValue& value);

    bool failed() const { return !m_status->problem.isEmpty(); }
    const QString& problem() const { return m_status->problem; }

    /// The object as it was given, for a reader that keeps what it read as it is.
    const QJsonObject& source() const { return m_object; }

    /// Every key of the object, for an object whose keys are data.
    QStringList keys() const { return m_object.keys(); }

    QString string(const QString& key);
    QString optionalString(const QString& key);
    qint64 integer(const QString& key);
    std::optional<qint64> optionalInteger(const QString& key);
    double number(const QString& key);
    std::optional<double> optionalNumber(const QString& key);
    bool boolean(const QString& key);

    /// A flag its writer writes only when true.
    bool optionalFlag(const QString& key);

    /// True, with key taken, when key holds null. False, with key left for the call that
    /// reads its value, otherwise.
    bool readNull(const QString& key);

    /// One of words.
    QString word(const QString& key, const QStringList& words);

    /// One of words, or empty when key is absent.
    QString optionalWord(const QString& key, const QStringList& words);

    /// The one of values whose word, as wordOf gives it, is under key.
    template<typename T, typename WordOf>
    T word(const QString& key, const QList<T>& values, WordOf wordOf)
    {
        QStringList words = wordsOf(values, wordOf);
        qsizetype index = words.indexOf(word(key, words));
        return values[index < 0 ? 0 : index];
    }

    /// The one of values whose word, as wordOf gives it, is under key, or none when key is
    /// absent.
    template<typename T, typename WordOf>
    std::optional<T> optionalWord(const QString& key, const QList<T>& values, WordOf wordOf)
    {
        QStringList words = wordsOf(values, wordOf);
        qsizetype index = words.indexOf(optionalWord(key, words));
        if (index < 0)
        {
            return std::nullopt;
        }
        return values[index];
    }

    /// A date, written as Qt::ISODate writes it (2026-10-01).
    QDate date(const QString& key);
    std::optional<QDate> optionalDate(const QString& key);

    /// A time in UTC, written as Qt::ISODateWithMs writes one (2026-10-01T08:00:00.000Z).
    QDateTime time(const QString& key);

    /// An id of type T, written as a non-empty string.
    template<typename T>
    T id(const QString& key)
    {
        return T::fromString(idText(key, string(key)));
    }

    /// An id of type T, or none when key is absent.
    template<typename T>
    std::optional<T> optionalId(const QString& key)
    {
        QString text = optionalString(key);
        if (text.isEmpty())
        {
            return std::nullopt;
        }
        return T::fromString(text);
    }

    /// A set of ids of type T, written as an array of non-empty strings.
    template<typename T>
    QSet<T> ids(const QString& key)
    {
        return idSet<T>(idTexts(key, strings(key)));
    }

    template<typename T>
    QSet<T> optionalIds(const QString& key)
    {
        return idSet<T>(idTexts(key, optionalStrings(key)));
    }

    /// Base64url of exactly length bytes.
    QByteArray bytes(const QString& key, int length);

    JsonReader object(const QString& key);

    /// An object its writer leaves out when empty, so an empty object written out fails.
    std::optional<JsonReader> optionalObject(const QString& key);

    QList<JsonReader> objects(const QString& key);
    QList<JsonReader> optionalObjects(const QString& key);
    QStringList strings(const QString& key);
    QStringList optionalStrings(const QString& key);

    /// An array of objects each read by T::fromJson, keyed by its id.
    template<typename Key, typename T>
    QHash<Key, T> hash(const QString& key)
    {
        return hashOf<Key, T>(objects(key));
    }

    template<typename Key, typename T>
    QHash<Key, T> optionalHash(const QString& key)
    {
        return hashOf<Key, T>(optionalObjects(key));
    }

    /// An array of base64url values of exactly length bytes each.
    QList<QByteArray> optionalBytesList(const QString& key, int length);

    /// Fails the read at key, unless it has already failed.
    void fail(const QString& key, const QString& what);

    /// Fails the read for the first field nothing took.
    void finish();

private:
    struct Status
    {
        QString problem;
    };

    JsonReader(const QJsonObject& object, const QString& path, const std::shared_ptr<Status>& status);

    QJsonValue take(const QString& key);
    QJsonValue takeOptional(const QString& key);
    QString pathOf(const QString& key) const;
    QList<JsonReader> readObjects(const QString& key, const QJsonArray& array);
    QStringList readStrings(const QString& key, const QJsonArray& array);
    QString checkedWord(const QString& key, const QString& text, const QStringList& words);

    /// text, failing the read at key when it is empty.
    QString idText(const QString& key, const QString& text);

    /// texts, failing the read at the first empty one, an element of the array under key.
    QStringList idTexts(const QString& key, const QStringList& texts);

    template<typename T, typename WordOf>
    static QStringList wordsOf(const QList<T>& values, WordOf wordOf)
    {
        QStringList words;
        for (const T& value : values)
        {
            words.append(std::invoke(wordOf, value));
        }
        return words;
    }

    template<typename T>
    static QSet<T> idSet(const QStringList& texts)
    {
        QSet<T> ids;
        for (const QString& text : texts)
        {
            ids.insert(T::fromString(text));
        }
        return ids;
    }

    template<typename Key, typename T>
    static QHash<Key, T> hashOf(const QList<JsonReader>& readers)
    {
        QHash<Key, T> items;
        for (const JsonReader& reader : readers)
        {
            T item = T::fromJson(reader);
            items.insert(item.id(), item);
        }
        return items;
    }

    QJsonObject m_object;
    QString m_path;
    QSet<QString> m_taken;
    std::shared_ptr<Status> m_status;
};
