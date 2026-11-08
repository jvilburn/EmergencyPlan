#include "Base64Url.h"

QString Base64Url::encode(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

std::optional<QByteArray> Base64Url::decode(const QString& text, int length)
{
    QByteArray::FromBase64Result result = QByteArray::fromBase64Encoding(
        text.toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    // Only the one spelling of the decoded bytes re-encodes to text.
    if (!result || result.decoded.size() != length || encode(result.decoded) != text)
    {
        return std::nullopt;
    }
    return result.decoded;
}
