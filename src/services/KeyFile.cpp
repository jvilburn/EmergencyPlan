#include "KeyFile.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "JsonReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

KeyFileResult KeyFile::loadOrCreate(const QString& path)
{
    if (!QFileInfo::exists(path))
    {
        return create(path);
    }
    return load(path);
}

KeyFileResult KeyFile::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return KeyFileResult{KeyFileStatus::Fault, std::nullopt, file.errorString()};
    }
    QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        return KeyFileResult{KeyFileStatus::Fault, std::nullopt, file.errorString()};
    }

    std::optional<QJsonObject> object = JsonReader::parseObject(bytes);
    if (!object.has_value())
    {
        return KeyFileResult{KeyFileStatus::Unreadable, std::nullopt, "the key file is not a JSON object"};
    }
    JsonReader json(*object);
    if (json.integer("format") != Format)
    {
        json.fail("format", "is not a version this app reads");
    }
    QByteArray sealingSecret = json.bytes("sealingSecret", Crypto::KeyBytes);
    QByteArray signingSeed = json.bytes("signingSeed", Crypto::KeyBytes);
    json.finish();
    if (json.failed())
    {
        return KeyFileResult{KeyFileStatus::Unreadable, std::nullopt, json.problem()};
    }
    return KeyFileResult{KeyFileStatus::Loaded, Identity::fromSecrets(sealingSecret, signingSeed), QString()};
}

KeyFileResult KeyFile::create(const QString& path)
{
    Identity identity = Identity::generate();
    QJsonObject json;
    json["format"] = Format;
    json["sealingSecret"] = Base64Url::encode(identity.sealingSecret());
    json["signingSeed"] = Base64Url::encode(identity.signingSeed());
    QByteArray bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit())
    {
        return KeyFileResult{KeyFileStatus::Fault, std::nullopt, file.errorString()};
    }
    // Restricts the file to its owner on POSIX systems; on Windows the per-user app data
    // folder protects it.
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return KeyFileResult{KeyFileStatus::Created, identity, QString()};
}
