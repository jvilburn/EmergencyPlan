#include "WardFileEdits.h"
#include "Crypto.h"
#include "WardHeader.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace
{

QJsonValue editedValue(const QJsonValue& json, const QStringList& path, const QJsonValue& value)
{
    if (path.isEmpty())
    {
        return value;
    }
    QString step = path.first();
    QStringList rest = path.mid(1);
    if (json.isArray())
    {
        QJsonArray array = json.toArray();
        int index = step.toInt();
        array[index] = editedValue(array[index], rest, value);
        return array;
    }
    QJsonObject object = json.toObject();
    QJsonValue replaced = editedValue(object.value(step), rest, value);
    if (replaced.isUndefined())
    {
        object.remove(step);
    }
    else
    {
        object[step] = replaced;
    }
    return object;
}

}  // namespace

QJsonObject WardFileEdits::headerOf(const QByteArray& file)
{
    return QJsonDocument::fromJson(file.left(file.indexOf('\n'))).object();
}

QByteArray WardFileEdits::withHeader(const QByteArray& file, const QJsonObject& header)
{
    return QJsonDocument(header).toJson(QJsonDocument::Compact) + file.mid(file.indexOf('\n'));
}

QJsonObject WardFileEdits::edited(const QJsonObject& json, const QStringList& path, const QJsonValue& value)
{
    return editedValue(json, path, value).toObject();
}

VersionRank WardFileEdits::version()
{
    return VersionRank{1, QByteArray(Crypto::FingerprintBytes, 'w')};
}

QByteArray WardFileEdits::sealedFile(const DocumentId& document, const QByteArray& dataKey,
                                     const QList<RecipientKeys>& recipients, const QJsonObject& payload)
{
    QByteArray compressed = qCompress(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    return sealedPlaintext(document, dataKey, recipients, compressed);
}

QByteArray WardFileEdits::sealedPlaintext(const DocumentId& document, const QByteArray& dataKey,
                                          const QList<RecipientKeys>& recipients, const QByteArray& plaintext)
{
    WardHeader header = WardHeader::build(document, version(), dataKey, recipients);
    return header.line() + '\n' + Crypto::encrypt(plaintext, header.line(), header.nonce(), dataKey);
}
