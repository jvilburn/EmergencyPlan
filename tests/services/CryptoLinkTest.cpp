#include "CryptoLinkTest.h"

#include <sodium.h>

#include <QtTest>

void CryptoLinkTest::sodiumInitialises()
{
    // 0 on the first call, 1 when already initialised; -1 is failure.
    QVERIFY(sodium_init() >= 0);
}
