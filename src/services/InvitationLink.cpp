#include "InvitationLink.h"

#include "Base64Url.h"
#include "Crypto.h"

QString InvitationLink::code(const DocumentId& document, const QByteArray& seed)
{
    return document.toString() + '.' + Base64Url::encode(seed);
}

std::optional<InvitationCode> InvitationLink::parse(const QString& code)
{
    QStringList parts = code.split('.');
    if (parts.size() != 2)
    {
        return std::nullopt;
    }
    std::optional<QByteArray> document = Base64Url::decode(parts[0], Crypto::IdBytes);
    std::optional<QByteArray> seed = Base64Url::decode(parts[1], Crypto::KeyBytes);
    if (!document.has_value() || !seed.has_value())
    {
        return std::nullopt;
    }
    return InvitationCode{DocumentId::fromString(parts[0]), *seed};
}

QString InvitationLink::link(const QString& serverAddress, const QString& code)
{
    return serverAddress + "/join#" + code;
}
