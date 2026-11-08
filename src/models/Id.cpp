#include "Id.h"
#include "Base64Url.h"
#include "Crypto.h"

DocumentId DocumentId::generate()
{
    return DocumentId(Base64Url::encode(Crypto::randomBytes(Crypto::IdBytes)));
}

UserId UserId::generate()
{
    return UserId(Base64Url::encode(Crypto::randomBytes(Crypto::IdBytes)));
}
