#pragma once

#include <QObject>

/// The ministering PDF parser reads, from pages laid out as the church's export lays them
/// out, each companionship's ministers with their contact details, the households or
/// sisters they minister to with each member's gender and birthday, and companionships on
/// every page of a long list.
class MinisteringPdfParserTest : public QObject
{
    Q_OBJECT

public:
    explicit MinisteringPdfParserTest(QObject* parent);

private slots:
    void eldersQuorumCompanionshipReadsMinistersAndHouseholds();
    void reliefSocietyCompanionshipMinistersToTheSisterItNames();
    void companionshipsOnEveryPageOfALongListStayApart();
    void presidencyMembersAreCreatedWithTheListsGender();
};
