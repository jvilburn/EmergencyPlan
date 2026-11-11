#include "InvitationFile.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "JsonReader.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace
{

InvitationRead notAnInvitation(const QString& problem)
{
    return InvitationRead{InvitationReadStatus::NotAnInvitation, problem, DocumentId(), QByteArray(), QByteArray()};
}

}  // namespace

QByteArray InvitationFile::write(const DocumentId& document, const QByteArray& seed, const QByteArray& version)
{
    QJsonObject json;
    json["format"] = Format;
    json["document"] = document.toString();
    json["seed"] = Base64Url::encode(seed);
    return QJsonDocument(json).toJson(QJsonDocument::Compact) + '\n' + version;
}

InvitationRead InvitationFile::read(const QByteArray& bytes)
{
    qsizetype boundary = bytes.indexOf('\n');
    if (boundary < 0)
    {
        return notAnInvitation("the file has no invitation line");
    }
    std::optional<QJsonObject> object = JsonReader::parseObject(bytes.left(boundary));
    if (!object.has_value())
    {
        return notAnInvitation("the invitation line is not a JSON object");
    }
    QJsonValue format = object->value("format");
    if (!JsonReader::isInteger(format))
    {
        return notAnInvitation("format: is not an integer");
    }
    if (format.toDouble() > Format)
    {
        return InvitationRead{InvitationReadStatus::Newer, QString(), DocumentId(), QByteArray(), QByteArray()};
    }
    if (format.toDouble() != Format)
    {
        return notAnInvitation("format: is not a version this app reads");
    }

    JsonReader json(*object);
    json.integer("format");
    QByteArray document = json.bytes("document", Crypto::IdBytes);
    QByteArray seed = json.bytes("seed", Crypto::KeyBytes);
    json.finish();
    if (json.failed())
    {
        return notAnInvitation(json.problem());
    }
    return InvitationRead{InvitationReadStatus::Read, QString(), DocumentId::fromBytes(document), seed,
                          bytes.mid(boundary + 1)};
}

QString InvitationFile::fileName(const QString& wardName, const QString& inviteDescription)
{
    return QString("%1 - %2.wardinvite").arg(wardName, inviteDescription);
}
