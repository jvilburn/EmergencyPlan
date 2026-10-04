#include "CryptoLinkTest.h"

#include <sodium.h>

#include <QVersionNumber>
#include <QtTest>

void CryptoLinkTest::sodiumIsAtLeastTheFixedRelease()
{
    // 1.0.22 carries the core_ed25519_is_valid_point fix; an older library
    // resolved from a stale vcpkg baseline would still link and initialise.
    const QVersionNumber version = QVersionNumber::fromString(sodium_version_string());
    QVERIFY2(version >= QVersionNumber(1, 0, 22), sodium_version_string());
}
