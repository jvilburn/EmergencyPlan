#pragma once

#include <QObject>

/// Proves libsodium is linked into this build, at the version the format depends on.
class CryptoLinkTest : public QObject
{
    Q_OBJECT

private slots:
    void sodiumIsAtLeastTheFixedRelease();
};
