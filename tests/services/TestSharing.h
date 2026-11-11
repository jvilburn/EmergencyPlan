#pragma once

#include "AccessList.h"
#include "Id.h"
#include "TestClient.h"
#include "WardSession.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

/// Wards shared among TestClients through update files and invitations.
class TestSharing
{
public:
    /// A user per client, named for its device and holding it alone: an admin for each of
    /// admins, then a member for each of members.
    static AccessList accessOf(const QList<TestClient*>& admins, const QList<TestClient*>& members);

    /// Gives each of holders the test ward with access as its access list, under a fresh
    /// document id and data key, as version 1 written by the first holder: the document id.
    static DocumentId share(const QList<TestClient*>& holders, const AccessList& access);

    /// Writes from's next update file and has each of to take it: each take's status, in
    /// order.
    static QList<TakeStatus> send(TestClient& from, const QList<TestClient*>& to);

    /// What follows the first line of an update file or an invitation: the version it carries.
    static QByteArray versionIn(const QByteArray& file);

    /// True when the payload of version decrypts under key.
    static bool opensUnder(const QByteArray& version, const QByteArray& key);

    /// The data key client's local file of its open ward is under.
    static QByteArray keyOf(TestClient& client);

    /// The user named name in client's open ward.
    static AccessUser userNamed(TestClient& client, const QString& name);

    /// The names of the users of client's open ward, in the list's order.
    static QStringList userNames(TestClient& client);

    /// A phone edit to the person in the family, opened on client's current family.
    static CommitStatus commitPhone(TestClient& client, const FamilyId& family, const PersonId& person,
                                    const QString& phone);

    /// Bob Smith's phone edited to phone on client.
    static CommitStatus commitBobsPhone(TestClient& client, const QString& phone);

    static QString phoneOf(TestClient& client, const PersonId& person);
    static QString bobsPhone(TestClient& client);
};
