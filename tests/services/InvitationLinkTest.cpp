#include "InvitationLinkTest.h"

#include "Base64Url.h"
#include "Crypto.h"
#include "InvitationLink.h"
#include "SendLinkDialog.h"

#include <QTest>

InvitationLinkTest::InvitationLinkTest(QObject* parent)
    : QObject(parent)
{
}

void InvitationLinkTest::codeRoundTripsAndTheLinkCarriesItAfterTheHash()
{
    DocumentId document = DocumentId::generate();
    QByteArray seed = Crypto::randomBytes(Crypto::KeyBytes);

    QString code = InvitationLink::code(document, seed);

    QCOMPARE(code.size(), qsizetype(66));
    QCOMPARE(code, document.toString() + "." + Base64Url::encode(seed));
    std::optional<InvitationCode> parsed = InvitationLink::parse(code);
    QVERIFY(parsed.has_value());
    QVERIFY(parsed->document == document);
    QCOMPARE(parsed->seed, seed);
    QCOMPARE(InvitationLink::link("https://example.test", code), "https://example.test/join#" + code);
}

void InvitationLinkTest::parseRefusesWhatIsNotACode()
{
    QString code = InvitationLink::code(DocumentId::generate(), Crypto::randomBytes(Crypto::KeyBytes));
    QVERIFY(InvitationLink::parse(code).has_value());

    QVERIFY(!InvitationLink::parse(QString()).has_value());
    QVERIFY(!InvitationLink::parse(code.left(65)).has_value());
    QVERIFY(!InvitationLink::parse(code + "A").has_value());
    QVERIFY(!InvitationLink::parse(QString(code).replace('.', '_')).has_value());
    QVERIFY(!InvitationLink::parse(code + "." + code).has_value());
    QVERIFY(!InvitationLink::parse(QString(code).replace(30, 1, "!")).has_value());
    QVERIFY(!InvitationLink::parse("https://example.test/join#" + code).has_value());
}

void InvitationLinkTest::messagesSpeakToTheRightPerson()
{
    QCOMPARE(SendLinkDialog::userInstructions("Paula"),
             QString("Your invitation for Paula is ready. Send Paula this message, by email or text:"));
    QCOMPARE(SendLinkDialog::userMessage("John", "Tanglewood Ward", "https://example.test/join#abc"),
             QString("John has invited you to Tanglewood Ward in Ward Planning. Click to join:\n"
                     "https://example.test/join#abc"));
    QCOMPARE(SendLinkDialog::deviceInstructions(),
             QString("Your invitation is ready. Open this link on your other device:"));
}
