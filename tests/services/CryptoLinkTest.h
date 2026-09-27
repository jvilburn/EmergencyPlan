#pragma once

#include <QObject>

/// Proves libsodium and qtkeychain are linked into this build, at the versions the
/// format depends on.
class CryptoLinkTest : public QObject
{
    Q_OBJECT

private slots:
    void sodiumIsAtLeastTheFixedRelease();
    void keychainLinks();
};
