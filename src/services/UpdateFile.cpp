#include "UpdateFile.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "SignedLine.h"

namespace
{

UpdateRead unreadable(const QString& problem)
{
    return UpdateRead{UpdateReadStatus::Unreadable, problem, QByteArray(), QByteArray(), std::nullopt};
}

}  // namespace

QByteArray UpdateFile::write(const DocumentId& document, const QByteArray& version, const Identity& writer)
{
    QByteArray signature = writer.sign(SignedLine::ofVersion(SignedCall::Update, document, version));
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
    QByteArray line = SignedLine::ofVersion(SignedCall::Update, update.header->document(), update.version);
    return Crypto::verify(update.signature, line, verifyKey);
}

QString UpdateFile::fileName(const QString& wardName, const QDate& date)
{
    return QString("%1 %2.wardupdate").arg(wardName, date.toString(Qt::ISODate));
}
