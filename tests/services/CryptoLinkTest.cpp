#include "CryptoLinkTest.h"

#include <qtkeychain/keychain.h>
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

void CryptoLinkTest::keychainLinks()
{
    // Constructing a job reaches into the library, so a missing or mis-exported
    // qtkeychain fails here, at link time, before any code relies on it.
    QKeychain::ReadPasswordJob job("Ward Planning Tests");
    job.setKey("link check");
    QCOMPARE(job.key(), QString("link check"));
    QCOMPARE(job.service(), QString("Ward Planning Tests"));
}
