#pragma once

#include "Identity.h"
#include "WardFile.h"

#include <QObject>

class WardFileTest : public QObject
{
    Q_OBJECT

public:
    explicit WardFileTest(QObject* parent);

private slots:
    void init();

    void roundTripRecoversThePayloadAndTheDataKey();
    void writesOfAnUnchangedPayloadUseFreshNonces();
    void headerIsBoundAsAssociatedData();
    void formatAboveEveryKnownVersionIsNewer();
    void formatThisAppDoesNotReadIsUnreadable_data();
    void formatThisAppDoesNotReadIsUnreadable();
    void headerWithAMissingOrMalformedFieldIsUnreadable_data();
    void headerWithAMissingOrMalformedFieldIsUnreadable();
    void payloadWithAMissingOrMalformedFieldIsUnreadable_data();
    void payloadWithAMissingOrMalformedFieldIsUnreadable();
    void versionCarriesNoDeviceStateAndALocalFileMust();
    void headerMustNameExactlyTheAccessListsKeys();
    void headerNamingNoOneIsADeletedWard();
    void headerNamingOthersButNotThisDeviceIsNotNamed();
    void dataKeyThatDoesNotUnsealIsUnreadable();
    void fileWithNoHeaderLineIsUnreadable();

private:
    QByteArray written() const;
    WardRead readSealedTo(const QList<RecipientKeys>& recipients) const;

    Identity m_laptop;
    Identity m_phone;
    DocumentId m_document;
    QByteArray m_dataKey;
    WardPayload m_payload;
};
