#include "TestSharing.h"
#include "Crypto.h"
#include "FamilyCommands.h"
#include "TestWard.h"
#include "WardFile.h"
#include "WardHeader.h"

AccessList TestSharing::accessOf(const QList<TestClient*>& admins, const QList<TestClient*>& members)
{
    QList<AccessUser> users;
    for (const TestClient* client : admins)
    {
        users.append(AccessUser(UserId::generate(), {client->deviceName(), ""}, AccessRole::Admin, {client->device()}));
    }
    for (const TestClient* client : members)
    {
        users.append(AccessUser(UserId::generate(), {client->deviceName(), ""}, AccessRole::Member, {client->device()}));
    }
    return AccessList(users, {}, {});
}

DocumentId TestSharing::share(const QList<TestClient*>& holders, const AccessList& access)
{
    DocumentId document = DocumentId::generate();
    TestClient::holdWard(holders, access, *holders.first(), document, Crypto::randomBytes(Crypto::KeyBytes),
                         TestWard::document());
    return document;
}

QList<TakeStatus> TestSharing::send(TestClient& from, const QList<TestClient*>& to)
{
    QByteArray update = from.session().writeUpdate();
    QList<TakeStatus> statuses;
    for (TestClient* client : to)
    {
        statuses.append(client->session().takeUpdate(update).status);
    }
    return statuses;
}

QByteArray TestSharing::versionIn(const QByteArray& file)
{
    return file.mid(file.indexOf('\n') + 1);
}

bool TestSharing::opensUnder(const QByteArray& version, const QByteArray& key)
{
    qsizetype boundary = version.indexOf('\n');
    HeaderRead header = WardHeader::parse(version.left(boundary));
    return Crypto::decrypt(version.mid(boundary + 1), header.header->line(), header.header->nonce(), key).has_value();
}

QByteArray TestSharing::keyOf(TestClient& client)
{
    return WardFile::read(client.store().file(client.session().documentId()), client.identity(), PayloadKind::LocalFile)
        .dataKey;
}

AccessUser TestSharing::userNamed(TestClient& client, const QString& name)
{
    for (const AccessUser& user : client.session().document().access().users())
    {
        if (user.name() == name)
        {
            return user;
        }
    }
    qFatal("TestSharing: %s holds no user named %s", qPrintable(client.deviceName()), qPrintable(name));
}

QStringList TestSharing::userNames(TestClient& client)
{
    QStringList names;
    for (const AccessUser& user : client.session().document().access().users())
    {
        names.append(user.name());
    }
    return names;
}

CommitStatus TestSharing::commitPhone(TestClient& client, const FamilyId& family, const PersonId& person,
                                      const QString& phone)
{
    Family opened = TestWard::family(client.session().document(), family);
    return client.session()
        .commit(UpdateFamilyCommand::fromEdit(opened, TestWard::withPhone(opened, person, phone),
                                              client.session().document()))
        .status;
}

CommitStatus TestSharing::commitBobsPhone(TestClient& client, const QString& phone)
{
    return commitPhone(client, TestWard::smithId(), TestWard::bobId(), phone);
}

QString TestSharing::phoneOf(TestClient& client, const PersonId& person)
{
    return TestWard::person(client.session().document(), person).phone();
}

QString TestSharing::bobsPhone(TestClient& client)
{
    return phoneOf(client, TestWard::bobId());
}
