#pragma once

#include <QObject>

/// Proves libsodium is linked and initialises in this build.
class CryptoLinkTest : public QObject
{
    Q_OBJECT

private slots:
    void sodiumInitialises();
};
