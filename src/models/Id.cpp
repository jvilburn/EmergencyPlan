#include "Id.h"
#include "Base64Url.h"
#include "Crypto.h"

DocumentId DocumentId::generate()
{
    return fromBytes(Crypto::randomBytes(Crypto::IdBytes));
}

DocumentId DocumentId::fromBytes(const QByteArray& bytes)
{
    return DocumentId(Base64Url::encode(bytes));
}

UserId UserId::generate()
{
    return fromBytes(Crypto::randomBytes(Crypto::IdBytes));
}

UserId UserId::fromBytes(const QByteArray& bytes)
{
    return UserId(Base64Url::encode(bytes));
}
