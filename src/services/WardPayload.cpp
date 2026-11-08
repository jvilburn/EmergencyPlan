#include "WardPayload.h"
#include "Base64Url.h"
#include "Crypto.h"

#include <QStringList>

QJsonObject WardPayload::toJson() const
{
    QJsonObject json;
    json["ward"] = ward.toJson();
    json["access"] = ward.access().toJson();
    QJsonObject landedJson;
    for (auto it = landed.constBegin(); it != landed.constEnd(); ++it)
    {
        landedJson[Base64Url::encode(it.key())] = it.value();
    }
    json["landed"] = landedJson;
    if (device.has_value())
    {
        json["device"] = device->toJson();
    }
    return json;
}

WardPayload WardPayload::fromJson(JsonReader json, PayloadKind kind)
{
    WardPayload payload;
    payload.ward = Document::fromJson(json.object("ward").source());
    payload.ward.setAccess(AccessList::fromJson(json.object("access")));
    JsonReader landed = json.object("landed");
    const QStringList fingerprints = landed.keys();
    for (const QString& key : fingerprints)
    {
        std::optional<QByteArray> fingerprint = Base64Url::decode(key, Crypto::FingerprintBytes);
        if (!fingerprint.has_value())
        {
            landed.fail(key, "is not a fingerprint");
            break;
        }
        payload.landed.insert(*fingerprint, landed.integer(key));
    }
    landed.finish();
    switch (kind)
    {
    case PayloadKind::LocalFile:
        payload.device = DeviceState::fromJson(json.object("device"));
        break;
    case PayloadKind::Version:
        break;
    }
    json.finish();
    return payload;
}

bool WardPayload::operator==(const WardPayload& other) const
{
    return ward == other.ward
        && landed == other.landed
        && device == other.device;
}
