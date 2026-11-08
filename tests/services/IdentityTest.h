#pragma once

#include <QObject>

class IdentityTest : public QObject
{
    Q_OBJECT

public:
    explicit IdentityTest(QObject* parent);

private slots:
    void fingerprintIsTheKnownAnswer();
    void keyFileSecretsGiveTheKnownKeysAndFingerprint();
    void keysAreMadeOnlyWhenNoKeyFileExists();
    void keyFileThatDoesNotParseIsUnreadableAndLeftAsItWas();
    void keyPathThatCannotBeReadIsAFaultAndNothingIsMade();
    void dataKeySealedToADeviceOpensForItAlone();
};
