#include "SignedLine.h"

#include "Base64Url.h"
#include "Crypto.h"

namespace
{
QByteArray wordFor(SignedCall call)
{
    switch (call)
    {
    case SignedCall::Create:
        return "create";
    case SignedCall::Push:
        return "push";
    case SignedCall::Update:
        return "update";
    }
    return QByteArray();
}

QByteArray hashOf(const QByteArray& bytes)
{
    return Base64Url::encode(Crypto::hash(bytes, Crypto::HashBytes, QByteArray())).toLatin1();
}
}  // namespace

QByteArray SignedLine::ofVersion(SignedCall call, const DocumentId& document, const QByteArray& version)
{
    qsizetype boundary = version.indexOf('\n');
    return "wardplanning-v1 " + wordFor(call) + ' ' + document.toString().toLatin1() + ' '
        + hashOf(version.left(boundary)) + ' ' + hashOf(version.mid(boundary + 1));
}

QByteArray SignedLine::ofChannel(const DocumentId& document)
{
    return "wardplanning-v1 channel " + document.toString().toLatin1() + " - -";
}
