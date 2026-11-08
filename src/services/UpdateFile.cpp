#include "UpdateFile.h"
#include "Base64Url.h"
#include "Crypto.h"

namespace
{

UpdateRead unreadable(const QString& problem)
{
    return UpdateRead{UpdateReadStatus::Unreadable, problem, QByteArray(), QByteArray(), std::nullopt};
}

}  // namespace

QByteArray UpdateFile::write(const DocumentId& document, const QByteArray& version, const Identity& writer)
{
    QByteArray signature = writer.sign(signedLine(document, version));
    return Base64Url::encode(signature).toLatin1() + '\n' + version;
}

UpdateRead UpdateFile::read(const QByteArray& bytes)
{
    qsizetype boundary = bytes.indexOf('\n');
    if (boundary < 0)
    {
        return unreadable("the file has no signature line");
    }
    std::optional<QByteArray> signature = Base64Url::decode(QString::fromLatin1(bytes.left(boundary)),
                                                            Crypto::SignatureBytes);
    if (!signature.has_value())
    {
        return unreadable("the signature line is not base64url of 64 bytes");
    }
    QByteArray version = bytes.mid(boundary + 1);
    qsizetype headerEnd = version.indexOf('\n');
    if (headerEnd < 0)
    {
        return unreadable("the version has no header line");
    }
    HeaderRead header = WardHeader::parse(version.left(headerEnd));
    switch (header.status)
    {
    case HeaderStatus::Read:
        break;
    case HeaderStatus::Newer:
        return UpdateRead{UpdateReadStatus::Newer, QString(), QByteArray(), QByteArray(), std::nullopt};
    case HeaderStatus::Unreadable:
        return unreadable(header.problem);
    }
    return UpdateRead{UpdateReadStatus::Read, QString(), *signature, version, header.header};
}

bool UpdateFile::verifies(const UpdateRead& update, const QByteArray& verifyKey)
{
    return Crypto::verify(update.signature, signedLine(update.header->document(), update.version), verifyKey);
}

QString UpdateFile::fileName(const QString& wardName, const QDate& date)
{
    return QString("%1 %2.wardupdate").arg(wardName, date.toString(Qt::ISODate));
}

QByteArray UpdateFile::signedLine(const DocumentId& document, const QByteArray& version)
{
    qsizetype boundary = version.indexOf('\n');
    QByteArray headerHash = Crypto::hash(version.left(boundary), Crypto::HashBytes, QByteArray());
    QByteArray payloadHash = Crypto::hash(version.mid(boundary + 1), Crypto::HashBytes, QByteArray());
    return "wardplanning-v1 update " + document.toString().toLatin1() + ' ' + Base64Url::encode(headerHash).toLatin1()
        + ' ' + Base64Url::encode(payloadHash).toLatin1();
}
