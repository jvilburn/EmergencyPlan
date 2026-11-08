#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>
#include <optional>

/// Reads one JSON object exactly as its writer wrote it. Every field is taken by its type: a
/// field missing, of the wrong type or malformed fails the read, and finish() fails it for
/// any field left untaken. A field its writer leaves out when empty is taken with an
/// optional... call, which reads its absence as empty and fails on an empty value written
/// out, since the writer never writes one.
///
/// Every fromJson that takes a JsonReader ends with finish(). A failure is shared with every
/// reader made from this one and the first one found is kept, with the path of the field it
/// names. A reader returns empty values once it has failed, so whoever reads a whole tree
/// asks failed() once, at the top.
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

    /// Base64url of exactly length bytes.
    QByteArray bytes(const QString& key, int length);

    JsonReader object(const QString& key);
    std::optional<JsonReader> optionalObject(const QString& key);
    QList<JsonReader> objects(const QString& key);
    QList<JsonReader> optionalObjects(const QString& key);
    QStringList strings(const QString& key);
    QStringList optionalStrings(const QString& key);

    /// An array of base64url values of exactly length bytes each.
    QList<QByteArray> optionalBytesList(const QString& key, int length);

    /// The value under key, for a type that reads its own JSON value.
    QJsonValue value(const QString& key);

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

    QJsonObject m_object;
    QString m_path;
    QSet<QString> m_taken;
    std::shared_ptr<Status> m_status;
};
