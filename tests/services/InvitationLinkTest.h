#pragma once

#include <QObject>

/// An invitation as a link: the code after the #, and what reads as one.
class InvitationLinkTest : public QObject
{
    Q_OBJECT

public:
    explicit InvitationLinkTest(QObject* parent);

private slots:
    void codeRoundTripsAndTheLinkCarriesItAfterTheHash();
    void parseRefusesWhatIsNotACode();
    void messagesSpeakToTheRightPerson();
    void theButtonCopiesTheMessageAlone();
};
