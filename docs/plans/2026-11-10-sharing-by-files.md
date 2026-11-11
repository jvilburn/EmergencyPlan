# Sharing by Files Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** A ward is shared with the server turned off: users and devices are added and removed
through invitation files and update files, every removal rotates the data key, the app says when
this device has changes to share, and *Settings* holds the server switch (built now, off by
default), this device's name, the user's name and the map home.

**Architecture:** `WardSession` stays the one place that changes a ward. Access-list changes are
commands like any other (`AccessCommands`), with their rules in `AccessRules`, so they journal,
replay and travel in update files by the path content takes. Writing a version decides rotation
against the recipients of the version this device holds, which the device state now keeps.
Taking a version accepts it when it shares a key with this device's copy. An invite is a command
whose seed the device state keeps; an invitation file carries the next version; joining takes
that version as a first download and runs `JoinCommand` on it. The joined device keeps the
invite's seed for as long as it holds the ward: it reads with it any version that names the
invite and not the device, and runs the join again on top; and it writes as the invite while the
version it holds does not name it. A version that leaves this device out makes the
ward no longer this device's. The interface (Settings, *Who has access*, the notices, receiving
an invitation) is built on those calls.

**Tech Stack:** Qt 6.11 (Core, Gui, Widgets, Test), C++17, MSVC 2022, CMake + Ninja via
`build.bat`, libsodium 1.0.22 from vcpkg, Qt Test.

**Design authority:** `docs/SHARED_WARD_DESIGN.md` at commit 7fa7b63. Section names in *italics*
are its sections. This plan builds on the code as commit 7fa7b63 leaves it, where update files
already exist (`WardSession::writeUpdate`, `takeUpdate`, `UpdateFile`, `UserFolder`).

**What this plan takes from `docs/plans/2026-10-17-sharing.md`** (the server plan, not executed):
its Task 1 `AccessList` additions, `AccessRules` and `RemoveUserCommand`; its Task 2 commands and
rules; its Task 5 invites and joining; its Task 8 *Who has access*; its Task 9 receiving an
invitation. Everything that exists only to talk to a server is left out: `Server`,
`HttpServer`, `Transport`, `FakeServer`, the live channel, presence, sync states, Leave and
*Remove this device* as pushes. Where that plan and the current code differ, this plan takes the
simpler of the two and says so:

- The journal keeps its current `dropLanded(mark)`, which drops and raises the counter in one
  call, in place of the server plan's `dropThrough` and `raiseCounterTo`.
- `AccessList::userOfDevice` answers the user, not their id, since every caller then reads the
  user.
- The invite's keys come from `Identity::fromInviteSeed`, an `Identity` like any device's, in
  place of a separate `InviteKeys` type.
- The invitation file is `InvitationFile`, shaped like the existing `UpdateFile`, in place of an
  `Invitation` model.
- Renames follow the design: a device renames only itself and its own user, and names change
  only in *Settings*, so the rename commands carry only the device that made them and there is
  no Rename in *Who has access*.

---

## Conventions for every task

- **The executor does not build.** The project's `CLAUDE.md` forbids running cmake or the app from
  this shell. The user builds with `build.bat` and runs `build\bin\WardPlanningTests.exe`. Each
  task's tests are written in that task and run when the user builds.
- **Core first.** Task 1 is the whole design-bearing core, then a STOP for the user's own review.
  Every later task is a consequence of it. The tree and the app may be red between tasks.
- **Delete with the last caller**, in the same task. Code that is going away goes in the
  earliest task it can.
- **Docs change in the task whose code they describe** (`docs/FORMAT.md`,
  `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`).
- **Style:** `docs/CODING_STYLE.md` and the project `CLAUDE.md`: Allman braces, braces on every
  control statement, only one-liners in headers, `std::optional` for nullable fields, leading
  operators on continuation lines, `json` as the variable in `toJson`/`fromJson`, equality over
  every field, `tr()` for every user-visible string. Comments say what is true now.
- **Every new command type** adds its entry to `CommandRegistry::types()` and its samples to
  `tests/services/CommandSamples.cpp` in the same task, and its fields to `docs/FORMAT.md` and to
  the reader in `tests/spec`.
- **Commits:** one per task, with the message the task gives, plain, no attribution lines.
  Before each commit, `git branch --show-current` prints the implementation branch. Stage the
  files the task changed by name.
- **Tests** pin observed values, use inputs that give non-trivial results, and anchor every
  absence assertion to a positive signal.

---

## Task 1: The core

The access commands and their rules; the device state that holds what sharing by files needs
(the held version's recipients, the seeds of invites made here, the seed of the invite this
device joined through, and the no-longer-shared flag); rotation and former keys when a written
version drops a recipient; the descent rule that lets crossed update files converge; making an
invite and writing its invitation; reading an invitation and joining from it; the
changes-to-share rule; the no-longer-shared state; renames across every ward this device holds;
and the multi-device tests that drive real sessions exchanging update files and invitations.

**Files:**
- Modify: `src/services/Crypto.h`, `src/services/Crypto.cpp` (subkeys, box seed keypair)
- Modify: `src/services/Identity.h`, `src/services/Identity.cpp` (`fromInviteSeed`)
- Modify: `src/services/JsonReader.h`, `src/services/JsonReader.cpp` (`bytesList`, `optionalBytes`)
- Modify: `src/models/AccessList.h`, `src/models/AccessList.cpp`
- Modify: `src/commands/FieldChangeJson.h`, `src/commands/FieldChangeJson.cpp` (`AccessRole`)
- Create: `src/commands/AccessRules.h`, `src/commands/AccessRules.cpp`
- Create: `src/commands/AccessCommands.h`, `src/commands/AccessCommands.cpp`
- Modify: `src/commands/CommandRegistry.cpp`
- Modify: `src/services/DeviceState.h`, `src/services/DeviceState.cpp`
- Create: `src/services/InvitationFile.h`, `src/services/InvitationFile.cpp`
- Modify: `src/services/WardSession.h`, `src/services/WardSession.cpp` (both given whole)
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp` (the prompt to send an update
  after taking one goes)
- Modify: `CMakeLists.txt`
- Modify: `docs/FORMAT.md`, `docs/ARCHITECTURE.md`
- Modify: `tests/services/TestClient.h`, `tests/services/TestClient.cpp`
- Create: `tests/services/TestSharing.h`, `tests/services/TestSharing.cpp`
- Create: `tests/services/AccessTest.h`, `tests/services/AccessTest.cpp`
- Create: `tests/services/FileSharingTest.h`, `tests/services/FileSharingTest.cpp`
- Modify: `tests/services/CommandSamples.cpp`, `tests/services/EveryField.h`,
  `tests/services/EveryField.cpp`, `tests/services/StrictReadersTest.cpp`,
  `tests/services/SpecReaderTest.cpp`, `tests/services/WardSessionSavingTest.cpp`,
  `tests/services/UpdateFileTest.h`, `tests/services/UpdateFileTest.cpp`
- Modify: `tests/spec/Payload.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/main.cpp`

### What happens to local files written before this task

The local file's `device` part gains a required `heldRecipients` field. A local file written
before this task lacks it, so the strict reader refuses it: `WardSession::readLocal` answers
`CannotRead`, the ward list shows that ward as one that cannot be read, and it does not open.
Such a ward is made again: a new ward, its PDFs imported again.

### `src/services/Crypto.h` and `src/services/Crypto.cpp`

Add to the class:

```cpp
    /// Subkey number id of key, KeyBytes long, under an eight-character context:
    /// crypto_kdf_derive_from_key.
    static QByteArray deriveSubkey(const QByteArray& key, quint64 id, const char* context);

    /// The X25519 secret key crypto_box_seed_keypair makes from seed.
    static QByteArray sealingSecretFromSeed(const QByteArray& seed);
```

Add beside the other `static_assert`s:

```cpp
static_assert(static_cast<int>(crypto_kdf_KEYBYTES) == Crypto::KeyBytes);
static_assert(static_cast<int>(crypto_box_SEEDBYTES) == Crypto::KeyBytes);
```

and the two functions:

```cpp
QByteArray Crypto::deriveSubkey(const QByteArray& key, quint64 id, const char* context)
{
    Q_ASSERT(qstrlen(context) == crypto_kdf_CONTEXTBYTES);
    QByteArray subkey(KeyBytes, '\0');
    crypto_kdf_derive_from_key(bytesOf(subkey), static_cast<size_t>(subkey.size()), id, context, bytesOf(key));
    return subkey;
}

QByteArray Crypto::sealingSecretFromSeed(const QByteArray& seed)
{
    QByteArray publicKey(crypto_box_PUBLICKEYBYTES, '\0');
    QByteArray secretKey(crypto_box_SECRETKEYBYTES, '\0');
    crypto_box_seed_keypair(bytesOf(publicKey), bytesOf(secretKey), bytesOf(seed));
    return secretKey;
}
```

`Identity` computes its X25519 public key from the secret with `crypto_scalarmult_base`, which is
how `crypto_box_seed_keypair` makes its public key from the secret it derives (recalled from
libsodium's source, not checked this session); task 6's independent reader derives the invite's
keys with `crypto_box_seed_keypair` itself, so a difference fails that test.

### `src/services/Identity.h` and `src/services/Identity.cpp`

Add to the class:

```cpp
    /// An invite's identity: subkeys 1 and 2 of seed under the context wpinvite seed its
    /// X25519 keypair, as crypto_box_seed_keypair makes it, and its Ed25519 keypair.
    static Identity fromInviteSeed(const QByteArray& seed);
```

```cpp
Identity Identity::fromInviteSeed(const QByteArray& seed)
{
    return fromSecrets(Crypto::sealingSecretFromSeed(Crypto::deriveSubkey(seed, 1, "wpinvite")),
                       Crypto::deriveSubkey(seed, 2, "wpinvite"));
}
```

### `src/services/JsonReader.h` and `src/services/JsonReader.cpp`

Add beside `optionalBytesList`:

```cpp
    /// An array of base64url values of exactly length bytes each, written when empty too.
    QList<QByteArray> bytesList(const QString& key, int length);

    /// A base64url value of exactly length bytes, or none when the key is absent.
    std::optional<QByteArray> optionalBytes(const QString& key, int length);
```

and to the private part:

```cpp
    /// texts, read under key, each decoded as base64url of exactly length bytes.
    QList<QByteArray> decodedBytes(const QString& key, const QStringList& texts, int length);
```

`optionalBytesList` keeps its behavior and moves its loop into `decodedBytes`:

```cpp
QList<QByteArray> JsonReader::bytesList(const QString& key, int length)
{
    return decodedBytes(key, strings(key), length);
}

QList<QByteArray> JsonReader::optionalBytesList(const QString& key, int length)
{
    return decodedBytes(key, optionalStrings(key), length);
}

std::optional<QByteArray> JsonReader::optionalBytes(const QString& key, int length)
{
    if (takeOptional(key).isUndefined())
    {
        return std::nullopt;
    }
    return bytes(key, length);
}

QList<QByteArray> JsonReader::decodedBytes(const QString& key, const QStringList& texts, int length)
{
    QList<QByteArray> values;
    for (int i = 0; i < texts.size(); ++i)
    {
        std::optional<QByteArray> decoded = Base64Url::decode(texts[i], length);
        if (!decoded.has_value())
        {
            fail(elementOf(key, i), QString("is not base64url of %1 bytes").arg(length));
            return {};
        }
        values.append(*decoded);
    }
    return values;
}
```

### `src/models/AccessList.h`

Given whole:

```cpp
#pragma once

#include "Id.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

class Identity;

enum class AccessRole
{
    Admin,
    Member
};

/// The word role is written as: admin or member.
QString accessRoleName(AccessRole role);

/// The role under key, read as one of the words accessRoleName writes.
AccessRole readAccessRole(JsonReader& json, const QString& key);

/// One of a user's devices, by the keys the data key is sealed to and its writes are
/// signed with.
class AccessDevice
{
public:
    AccessDevice(const QString& name, const QByteArray& publicKey, const QByteArray& verifyKey);

    const QString& name() const { return m_name; }
    const QByteArray& publicKey() const { return m_publicKey; }
    const QByteArray& verifyKey() const { return m_verifyKey; }

    /// The fingerprint of the two public keys: what a header names this device by.
    QByteArray fingerprint() const;

    void setName(const QString& name) { m_name = name; }

    QJsonObject toJson() const;
    static AccessDevice fromJson(JsonReader json);

    bool operator==(const AccessDevice& other) const;
    bool operator!=(const AccessDevice& other) const { return !(*this == other); }

private:
    QString m_name;
    QByteArray m_publicKey;
    QByteArray m_verifyKey;
};

/// A user of the ward: their id within it, the name they gave, their role, and their devices.
class AccessUser
{
public:
    AccessUser(const UserId& id, const QString& name, AccessRole role, const QList<AccessDevice>& devices);

    const UserId& id() const { return m_id; }
    const QString& name() const { return m_name; }
    AccessRole role() const { return m_role; }
    const QList<AccessDevice>& devices() const { return m_devices; }

    void setName(const QString& name) { m_name = name; }
    void setRole(AccessRole role) { m_role = role; }
    void setDevices(const QList<AccessDevice>& devices) { m_devices = devices; }

    QJsonObject toJson() const;
    static AccessUser fromJson(JsonReader json);

    bool operator==(const AccessUser& other) const;
    bool operator!=(const AccessUser& other) const { return !(*this == other); }

private:
    UserId m_id;
    QString m_name;
    AccessRole m_role;
    QList<AccessDevice> m_devices;
};

/// Who a user invite is for: the role its joiner takes and a label naming them.
struct InvitedUser
{
    AccessRole role;
    QString label;

    bool operator==(const InvitedUser& other) const { return role == other.role && label == other.label; }
};

/// An open invite: a one-time bearer key the data key is sealed to until it is used or
/// withdrawn. A user invite names the user it is for; a device invite names none, and joins
/// as one of its maker's devices.
class AccessInvite
{
public:
    static AccessInvite forDevice(const UserId& maker, const QByteArray& publicKey, const QByteArray& verifyKey);
    static AccessInvite forUser(const UserId& maker, AccessRole role, const QString& label,
                                const QByteArray& publicKey, const QByteArray& verifyKey);

    const UserId& maker() const { return m_maker; }

    /// The user a user invite is for; empty for a device invite.
    const std::optional<InvitedUser>& invitedUser() const { return m_invitedUser; }

    const QByteArray& publicKey() const { return m_publicKey; }
    const QByteArray& verifyKey() const { return m_verifyKey; }

    /// The fingerprint of the two public keys: what a header names this invite by.
    QByteArray fingerprint() const;

    QJsonObject toJson() const;
    static AccessInvite fromJson(JsonReader json);

    bool operator==(const AccessInvite& other) const;
    bool operator!=(const AccessInvite& other) const { return !(*this == other); }

private:
    AccessInvite(const UserId& maker, const std::optional<InvitedUser>& invitedUser, const QByteArray& publicKey,
                 const QByteArray& verifyKey);

    UserId m_maker;
    std::optional<InvitedUser> m_invitedUser;
    QByteArray m_publicKey;
    QByteArray m_verifyKey;
};

/// The public keys of one recipient, a device or an open invite.
struct RecipientKeys
{
    QByteArray publicKey;
    QByteArray verifyKey;
};

/// Who may read the ward: its users and their devices, its open invites, and a hash of every
/// data key it used before its current one.
class AccessList
{
public:
    AccessList() = default;
    AccessList(const QList<AccessUser>& users, const QList<AccessInvite>& invites, const QList<QByteArray>& formerKeys);

    /// A new ward's list: its creator, an admin, with this device and nobody else.
    static AccessList founding(const UserId& user, const QString& userName, const QString& deviceName,
                               const Identity& device);

    /// What a former data key is kept as: 32-byte BLAKE2b of the key, personalized
    /// wp-former-key.
    static QByteArray formerKeyHash(const QByteArray& dataKey);

    const QList<AccessUser>& users() const { return m_users; }
    const QList<AccessInvite>& invites() const { return m_invites; }
    const QList<QByteArray>& formerKeys() const { return m_formerKeys; }

    /// Every device of every user, then every open invite: exactly the keys every write seals
    /// the data key to.
    QList<RecipientKeys> recipients() const;

    /// The fingerprints of recipients(): whom a write seals the data key to.
    QSet<QByteArray> recipientFingerprints() const;

    /// The fingerprints of every device of every user.
    QSet<QByteArray> deviceFingerprints() const;

    /// The verify key of the device or open invite whose fingerprint is fingerprint, or none
    /// when the list holds neither.
    std::optional<QByteArray> verifyKeyOf(const QByteArray& fingerprint) const;

    std::optional<AccessUser> findUser(const UserId& id) const;

    /// The user holding the device whose fingerprint is device, or none.
    std::optional<AccessUser> userOfDevice(const QByteArray& device) const;

    std::optional<AccessDevice> findDevice(const QByteArray& device) const;
    std::optional<AccessInvite> findInvite(const QByteArray& invite) const;

    /// The admins' names, sorted without regard to case.
    QStringList adminNames() const;

    /// How the interface names an open invite: "for Bob" for a user invite labelled Bob, "a
    /// device for Alice" for a device invite Alice made.
    QString describeInvite(const AccessInvite& invite) const;

    /// Replaces the user who has user's id with user, or adds user at the end when there is
    /// none.
    void setUser(const AccessUser& user);

    /// Takes the user out, with every device they hold and every invite they made.
    void removeUser(const UserId& id);

    /// Takes the device whose fingerprint is device out of whichever user holds it.
    void removeDevice(const QByteArray& device);

    void addInvite(const AccessInvite& invite) { m_invites.append(invite); }

    /// Takes the open invite whose fingerprint is invite out.
    void removeInvite(const QByteArray& invite);

    void setFormerKeys(const QList<QByteArray>& formerKeys) { m_formerKeys = formerKeys; }

    QJsonObject toJson() const;
    static AccessList fromJson(JsonReader json);

    bool operator==(const AccessList& other) const;
    bool operator!=(const AccessList& other) const { return !(*this == other); }

private:
    QList<AccessUser> m_users;
    QList<AccessInvite> m_invites;
    QList<QByteArray> m_formerKeys;
};
```

### `src/models/AccessList.cpp`

The anonymous `roleName` and `readRole` become the public `accessRoleName` and
`readAccessRole`, and every use of them in the file is renamed to match. Add `#include
<QObject>`. The added functions:

```cpp
QString accessRoleName(AccessRole role)
{
    switch (role)
    {
    case AccessRole::Admin:
        return "admin";
    case AccessRole::Member:
        return "member";
    }
    return QString();
}

AccessRole readAccessRole(JsonReader& json, const QString& key)
{
    return json.word(key, QList<AccessRole>{AccessRole::Admin, AccessRole::Member}, &accessRoleName);
}

QByteArray AccessDevice::fingerprint() const
{
    return Identity::fingerprintOf(m_publicKey, m_verifyKey);
}

QByteArray AccessInvite::fingerprint() const
{
    return Identity::fingerprintOf(m_publicKey, m_verifyKey);
}

QByteArray AccessList::formerKeyHash(const QByteArray& dataKey)
{
    return Crypto::hash(dataKey, Crypto::HashBytes, "wp-former-key");
}

QSet<QByteArray> AccessList::recipientFingerprints() const
{
    QSet<QByteArray> recipients = deviceFingerprints();
    for (const AccessInvite& invite : m_invites)
    {
        recipients.insert(invite.fingerprint());
    }
    return recipients;
}

QSet<QByteArray> AccessList::deviceFingerprints() const
{
    QSet<QByteArray> devices;
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& device : user.devices())
        {
            devices.insert(device.fingerprint());
        }
    }
    return devices;
}

std::optional<AccessUser> AccessList::findUser(const UserId& id) const
{
    for (const AccessUser& user : m_users)
    {
        if (user.id() == id)
        {
            return user;
        }
    }
    return std::nullopt;
}

std::optional<AccessUser> AccessList::userOfDevice(const QByteArray& device) const
{
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& entry : user.devices())
        {
            if (entry.fingerprint() == device)
            {
                return user;
            }
        }
    }
    return std::nullopt;
}

std::optional<AccessDevice> AccessList::findDevice(const QByteArray& device) const
{
    for (const AccessUser& user : m_users)
    {
        for (const AccessDevice& entry : user.devices())
        {
            if (entry.fingerprint() == device)
            {
                return entry;
            }
        }
    }
    return std::nullopt;
}

std::optional<AccessInvite> AccessList::findInvite(const QByteArray& invite) const
{
    for (const AccessInvite& entry : m_invites)
    {
        if (entry.fingerprint() == invite)
        {
            return entry;
        }
    }
    return std::nullopt;
}

QStringList AccessList::adminNames() const
{
    QStringList names;
    for (const AccessUser& user : m_users)
    {
        if (user.role() == AccessRole::Admin)
        {
            names.append(user.name());
        }
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString AccessList::describeInvite(const AccessInvite& invite) const
{
    if (invite.invitedUser().has_value())
    {
        return QObject::tr("for %1").arg(invite.invitedUser()->label);
    }
    return QObject::tr("a device for %1").arg(findUser(invite.maker())->name());
}

void AccessList::setUser(const AccessUser& user)
{
    for (AccessUser& existing : m_users)
    {
        if (existing.id() == user.id())
        {
            existing = user;
            return;
        }
    }
    m_users.append(user);
}

void AccessList::removeUser(const UserId& id)
{
    m_users.removeIf([&id](const AccessUser& user) { return user.id() == id; });
    m_invites.removeIf([&id](const AccessInvite& invite) { return invite.maker() == id; });
}

void AccessList::removeDevice(const QByteArray& device)
{
    for (AccessUser& user : m_users)
    {
        QList<AccessDevice> devices = user.devices();
        devices.removeIf([&device](const AccessDevice& entry) { return entry.fingerprint() == device; });
        user.setDevices(devices);
    }
}

void AccessList::removeInvite(const QByteArray& invite)
{
    m_invites.removeIf([&invite](const AccessInvite& entry) { return entry.fingerprint() == invite; });
}
```

A device invite's maker is always on the list, since removing a user takes the invites they made
with them, so `describeInvite` reads the maker's name directly.

### `src/commands/FieldChangeJson.h` and `src/commands/FieldChangeJson.cpp`

`FieldChangeJson.h` includes `AccessList.h` and declares:

```cpp
QJsonValue toJsonValue(AccessRole value);
```

```cpp
template<>
AccessRole readValue<AccessRole>(JsonReader& json, const QString& key);
```

```cpp
QJsonValue toJsonValue(AccessRole value)
{
    return accessRoleName(value);
}

template<>
AccessRole readValue<AccessRole>(JsonReader& json, const QString& key)
{
    return readAccessRole(json, key);
}
```

### `src/commands/AccessRules.h`

```cpp
#pragma once

#include "AccessList.h"

#include <QByteArray>
#include <QString>

/// The rules that decide which access-list changes a device's user may make. Each answers the
/// wording the app gives when the rule refuses, or an empty string when it allows. A command
/// asks its rule of the version in front of it, the first time and on replay, about the user
/// of the device that made it, named by that device's fingerprint (actor); the interface asks
/// the same rule before it offers the action.
class AccessRules
{
public:
    /// Making an invite: a user invite is for admins only; anyone adds their own devices.
    static QString invite(const AccessList& access, const QByteArray& actor, bool forUser);

    /// Removing a user: admins only, and never yourself.
    static QString removeUser(const AccessList& access, const QByteArray& actor, const UserId& user);

    /// Removing a device: only one of your own, and never your last.
    static QString removeDevice(const AccessList& access, const QByteArray& actor, const QByteArray& device);

    /// Withdrawing an invite: its maker, or an admin.
    static QString withdrawInvite(const AccessList& access, const QByteArray& actor, const QByteArray& invite);

    /// Setting a role: admins only, and the only admin cannot step down.
    static QString setRole(const AccessList& access, const QByteArray& actor, const UserId& user, AccessRole role);

    /// Joining device through invite: only while the list holds the invite, or already holds
    /// the device, which is the join's result.
    static QString join(const AccessList& access, const QByteArray& invite, const QByteArray& device);
};
```

### `src/commands/AccessRules.cpp`

```cpp
#include "AccessRules.h"

#include <QObject>

namespace
{

/// What a member is told when they ask for what only admins do.
QString askAnAdmin(const AccessList& access)
{
    return QObject::tr("To add or remove users, ask an admin: %1").arg(access.adminNames().join(", "));
}

QString useLeave()
{
    return QObject::tr("To stop using this ward, use Leave.");
}

bool isAdmin(const AccessList& access, const QByteArray& actor)
{
    std::optional<AccessUser> user = access.userOfDevice(actor);
    return user.has_value() && user->role() == AccessRole::Admin;
}

}  // namespace

QString AccessRules::invite(const AccessList& access, const QByteArray& actor, bool forUser)
{
    if (forUser && !isAdmin(access, actor))
    {
        return askAnAdmin(access);
    }
    return QString();
}

QString AccessRules::removeUser(const AccessList& access, const QByteArray& actor, const UserId& user)
{
    if (!isAdmin(access, actor))
    {
        return askAnAdmin(access);
    }
    if (access.userOfDevice(actor)->id() == user)
    {
        return useLeave();
    }
    return QString();
}

QString AccessRules::removeDevice(const AccessList& access, const QByteArray& actor, const QByteArray& device)
{
    std::optional<AccessUser> owner = access.userOfDevice(device);
    if (!owner.has_value())
    {
        return QString();
    }
    if (owner->id() != access.userOfDevice(actor)->id())
    {
        return QObject::tr("You can remove only your own devices.");
    }
    if (owner->devices().size() == 1)
    {
        return useLeave();
    }
    return QString();
}

QString AccessRules::withdrawInvite(const AccessList& access, const QByteArray& actor, const QByteArray& invite)
{
    std::optional<AccessInvite> entry = access.findInvite(invite);
    if (!entry.has_value()
        || isAdmin(access, actor)
        || entry->maker() == access.userOfDevice(actor)->id())
    {
        return QString();
    }
    return askAnAdmin(access);
}

QString AccessRules::setRole(const AccessList& access, const QByteArray& actor, const UserId& user, AccessRole role)
{
    if (!isAdmin(access, actor))
    {
        return askAnAdmin(access);
    }
    std::optional<AccessUser> target = access.findUser(user);
    if (target.has_value()
        && target->role() == AccessRole::Admin
        && role == AccessRole::Member
        && access.adminNames().size() == 1)
    {
        return QObject::tr("You are the only admin. Make someone else an admin first.");
    }
    return QString();
}

QString AccessRules::join(const AccessList& access, const QByteArray& invite, const QByteArray& device)
{
    if (!access.findInvite(invite).has_value() && !access.findDevice(device).has_value())
    {
        return QObject::tr("This invite has been used or withdrawn.");
    }
    return QString();
}
```

A user or device a removal names that is already gone, an invite a withdrawal names that is
already gone, and a joining device already on the list pass their rules, so the command's result
already holds and it applies. Every
command's actor is the device that made it, which is on the access list of every version that
device takes, so the rules read the actor's user directly.

### `src/commands/AccessCommands.h`

```cpp
#pragma once

#include "AccessList.h"
#include "Command.h"
#include "FieldChange.h"
#include "JsonReader.h"

#include <QByteArray>
#include <optional>

class Identity;

/// Adds an open invite, made by the user of the device actor. Its result holds when the invite
/// is there already. Its rule: a user invite is for admins only.
class InviteCommand : public Command
{
public:
    static constexpr const char* TypeName = "invite";

    /// An invite under keys, made on the device actor in access: for user when it is set,
    /// otherwise for one of the maker's devices.
    static CommandPtr create(const QByteArray& actor, const AccessList& access, const std::optional<InvitedUser>& user,
                             const Identity& keys);

    InviteCommand(const QByteArray& actor, const AccessInvite& invite, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    AccessInvite m_invite;
};

/// Removes a user: their entry, every device it holds by then, and every invite they have made
/// by then. It never conflicts, since its intent is that this person is out: a device they
/// added or an invite they made meanwhile goes with them, and a user already gone is its result
/// already holding. The version that carries it rotates the data key.
class RemoveUserCommand : public Command
{
public:
    static constexpr const char* TypeName = "removeUser";

    /// The removal of user, made on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessUser& user);

    RemoveUserCommand(const QByteArray& actor, const UserId& userId, const CommandDescription& description);

    const UserId& userId() const { return m_userId; }

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    UserId m_userId;
};

/// Removes one device from whichever user holds it. Its result holds when the device is gone.
/// Its rule: only one of the actor's own devices, and never their last.
class RemoveDeviceCommand : public Command
{
public:
    static constexpr const char* TypeName = "removeDevice";

    /// The removal of device, made on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessDevice& device);

    RemoveDeviceCommand(const QByteArray& actor, const QByteArray& device, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    QByteArray m_device;  ///< the fingerprint of the device removed
};

/// Takes an open invite out. Its result holds when the invite is gone. Its rule: its maker, or
/// an admin.
class WithdrawInviteCommand : public Command
{
public:
    static constexpr const char* TypeName = "withdrawInvite";

    /// The withdrawal of invite, which access holds, made on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessInvite& invite, const AccessList& access);

    WithdrawInviteCommand(const QByteArray& actor, const QByteArray& invite, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    QByteArray m_invite;  ///< the fingerprint of the invite withdrawn
};

/// Sets one user's role, judged per value as UpdateFamilyCommand judges a field: it conflicts
/// when the user is gone, and with two roles it meets nothing else to conflict with. Its rule:
/// admins only, and the only admin cannot step down.
class SetRoleCommand : public Command
{
public:
    static constexpr const char* TypeName = "setRole";

    /// user's role, as the user saw it, set to role on the device actor.
    static CommandPtr create(const QByteArray& actor, const AccessUser& user, AccessRole role);

    SetRoleCommand(const QByteArray& actor, const UserId& userId, const FieldChange<AccessRole>& role,
                   const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    UserId m_userId;
    FieldChange<AccessRole> m_role;
};

/// Renames the user of the device that made it, judged per value. It names no other user, so a
/// device renames only its own user.
class RenameUserCommand : public Command
{
public:
    static constexpr const char* TypeName = "renameUser";

    /// The user of the device actor in access renamed to name, or an empty pointer when access
    /// names them so already, which WardSession::commit takes as nothing to do.
    static CommandPtr create(const QByteArray& actor, const AccessList& access, const QString& name);

    RenameUserCommand(const QByteArray& actor, const FieldChange<QString>& name, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    FieldChange<QString> m_name;
};

/// Renames the device that made it, judged per value. It names no other device, so a device
/// renames only itself.
class RenameDeviceCommand : public Command
{
public:
    static constexpr const char* TypeName = "renameDevice";

    /// The device actor in access renamed to name, or an empty pointer when access names it so
    /// already.
    static CommandPtr create(const QByteArray& actor, const AccessList& access, const QString& name);

    RenameDeviceCommand(const QByteArray& actor, const FieldChange<QString>& name, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    QByteArray m_actor;
    FieldChange<QString> m_name;
};

/// The user a device joining through a user invite becomes: a new id, and the name they gave.
struct JoiningUser
{
    UserId id;
    QString name;
};

/// Joins the ward through an open invite: the invite's entry becomes the joining device's,
/// under a new user with the invite's role for a user invite, and as one of the maker's
/// devices for a device invite. The joining device runs it on the version its invitation
/// carries, and runs it again on any version it takes that names the invite and not the
/// device. Its result holds once the list holds the device; its rule refuses it once the
/// invite is used or withdrawn and the device is not on the list.
class JoinCommand : public Command
{
public:
    static constexpr const char* TypeName = "join";

    /// Joining the ward named wardName through invite as device: under a new user named
    /// userName for a user invite, and as one of the invite's maker's devices for a device
    /// invite, which leaves userName unused.
    static CommandPtr create(const AccessInvite& invite, const AccessDevice& device, const QString& userName,
                             const QString& wardName);

    /// The device whose fingerprint is device joining the ward named wardName through invite
    /// again, as held holds it: under the same device entry, and for a user invite as the same
    /// user, id and name, so every copy ends with one entry for it.
    static CommandPtr rejoin(const AccessInvite& invite, const AccessList& held, const QByteArray& device,
                             const QString& wardName);

    JoinCommand(const AccessInvite& invite, const AccessDevice& device, const std::optional<JoiningUser>& user,
                const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    QString refusal(const Document& document) const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(JsonReader json, const CommandDescription& description);

private:
    AccessInvite m_invite;
    AccessDevice m_device;
    std::optional<JoiningUser> m_user;  ///< for a user invite
};
```

Every access command derives from `Command` and not `UndoableCommand` (*Saving*: access-list
commands are not undoable), and its `documentChange` is `DocumentChange::full()`, since the
access list feeds no targeted view.

### `src/commands/AccessCommands.cpp`

```cpp
#include "AccessCommands.h"
#include "AccessRules.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "Document.h"
#include "FieldChangeJson.h"
#include "Identity.h"

#include <QObject>

namespace
{

QByteArray readActor(JsonReader& json)
{
    return json.bytes("actor", Crypto::FingerprintBytes);
}

UserId readUserId(JsonReader& json, const QString& key)
{
    return UserId::fromBytes(json.bytes(key, Crypto::IdBytes));
}

}  // namespace

// ============================================================================
// InviteCommand
// ============================================================================

CommandPtr InviteCommand::create(const QByteArray& actor, const AccessList& access,
                                 const std::optional<InvitedUser>& user, const Identity& keys)
{
    UserId maker = access.userOfDevice(actor)->id();
    if (user.has_value())
    {
        AccessInvite invite = AccessInvite::forUser(maker, user->role, user->label, keys.publicKey(), keys.verifyKey());
        CommandDescription description(QObject::tr("Add a user: %1").arg(user->label), {});
        return std::make_shared<InviteCommand>(actor, invite, description);
    }
    AccessInvite invite = AccessInvite::forDevice(maker, keys.publicKey(), keys.verifyKey());
    return std::make_shared<InviteCommand>(actor, invite, CommandDescription(QObject::tr("Add a device"), {}));
}

InviteCommand::InviteCommand(const QByteArray& actor, const AccessInvite& invite, const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_invite(invite)
{
}

CommandOutcome InviteCommand::execute(Document& document) const
{
    AccessList access = document.access();
    if (!access.findInvite(m_invite.fingerprint()).has_value())
    {
        access.addInvite(m_invite);
        document.setAccess(access);
    }
    return CommandOutcome::Applied;
}

QString InviteCommand::refusal(const Document& document) const
{
    return AccessRules::invite(document.access(), m_actor, m_invite.invitedUser().has_value());
}

QJsonObject InviteCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["invite"] = m_invite.toJson();
    return json;
}

DocumentChange InviteCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr InviteCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    AccessInvite invite = AccessInvite::fromJson(json.object("invite"));
    json.finish();
    return std::make_shared<InviteCommand>(actor, invite, description);
}

// ============================================================================
// RemoveUserCommand
// ============================================================================

CommandPtr RemoveUserCommand::create(const QByteArray& actor, const AccessUser& user)
{
    CommandDescription description(QObject::tr("Remove %1").arg(user.name()), {});
    return std::make_shared<RemoveUserCommand>(actor, user.id(), description);
}

RemoveUserCommand::RemoveUserCommand(const QByteArray& actor, const UserId& userId,
                                     const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_userId(userId)
{
}

CommandOutcome RemoveUserCommand::execute(Document& document) const
{
    AccessList access = document.access();
    access.removeUser(m_userId);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString RemoveUserCommand::refusal(const Document& document) const
{
    return AccessRules::removeUser(document.access(), m_actor, m_userId);
}

QJsonObject RemoveUserCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["user"] = m_userId.toString();
    return json;
}

DocumentChange RemoveUserCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RemoveUserCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    UserId user = readUserId(json, "user");
    json.finish();
    return std::make_shared<RemoveUserCommand>(actor, user, description);
}

// ============================================================================
// RemoveDeviceCommand
// ============================================================================

CommandPtr RemoveDeviceCommand::create(const QByteArray& actor, const AccessDevice& device)
{
    CommandDescription description(QObject::tr("Remove device %1").arg(device.name()), {});
    return std::make_shared<RemoveDeviceCommand>(actor, device.fingerprint(), description);
}

RemoveDeviceCommand::RemoveDeviceCommand(const QByteArray& actor, const QByteArray& device,
                                         const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_device(device)
{
}

CommandOutcome RemoveDeviceCommand::execute(Document& document) const
{
    AccessList access = document.access();
    access.removeDevice(m_device);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString RemoveDeviceCommand::refusal(const Document& document) const
{
    return AccessRules::removeDevice(document.access(), m_actor, m_device);
}

QJsonObject RemoveDeviceCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["device"] = Base64Url::encode(m_device);
    return json;
}

DocumentChange RemoveDeviceCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RemoveDeviceCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    QByteArray device = json.bytes("device", Crypto::FingerprintBytes);
    json.finish();
    return std::make_shared<RemoveDeviceCommand>(actor, device, description);
}

// ============================================================================
// WithdrawInviteCommand
// ============================================================================

CommandPtr WithdrawInviteCommand::create(const QByteArray& actor, const AccessInvite& invite, const AccessList& access)
{
    QString heading = invite.invitedUser().has_value()
        ? QObject::tr("Withdraw the invite for %1").arg(invite.invitedUser()->label)
        : QObject::tr("Withdraw the device invite from %1").arg(access.findUser(invite.maker())->name());
    return std::make_shared<WithdrawInviteCommand>(actor, invite.fingerprint(), CommandDescription(heading, {}));
}

WithdrawInviteCommand::WithdrawInviteCommand(const QByteArray& actor, const QByteArray& invite,
                                             const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_invite(invite)
{
}

CommandOutcome WithdrawInviteCommand::execute(Document& document) const
{
    AccessList access = document.access();
    access.removeInvite(m_invite);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString WithdrawInviteCommand::refusal(const Document& document) const
{
    return AccessRules::withdrawInvite(document.access(), m_actor, m_invite);
}

QJsonObject WithdrawInviteCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["invite"] = Base64Url::encode(m_invite);
    return json;
}

DocumentChange WithdrawInviteCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr WithdrawInviteCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    QByteArray invite = json.bytes("invite", Crypto::FingerprintBytes);
    json.finish();
    return std::make_shared<WithdrawInviteCommand>(actor, invite, description);
}

// ============================================================================
// SetRoleCommand
// ============================================================================

CommandPtr SetRoleCommand::create(const QByteArray& actor, const AccessUser& user, AccessRole role)
{
    QString heading = role == AccessRole::Admin
        ? QObject::tr("Make %1 an admin").arg(user.name())
        : QObject::tr("Make %1 a member").arg(user.name());
    return std::make_shared<SetRoleCommand>(actor, user.id(), FieldChange<AccessRole>(user.role(), role),
                                            CommandDescription(heading, {}));
}

SetRoleCommand::SetRoleCommand(const QByteArray& actor, const UserId& userId, const FieldChange<AccessRole>& role,
                               const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_userId(userId)
    , m_role(role)
{
}

CommandOutcome SetRoleCommand::execute(Document& document) const
{
    AccessList access = document.access();
    std::optional<AccessUser> user = access.findUser(m_userId);
    if (!user.has_value() || !m_role.holds(user->role()))
    {
        return CommandOutcome::Conflicted;
    }
    user->setRole(m_role.after());
    access.setUser(*user);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString SetRoleCommand::refusal(const Document& document) const
{
    return AccessRules::setRole(document.access(), m_actor, m_userId, m_role.after());
}

QJsonObject SetRoleCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["user"] = m_userId.toString();
    json["role"] = changeToJson(m_role);
    return json;
}

DocumentChange SetRoleCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr SetRoleCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    UserId user = readUserId(json, "user");
    FieldChange<AccessRole> role = changeFromJson<AccessRole>(json.object("role"));
    json.finish();
    return std::make_shared<SetRoleCommand>(actor, user, role, description);
}

// ============================================================================
// RenameUserCommand
// ============================================================================

CommandPtr RenameUserCommand::create(const QByteArray& actor, const AccessList& access, const QString& name)
{
    QString current = access.userOfDevice(actor)->name();
    if (current == name)
    {
        return nullptr;
    }
    return std::make_shared<RenameUserCommand>(actor, FieldChange<QString>(current, name),
                                               CommandDescription(QObject::tr("Rename yourself to %1").arg(name), {}));
}

RenameUserCommand::RenameUserCommand(const QByteArray& actor, const FieldChange<QString>& name,
                                     const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_name(name)
{
}

CommandOutcome RenameUserCommand::execute(Document& document) const
{
    AccessList access = document.access();
    AccessUser user = *access.userOfDevice(m_actor);
    if (!m_name.holds(user.name()))
    {
        return CommandOutcome::Conflicted;
    }
    user.setName(m_name.after());
    access.setUser(user);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QJsonObject RenameUserCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["name"] = changeToJson(m_name);
    return json;
}

DocumentChange RenameUserCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RenameUserCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    FieldChange<QString> name = changeFromJson<QString>(json.object("name"));
    json.finish();
    return std::make_shared<RenameUserCommand>(actor, name, description);
}

// ============================================================================
// RenameDeviceCommand
// ============================================================================

CommandPtr RenameDeviceCommand::create(const QByteArray& actor, const AccessList& access, const QString& name)
{
    QString current = access.findDevice(actor)->name();
    if (current == name)
    {
        return nullptr;
    }
    return std::make_shared<RenameDeviceCommand>(
        actor, FieldChange<QString>(current, name),
        CommandDescription(QObject::tr("Rename this device to %1").arg(name), {}));
}

RenameDeviceCommand::RenameDeviceCommand(const QByteArray& actor, const FieldChange<QString>& name,
                                         const CommandDescription& description)
    : Command(description)
    , m_actor(actor)
    , m_name(name)
{
}

CommandOutcome RenameDeviceCommand::execute(Document& document) const
{
    AccessList access = document.access();
    AccessUser user = *access.userOfDevice(m_actor);
    QList<AccessDevice> devices = user.devices();
    for (AccessDevice& device : devices)
    {
        if (device.fingerprint() == m_actor)
        {
            if (!m_name.holds(device.name()))
            {
                return CommandOutcome::Conflicted;
            }
            device.setName(m_name.after());
        }
    }
    user.setDevices(devices);
    access.setUser(user);
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QJsonObject RenameDeviceCommand::toJson() const
{
    QJsonObject json;
    json["actor"] = Base64Url::encode(m_actor);
    json["name"] = changeToJson(m_name);
    return json;
}

DocumentChange RenameDeviceCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr RenameDeviceCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    QByteArray actor = readActor(json);
    FieldChange<QString> name = changeFromJson<QString>(json.object("name"));
    json.finish();
    return std::make_shared<RenameDeviceCommand>(actor, name, description);
}

// ============================================================================
// JoinCommand
// ============================================================================

CommandPtr JoinCommand::create(const AccessInvite& invite, const AccessDevice& device, const QString& userName,
                               const QString& wardName)
{
    std::optional<JoiningUser> user;
    if (invite.invitedUser().has_value())
    {
        user = JoiningUser{UserId::generate(), userName};
    }
    return std::make_shared<JoinCommand>(invite, device, user,
                                         CommandDescription(QObject::tr("Join %1").arg(wardName), {}));
}

CommandPtr JoinCommand::rejoin(const AccessInvite& invite, const AccessList& held, const QByteArray& device,
                               const QString& wardName)
{
    std::optional<JoiningUser> user;
    if (invite.invitedUser().has_value())
    {
        AccessUser joined = *held.userOfDevice(device);
        user = JoiningUser{joined.id(), joined.name()};
    }
    return std::make_shared<JoinCommand>(invite, *held.findDevice(device), user,
                                         CommandDescription(QObject::tr("Join %1").arg(wardName), {}));
}

JoinCommand::JoinCommand(const AccessInvite& invite, const AccessDevice& device,
                         const std::optional<JoiningUser>& user, const CommandDescription& description)
    : Command(description)
    , m_invite(invite)
    , m_device(device)
    , m_user(user)
{
}

CommandOutcome JoinCommand::execute(Document& document) const
{
    AccessList access = document.access();
    if (access.findDevice(m_device.fingerprint()).has_value())
    {
        return CommandOutcome::Applied;
    }
    access.removeInvite(m_invite.fingerprint());
    if (m_user.has_value())
    {
        access.setUser(AccessUser(m_user->id, m_user->name, m_invite.invitedUser()->role, {m_device}));
    }
    else
    {
        AccessUser maker = *access.findUser(m_invite.maker());
        QList<AccessDevice> devices = maker.devices();
        devices.append(m_device);
        maker.setDevices(devices);
        access.setUser(maker);
    }
    document.setAccess(access);
    return CommandOutcome::Applied;
}

QString JoinCommand::refusal(const Document& document) const
{
    return AccessRules::join(document.access(), m_invite.fingerprint(), m_device.fingerprint());
}

QJsonObject JoinCommand::toJson() const
{
    QJsonObject json;
    json["invite"] = m_invite.toJson();
    json["device"] = m_device.toJson();
    if (m_user.has_value())
    {
        QJsonObject user;
        user["id"] = m_user->id.toString();
        user["name"] = m_user->name;
        json["user"] = user;
    }
    return json;
}

DocumentChange JoinCommand::documentChange() const
{
    return DocumentChange::full();
}

CommandPtr JoinCommand::fromJson(JsonReader json, const CommandDescription& description)
{
    AccessInvite invite = AccessInvite::fromJson(json.object("invite"));
    AccessDevice device = AccessDevice::fromJson(json.object("device"));
    std::optional<JoiningUser> user;
    if (invite.invitedUser().has_value())
    {
        JsonReader userJson = json.object("user");
        UserId id = readUserId(userJson, "id");
        QString name = userJson.string("name");
        userJson.finish();
        user = JoiningUser{id, name};
    }
    json.finish();
    return std::make_shared<JoinCommand>(invite, device, user, description);
}
```

A device-invite join's maker is on the list while the invite is, since removing a user takes the
invites they made, and when the device is not on the list the join's rule has already asked
that the invite is there, so `execute` reads the maker directly. A join carrying a `user` for a device invite is refused by
`json.finish()`, which finds `user` not taken.

### `src/commands/CommandRegistry.cpp`

Add `#include "AccessCommands.h"` and, at the end of `types()`:

```cpp
        {InviteCommand::TypeName, &InviteCommand::fromJson},
        {RemoveUserCommand::TypeName, &RemoveUserCommand::fromJson},
        {RemoveDeviceCommand::TypeName, &RemoveDeviceCommand::fromJson},
        {WithdrawInviteCommand::TypeName, &WithdrawInviteCommand::fromJson},
        {SetRoleCommand::TypeName, &SetRoleCommand::fromJson},
        {RenameUserCommand::TypeName, &RenameUserCommand::fromJson},
        {RenameDeviceCommand::TypeName, &RenameDeviceCommand::fromJson},
        {JoinCommand::TypeName, &JoinCommand::fromJson},
```

### `src/services/DeviceState.h`

Given whole:

```cpp
#pragma once

#include "Journal.h"
#include "JsonReader.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <optional>

/// What belongs to this device alone, saved in its local file beside the ward and never in
/// a version another device reads.
struct DeviceState
{
    Journal journal;                     ///< with its sequence counter
    QSet<QByteArray> heldRecipients;     ///< the fingerprints the version this device holds seals to
    QList<QByteArray> inviteSeeds;       ///< the seed of each open invite this device made, while the access list names it
    std::optional<QByteArray> joinSeed;  ///< the seed of the invite this device joined through, for as long as it holds the ward
    bool noLongerShared = false;         ///< a version this device took left it out, so its copy is read-only

    QJsonObject toJson() const;
    static DeviceState fromJson(JsonReader json);

    bool operator==(const DeviceState& other) const;
};
```

### `src/services/DeviceState.cpp`

Given whole:

```cpp
#include "DeviceState.h"
#include "Base64Url.h"
#include "Crypto.h"

#include <QJsonArray>
#include <QStringList>

QJsonObject DeviceState::toJson() const
{
    QJsonObject json;
    json["journal"] = journal.toJson();
    QStringList held;
    for (const QByteArray& fingerprint : heldRecipients)
    {
        held.append(Base64Url::encode(fingerprint));
    }
    held.sort();
    json["heldRecipients"] = QJsonArray::fromStringList(held);
    if (!inviteSeeds.isEmpty())
    {
        QJsonArray seeds;
        for (const QByteArray& seed : inviteSeeds)
        {
            seeds.append(Base64Url::encode(seed));
        }
        json["inviteSeeds"] = seeds;
    }
    if (joinSeed.has_value())
    {
        json["joinSeed"] = Base64Url::encode(*joinSeed);
    }
    if (noLongerShared)
    {
        json["noLongerShared"] = true;
    }
    return json;
}

DeviceState DeviceState::fromJson(JsonReader json)
{
    DeviceState device;
    device.journal = Journal::fromJson(json.object("journal"));
    const QList<QByteArray> held = json.bytesList("heldRecipients", Crypto::FingerprintBytes);
    device.heldRecipients = QSet<QByteArray>(held.begin(), held.end());
    device.inviteSeeds = json.optionalBytesList("inviteSeeds", Crypto::KeyBytes);
    device.joinSeed = json.optionalBytes("joinSeed", Crypto::KeyBytes);
    device.noLongerShared = json.optionalFlag("noLongerShared");
    json.finish();
    return device;
}

bool DeviceState::operator==(const DeviceState& other) const
{
    return journal == other.journal
        && heldRecipients == other.heldRecipients
        && inviteSeeds == other.inviteSeeds
        && joinSeed == other.joinSeed
        && noLongerShared == other.noLongerShared;
}
```

### `src/services/InvitationFile.h`

```cpp
#pragma once

#include "Id.h"

#include <QByteArray>
#include <QString>

enum class InvitationReadStatus
{
    Read,
    Newer,           ///< its format is above every version this app reads
    NotAnInvitation  ///< it is not an invitation this app reads; problem says where
};

/// An invitation file split into its parts.
struct InvitationRead
{
    InvitationReadStatus status;
    QString problem;      ///< when NotAnInvitation
    DocumentId document;  ///< when Read
    QByteArray seed;      ///< when Read
    QByteArray version;   ///< when Read: the version, in a ward file's framing
};

/// A .wardinvite file: one line of compact JSON holding its own format, the document id and
/// the invite's seed, one newline, then the version that opened the invite, in a ward file's
/// framing, with no device part.
class InvitationFile
{
public:
    /// The format this app writes, and the only one it reads.
    static constexpr int Format = 1;

    /// The bytes of an invitation to document for the invite seed gives, carrying version.
    static QByteArray write(const DocumentId& document, const QByteArray& seed, const QByteArray& version);

    /// bytes split at the first newline, the line read strictly: a format above Format is
    /// Newer, and anything that is not this format's line is NotAnInvitation.
    static InvitationRead read(const QByteArray& bytes);

    /// The name an invitation to the ward named wardName is saved under, from how the
    /// interface names the invite: Tanglewood Ward - for Bob.wardinvite.
    static QString fileName(const QString& wardName, const QString& inviteDescription);
};
```

### `src/services/InvitationFile.cpp`

```cpp
#include "InvitationFile.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "JsonReader.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace
{

InvitationRead notAnInvitation(const QString& problem)
{
    return InvitationRead{InvitationReadStatus::NotAnInvitation, problem, DocumentId(), QByteArray(), QByteArray()};
}

}  // namespace

QByteArray InvitationFile::write(const DocumentId& document, const QByteArray& seed, const QByteArray& version)
{
    QJsonObject json;
    json["format"] = Format;
    json["document"] = document.toString();
    json["seed"] = Base64Url::encode(seed);
    return QJsonDocument(json).toJson(QJsonDocument::Compact) + '\n' + version;
}

InvitationRead InvitationFile::read(const QByteArray& bytes)
{
    qsizetype boundary = bytes.indexOf('\n');
    if (boundary < 0)
    {
        return notAnInvitation("the file has no invitation line");
    }
    std::optional<QJsonObject> object = JsonReader::parseObject(bytes.left(boundary));
    if (!object.has_value())
    {
        return notAnInvitation("the invitation line is not a JSON object");
    }
    QJsonValue format = object->value("format");
    if (!JsonReader::isInteger(format))
    {
        return notAnInvitation("format: is not an integer");
    }
    if (format.toDouble() > Format)
    {
        return InvitationRead{InvitationReadStatus::Newer, QString(), DocumentId(), QByteArray(), QByteArray()};
    }
    if (format.toDouble() != Format)
    {
        return notAnInvitation("format: is not a version this app reads");
    }

    JsonReader json(*object);
    json.integer("format");
    QByteArray document = json.bytes("document", Crypto::IdBytes);
    QByteArray seed = json.bytes("seed", Crypto::KeyBytes);
    json.finish();
    if (json.failed())
    {
        return notAnInvitation(json.problem());
    }
    return InvitationRead{InvitationReadStatus::Read, QString(), DocumentId::fromBytes(document), seed,
                          bytes.mid(boundary + 1)};
}

QString InvitationFile::fileName(const QString& wardName, const QString& inviteDescription)
{
    return QString("%1 - %2.wardinvite").arg(wardName, inviteDescription);
}
```

A ward file's first line is its header, which has `format` 1 and `document` but no `seed`, and
holds keys the invitation line does not, so it is refused; an update file's first line is a
signature, which is not JSON.

### `src/services/WardSession.h`

Given whole. What changes from the current header: `ReplayResult` gains `stillHaveAccess`;
`TakeStatus` gains `InviteUsedOrWithdrawn` and `NotNamed` now ends the share; the invitation
types; the sharing functions; the constructor keeps the geocoder and unit lookup to open other
wards for a rename; and the private parts that write and take versions.

```cpp
#pragma once

#include "AccessList.h"
#include "AutomaticUpdates.h"
#include "Command.h"
#include "DeviceState.h"
#include "Document.h"
#include "DocumentChange.h"
#include "Identity.h"
#include "Journal.h"
#include "WardFile.h"
#include "WardHeader.h"
#include "WardPayload.h"
#include "WardStore.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

class Geocoder;
class UnitLookup;

/// How the commit step ended for one command.
enum class CommitStatus
{
    Applied,
    Conflicted,
    Refused,
    NothingToDo  ///< there was no command, as from an edit that changed nothing
};

struct CommitResult
{
    CommitStatus status;
    QString refusal;  ///< the rule's wording, when status is Refused
    CommandDescription description;  ///< the description of the command that ran
};

/// How a replay onto a version ended.
enum class ReplayStatus
{
    FastForwarded,  ///< the journal was empty, and the version was taken as it stands
    Replayed,       ///< every entry applied on top of the version
    Abandoned       ///< one entry stopped it, and the version was taken as it stands
};

struct ReplayResult
{
    ReplayStatus status;
    QString toRedo;               ///< when status is Abandoned, the abandoned changes as plain text
    QStringList stillHaveAccess;  ///< when status is Abandoned, each user an abandoned removal named whom the version holds
};

/// Whether an update file was taken.
enum class TakeStatus
{
    Taken,                  ///< its version is now the one this device holds, with the journal replayed on it
    NotNewer,               ///< it does not rank above the version this device holds
    OtherWard,              ///< it is a version of a ward other than the open one
    UnknownWriter,          ///< the access list this device holds names no device or invite as its writer
    NotNamed,               ///< it names no recipient, or others and not this device: the ward is no longer this device's
    InviteUsedOrWithdrawn,  ///< it names neither this device nor its invite while the journal still holds this device's join: the join is refused, and the ward is no longer this device's
    Newer,                  ///< a newer Ward Planning wrote it
    Unreadable              ///< it fails another check; problem says which
};

/// How taking an update file ended. NotNamed and InviteUsedOrWithdrawn end this device's share
/// of the ward; every status but those and Taken changes nothing. When it was taken, a replay
/// that is Replayed put this device's own changes on top, which the other devices lack until
/// it sends an update.
struct TakeResult
{
    TakeStatus status;
    QString problem;      ///< what went wrong, for the log, when Unreadable
    ReplayResult replay;  ///< when Taken
};

/// How ending an emergency went.
enum class EndResult
{
    Ended,         ///< the report was written, and the emergency removed
    AlreadyEnded,  ///< no emergency is in progress, so nothing was written
    ReportFailed   ///< the report could not be written, so the emergency stays
};

enum class OpenStatus
{
    Opened,
    Missing,     ///< this device holds no file for the ward
    CannotRead,  ///< the bytes were read and do not open as this ward on this device
    Newer,       ///< a newer Ward Planning wrote the file
    Fault        ///< the bytes could not be read at all; nothing changed, and opening again retries
};

struct OpenResult
{
    OpenStatus status;
    QString problem;  ///< what went wrong, for the log, when not Opened
};

/// How a ward's local file reads on this device.
struct LocalRead
{
    OpenResult result;
    std::optional<WardPayload> payload;  ///< when result is Opened
    QByteArray dataKey;                  ///< when result is Opened
    VersionRank version;                 ///< when result is Opened: the version its header names
};

/// What an invitation file holds, as this device reads it.
enum class InvitationStatus
{
    Joinable,          ///< its version names its invite, which this device can join through
    AlreadyHasAccess,  ///< its version already names this device
    UsedOrWithdrawn,   ///< its version names neither this device nor its invite
    NotAnInvitation,   ///< the file is not an invitation, or its version fails a check; problem says which
    Newer              ///< a newer Ward Planning wrote it
};

/// An invitation's ward, read for the user to confirm by the ward's own name.
struct InvitedWard
{
    InvitationStatus status;
    QString problem;                     ///< when NotAnInvitation
    DocumentId document;                 ///< when Joinable or AlreadyHasAccess
    QByteArray seed;                     ///< when Joinable
    std::optional<WardRead> version;     ///< when Joinable or AlreadyHasAccess: the version it carries, read
    std::optional<AccessInvite> invite;  ///< when Joinable: the open invite it joins through
    QString wardName;                    ///< when Joinable or AlreadyHasAccess
};

/// How making an invite ended.
struct InviteResult
{
    CommitResult commit;
    QByteArray invite;  ///< the invite's fingerprint, when the commit applied
};

/// One open ward: its document, journal and undo stack, and the commit step and replay
/// that change them. Every change to the document or this device's state is saved, the two
/// together in one write of the ward's local file. It owns the ward's AutomaticUpdates,
/// which change the same document outside the journal. Holds no global state, so a test
/// creates as many sessions as it needs; the app holds one.
class WardSession : public QObject
{
    Q_OBJECT

public:
    /// identity is this device's, which opens its local files, and store holds them.
    /// geocoder and unitLookup answer the automatic updates. The session owns none of them.
    WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                QObject* parent);

    const Document& document() const { return m_document; }
    const Journal& journal() const { return m_device.journal; }

    /// The version this device holds: the one it last took or wrote, which its local file's
    /// header names.
    const VersionRank& version() const { return m_version; }

    /// The ward, its landed marks and this device's state, as a save writes them.
    WardPayload payload() const { return WardPayload{m_document, m_landed, m_device}; }
    AutomaticUpdates& automaticUpdates() { return m_automaticUpdates; }

    const QByteArray& deviceFingerprint() const { return m_identity->fingerprint(); }

    /// Creates a ward holding content under a fresh document id and data key, and saves it as
    /// its version 1, written by this device. Its access list, whatever content held, names
    /// one user: userName, an admin under a new user id, with this device alone, named
    /// deviceName. The journal, the undo and redo stacks, the rest of this device's state and
    /// the failed list start empty.
    void createWard(const Document& content, const QString& userName, const QString& deviceName);

    /// Opens the ward this device holds under document: its document with its access list,
    /// the version it holds, its landed marks and this device's state come from its local
    /// file, and the undo stack and the failed list start empty. Anything but Opened leaves
    /// the session as it was and writes nothing.
    OpenResult openWard(const DocumentId& document);

    /// How document's local file in store reads for identity: opened, or why not.
    static LocalRead readLocal(const WardStore& store, const Identity& identity, const DocumentId& document);

    const DocumentId& documentId() const { return m_documentId; }

    /// True while the latest save failed: memory holds changes the file does not.
    bool savePending() const { return m_savePending; }

    /// Saves again when a save is pending; true when nothing is left unsaved.
    bool retryPendingSave();

    /// True once a version this device took left it out: the ward is no longer this device's,
    /// and its copy is read-only.
    bool isNoLongerShared() const { return m_device.noLongerShared; }

    /// True when journal holds an entry above device's own mark in landed: a change no version
    /// held with those marks carries.
    static bool changesToShare(const Journal& journal, const QHash<QByteArray, qint64>& landed,
                               const QByteArray& device);

    /// You have changes to share: changes no version this device holds carries.
    bool hasChangesToShare() const { return changesToShare(m_device.journal, m_landed, m_identity->fingerprint()); }

    /// The commit step: the command's rule, then execute, then the journal and a save. A
    /// command that applies empties the redo stack, and enters the undo stack when it is an
    /// UndoableCommand. One that conflicts or is refused changes nothing and enters neither.
    /// An empty command is nothing to do and changes nothing. Every command is refused while
    /// the ward is no longer this device's. A command that applies is handed to the automatic
    /// updates, which start a run when it gives them work; an undo or redo is handed on the
    /// same way.
    CommitResult commit(CommandPtr command);

    /// Builds the inverse of the command on top of the undo stack and runs it through the
    /// commit step. When the inverse applies, the command moves to the redo stack; otherwise
    /// it stays on the undo stack.
    CommitResult undo();

    /// Runs the command on top of the redo stack again through the commit step. When it
    /// applies, it moves back to the undo stack; otherwise it stays on the redo stack.
    CommitResult redo();

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    QString undoHeading() const;
    QString redoHeading() const;

    /// Ends the emergency in progress: reads it, writes its report to reportPath, and
    /// commits EndEmergencyCommand with the emergency as the report was written from.
    /// Nothing runs between the read and the commit, so the commit applies.
    EndResult endEmergency(const QString& reportPath);

    /// Replays the journal onto version. With the journal empty, version becomes the
    /// document as it stands. Otherwise each entry is rebuilt from its serialised form and
    /// put through the rule and execute, in order, on a copy of version, and when every
    /// entry applies the copy becomes the document. The first entry that conflicts or is
    /// refused abandons the replay: version becomes the document as it stands, the journal
    /// and the undo and redo stacks are emptied, and the result's toRedo holds the
    /// abandoned changes as text, which nothing in the session keeps. Each outcome is saved.
    /// A replay starts no automatic update.
    ReplayResult replayOnto(const Document& version);

    /// Writes the next version as an update file, signed by its writer, and this device then
    /// holds that version: see nextVersion. The journal keeps its entries until a version this
    /// device takes carries them.
    QByteArray writeUpdate();

    /// Takes the version an update file carries, checked in this order: its format; that it
    /// is a version of the open ward; that its signature verifies under the verify key the
    /// access list this device holds gives its writer; that it ranks above the version this
    /// device holds; that it names this device, or, for a device that joined through an
    /// invite, that invite, which it then reads with; that the data key unseals and the payload
    /// reads as a version whose access list the header matches; and that it descends from the
    /// copy this device holds, by sharing a key with it: the key either is under, or one either
    /// names among its former keys. A version that names neither ends this device's share: see
    /// becomeNoLongerShared. A version that names the invite and not this device comes from a
    /// member who has not taken the join, so the join runs on it again (JoinCommand::rejoin)
    /// before anything replays, and enters the journal, so it goes out with the next update.
    /// Taking it drops the journal entries its landed mark for this device covers, raising the
    /// counter to that mark, replays the rest onto it as replayOnto does, and makes it the
    /// version this device holds, under its data key, with its landed marks and its recipients.
    /// Then it saves, and starts a run of the automatic updates for what the version lacks.
    TakeResult takeUpdate(const QByteArray& file);

    /// Makes an invite under a fresh seed, which this device's state keeps while the access
    /// list names the invite, and puts InviteCommand through the commit step: for user when it
    /// is set, otherwise for another of this device's user's devices. A refused invite keeps no
    /// seed.
    InviteResult invite(const std::optional<InvitedUser>& user);

    /// The fingerprints of the open invites this device made and keeps the seed of: those it
    /// can write an invitation for.
    QSet<QByteArray> invitesMadeHere() const;

    /// An invitation for invite, one of invitesMadeHere: its seed and the version nextVersion
    /// would write, which names the invite while it is open. This device does not hold that
    /// version, so its changes stay to share until it sends an update.
    QByteArray writeInvitation(const QByteArray& invite) const;

    /// What file holds as an invitation, read by this device: its line, then its version read
    /// as this device, which finds it AlreadyHasAccess when the version names it, and
    /// otherwise as the invite its seed gives. Changes nothing.
    InvitedWard readInvitation(const QByteArray& file) const;

    /// Joins the ward invited carries, which readInvitation found Joinable or
    /// AlreadyHasAccess. Joinable: its version becomes this device's copy as a first download,
    /// replacing any copy held before, with nothing of that copy kept - under the version's
    /// key, with its landed marks and its recipients, and an empty journal counting on from
    /// this device's mark in it - and JoinCommand runs on it through the commit step, naming
    /// this device deviceName and, for a user invite, its new user userName. This device keeps
    /// the invite's seed for as long as it holds the ward.
    /// AlreadyHasAccess: opens the copy this device holds; only when it holds none, or one it
    /// cannot read, takes the version as its copy with an empty journal, so a read that fails
    /// for a moment never overwrites the local file.
    void join(const InvitedWard& invited, const QString& userName, const QString& deviceName);

    /// The name of this device's user in the open ward.
    QString ownUserName() const;

    /// Renames this device to deviceName in every ward the store holds that is still this
    /// device's and names it otherwise: the open ward through this session's commit step,
    /// each other ward through a session opened on it here. Each rename is a command in its
    /// ward's journal.
    void renameDeviceEverywhere(const QString& deviceName);

    /// Renames this device's user to userName in every ward the store holds that is still
    /// this device's and names them otherwise, as renameDeviceEverywhere does.
    void renameUserEverywhere(const QString& userName);

    /// When this device's user in the open ward is named other than yourName, renames the
    /// user to that name in every other ward this device holds that names them otherwise, and
    /// answers the name, which is Your name from then on. Otherwise answers none.
    std::optional<QString> followUserName(const QString& yourName);

signals:
    /// The document changed, through a command, a replay, an open or an automatic update.
    void documentChanged(const DocumentChange& change);
    void undoStackChanged();

    /// A save failed after the one before it succeeded; told once until a save succeeds again.
    void saveFailed(const QString& problem);

private:
    /// The next version this device writes, and what holding it changes.
    struct NextVersion
    {
        QByteArray bytes;  ///< in a ward file's framing, with no device part
        VersionRank version;
        QByteArray dataKey;
        QList<QByteArray> formerKeys;
        QHash<QByteArray, qint64> landed;
        QSet<QByteArray> recipients;
        Identity writer;  ///< the header's writer, which signs the update file too
    };

    CommitResult run(const Command& command);

    /// The replay replayOnto runs before it saves.
    ReplayResult replay(const Document& version);

    /// One write of the document and this device's state together, so a crash cannot leave
    /// one without the other. The file is sealed to exactly the access list, under the data
    /// key the ward already has, and its header names the version this device holds. The seed
    /// of an invite the access list no longer names leaves the state as it is written.
    void save();

    /// The journal's changes as text for the user to redo: what stopped the replay, then
    /// each entry's heading and lines in order, with the entry at index stopped marked.
    QString toRedoText(int stopped, const CommitResult& stop) const;

    /// The name of each user a removal in the journal names whom access holds.
    QStringList stillHavingAccess(const AccessList& access) const;

    /// One above the version this device holds, written by writer(), holding the document,
    /// its access list and the landed marks, with this device's set to its journal's last
    /// sequence number and those of devices the access list no longer holds dropped. When
    /// the access list has lost anyone the version this device holds seals to, it rotates: a
    /// fresh data key, and the hash of the key this device holds appended to the former keys.
    NextVersion nextVersion() const;

    /// Who writes this device's next version: the invite it joined through, while the version
    /// it holds does not name this device, since a member who has not taken the join lists the
    /// invite and not the device; otherwise this device.
    Identity writer() const;

    /// True while the journal holds this device's join: no version it took carries it yet.
    bool joinUnlanded() const;

    /// version read as this device, or as the invite this device joined through when the
    /// version names that and not this device.
    WardRead readVersion(const QByteArray& version) const;

    /// version, read, becomes this device's copy of document as it stands: its key, landed
    /// marks and recipients, a fresh device state whose counter starts at this device's mark
    /// in it, and empty undo stacks and failed list. Saves nothing.
    void takeFirstDownload(const DocumentId& document, const WardRead& version);

    /// The ward is no longer this device's: unsent changes are dropped, the undo stacks
    /// cleared, the copy kept read-only with the flag saved, so a device reopened still knows.
    void becomeNoLongerShared();

    /// Commits, in every ward the store holds, the command rename builds from that ward's
    /// access list for this device and name: the open ward through this session, each other
    /// ward through a session opened on it here. A ward that names it so already gets no
    /// command, and one that is no longer this device's refuses it.
    void renameEverywhere(CommandPtr (*rename)(const QByteArray&, const AccessList&, const QString&),
                          const QString& name);

    void replaceDocument(const Document& document);
    void clearUndoStack();

    const Identity* m_identity;
    WardStore* m_store;
    Geocoder* m_geocoder;
    UnitLookup* m_unitLookup;
    DocumentId m_documentId;
    VersionRank m_version;
    QByteArray m_dataKey;
    Document m_document;
    QHash<QByteArray, qint64> m_landed;
    DeviceState m_device;
    bool m_savePending = false;
    QList<UndoableCommandPtr> m_undoStack;
    QList<UndoableCommandPtr> m_redoStack;
    AutomaticUpdates m_automaticUpdates;  ///< declared after m_document, which it works on

    static constexpr int UndoLimit = 100;
};
```

### `src/services/WardSession.cpp`

Given whole. `openWard`, `readLocal`, `retryPendingSave`, `commit`, `undo`, `redo`, the
headings, `endEmergency`, `replayOnto`, `toRedoText`, `replaceDocument` and `clearUndoStack`
are as they are now.

```cpp
#include "WardSession.h"
#include "AccessCommands.h"
#include "CommandRegistry.h"
#include "Crypto.h"
#include "EmergencyCommands.h"
#include "InvitationFile.h"
#include "ReportGenerator.h"
#include "UpdateFile.h"

#include <QDateTime>
#include <QStringList>

namespace
{

/// The command's rule, then execute, on document: the one rule a first run and a replay
/// both follow.
CommitResult attempt(const Command& command, Document& document)
{
    QString refusal = command.refusal(document);
    if (!refusal.isEmpty())
    {
        return CommitResult{CommitStatus::Refused, refusal, command.description()};
    }
    if (command.execute(document) == CommandOutcome::Conflicted)
    {
        return CommitResult{CommitStatus::Conflicted, QString(), command.description()};
    }
    document.onDocumentChanged(command.documentChange());
    return CommitResult{CommitStatus::Applied, QString(), command.description()};
}

LocalRead notOpened(OpenStatus status, const QString& problem)
{
    return LocalRead{OpenResult{status, problem}, std::nullopt, QByteArray(), VersionRank()};
}

TakeResult notTaken(TakeStatus status, const QString& problem)
{
    return TakeResult{status, problem, ReplayResult{ReplayStatus::FastForwarded, QString(), QStringList()}};
}

InvitedWard notJoinable(InvitationStatus status, const QString& problem)
{
    return InvitedWard{status, problem, DocumentId(), QByteArray(), std::nullopt, std::nullopt, QString()};
}

/// The keys a copy under dataKey with access has held: the hash of dataKey, and every former
/// key access names.
QSet<QByteArray> keyHistory(const QByteArray& dataKey, const AccessList& access)
{
    QSet<QByteArray> keys(access.formerKeys().begin(), access.formerKeys().end());
    keys.insert(AccessList::formerKeyHash(dataKey));
    return keys;
}

/// True when a version read under its data key descends from the copy this device holds under
/// heldKey with the access list held: the two share a key. Keys are held per ward, so another
/// ward's version shares none, and only a holder of a key can name it, inside the payload.
bool descends(const WardRead& version, const QByteArray& heldKey, const AccessList& held)
{
    return keyHistory(version.dataKey, version.payload->ward.access()).intersects(keyHistory(heldKey, held));
}

}  // namespace

WardSession::WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                         QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_store(store)
    , m_geocoder(geocoder)
    , m_unitLookup(unitLookup)
    , m_automaticUpdates(m_document, geocoder, unitLookup, nullptr)
{
    connect(&m_automaticUpdates, &AutomaticUpdates::documentChanged,
            this, &WardSession::documentChanged);
    connect(&m_automaticUpdates, &AutomaticUpdates::applied, this, &WardSession::save);
}

void WardSession::createWard(const Document& content, const QString& userName, const QString& deviceName)
{
    Document ward = content;
    ward.setAccess(AccessList::founding(UserId::generate(), userName, deviceName, *m_identity));
    m_documentId = DocumentId::generate();
    m_version = VersionRank{1, m_identity->fingerprint()};
    m_dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    m_landed.clear();
    m_device = DeviceState();
    m_device.heldRecipients = ward.access().recipientFingerprints();
    m_savePending = false;
    m_automaticUpdates.wardOpened();
    clearUndoStack();
    replaceDocument(ward);
    save();
}

// openWard, readLocal, retryPendingSave: as they are now.

bool WardSession::changesToShare(const Journal& journal, const QHash<QByteArray, qint64>& landed,
                                 const QByteArray& device)
{
    return !journal.isEmpty()
        && journal.entries().last().sequence > landed.value(device);
}

// commit, undo, redo, undoHeading, redoHeading, endEmergency, replayOnto: as they are now.

QByteArray WardSession::writeUpdate()
{
    NextVersion next = nextVersion();
    AccessList access = m_document.access();
    access.setFormerKeys(next.formerKeys);
    m_document.setAccess(access);
    m_version = next.version;
    m_dataKey = next.dataKey;
    m_landed = next.landed;
    m_device.heldRecipients = next.recipients;
    save();
    return UpdateFile::write(m_documentId, next.bytes, next.writer);
}

TakeResult WardSession::takeUpdate(const QByteArray& file)
{
    UpdateRead update = UpdateFile::read(file);
    switch (update.status)
    {
    case UpdateReadStatus::Read:
        break;
    case UpdateReadStatus::Newer:
        return notTaken(TakeStatus::Newer, QString());
    case UpdateReadStatus::Unreadable:
        return notTaken(TakeStatus::Unreadable, update.problem);
    }
    const WardHeader& header = *update.header;
    if (header.document() != m_documentId)
    {
        return notTaken(TakeStatus::OtherWard, QString());
    }
    std::optional<QByteArray> verifyKey = m_document.access().verifyKeyOf(header.version().writer);
    if (!verifyKey.has_value())
    {
        return notTaken(TakeStatus::UnknownWriter, QString());
    }
    if (!UpdateFile::verifies(update, *verifyKey))
    {
        return notTaken(TakeStatus::Unreadable, "the signature does not verify");
    }
    if (!header.version().ranksAbove(m_version))
    {
        return notTaken(TakeStatus::NotNewer, QString());
    }

    WardRead read = readVersion(update.version);
    switch (read.status)
    {
    case WardReadStatus::Read:
        break;
    case WardReadStatus::Newer:
        return notTaken(TakeStatus::Newer, QString());
    case WardReadStatus::Unreadable:
        return notTaken(TakeStatus::Unreadable, read.problem);
    case WardReadStatus::Deleted:
    case WardReadStatus::NotNamed:
    {
        TakeStatus status = joinUnlanded() ? TakeStatus::InviteUsedOrWithdrawn : TakeStatus::NotNamed;
        becomeNoLongerShared();
        return notTaken(status, QString());
    }
    }
    if (!descends(read, m_dataKey, m_document.access()))
    {
        return notTaken(TakeStatus::Unreadable, "the version does not descend from the one this device holds");
    }

    // A version read with the join seed comes from a member who has not taken the join: the
    // join runs again on it first, so this device is on the list every replayed entry meets.
    Document base = read.payload->ward;
    CommandPtr rejoin;
    if (!read.header->sealedKeyFor(m_identity->fingerprint()).has_value())
    {
        QByteArray invite = Identity::fromInviteSeed(*m_device.joinSeed).fingerprint();
        rejoin = JoinCommand::rejoin(*base.access().findInvite(invite), m_document.access(),
                                     m_identity->fingerprint(), base.wardName());
        attempt(*rejoin, base);
    }
    m_device.journal.dropLanded(read.payload->landed.value(m_identity->fingerprint()));
    ReplayResult replayed = replay(base);
    if (rejoin)
    {
        m_device.journal.append(*rejoin);
    }
    m_version = header.version();
    m_dataKey = read.dataKey;
    m_landed = read.payload->landed;
    m_device.heldRecipients = read.payload->ward.access().recipientFingerprints();
    save();
    m_automaticUpdates.run();
    return TakeResult{TakeStatus::Taken, QString(), replayed};
}

InviteResult WardSession::invite(const std::optional<InvitedUser>& user)
{
    QByteArray seed = Crypto::randomBytes(Crypto::KeyBytes);
    Identity keys = Identity::fromInviteSeed(seed);
    m_device.inviteSeeds.append(seed);
    CommitResult result = commit(InviteCommand::create(m_identity->fingerprint(), m_document.access(), user, keys));
    if (result.status != CommitStatus::Applied)
    {
        m_device.inviteSeeds.removeOne(seed);
        return InviteResult{result, QByteArray()};
    }
    return InviteResult{result, keys.fingerprint()};
}

QSet<QByteArray> WardSession::invitesMadeHere() const
{
    QSet<QByteArray> invites;
    for (const QByteArray& seed : m_device.inviteSeeds)
    {
        invites.insert(Identity::fromInviteSeed(seed).fingerprint());
    }
    return invites;
}

QByteArray WardSession::writeInvitation(const QByteArray& invite) const
{
    QByteArray seed;
    for (const QByteArray& candidate : m_device.inviteSeeds)
    {
        if (Identity::fromInviteSeed(candidate).fingerprint() == invite)
        {
            seed = candidate;
        }
    }
    return InvitationFile::write(m_documentId, seed, nextVersion().bytes);
}

InvitedWard WardSession::readInvitation(const QByteArray& file) const
{
    InvitationRead invitation = InvitationFile::read(file);
    switch (invitation.status)
    {
    case InvitationReadStatus::Read:
        break;
    case InvitationReadStatus::Newer:
        return notJoinable(InvitationStatus::Newer, QString());
    case InvitationReadStatus::NotAnInvitation:
        return notJoinable(InvitationStatus::NotAnInvitation, invitation.problem);
    }

    WardRead asThisDevice = WardFile::read(invitation.version, *m_identity, PayloadKind::Version);
    if (asThisDevice.status == WardReadStatus::Read)
    {
        return InvitedWard{InvitationStatus::AlreadyHasAccess, QString(), invitation.document, QByteArray(),
                           asThisDevice, std::nullopt, asThisDevice.payload->ward.wardName()};
    }

    Identity keys = Identity::fromInviteSeed(invitation.seed);
    WardRead read = WardFile::read(invitation.version, keys, PayloadKind::Version);
    switch (read.status)
    {
    case WardReadStatus::Read:
        break;
    case WardReadStatus::Newer:
        return notJoinable(InvitationStatus::Newer, QString());
    case WardReadStatus::Unreadable:
        return notJoinable(InvitationStatus::NotAnInvitation, read.problem);
    case WardReadStatus::Deleted:
    case WardReadStatus::NotNamed:
        return notJoinable(InvitationStatus::UsedOrWithdrawn, QString());
    }
    if (read.header->document() != invitation.document)
    {
        return notJoinable(InvitationStatus::NotAnInvitation, "the version is of another ward");
    }
    return InvitedWard{InvitationStatus::Joinable, QString(), invitation.document, invitation.seed, read,
                       read.payload->ward.access().findInvite(keys.fingerprint()), read.payload->ward.wardName()};
}

void WardSession::join(const InvitedWard& invited, const QString& userName, const QString& deviceName)
{
    if (invited.status == InvitationStatus::AlreadyHasAccess)
    {
        OpenStatus opened = openWard(invited.document).status;
        if (opened != OpenStatus::Missing && opened != OpenStatus::CannotRead)
        {
            return;
        }
    }
    takeFirstDownload(invited.document, *invited.version);
    if (invited.status == InvitationStatus::Joinable)
    {
        m_device.joinSeed = invited.seed;
        AccessDevice device(deviceName, m_identity->publicKey(), m_identity->verifyKey());
        commit(JoinCommand::create(*invited.invite, device, userName, invited.wardName));
        return;
    }
    save();
}

QString WardSession::ownUserName() const
{
    return m_document.access().userOfDevice(m_identity->fingerprint())->name();
}

void WardSession::renameDeviceEverywhere(const QString& deviceName)
{
    renameEverywhere(&RenameDeviceCommand::create, deviceName);
}

void WardSession::renameUserEverywhere(const QString& userName)
{
    renameEverywhere(&RenameUserCommand::create, userName);
}

void WardSession::renameEverywhere(CommandPtr (*rename)(const QByteArray&, const AccessList&, const QString&),
                                   const QString& name)
{
    const QList<DocumentId> documents = m_store->documents();
    for (const DocumentId& document : documents)
    {
        if (document == m_documentId)
        {
            commit(rename(m_identity->fingerprint(), m_document.access(), name));
            continue;
        }
        WardSession other(m_identity, m_store, m_geocoder, m_unitLookup, nullptr);
        if (other.openWard(document).status == OpenStatus::Opened)
        {
            other.commit(rename(m_identity->fingerprint(), other.document().access(), name));
        }
    }
}

std::optional<QString> WardSession::followUserName(const QString& yourName)
{
    QString name = ownUserName();
    if (name == yourName)
    {
        return std::nullopt;
    }
    renameUserEverywhere(name);
    return name;
}

ReplayResult WardSession::replay(const Document& version)
{
    if (m_device.journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayResult{ReplayStatus::FastForwarded, QString(), QStringList()};
    }

    Document replayed = version;
    replayed.onDocumentChanged(DocumentChange::full());
    const QList<JournalEntry>& entries = m_device.journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(JsonReader(entries[i].command));
        CommitResult result = attempt(*command, replayed);
        if (result.status != CommitStatus::Applied)
        {
            QString toRedo = toRedoText(i, result);
            QStringList stillHaveAccess = stillHavingAccess(version.access());
            m_device.journal.clear();
            clearUndoStack();
            replaceDocument(version);
            return ReplayResult{ReplayStatus::Abandoned, toRedo, stillHaveAccess};
        }
    }
    replaceDocument(replayed);
    return ReplayResult{ReplayStatus::Replayed, QString(), QStringList()};
}

CommitResult WardSession::run(const Command& command)
{
    if (m_device.noLongerShared)
    {
        return CommitResult{CommitStatus::Refused, tr("This ward is no longer shared with this device."),
                            command.description()};
    }
    CommitResult result = attempt(command, m_document);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }
    m_device.journal.append(command);
    save();
    emit documentChanged(command.documentChange());
    m_automaticUpdates.commandApplied(command);
    return result;
}

// toRedoText: as it is now.

QStringList WardSession::stillHavingAccess(const AccessList& access) const
{
    QStringList names;
    for (const JournalEntry& entry : m_device.journal.entries())
    {
        std::shared_ptr<const RemoveUserCommand> removal
            = std::dynamic_pointer_cast<const RemoveUserCommand>(CommandRegistry::fromJson(JsonReader(entry.command)));
        if (!removal)
        {
            continue;
        }
        std::optional<AccessUser> user = access.findUser(removal->userId());
        if (user.has_value())
        {
            names.append(user->name());
        }
    }
    return names;
}

void WardSession::save()
{
    const AccessList& access = m_document.access();
    m_device.inviteSeeds.removeIf([&access](const QByteArray& seed) {
        return !access.findInvite(Identity::fromInviteSeed(seed).fingerprint()).has_value();
    });
    QByteArray bytes = WardFile::write(m_documentId, m_version, m_dataKey, payload());
    QString problem;
    if (m_store->write(m_documentId, bytes, &problem))
    {
        m_savePending = false;
        return;
    }
    if (!m_savePending)
    {
        m_savePending = true;
        emit saveFailed(problem);
    }
}

WardSession::NextVersion WardSession::nextVersion() const
{
    Document document = m_document;
    AccessList access = document.access();
    QSet<QByteArray> recipients = access.recipientFingerprints();
    QByteArray dataKey = m_dataKey;
    if (!recipients.contains(m_device.heldRecipients))
    {
        QList<QByteArray> formerKeys = access.formerKeys();
        formerKeys.append(AccessList::formerKeyHash(m_dataKey));
        access.setFormerKeys(formerKeys);
        document.setAccess(access);
        dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    }

    QHash<QByteArray, qint64> landed;
    const QSet<QByteArray> devices = access.deviceFingerprints();
    for (auto it = m_landed.constBegin(); it != m_landed.constEnd(); ++it)
    {
        if (devices.contains(it.key()))
        {
            landed.insert(it.key(), it.value());
        }
    }
    landed.insert(m_identity->fingerprint(), m_device.journal.lastSequence());

    Identity signer = writer();
    VersionRank version{m_version.number + 1, signer.fingerprint()};
    QByteArray bytes = WardFile::write(m_documentId, version, dataKey, WardPayload{document, landed, std::nullopt});
    return NextVersion{bytes, version, dataKey, access.formerKeys(), landed, recipients, signer};
}

Identity WardSession::writer() const
{
    if (!m_device.heldRecipients.contains(m_identity->fingerprint()))
    {
        return Identity::fromInviteSeed(*m_device.joinSeed);
    }
    return *m_identity;
}

bool WardSession::joinUnlanded() const
{
    for (const JournalEntry& entry : m_device.journal.entries())
    {
        if (entry.command["type"].toString() == JoinCommand::TypeName)
        {
            return true;
        }
    }
    return false;
}

WardRead WardSession::readVersion(const QByteArray& version) const
{
    WardRead read = WardFile::read(version, *m_identity, PayloadKind::Version);
    if (read.status == WardReadStatus::NotNamed && m_device.joinSeed.has_value())
    {
        return WardFile::read(version, Identity::fromInviteSeed(*m_device.joinSeed), PayloadKind::Version);
    }
    return read;
}

void WardSession::takeFirstDownload(const DocumentId& document, const WardRead& version)
{
    m_documentId = document;
    m_version = version.header->version();
    m_dataKey = version.dataKey;
    m_landed = version.payload->landed;
    m_device = DeviceState();
    m_device.journal.dropLanded(m_landed.value(m_identity->fingerprint()));
    m_device.heldRecipients = version.payload->ward.access().recipientFingerprints();
    m_savePending = false;
    m_automaticUpdates.wardOpened();
    clearUndoStack();
    replaceDocument(version.payload->ward);
}

void WardSession::becomeNoLongerShared()
{
    m_device.noLongerShared = true;
    m_device.journal.clear();
    clearUndoStack();
    save();
}

// replaceDocument, clearUndoStack: as they are now.
```

The local-file invariant follows from these functions: once a ward is open, `m_dataKey` changes
only in `writeUpdate`, `takeUpdate` and `takeFirstDownload`, so the local file is always under
the key of the last version this device took or wrote, and a local save never rotates. Former
keys change only there too: a command never sets them, and a replay takes them from the version
it replays onto.

A ward that is no longer this device's keeps the copy it had, sealed to the access list that
copy holds, which still names this device, so the copy opens, read-only, after a restart.

**How a joined device's join travels.** The join is a journal entry like any command, and the
landed marks settle it: it leaves the journal once a version this device takes carries this
device's mark at or above it. Until then a version that names neither this device nor its
invite refuses the join, which is `InviteUsedOrWithdrawn`; after it, the same version is a
removal, `NotNamed`. That is the test the message follows: whether the journal still holds this
device's join. A version that names the invite and not this device runs the join again, as
`JoinCommand::rejoin`, on the version itself before the replay, so every replayed entry, and an
abandoned replay's result, finds this device on the list, and the local file stays readable. The
rejoin then enters the journal, so *You have changes to share* shows and the next update carries
it; an earlier join still in the journal meets this device already on the list, its result
holding. What this device writes is signed by the invite while the version it holds does not
name the device, which covers the first join and every rejoin, and by the device otherwise.

### `src/widgets/MainWindow.h` and `src/widgets/MainWindow.cpp`

The prompt that asks the user to send an update after taking one goes now; task 3's notice
says when there are changes to share. In `openUpdateFile`, the `ReplayStatus::Replayed` case
joins `FastForwarded`:

```cpp
    switch (result.replay.status)
    {
    case ReplayStatus::FastForwarded:
    case ReplayStatus::Replayed:
        QMessageBox::information(this, tr("Open Update"), tr("The update has been added to %1.").arg(wardName));
        break;
    case ReplayStatus::Abandoned:
        showChangesToRedo(result.replay.toRedo);
        break;
    }
```

and the comment on `openUpdateFile` in `MainWindow.h` becomes:

```cpp
    /// Takes the update file at path, opening its ward first when another is open, and tells
    /// the user how it went.
```

The `TakeStatus` switch above it gains the new status, beside `NotNamed`:

```cpp
    case TakeStatus::InviteUsedOrWithdrawn:
        QMessageBox::warning(this, tr("Open Update"), tr("This invite has been used or withdrawn. Ask the sender."));
        return;
```

### `CMakeLists.txt`

Add `src/commands/AccessRules.cpp` and `src/commands/AccessCommands.cpp` to `COMMAND_SOURCES`,
`src/commands/AccessRules.h` and `src/commands/AccessCommands.h` to `COMMAND_HEADERS`,
`src/services/InvitationFile.cpp` to `SERVICE_SOURCES` and `src/services/InvitationFile.h` to
`SERVICE_HEADERS`.

### `docs/FORMAT.md`

Under *device*, the table becomes:

| Key | Type | Holds |
|---|---|---|
| `journal` | journal object | this device's commands not yet seen to land |
| `heldRecipients` | array of fingerprints | the fingerprints the version this device holds seals the data key to, sorted; written when empty too |
| `inviteSeeds` | array of 32-byte values | the seed of each open invite this device made, in the order it made them; omitted when empty |
| `joinSeed` | 32 bytes | the seed of the invite this device joined through; omitted on a device that did not join through an invite |
| `noLongerShared` | `true` | a version this device took left it out; omitted while the ward is this device's |

Under *Commands*, after `importMinistering`, add:

```markdown
`invite`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it |
| `invite` | invite object (see *access*) | the invite it adds |

`removeUser`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it |
| `user` | 16 bytes | the id of the user it removes |

`removeDevice`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it |
| `device` | 16 bytes | the fingerprint of the device it removes |

`withdrawInvite`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it |
| `invite` | 16 bytes | the fingerprint of the invite it withdraws |

`setRole`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it |
| `user` | 16 bytes | the id of the user whose role it sets |
| `role` | change of a word: `admin` or `member` | the role |

`renameUser`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it, whose user it renames |
| `name` | change of a string | the user's name |

`renameDevice`:

| Key | Type | Holds |
|---|---|---|
| `actor` | 16 bytes | the fingerprint of the device that made it, which it renames |
| `name` | change of a string | the device's name |

`join`:

| Key | Type | Holds |
|---|---|---|
| `invite` | invite object (see *access*) | the invite it joins through, as the joining device read it |
| `device` | device object (see *access*) | the joining device |
| `user` | joining user object | the user it joins as; required when the invite's `for` is `user`, and refused when it is `device` |

A joining user:

| Key | Type | Holds |
|---|---|---|
| `id` | 16 bytes | the new user's id |
| `name` | string | the name they gave |
```

Under *Update files*, after its last sentence, add: "A device that joined through an invite
writes its versions as that invite, signed with the invite's keys, while the version it holds
does not name the device."

### `docs/ARCHITECTURE.md`

In *WardSession*'s code block, add after `replayOnto`:

```cpp
    // The next version, written as an update file that this device then holds; an update
    // file taken
    QByteArray writeUpdate();
    TakeResult takeUpdate(const QByteArray& file);

    // An invite, its invitation, and joining from one
    InviteResult invite(const std::optional<InvitedUser>& user);
    QByteArray writeInvitation(const QByteArray& invite) const;
    InvitedWard readInvitation(const QByteArray& file) const;
    void join(const InvitedWard& invited, const QString& userName, const QString& deviceName);

    // You have changes to share; the ward no longer this device's
    bool hasChangesToShare() const;
    bool isNoLongerShared() const;

    // Renames across every ward this device holds
    void renameDeviceEverywhere(const QString& deviceName);
    void renameUserEverywhere(const QString& userName);
    std::optional<QString> followUserName(const QString& yourName);
```

and after the *Typed coordinates* bullet add:

```markdown
- **Access-list changes** are commands (`AccessCommands`): `InviteCommand`, `RemoveUserCommand`,
  `RemoveDeviceCommand`, `WithdrawInviteCommand`, `SetRoleCommand`, `RenameUserCommand`,
  `RenameDeviceCommand` and `JoinCommand`. Each carries the fingerprint of the device that made
  it, and its rule in `AccessRules` asks about that device's user in the version in front of it,
  the first time and on replay. None is undoable.
- **Sharing by files.** `writeUpdate` writes the next version, one above the version this device
  holds, and this device then holds it. When its access list has lost anyone the held version
  seals to (the device state keeps those recipients), it rotates: a fresh data key, and the old
  key's hash appended to the former keys. A local save never rotates. `takeUpdate` takes a
  version through the checks its comment lists; a version descends when it shares a key with
  this device's copy, the key either is under or one either names among its former keys, so
  update files that crossed a rotation still converge. A version that leaves this device out
  makes the ward no longer this device's: the journal is dropped, every command is refused, and
  the copy stays, read-only.
- **Invites and joining.** `invite` keeps a fresh seed in the device state, while the access list
  names its invite, and commits `InviteCommand`. `writeInvitation` writes an `InvitationFile`
  carrying that seed and the next version, which this device does not hold, so its changes stay
  to share. `readInvitation` reads an invitation's version with the invite's keys
  (`Identity::fromInviteSeed`), and `join` takes it as a first download and commits
  `JoinCommand` on it. The joined device keeps the seed for as long as it holds the ward: a
  version that names the invite and not the device is read with it, and the join runs again on
  it before the replay (`JoinCommand::rejoin`) and enters the journal. The device writes as the
  invite while the version it holds does not name it. A version naming neither refuses the join
  while the journal still holds it (*This invite has been used or withdrawn*), and is a removal
  once it has landed.
- **Changes to share** are journal entries above this device's own landed mark in the version
  it holds.
- **Renames** run in every ward the store holds that is still this device's: the open ward
  through the session, each other through a session opened on it. `followUserName` takes a
  user name another of this user's devices set, after a version is taken.
```

### `tests/services/TestClient.h` and `tests/services/TestClient.cpp`

Add to the class:

```cpp
    /// This device as an access-list entry, named deviceName.
    AccessDevice device() const;

    /// Gives each of holders a local file of one ward and opens it: document under dataKey,
    /// holding content with access as its access list, as version 1 written by writer, with
    /// no landed marks, an empty journal, and access's recipients as the held version's.
    /// Every holder is on access, so each can open its file.
    static void holdWard(const QList<TestClient*>& holders, const AccessList& access, const TestClient& writer,
                         const DocumentId& document, const QByteArray& dataKey, const Document& content);
```

The existing `holdWard(holders, listed, ...)` builds its list and calls the new one:

```cpp
AccessDevice TestClient::device() const
{
    return AccessDevice(m_deviceName, m_identity.publicKey(), m_identity.verifyKey());
}

void TestClient::holdWard(const QList<TestClient*>& holders, const QList<TestClient*>& listed,
                          const DocumentId& document, const QByteArray& dataKey, const Document& content)
{
    QList<AccessUser> users;
    for (const TestClient* client : listed)
    {
        users.append(AccessUser(UserId::generate(), client->deviceName(), AccessRole::Admin, {client->device()}));
    }
    holdWard(holders, AccessList(users, {}, {}), *listed.first(), document, dataKey, content);
}

void TestClient::holdWard(const QList<TestClient*>& holders, const AccessList& access, const TestClient& writer,
                          const DocumentId& document, const QByteArray& dataKey, const Document& content)
{
    Document ward = content;
    ward.setAccess(access);
    DeviceState device;
    device.heldRecipients = access.recipientFingerprints();
    VersionRank first{1, writer.identity().fingerprint()};
    QByteArray file = WardFile::write(document, first, dataKey, WardPayload{ward, {}, device});
    for (TestClient* holder : holders)
    {
        holder->store().put(document, file);
        if (holder->session().openWard(document).status != OpenStatus::Opened)
        {
            qFatal("TestClient: the ward %s did not open on %s", qPrintable(document.toString()),
                   qPrintable(holder->deviceName()));
        }
    }
}
```

### `tests/services/TestSharing.h`

```cpp
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
```

### `tests/services/TestSharing.cpp`

```cpp
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
        users.append(AccessUser(UserId::generate(), client->deviceName(), AccessRole::Admin, {client->device()}));
    }
    for (const TestClient* client : members)
    {
        users.append(AccessUser(UserId::generate(), client->deviceName(), AccessRole::Member, {client->device()}));
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
```

The test ward holds Bob Smith's phone as 555-1111 and Carol Jones's as 555-4444, as
`UpdateFileTest` relies on.

### `tests/services/AccessTest.h`

```cpp
#pragma once

#include <QObject>

/// Access-list changes as commands: their rules on the first run and on replay, and removals
/// and rotations reaching every device through update files.
class AccessTest : public QObject
{
    Q_OBJECT

public:
    explicit AccessTest(QObject* parent);

private slots:
    void removalRotatesAndReachesEveryDevice();
    void writeThatRemovesNobodyKeepsTheKeyAndSealsToExactlyTheAccessList();
    void twoAdminsRemovingDifferentUsersBothApply();
    void removingTheSameUserTwiceAppliesInFull();
    void removalReplayedAfterThatUserAddedADeviceTakesTheNewDeviceToo();
    void removalReplayedAfterThatUserMadeAnInviteWithdrawsIt();
    void demotedAdminsRemovalIsRefusedWithTheMembersWording();
    void onlyAdminCannotStepDownFirstRunOrOnReplay();
    void removingYourselfOrYourLastDeviceIsRefusedWithUseLeave();
    void memberCannotRemoveUsersSetRolesOrWithdrawAnothersInvite();
    void removingAnotherDeviceOfYourOwnRotatesItOut();
    void roleChangeReplayedAfterItsUserWasRemovedConflicts();
    void offlineAcrossTwoRotationsTakesTheLatest();
    void updatesThatCrossARotationConverge();
    void renamesReachEveryDevice();
};
```

### `tests/services/AccessTest.cpp`

```cpp
#include "AccessTest.h"
#include "AccessCommands.h"
#include "InvitationFile.h"
#include "TestSharing.h"
#include "TestWard.h"
#include "WardFile.h"

#include <QTest>

namespace
{

CommitResult removeUser(TestClient& actor, const QString& name)
{
    return actor.session().commit(
        RemoveUserCommand::create(actor.identity().fingerprint(), TestSharing::userNamed(actor, name)));
}

CommitResult setRole(TestClient& actor, const QString& name, AccessRole role)
{
    return actor.session().commit(
        SetRoleCommand::create(actor.identity().fingerprint(), TestSharing::userNamed(actor, name), role));
}

QSet<QByteArray> sealedTo(const WardRead& read)
{
    QSet<QByteArray> fingerprints;
    for (const Recipient& recipient : read.header->recipients())
    {
        fingerprints.insert(recipient.fingerprint);
    }
    return fingerprints;
}

const QString AskAnAdmin = "To add or remove users, ask an admin: Alice";

}  // namespace

AccessTest::AccessTest(QObject* parent)
    : QObject(parent)
{
}

void AccessTest::removalRotatesAndReachesEveryDevice()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QByteArray heldKey = TestSharing::keyOf(alice);
    // Carol writes 2 before the removal reaches anyone; Alice's removal is her 3, so it outranks it.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-3333") == CommitStatus::Applied);
    QByteArray carols = carol.session().writeUpdate();
    alice.session().writeUpdate();
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    // A local save does not rotate.
    QCOMPARE(TestSharing::keyOf(alice), heldKey);

    QByteArray alices = alice.session().writeUpdate();

    QByteArray version = TestSharing::versionIn(alices);
    WardRead read = WardFile::read(version, bob.identity(), PayloadKind::Version);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QVERIFY(read.dataKey != heldKey);
    QVERIFY(TestSharing::opensUnder(version, read.dataKey));
    QVERIFY(!TestSharing::opensUnder(version, heldKey));
    QCOMPARE(read.payload->ward.access().formerKeys(), QList<QByteArray>{AccessList::formerKeyHash(heldKey)});
    QVERIFY(sealedTo(read) == (QSet<QByteArray>{alice.identity().fingerprint(), bob.identity().fingerprint()}));
    QCOMPARE(TestSharing::keyOf(alice), read.dataKey);

    // Bob takes the removal, then refuses what Carol wrote before it.
    QVERIFY(bob.session().takeUpdate(alices).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
    WardPayload bobBefore = bob.session().payload();
    QVERIFY(bob.session().takeUpdate(carols).status == TakeStatus::UnknownWriter);
    QVERIFY(bob.session().payload() == bobBefore);
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-1111"));

    // Carol cannot open the removal, and her copy is no longer hers.
    QVERIFY(carol.session().takeUpdate(alices).status == TakeStatus::NotNamed);
    QVERIFY(carol.session().isNoLongerShared());
    QVERIFY(carol.session().journal().isEmpty());
    CommitResult refused = carol.session().commit(UpdateFamilyCommand::fromEdit(
        TestWard::family(carol.session().document(), TestWard::jonesId()),
        TestWard::withPhone(TestWard::family(carol.session().document(), TestWard::jonesId()), TestWard::carolId(),
                            "555-7777"),
        carol.session().document()));
    QVERIFY(refused.status == CommitStatus::Refused);
    QCOMPARE(refused.refusal, QString("This ward is no longer shared with this device."));
    QVERIFY(carol.reopen().status == OpenStatus::Opened);
    QVERIFY(carol.session().isNoLongerShared());
    QCOMPARE(TestSharing::bobsPhone(carol), QString("555-3333"));
}

void AccessTest::writeThatRemovesNobodyKeepsTheKeyAndSealsToExactlyTheAccessList()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice}, {&bob}));
    QByteArray heldKey = TestSharing::keyOf(alice);
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);

    QByteArray version = TestSharing::versionIn(alice.session().writeUpdate());

    QVERIFY(TestSharing::opensUnder(version, heldKey));
    WardRead read = WardFile::read(version, bob.identity(), PayloadKind::Version);
    QVERIFY2(read.status == WardReadStatus::Read, qPrintable(read.problem));
    QCOMPARE(read.dataKey, heldKey);
    QVERIFY(read.payload->ward.access().formerKeys().isEmpty());
    QVERIFY(sealedTo(read) == (QSet<QByteArray>{alice.identity().fingerprint(), bob.identity().fingerprint()}));
}

void AccessTest::twoAdminsRemovingDifferentUsersBothApply()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestSharing::share({&alice, &bob, &carol, &dan}, TestSharing::accessOf({&alice, &bob}, {&carol, &dan}));
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QVERIFY(removeUser(bob, "Dan").status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
    QVERIFY(TestSharing::send(bob, {&alice}) == QList<TakeStatus>{TakeStatus::Taken});
    QVERIFY(alice.session().document() == bob.session().document());
    QVERIFY(alice.session().journal().isEmpty());
}

void AccessTest::removingTheSameUserTwiceAppliesInFull()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QVERIFY(removeUser(bob, "Carol").status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
}

void AccessTest::removalReplayedAfterThatUserAddedADeviceTakesTheNewDeviceToo()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient carolsPhone("Carol's phone");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    // Alice removes Carol before she has heard of Carol's phone.
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    InviteResult made = carol.session().invite(std::nullopt);
    QVERIFY(made.commit.status == CommitStatus::Applied);
    QByteArray invitation = carol.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(carol, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    carolsPhone.session().join(carolsPhone.session().readInvitation(invitation), QString(), "Carol's phone");
    QVERIFY(TestSharing::send(carolsPhone, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(TestSharing::userNamed(bob, "Carol").devices().size(), qsizetype(2));
    QVERIFY(TestSharing::send(bob, {&carolsPhone, &alice}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));

    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Bob"}));
    QByteArray alices = alice.session().writeUpdate();
    QVERIFY(carolsPhone.session().takeUpdate(alices).status == TakeStatus::NotNamed);
    QVERIFY(carolsPhone.session().isNoLongerShared());
}

void AccessTest::removalReplayedAfterThatUserMadeAnInviteWithdrawsIt()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    InviteResult made = carol.session().invite(std::nullopt);
    QVERIFY(made.commit.status == CommitStatus::Applied);
    QByteArray invitation = carol.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(carol, {&alice}) == QList<TakeStatus>{TakeStatus::Taken});

    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Bob"}));
    QVERIFY(alice.session().document().access().invites().isEmpty());
    QByteArray version = TestSharing::versionIn(alice.session().writeUpdate());
    Identity inviteKeys = Identity::fromInviteSeed(InvitationFile::read(invitation).seed);
    QByteArray inviteKey = WardFile::read(TestSharing::versionIn(invitation), inviteKeys, PayloadKind::Version).dataKey;
    QCOMPARE(inviteKey.size(), qsizetype(Crypto::KeyBytes));
    QVERIFY(WardFile::read(version, inviteKeys, PayloadKind::Version).status == WardReadStatus::NotNamed);
    QVERIFY(!TestSharing::opensUnder(version, inviteKey));
}

void AccessTest::demotedAdminsRemovalIsRefusedWithTheMembersWording()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(removeUser(bob, "Carol").status == CommitStatus::Applied);
    QVERIFY(setRole(alice, "Bob", AccessRole::Member).status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(bobTakes.replay.toRedo, AskAnAdmin + "\n"
                                     "These need doing again:\n"
                                     "\n"
                                     "Remove Carol (stopped here)\n");
    QCOMPARE(bobTakes.replay.stillHaveAccess, QStringList{"Carol"});
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob", "Carol"}));
    QVERIFY(TestSharing::userNamed(bob, "Bob").role() == AccessRole::Member);
    QVERIFY(bob.session().journal().isEmpty());
}

void AccessTest::onlyAdminCannotStepDownFirstRunOrOnReplay()
{
    const QString onlyAdmin = "You are the only admin. Make someone else an admin first.";
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));

    CommitResult firstRun = setRole(alice, "Alice", AccessRole::Member);

    QVERIFY(firstRun.status == CommitStatus::Refused);
    QCOMPARE(firstRun.refusal, onlyAdmin);
    QVERIFY(TestSharing::userNamed(alice, "Alice").role() == AccessRole::Admin);
    QVERIFY(alice.session().journal().isEmpty());

    // Two admins each step down while the other is still an admin; the second to land is refused.
    TestClient erin("Erin");
    TestClient frank("Frank");
    TestSharing::share({&erin, &frank}, TestSharing::accessOf({&erin, &frank}, {}));
    QVERIFY(setRole(frank, "Frank", AccessRole::Member).status == CommitStatus::Applied);
    QVERIFY(setRole(erin, "Erin", AccessRole::Member).status == CommitStatus::Applied);

    TakeResult frankTakes = frank.session().takeUpdate(erin.session().writeUpdate());

    QVERIFY(frankTakes.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(frankTakes.replay.toRedo.section('\n', 0, 0), onlyAdmin);
    QVERIFY(TestSharing::userNamed(frank, "Frank").role() == AccessRole::Admin);
    QVERIFY(TestSharing::userNamed(frank, "Erin").role() == AccessRole::Member);
}

void AccessTest::removingYourselfOrYourLastDeviceIsRefusedWithUseLeave()
{
    const QString useLeave = "To stop using this ward, use Leave.";
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));

    CommitResult yourself = removeUser(alice, "Alice");
    CommitResult lastDevice = carol.session().commit(
        RemoveDeviceCommand::create(carol.identity().fingerprint(), carol.device()));
    CommitResult anothersDevice = alice.session().commit(
        RemoveDeviceCommand::create(alice.identity().fingerprint(), carol.device()));

    QVERIFY(yourself.status == CommitStatus::Refused);
    QCOMPARE(yourself.refusal, useLeave);
    QVERIFY(lastDevice.status == CommitStatus::Refused);
    QCOMPARE(lastDevice.refusal, useLeave);
    QVERIFY(anothersDevice.status == CommitStatus::Refused);
    QCOMPARE(anothersDevice.refusal, QString("You can remove only your own devices."));
    QVERIFY(alice.session().journal().isEmpty());
    QVERIFY(carol.session().journal().isEmpty());
    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Carol"}));
}

void AccessTest::memberCannotRemoveUsersSetRolesOrWithdrawAnothersInvite()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestSharing::share({&alice, &carol, &dan}, TestSharing::accessOf({&alice}, {&carol, &dan}));
    InviteResult alicesInvite = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(alicesInvite.commit.status == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    AccessInvite invite = *carol.session().document().access().findInvite(alicesInvite.invite);

    CommitResult remove = removeUser(carol, "Dan");
    CommitResult promote = setRole(carol, "Dan", AccessRole::Admin);
    CommitResult withdraw = carol.session().commit(
        WithdrawInviteCommand::create(carol.identity().fingerprint(), invite, carol.session().document().access()));

    QVERIFY(remove.status == CommitStatus::Refused);
    QCOMPARE(remove.refusal, AskAnAdmin);
    QVERIFY(promote.status == CommitStatus::Refused);
    QCOMPARE(promote.refusal, AskAnAdmin);
    QVERIFY(withdraw.status == CommitStatus::Refused);
    QCOMPARE(withdraw.refusal, AskAnAdmin);
    QVERIFY(carol.session().journal().isEmpty());

    // A member withdraws an invite of their own.
    InviteResult carolsInvite = carol.session().invite(std::nullopt);
    QVERIFY(carolsInvite.commit.status == CommitStatus::Applied);
    AccessInvite own = *carol.session().document().access().findInvite(carolsInvite.invite);
    CommitResult withdrawOwn = carol.session().commit(
        WithdrawInviteCommand::create(carol.identity().fingerprint(), own, carol.session().document().access()));
    QVERIFY(withdrawOwn.status == CommitStatus::Applied);
    QVERIFY(!carol.session().document().access().findInvite(carolsInvite.invite).has_value());
    QVERIFY(carol.session().document().access().findInvite(alicesInvite.invite).has_value());
}

void AccessTest::removingAnotherDeviceOfYourOwnRotatesItOut()
{
    TestClient laptop("Laptop");
    TestClient phone("Phone");
    AccessList access({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {laptop.device(), phone.device()})},
                      {}, {});
    TestSharing::share({&laptop, &phone}, access);
    QByteArray heldKey = TestSharing::keyOf(phone);

    QVERIFY(laptop.session().commit(RemoveDeviceCommand::create(laptop.identity().fingerprint(), phone.device())).status
            == CommitStatus::Applied);
    QByteArray laptops = laptop.session().writeUpdate();

    QCOMPARE(TestSharing::userNamed(laptop, "Alice").devices().size(), qsizetype(1));
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(laptops), TestSharing::keyOf(laptop)));
    QVERIFY(!TestSharing::opensUnder(TestSharing::versionIn(laptops), heldKey));
    QVERIFY(phone.session().takeUpdate(laptops).status == TakeStatus::NotNamed);
    QVERIFY(phone.session().isNoLongerShared());
}

void AccessTest::roleChangeReplayedAfterItsUserWasRemovedConflicts()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestSharing::share({&alice, &bob, &carol}, TestSharing::accessOf({&alice, &bob}, {&carol}));
    QVERIFY(setRole(bob, "Carol", AccessRole::Admin).status == CommitStatus::Applied);
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);

    TakeResult bobTakes = bob.session().takeUpdate(alice.session().writeUpdate());

    QVERIFY(bobTakes.replay.status == ReplayStatus::Abandoned);
    QCOMPARE(bobTakes.replay.toRedo.section('\n', 0, 0),
             QString("Your changes could not be applied, because the ward changed first."));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Bob"}));
}

void AccessTest::offlineAcrossTwoRotationsTakesTheLatest()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestClient erin("Erin");
    TestSharing::share({&alice, &bob, &carol, &dan, &erin},
                       TestSharing::accessOf({&alice, &bob}, {&carol, &dan, &erin}));
    QByteArray firstKey = TestSharing::keyOf(erin);
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QByteArray secondKey = TestSharing::keyOf(bob);
    QVERIFY(secondKey != firstKey);
    QVERIFY(removeUser(bob, "Dan").status == CommitStatus::Applied);

    QVERIFY(TestSharing::send(bob, {&erin}) == QList<TakeStatus>{TakeStatus::Taken});

    QCOMPARE(erin.session().document().access().formerKeys(),
             (QList<QByteArray>{AccessList::formerKeyHash(firstKey), AccessList::formerKeyHash(secondKey)}));
    QCOMPARE(TestSharing::userNames(erin), (QStringList{"Alice", "Bob", "Erin"}));
    QVERIFY(TestSharing::keyOf(erin) != secondKey);
}

void AccessTest::updatesThatCrossARotationConverge()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestSharing::share({&alice, &bob, &carol, &dan}, TestSharing::accessOf({&alice, &bob}, {&carol, &dan}));
    QByteArray firstKey = TestSharing::keyOf(alice);
    // Bob sends twice before he hears from Alice, so his 3 outranks her 2.
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-2222") == CommitStatus::Applied);
    bob.session().writeUpdate();
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-3333") == CommitStatus::Applied);
    QByteArray bobs = bob.session().writeUpdate();
    QVERIFY(removeUser(alice, "Carol").status == CommitStatus::Applied);
    QByteArray alices2 = alice.session().writeUpdate();
    QVERIFY(dan.session().takeUpdate(alices2).status == TakeStatus::Taken);
    QVERIFY(TestSharing::keyOf(dan) != firstKey);

    // Bob's 3 is under the key Alice rotated away from; she takes it and her removal replays.
    TakeResult aliceTakes = alice.session().takeUpdate(bobs);

    QVERIFY(aliceTakes.status == TakeStatus::Taken);
    QVERIFY(aliceTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Bob", "Dan"}));
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-3333"));

    // Her next version is under a third key. Dan, under the second, takes it by the first, which both name.
    QByteArray alices4 = alice.session().writeUpdate();
    QVERIFY(dan.session().takeUpdate(alices4).status == TakeStatus::Taken);
    QVERIFY(bob.session().takeUpdate(alices4).status == TakeStatus::Taken);
    QVERIFY(dan.session().document() == alice.session().document());
    QVERIFY(bob.session().document() == alice.session().document());
    QVERIFY(bob.session().journal().isEmpty());

    // Bob, having taken a removal, writes under the third key too.
    QVERIFY(TestSharing::commitPhone(bob, TestWard::jonesId(), TestWard::carolId(), "555-6666") == CommitStatus::Applied);
    QByteArray bobs5 = bob.session().writeUpdate();
    QVERIFY(alice.session().takeUpdate(bobs5).status == TakeStatus::Taken);
    QVERIFY(dan.session().takeUpdate(bobs5).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::phoneOf(dan, TestWard::carolId()), QString("555-6666"));

    // Carol, removed, opens none of the versions written after her removal.
    const QList<QByteArray> afterRemoval = {alices2, alices4, bobs5};
    for (const QByteArray& update : afterRemoval)
    {
        QByteArray version = TestSharing::versionIn(update);
        QVERIFY(WardFile::read(version, carol.identity(), PayloadKind::Version).status == WardReadStatus::NotNamed);
        QVERIFY(!TestSharing::opensUnder(version, firstKey));
    }
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(bobs), firstKey));
}

void AccessTest::renamesReachEveryDevice()
{
    TestClient alice("Alice");
    TestClient bob("Bob");
    TestSharing::share({&alice, &bob}, TestSharing::accessOf({&alice, &bob}, {}));
    QByteArray actor = alice.identity().fingerprint();
    QVERIFY(alice.session().commit(RenameUserCommand::create(actor, alice.session().document().access(), "Alice Smith"))
                .status
            == CommitStatus::Applied);
    QVERIFY(alice.session()
                .commit(RenameDeviceCommand::create(actor, alice.session().document().access(), "Alice's laptop"))
                .status
            == CommitStatus::Applied);
    QVERIFY(alice.session().commit(RenameUserCommand::create(actor, alice.session().document().access(), "Alice Smith"))
                .status
            == CommitStatus::NothingToDo);

    QVERIFY(TestSharing::send(alice, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});

    AccessUser renamed = TestSharing::userNamed(bob, "Alice Smith");
    QCOMPARE(renamed.devices().size(), qsizetype(1));
    QCOMPARE(renamed.devices()[0].name(), QString("Alice's laptop"));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice Smith", "Bob"}));
}
```

`AccessTest.cpp` also includes `"Crypto.h"` and `"FamilyCommands.h"`, for `Crypto::KeyBytes` and
`UpdateFamilyCommand`.

### `tests/services/FileSharingTest.h`

```cpp
#pragma once

#include <QObject>

/// Users and devices invited and joined entirely through files, the changes-to-share rule
/// through crossed updates, and names changed in Settings reaching every ward.
class FileSharingTest : public QObject
{
    Q_OBJECT

public:
    explicit FileSharingTest(QObject* parent);

private slots:
    void invitationReadsBackItsDocumentAndSeed();
    void anyOtherFileIsNotAnInvitation();
    void userInvitedAndJoinedEntirelyThroughFiles();
    void memberVersionWrittenBeforeTakingTheJoinIsReadWithTheSeed();
    void versionFromAMemberWhoNeverTookTheJoinRunsTheJoinAgain();
    void sameInvitationUsedOnTwoDevices();
    void withdrawnInviteRotatesAndItsInvitationJoinsNothingLater();
    void invitationOpenedOnADeviceTheWardNamesJoinsNothing();
    void deviceInviteJoinsAsOneOfItsMakersDevices();
    void seedLeavesTheFileOnceTheInviteIsWithdrawnOrAbandoned();
    void addAUserIsForAdminsAndAnyoneAddsADevice();
    void changesToShareThroughCrossedUpdates();
    void renamingThisDeviceJournalsOneRenameInEachWardThatNamesItOtherwise();
    void secondDeviceTakingTheNewUserNameFollowsIt();
};
```

### `tests/services/FileSharingTest.cpp`

```cpp
#include "FileSharingTest.h"
#include "AccessCommands.h"
#include "Base64Url.h"
#include "Crypto.h"
#include "InvitationFile.h"
#include "TestSharing.h"
#include "TestWard.h"
#include "WardFile.h"
#include "WardFileEdits.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace
{

QString writerOf(const QByteArray& update)
{
    return WardFileEdits::headerOf(TestSharing::versionIn(update))["writer"].toString();
}

}  // namespace

FileSharingTest::FileSharingTest(QObject* parent)
    : QObject(parent)
{
}

void FileSharingTest::invitationReadsBackItsDocumentAndSeed()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    VersionRank held = alice.session().version();

    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);

    QVERIFY(made.commit.status == CommitStatus::Applied);
    QList<QByteArray> seeds = alice.session().payload().device->inviteSeeds;
    QCOMPARE(seeds.size(), qsizetype(1));
    QCOMPARE(Identity::fromInviteSeed(seeds[0]).fingerprint(), made.invite);
    InvitationRead read = InvitationFile::read(invitation);
    QVERIFY(read.status == InvitationReadStatus::Read);
    QVERIFY(read.document == alice.session().documentId());
    QCOMPARE(read.seed, seeds[0]);
    // The invitation carries the next version, which names the invite; this device still holds
    // the version before it, so the invite is still a change to share.
    QCOMPARE(WardFileEdits::headerOf(read.version)["number"].toInteger(), held.number + 1);
    WardRead version = WardFile::read(read.version, Identity::fromInviteSeed(read.seed), PayloadKind::Version);
    QVERIFY2(version.status == WardReadStatus::Read, qPrintable(version.problem));
    QVERIFY(alice.session().version() == held);
    QVERIFY(alice.session().hasChangesToShare());
    AccessInvite invite = *alice.session().document().access().findInvite(made.invite);
    QCOMPARE(alice.session().document().access().describeInvite(invite), QString("for Bob"));
    QCOMPARE(InvitationFile::fileName("Tanglewood Ward", "for Bob"), QString("Tanglewood Ward - for Bob.wardinvite"));
}

void FileSharingTest::anyOtherFileIsNotAnInvitation()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    DocumentId ward = TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(std::nullopt);
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QByteArray wardFile = alice.store().file(ward);
    QByteArray update = alice.session().writeUpdate();
    QJsonObject newerLine{{"format", 2},
                          {"document", ward.toString()},
                          {"seed", Base64Url::encode(Crypto::randomBytes(Crypto::KeyBytes))}};
    QByteArray newer = QJsonDocument(newerLine).toJson(QJsonDocument::Compact) + '\n' + TestSharing::versionIn(invitation);
    TestClient stranger("Stranger");

    const QList<QByteArray> others = {wardFile, update, QByteArray(), QByteArray("not an invitation\n")};
    for (const QByteArray& file : others)
    {
        QVERIFY(stranger.session().readInvitation(file).status == InvitationStatus::NotAnInvitation);
    }
    QVERIFY(stranger.session().readInvitation(newer).status == InvitationStatus::Newer);
    QVERIFY(stranger.session().readInvitation(invitation).status == InvitationStatus::Joinable);
}

void FileSharingTest::userInvitedAndJoinedEntirelyThroughFiles()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    QByteArray firstKey = TestSharing::keyOf(alice);

    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(made.commit.status == CommitStatus::Applied);
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(alice.session().hasChangesToShare());
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    QVERIFY(!alice.session().hasChangesToShare());
    QVERIFY(carol.session().document().access().findInvite(made.invite).has_value());

    InvitedWard invited = bob.session().readInvitation(invitation);

    QVERIFY(invited.status == InvitationStatus::Joinable);
    QCOMPARE(invited.wardName, alice.session().document().wardName());
    QCOMPARE(invited.invite->invitedUser()->label, QString("Bob"));

    bob.session().join(invited, "Bob Brown", "Bob's phone");

    QVERIFY(bob.session().documentId() == alice.session().documentId());
    AccessUser bobUser = TestSharing::userNamed(bob, "Bob Brown");
    QVERIFY(bobUser.role() == AccessRole::Member);
    QCOMPARE(bobUser.devices().size(), qsizetype(1));
    QCOMPARE(bobUser.devices()[0].fingerprint(), bob.identity().fingerprint());
    QVERIFY(bob.session().document().access().invites().isEmpty());
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    QVERIFY(bob.session().hasChangesToShare());
    QVERIFY(bob.reopen().status == OpenStatus::Opened);
    QVERIFY(bob.session().payload().device->joinSeed == InvitationFile::read(invitation).seed);

    // The join goes out written by the invite, under a key the invite's seed cannot reach.
    QByteArray join = bob.session().writeUpdate();
    QCOMPARE(writerOf(join), Base64Url::encode(made.invite));
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(invitation), firstKey));
    QVERIFY(!TestSharing::opensUnder(TestSharing::versionIn(join), firstKey));
    QVERIFY(!bob.session().hasChangesToShare());
    QVERIFY(alice.session().takeUpdate(join).status == TakeStatus::Taken);
    QVERIFY(carol.session().takeUpdate(join).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(alice), (QStringList{"Alice", "Carol", "Bob Brown"}));
    QVERIFY(alice.session().invitesMadeHere().isEmpty());
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().payload().device->inviteSeeds.isEmpty());

    // Carol's next version names Bob, and carries his join; taking it, Bob keeps the seed and,
    // holding a version that names him, writes as himself.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(carol, {&bob, &alice}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(bob.session().payload().device->joinSeed == InvitationFile::read(invitation).seed);
    QVERIFY(bob.session().journal().isEmpty());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QVERIFY(TestSharing::commitBobsPhone(bob, "555-3333") == CommitStatus::Applied);
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(writerOf(bobs), Base64Url::encode(bob.identity().fingerprint()));
    QVERIFY(alice.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-3333"));
    QVERIFY(alice.session().document() == bob.session().document());
}

void FileSharingTest::memberVersionWrittenBeforeTakingTheJoinIsReadWithTheSeed()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    bob.session().join(bob.session().readInvitation(invitation), "Bob Brown", "Bob's phone");
    UserId bobsId = TestSharing::userNamed(bob, "Bob Brown").id();
    // Carol writes before she has Bob's join: her version is sealed to the open invite, not to Bob.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-2222") == CommitStatus::Applied);
    QByteArray carols = carol.session().writeUpdate();
    QVERIFY(WardFile::read(TestSharing::versionIn(carols), bob.identity(), PayloadKind::Version).status
            == WardReadStatus::NotNamed);

    TakeResult bobTakes = bob.session().takeUpdate(carols);

    // The join runs again on Carol's version; the first join, still in the journal, finds its
    // result already there.
    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(bobTakes.replay.status == ReplayStatus::Replayed);
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Carol", "Bob Brown"}));
    QVERIFY(TestSharing::userNamed(bob, "Bob Brown").id() == bobsId);
    QCOMPARE(TestSharing::userNamed(bob, "Bob Brown").devices()[0].fingerprint(), bob.identity().fingerprint());
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(2));
    QCOMPARE(bob.session().journal().entries()[0].command["type"].toString(), QString("join"));
    QCOMPARE(bob.session().journal().entries()[1].command["type"].toString(), QString("join"));
    QVERIFY(bob.session().hasChangesToShare());
    QVERIFY(bob.reopen().status == OpenStatus::Opened);

    // The version Bob holds does not name him, so his next is written as the invite, which
    // both copies still list.
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(writerOf(bobs), Base64Url::encode(made.invite));
    QVERIFY(carol.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QVERIFY(alice.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(carol), (QStringList{"Alice", "Carol", "Bob Brown"}));
    QVERIFY(carol.session().document() == bob.session().document());
    QVERIFY(alice.session().document() == bob.session().document());
}

void FileSharingTest::versionFromAMemberWhoNeverTookTheJoinRunsTheJoinAgain()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient dan("Dan");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol, &dan}, TestSharing::accessOf({&alice}, {&carol, &dan}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(alice, {&carol, &dan}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    bob.session().join(bob.session().readInvitation(invitation), "Bob Brown", "Bob's phone");
    UserId bobsId = TestSharing::userNamed(bob, "Bob Brown").id();
    QVERIFY(TestSharing::send(bob, {&alice, &carol}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    // Carol's 4 names Bob, and Bob takes it: his join has landed.
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::send(carol, {&bob, &alice}) == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(bob.session().journal().isEmpty());
    // Dan never took the join, and sends three times, so his 5 outranks Carol's 4.
    dan.session().writeUpdate();
    dan.session().writeUpdate();
    QVERIFY(TestSharing::commitPhone(dan, TestWard::jonesId(), TestWard::carolId(), "555-6666") == CommitStatus::Applied);
    QByteArray dans = dan.session().writeUpdate();
    QCOMPARE(WardFileEdits::headerOf(TestSharing::versionIn(dans))["number"].toInteger(), qint64(5));
    QVERIFY(WardFile::read(TestSharing::versionIn(dans), bob.identity(), PayloadKind::Version).status
            == WardReadStatus::NotNamed);

    TakeResult bobTakes = bob.session().takeUpdate(dans);

    QVERIFY(bobTakes.status == TakeStatus::Taken);
    QVERIFY(!bob.session().isNoLongerShared());
    QCOMPARE(TestSharing::userNames(bob), (QStringList{"Alice", "Carol", "Dan", "Bob Brown"}));
    QVERIFY(TestSharing::userNamed(bob, "Bob Brown").id() == bobsId);
    QCOMPARE(TestSharing::phoneOf(bob, TestWard::carolId()), QString("555-6666"));
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-1111"));
    QCOMPARE(bob.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(bob.session().journal().entries()[0].command["type"].toString(), QString("join"));
    QVERIFY(bob.session().hasChangesToShare());
    QVERIFY(bob.reopen().status == OpenStatus::Opened);

    // Bob's 6 is written as the invite, which Dan's copy lists; Dan takes the join with it.
    QByteArray bobs = bob.session().writeUpdate();
    QCOMPARE(writerOf(bobs), Base64Url::encode(made.invite));
    QVERIFY(dan.session().takeUpdate(bobs).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::userNames(dan), (QStringList{"Alice", "Carol", "Dan", "Bob Brown"}));

    // Dan's 7 reaches everyone, Carol's edit goes on top of it in her 8, and all hold equal wards.
    QVERIFY(TestSharing::send(dan, {&alice, &carol, &bob})
            == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(TestSharing::send(carol, {&alice, &bob, &dan})
            == (QList<TakeStatus>{TakeStatus::Taken, TakeStatus::Taken, TakeStatus::Taken}));
    QVERIFY(alice.session().document() == carol.session().document());
    QVERIFY(bob.session().document() == carol.session().document());
    QVERIFY(dan.session().document() == carol.session().document());
    QCOMPARE(TestSharing::bobsPhone(bob), QString("555-2222"));
    QCOMPARE(TestSharing::phoneOf(alice, TestWard::carolId()), QString("555-6666"));
    QVERIFY(bob.session().journal().isEmpty());
}

void FileSharingTest::sameInvitationUsedOnTwoDevices()
{
    TestClient alice("Alice");
    TestClient phone("Bob's phone");
    TestClient tablet("Bob's tablet");
    TestSharing::share({&alice}, TestSharing::accessOf({&alice}, {}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    phone.session().join(phone.session().readInvitation(invitation), "Bob", "Bob's phone");
    tablet.session().join(tablet.session().readInvitation(invitation), "Bob", "Bob's tablet");
    QByteArray phones = phone.session().writeUpdate();
    QByteArray tablets = tablet.session().writeUpdate();

    QVERIFY(alice.session().takeUpdate(phones).status == TakeStatus::Taken);
    // The tablet's join is written by the invite, which Alice's list no longer holds.
    QVERIFY(alice.session().takeUpdate(tablets).status == TakeStatus::UnknownWriter);
    QCOMPARE(TestSharing::userNamed(alice, "Bob").devices()[0].fingerprint(), phone.identity().fingerprint());

    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    QByteArray alices = alice.session().writeUpdate();

    QVERIFY(tablet.session().takeUpdate(alices).status == TakeStatus::InviteUsedOrWithdrawn);
    QVERIFY(tablet.session().isNoLongerShared());
    QVERIFY(phone.session().takeUpdate(alices).status == TakeStatus::Taken);
    QCOMPARE(TestSharing::bobsPhone(phone), QString("555-2222"));
}

void FileSharingTest::withdrawnInviteRotatesAndItsInvitationJoinsNothingLater()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestClient bob("Bob's phone");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    QVERIFY(TestSharing::send(alice, {&carol}) == QList<TakeStatus>{TakeStatus::Taken});
    AccessInvite invite = *alice.session().document().access().findInvite(made.invite);
    QVERIFY(alice.session()
                .commit(WithdrawInviteCommand::create(alice.identity().fingerprint(), invite,
                                                      alice.session().document().access()))
                .status
            == CommitStatus::Applied);

    QByteArray alices = alice.session().writeUpdate();

    Identity inviteKeys = Identity::fromInviteSeed(InvitationFile::read(invitation).seed);
    QByteArray inviteKey = WardFile::read(TestSharing::versionIn(invitation), inviteKeys, PayloadKind::Version).dataKey;
    QVERIFY(TestSharing::opensUnder(TestSharing::versionIn(invitation), inviteKey));
    QVERIFY(WardFile::read(TestSharing::versionIn(alices), inviteKeys, PayloadKind::Version).status
            == WardReadStatus::NotNamed);
    QVERIFY(!TestSharing::opensUnder(TestSharing::versionIn(alices), inviteKey));
    QVERIFY(carol.session().takeUpdate(alices).status == TakeStatus::Taken);
    QVERIFY(carol.session().document().access().invites().isEmpty());

    // The invitation still opens the version it carries, and joins nothing after it.
    InvitedWard invited = bob.session().readInvitation(invitation);
    QVERIFY(invited.status == InvitationStatus::Joinable);
    bob.session().join(invited, "Bob Brown", "Bob's phone");
    QVERIFY(bob.session().takeUpdate(alices).status == TakeStatus::InviteUsedOrWithdrawn);
    QVERIFY(bob.session().isNoLongerShared());
}

void FileSharingTest::invitationOpenedOnADeviceTheWardNamesJoinsNothing()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult made = alice.session().invite(std::nullopt);
    QByteArray invitation = alice.session().writeInvitation(made.invite);
    WardPayload carolBefore = carol.session().payload();
    VersionRank carolVersion = carol.session().version();

    InvitedWard invited = carol.session().readInvitation(invitation);
    carol.session().join(invited, QString(), "Carol");

    QVERIFY(invited.status == InvitationStatus::AlreadyHasAccess);
    QCOMPARE(invited.wardName, alice.session().document().wardName());
    QVERIFY(carol.session().payload() == carolBefore);
    QVERIFY(carol.session().version() == carolVersion);
    QVERIFY(alice.session().document().access().findInvite(made.invite).has_value());
}

void FileSharingTest::deviceInviteJoinsAsOneOfItsMakersDevices()
{
    TestClient laptop("Laptop");
    TestClient phone("Phone");
    TestSharing::share({&laptop},
                       AccessList({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {laptop.device()})}, {}, {}));
    InviteResult made = laptop.session().invite(std::nullopt);
    AccessInvite invite = *laptop.session().document().access().findInvite(made.invite);
    QCOMPARE(laptop.session().document().access().describeInvite(invite), QString("a device for Alice"));
    QByteArray invitation = laptop.session().writeInvitation(made.invite);

    InvitedWard invited = phone.session().readInvitation(invitation);
    phone.session().join(invited, QString(), "Phone");

    QVERIFY(invited.status == InvitationStatus::Joinable);
    QVERIFY(!invited.invite->invitedUser().has_value());
    AccessUser alice = TestSharing::userNamed(phone, "Alice");
    QCOMPARE(alice.devices().size(), qsizetype(2));
    QCOMPARE(alice.devices()[1].name(), QString("Phone"));
    QCOMPARE(alice.devices()[1].fingerprint(), phone.identity().fingerprint());
    QCOMPARE(phone.session().ownUserName(), QString("Alice"));
    QVERIFY(phone.session().followUserName(QString()) == QString("Alice"));
    QVERIFY(TestSharing::send(phone, {&laptop}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(TestSharing::userNamed(laptop, "Alice").devices().size(), qsizetype(2));
}

void FileSharingTest::seedLeavesTheFileOnceTheInviteIsWithdrawnOrAbandoned()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));
    InviteResult first = alice.session().invite(InvitedUser{AccessRole::Member, "Bob"});
    QVERIFY(first.commit.status == CommitStatus::Applied);
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QCOMPARE(alice.session().payload().device->inviteSeeds.size(), qsizetype(1));
    QVERIFY(alice.session().invitesMadeHere() == QSet<QByteArray>{first.invite});

    AccessInvite invite = *alice.session().document().access().findInvite(first.invite);
    QVERIFY(alice.session()
                .commit(WithdrawInviteCommand::create(alice.identity().fingerprint(), invite,
                                                      alice.session().document().access()))
                .status
            == CommitStatus::Applied);
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().payload().device->inviteSeeds.isEmpty());

    // An invite abandoned with a replay takes its seed with it.
    QVERIFY(TestSharing::commitBobsPhone(alice, "555-2222") == CommitStatus::Applied);
    InviteResult second = alice.session().invite(InvitedUser{AccessRole::Member, "Dan"});
    QVERIFY(second.commit.status == CommitStatus::Applied);
    QCOMPARE(alice.session().payload().device->inviteSeeds.size(), qsizetype(1));
    QVERIFY(TestSharing::commitBobsPhone(carol, "555-9999") == CommitStatus::Applied);

    TakeResult aliceTakes = alice.session().takeUpdate(carol.session().writeUpdate());

    QVERIFY(aliceTakes.replay.status == ReplayStatus::Abandoned);
    QVERIFY(alice.session().invitesMadeHere().isEmpty());
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().payload().device->inviteSeeds.isEmpty());
    QCOMPARE(TestSharing::bobsPhone(alice), QString("555-9999"));
}

void FileSharingTest::addAUserIsForAdminsAndAnyoneAddsADevice()
{
    TestClient alice("Alice");
    TestClient carol("Carol");
    TestSharing::share({&alice, &carol}, TestSharing::accessOf({&alice}, {&carol}));

    InviteResult refused = carol.session().invite(InvitedUser{AccessRole::Member, "Dan"});
    InviteResult device = carol.session().invite(std::nullopt);

    QVERIFY(refused.commit.status == CommitStatus::Refused);
    QCOMPARE(refused.commit.refusal, QString("To add or remove users, ask an admin: Alice"));
    QVERIFY(refused.invite.isEmpty());
    QVERIFY(device.commit.status == CommitStatus::Applied);
    QVERIFY(carol.session().invitesMadeHere() == QSet<QByteArray>{device.invite});
    QCOMPARE(carol.session().journal().entries().size(), qsizetype(1));
    QVERIFY(carol.session().document().access().findInvite(device.invite)->maker()
            == TestSharing::userNamed(carol, "Carol").id());
}

void FileSharingTest::changesToShareThroughCrossedUpdates()
{
    TestClient first("Alice");
    TestClient second("Bob");
    TestSharing::share({&first, &second}, TestSharing::accessOf({&first, &second}, {}));
    bool firstIsHigher = first.identity().fingerprint() > second.identity().fingerprint();
    TestClient& high = firstIsHigher ? first : second;
    TestClient& low = firstIsHigher ? second : first;
    QVERIFY(!high.session().hasChangesToShare());
    QVERIFY(!low.session().hasChangesToShare());

    // Shown on both after their edits, and cleared on both by sending 2.
    QVERIFY(TestSharing::commitBobsPhone(high, "555-2222") == CommitStatus::Applied);
    QVERIFY(TestSharing::commitPhone(low, TestWard::jonesId(), TestWard::carolId(), "555-5555")
            == CommitStatus::Applied);
    QVERIFY(high.session().hasChangesToShare());
    QVERIFY(low.session().hasChangesToShare());
    QByteArray highs = high.session().writeUpdate();
    QByteArray lows = low.session().writeUpdate();
    QVERIFY(!high.session().hasChangesToShare());
    QVERIFY(!low.session().hasChangesToShare());

    // The higher writer sets the lower 2 aside and keeps its entry, which its own 2 carries.
    QVERIFY(high.session().takeUpdate(lows).status == TakeStatus::NotNewer);
    QVERIFY(!high.session().hasChangesToShare());
    QCOMPARE(high.session().journal().entries().size(), qsizetype(1));

    // The lower writer takes the higher 2 and its change goes on top: shown again, until it sends 3.
    TakeResult lowTakes = low.session().takeUpdate(highs);
    QVERIFY(lowTakes.replay.status == ReplayStatus::Replayed);
    QVERIFY(low.session().hasChangesToShare());
    QByteArray lows3 = low.session().writeUpdate();
    QVERIFY(!low.session().hasChangesToShare());

    QVERIFY(high.session().takeUpdate(lows3).status == TakeStatus::Taken);
    QVERIFY(!high.session().hasChangesToShare());
    QVERIFY(high.session().journal().isEmpty());
    QCOMPARE(TestSharing::phoneOf(high, TestWard::carolId()), QString("555-5555"));
}

void FileSharingTest::renamingThisDeviceJournalsOneRenameInEachWardThatNamesItOtherwise()
{
    TestClient laptop("Laptop");
    TestClient bob("Bob");
    DocumentId renamed = TestSharing::share({&laptop, &bob}, TestSharing::accessOf({&laptop, &bob}, {}));
    AccessDevice workLaptop("Work laptop", laptop.identity().publicKey(), laptop.identity().verifyKey());
    DocumentId alreadyNamed = TestSharing::share(
        {&laptop}, AccessList({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {workLaptop})}, {}, {}));
    DocumentId gone = TestSharing::share({&laptop, &bob}, TestSharing::accessOf({&bob}, {&laptop}));
    QVERIFY(bob.session()
                .commit(RemoveUserCommand::create(bob.identity().fingerprint(), TestSharing::userNamed(bob, "Laptop")))
                .status
            == CommitStatus::Applied);
    QVERIFY(TestSharing::send(bob, {&laptop}) == QList<TakeStatus>{TakeStatus::NotNamed});
    QVERIFY(laptop.session().openWard(renamed).status == OpenStatus::Opened);

    laptop.session().renameDeviceEverywhere("Work laptop");

    const QByteArray& self = laptop.identity().fingerprint();
    QCOMPARE(laptop.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(laptop.session().journal().entries()[0].command["type"].toString(), QString("renameDevice"));
    QCOMPARE(laptop.session().document().access().findDevice(self)->name(), QString("Work laptop"));
    QVERIFY(laptop.session().openWard(alreadyNamed).status == OpenStatus::Opened);
    QVERIFY(laptop.session().journal().isEmpty());
    QCOMPARE(laptop.session().document().access().findDevice(self)->name(), QString("Work laptop"));
    QVERIFY(laptop.session().openWard(gone).status == OpenStatus::Opened);
    QVERIFY(laptop.session().isNoLongerShared());
    QVERIFY(laptop.session().journal().isEmpty());
    QCOMPARE(laptop.session().document().access().findDevice(self)->name(), QString("Laptop"));

    // The rename reaches Bob by the ordinary path.
    QVERIFY(laptop.session().openWard(renamed).status == OpenStatus::Opened);
    QVERIFY(bob.session().openWard(renamed).status == OpenStatus::Opened);
    QVERIFY(TestSharing::send(laptop, {&bob}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(bob.session().document().access().findDevice(self)->name(), QString("Work laptop"));
}

void FileSharingTest::secondDeviceTakingTheNewUserNameFollowsIt()
{
    TestClient laptop("Laptop");
    TestClient phone("Phone");
    TestClient carol("Carol");
    DocumentId first = TestSharing::share(
        {&laptop, &phone},
        AccessList({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {laptop.device(), phone.device()})}, {},
                   {}));
    DocumentId second = TestSharing::share(
        {&laptop, &phone},
        AccessList({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {laptop.device(), phone.device()})}, {},
                   {}));
    DocumentId phoneOnly = TestSharing::share(
        {&phone, &carol},
        AccessList({AccessUser(UserId::generate(), "Alice", AccessRole::Admin, {phone.device()}),
                    AccessUser(UserId::generate(), "Carol", AccessRole::Member, {carol.device()})},
                   {}, {}));
    QVERIFY(laptop.session().openWard(first).status == OpenStatus::Opened);
    laptop.session().renameUserEverywhere("Alice Jones");
    QVERIFY(phone.session().openWard(first).status == OpenStatus::Opened);
    QVERIFY(TestSharing::send(laptop, {&phone}) == QList<TakeStatus>{TakeStatus::Taken});
    QCOMPARE(phone.session().ownUserName(), QString("Alice Jones"));

    std::optional<QString> followed = phone.session().followUserName("Alice");

    QVERIFY(followed == QString("Alice Jones"));
    QVERIFY(phone.session().journal().isEmpty());
    QVERIFY(phone.session().openWard(second).status == OpenStatus::Opened);
    QCOMPARE(phone.session().journal().entries().size(), qsizetype(1));
    QCOMPARE(phone.session().journal().entries()[0].command["type"].toString(), QString("renameUser"));
    QCOMPARE(phone.session().ownUserName(), QString("Alice Jones"));
    QVERIFY(phone.session().openWard(phoneOnly).status == OpenStatus::Opened);
    QCOMPARE(phone.session().ownUserName(), QString("Alice Jones"));
    QCOMPARE(TestSharing::userNames(phone), (QStringList{"Alice Jones", "Carol"}));
    QVERIFY(!phone.session().followUserName("Alice Jones").has_value());
}
```

### `tests/services/CommandSamples.cpp`

Add `#include "AccessCommands.h"` and `#include "Identity.h"`. In the anonymous namespace:

```cpp
/// The test ward whose access list has Alice, an admin, on her laptop and phone; Carol, a
/// member, on her desktop; and a member invite Alice made for Bob, with the keys behind them.
struct AccessSampleWard
{
    Document ward;
    Identity laptop;
    Identity phone;
    Identity desktop;
};

AccessSampleWard accessSampleWard()
{
    Identity laptop = Identity::generate();
    Identity phone = Identity::generate();
    Identity desktop = Identity::generate();
    Identity bobsInvite = Identity::generate();
    UserId alice = UserId::generate();
    AccessList access({AccessUser(alice, "Alice", AccessRole::Admin,
                                  {AccessDevice("Laptop", laptop.publicKey(), laptop.verifyKey()),
                                   AccessDevice("Phone", phone.publicKey(), phone.verifyKey())}),
                       AccessUser(UserId::generate(), "Carol", AccessRole::Member,
                                  {AccessDevice("Desktop", desktop.publicKey(), desktop.verifyKey())})},
                      {AccessInvite::forUser(alice, AccessRole::Member, "Bob", bobsInvite.publicKey(),
                                             bobsInvite.verifyKey())},
                      {});
    Document ward = TestWard::document();
    ward.setAccess(access);
    return AccessSampleWard{ward, laptop, phone, desktop};
}

/// The user named name in ward's access list.
AccessUser accessUserNamed(const Document& ward, const QString& name)
{
    for (const AccessUser& user : ward.access().users())
    {
        if (user.name() == name)
        {
            return user;
        }
    }
    qFatal("CommandSamples: the ward holds no user named %s", qPrintable(name));
}

/// ward with user in place of the user who has user's id.
Document withAccessUser(const Document& ward, const AccessUser& user)
{
    AccessList access = ward.access();
    access.setUser(user);
    Document changed = ward;
    changed.setAccess(access);
    return changed;
}

/// Invites and removals never conflict, so they have no Conflicts sample.
QList<CommandSample> inviteSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr invite = InviteCommand::create(sample.laptop.fingerprint(), sample.ward.access(), std::nullopt,
                                              Identity::generate());
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, invite},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, invite), invite},
    };
}

QList<CommandSample> removeUserSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr remove = RemoveUserCommand::create(sample.laptop.fingerprint(), accessUserNamed(sample.ward, "Carol"));
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, remove},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, remove), remove},
    };
}

QList<CommandSample> removeDeviceSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr remove = RemoveDeviceCommand::create(
        sample.laptop.fingerprint(), AccessDevice("Phone", sample.phone.publicKey(), sample.phone.verifyKey()));
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, remove},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, remove), remove},
    };
}

QList<CommandSample> withdrawInviteSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr withdraw = WithdrawInviteCommand::create(sample.laptop.fingerprint(),
                                                        sample.ward.access().invites().first(), sample.ward.access());
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, withdraw},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, withdraw), withdraw},
    };
}

/// Carol made an admin; with two roles, it conflicts only once she is gone.
QList<CommandSample> setRoleSamples()
{
    AccessSampleWard sample = accessSampleWard();
    AccessUser carol = accessUserNamed(sample.ward, "Carol");
    CommandPtr promote = SetRoleCommand::create(sample.laptop.fingerprint(), carol, AccessRole::Admin);
    CommandPtr remove = RemoveUserCommand::create(sample.laptop.fingerprint(), carol);
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, promote},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, promote), promote},
        CommandSample{SampleCase::Conflicts, runOn(sample.ward, remove), promote},
    };
}

QList<CommandSample> renameUserSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr rename = RenameUserCommand::create(sample.laptop.fingerprint(), sample.ward.access(), "Alice Jones");
    AccessUser renamedElsewhere = accessUserNamed(sample.ward, "Alice");
    renamedElsewhere.setName("Alice Brown");
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, rename},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, rename), rename},
        CommandSample{SampleCase::Conflicts, withAccessUser(sample.ward, renamedElsewhere), rename},
    };
}

QList<CommandSample> renameDeviceSamples()
{
    AccessSampleWard sample = accessSampleWard();
    CommandPtr rename = RenameDeviceCommand::create(sample.laptop.fingerprint(), sample.ward.access(), "Work laptop");
    AccessUser renamedElsewhere = accessUserNamed(sample.ward, "Alice");
    QList<AccessDevice> devices = renamedElsewhere.devices();
    devices[0].setName("Old laptop");
    renamedElsewhere.setDevices(devices);
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, rename},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, rename), rename},
        CommandSample{SampleCase::Conflicts, withAccessUser(sample.ward, renamedElsewhere), rename},
    };
}

/// Bob joining through his user invite, its result holding once his phone is on the list, then
/// a tablet joining through a device invite Carol made. A join never conflicts.
QList<CommandSample> joinSamples()
{
    AccessSampleWard sample = accessSampleWard();
    Identity bobsPhone = Identity::generate();
    CommandPtr userJoin = JoinCommand::create(sample.ward.access().invites().first(),
                                              AccessDevice("Bob's phone", bobsPhone.publicKey(), bobsPhone.verifyKey()),
                                              "Bob Brown", "Tanglewood Ward");

    Identity carolsInvite = Identity::generate();
    Identity carolsTablet = Identity::generate();
    AccessInvite deviceInvite = AccessInvite::forDevice(accessUserNamed(sample.ward, "Carol").id(),
                                                        carolsInvite.publicKey(), carolsInvite.verifyKey());
    AccessList access = sample.ward.access();
    access.addInvite(deviceInvite);
    Document withDeviceInvite = sample.ward;
    withDeviceInvite.setAccess(access);
    CommandPtr deviceJoin = JoinCommand::create(
        deviceInvite, AccessDevice("Carol's tablet", carolsTablet.publicKey(), carolsTablet.verifyKey()), QString(),
        "Tanglewood Ward");
    return {
        CommandSample{SampleCase::StartHolds, sample.ward, userJoin},
        CommandSample{SampleCase::ResultHolds, runOn(sample.ward, userJoin), userJoin},
        CommandSample{SampleCase::StartHolds, withDeviceInvite, deviceJoin},
    };
}
```

and in `CommandSamples::forType`, before the final `return {};`:

```cpp
    if (type == InviteCommand::TypeName)
    {
        return inviteSamples();
    }
    if (type == RemoveUserCommand::TypeName)
    {
        return removeUserSamples();
    }
    if (type == RemoveDeviceCommand::TypeName)
    {
        return removeDeviceSamples();
    }
    if (type == WithdrawInviteCommand::TypeName)
    {
        return withdrawInviteSamples();
    }
    if (type == SetRoleCommand::TypeName)
    {
        return setRoleSamples();
    }
    if (type == RenameUserCommand::TypeName)
    {
        return renameUserSamples();
    }
    if (type == RenameDeviceCommand::TypeName)
    {
        return renameDeviceSamples();
    }
    if (type == JoinCommand::TypeName)
    {
        return joinSamples();
    }
```

### `tests/services/EveryField.h` and `tests/services/EveryField.cpp`

`everyDeviceField` also fills the device state's other fields:

```cpp
DeviceState everyDeviceField(const Identity& laptop, const Identity& phone)
{
    DeviceState device;
    for (const CommandType& type : CommandRegistry::types())
    {
        QList<CommandSample> samples = CommandSamples::forType(type.name);
        if (samples.isEmpty())
        {
            qFatal("EveryField: the command type %s has no sample", qPrintable(type.name));
        }
        device.journal.append(*samples.front().command);
    }
    device.heldRecipients = {laptop.fingerprint(), phone.fingerprint()};
    device.inviteSeeds = {Crypto::randomBytes(Crypto::KeyBytes)};
    device.joinSeed = Crypto::randomBytes(Crypto::KeyBytes);
    device.noLongerShared = true;
    return device;
}
```

`EveryField::payload` passes `laptop` and `phone` to it. Its comment in `EveryField.h` ends:
"... and a value in every field of this device's state: a journal holding the first sample of
every registered command type in registry order, the laptop and phone as the held version's
recipients, one invite seed, a join seed, and the no-longer-shared flag."

### `tests/services/StrictReadersTest.cpp`

- In `emptiestRoundTrips`, the device part's keys are now
  `QStringList({"heldRecipients", "journal"})`, and the comment on `emptiestPayload` says "a device
  state with an empty journal, no held recipients and no flags".
- `sparsestCommands` adds one command of each new type with every field it may leave out left
  out. It includes `"AccessCommands.h"` and builds, before its `return`:

```cpp
    Identity laptop = Identity::generate();
    Identity phone = Identity::generate();
    Identity invited = Identity::generate();
    AccessUser alice(UserId::generate(), "Alice", AccessRole::Admin,
                     {AccessDevice("Laptop", laptop.publicKey(), laptop.verifyKey()),
                      AccessDevice("Phone", phone.publicKey(), phone.verifyKey())});
    AccessList access({alice}, {}, {});
    AccessInvite deviceInvite = AccessInvite::forDevice(alice.id(), invited.publicKey(), invited.verifyKey());
```

  and appends to the returned list:

```cpp
        InviteCommand::create(laptop.fingerprint(), access, std::nullopt, invited),
        RemoveUserCommand::create(laptop.fingerprint(), alice),
        RemoveDeviceCommand::create(laptop.fingerprint(), alice.devices()[1]),
        WithdrawInviteCommand::create(laptop.fingerprint(), deviceInvite, access),
        SetRoleCommand::create(laptop.fingerprint(), alice, AccessRole::Member),
        RenameUserCommand::create(laptop.fingerprint(), access, "Alice Jones"),
        RenameDeviceCommand::create(laptop.fingerprint(), access, "Work laptop"),
        JoinCommand::create(deviceInvite, AccessDevice("Tablet", phone.publicKey(), phone.verifyKey()), QString(),
                            "Tanglewood Ward"),
```

  Its comment adds "an invite and a join for a device".

### `tests/services/SpecReaderTest.cpp`

In `specReaderRefusesAFieldTheSpecLacksInEveryTable`, add to `objects` the nested objects of the
new commands:

```cpp
        fieldsOf(json, "invite") + QStringList{"invite"},
        fieldsOf(json, "setRole") + QStringList{"role"},
        fieldsOf(json, "renameUser") + QStringList{"name"},
        fieldsOf(json, "renameDevice") + QStringList{"name"},
        fieldsOf(json, "join") + QStringList{"invite"},
        fieldsOf(json, "join") + QStringList{"device"},
        fieldsOf(json, "join") + QStringList{"user"},
```

The every-field journal's join is Bob's user-invite join, the first `join` sample, so it has a
`user` object.

### `tests/spec/Payload.cpp`

The *access and landed* section (`readPublicKeys`, `readUser`, `readInvite`, `readAccess`,
`readLanded`) moves above `struct CommandType`, so the access commands' readers reuse it. In it,
`readUser`'s device loop becomes `readDevice`, and `readInvite` answers whether the invite is
for a user (`readAccess` ignores the answer):

```cpp
void readDevice(Fields fields, std::vector<PublicKeys>* keys)
{
    fields.text("name");
    keys->push_back(readPublicKeys(fields));
    fields.done();
}

void readUser(Fields fields, std::vector<PublicKeys>* keys)
{
    fields.bytes("id", IdBytes);
    fields.text("name");
    fields.word("role", Roles);
    for (const Fields& device : fields.objects("devices"))
    {
        readDevice(device, keys);
    }
    fields.done();
}

/// An invite object, whose keys it adds to keys: true when it is for a user.
bool readInvite(Fields fields, std::vector<PublicKeys>* keys)
{
    std::string kind = fields.word("for", InviteKinds);
    fields.bytes("maker", IdBytes);
    if (kind == "user")
    {
        fields.word("role", Roles);
        fields.text("label");
    }
    keys->push_back(readPublicKeys(fields));
    fields.done();
    return kind == "user";
}
```

After that section, still before `struct CommandType`, add the access commands' readers:

```cpp
// ============================================================================
// access commands
// ============================================================================

void roleValue(Fields& owner, const std::string& key)
{
    owner.word(key, Roles);
}

void readInviteFields(Fields fields)
{
    std::vector<PublicKeys> keys;
    fields.bytes("actor", FingerprintBytes);
    readInvite(fields.object("invite"), &keys);
    fields.done();
}

void readRemoveUserFields(Fields fields)
{
    fields.bytes("actor", FingerprintBytes);
    fields.bytes("user", IdBytes);
    fields.done();
}

void readRemoveDeviceFields(Fields fields)
{
    fields.bytes("actor", FingerprintBytes);
    fields.bytes("device", FingerprintBytes);
    fields.done();
}

void readWithdrawInviteFields(Fields fields)
{
    fields.bytes("actor", FingerprintBytes);
    fields.bytes("invite", FingerprintBytes);
    fields.done();
}

void readSetRoleFields(Fields fields)
{
    fields.bytes("actor", FingerprintBytes);
    fields.bytes("user", IdBytes);
    readChange(fields.object("role"), roleValue);
    fields.done();
}

void readRenameFields(Fields fields)
{
    fields.bytes("actor", FingerprintBytes);
    readChange(fields.object("name"), textValue);
    fields.done();
}

void readJoinFields(Fields fields)
{
    std::vector<PublicKeys> keys;
    bool forUser = readInvite(fields.object("invite"), &keys);
    readDevice(fields.object("device"), &keys);
    if (forUser)
    {
        Fields user = fields.object("user");
        user.bytes("id", IdBytes);
        user.text("name");
        user.done();
    }
    fields.done();
}
```

add to `CommandTypes`:

```cpp
    {"invite", readInviteFields},
    {"removeUser", readRemoveUserFields},
    {"removeDevice", readRemoveDeviceFields},
    {"withdrawInvite", readWithdrawInviteFields},
    {"setRole", readSetRoleFields},
    {"renameUser", readRenameFields},
    {"renameDevice", readRenameFields},
    {"join", readJoinFields},
```

and `readDeviceState` becomes:

```cpp
void readDeviceState(Fields fields)
{
    readJournal(fields.object("journal"));
    std::vector<std::string> held = fields.texts("heldRecipients");
    for (std::size_t i = 0; i < held.size(); ++i)
    {
        if (!decode(held[i], FingerprintBytes).has_value())
        {
            throw Refusal(fields.pathOf("heldRecipients") + "[" + std::to_string(i) + "] is not a fingerprint");
        }
    }
    fields.optionalBytesList("inviteSeeds", KeyBytes);
    std::string joinSeed = fields.optionalText("joinSeed");
    if (!joinSeed.empty() && !decode(joinSeed, KeyBytes).has_value())
    {
        throw Refusal(fields.pathOf("joinSeed") + " is not base64url of 32 bytes");
    }
    fields.optionalTrue("noLongerShared");
    fields.done();
}
```

### `tests/services/WardSessionSavingTest.cpp`

The new ward's device state now holds this device as the held version's one recipient. In the
test that reads a new ward's file back, the line comparing the device state to `DeviceState()`
becomes:

```cpp
    DeviceState expectedDevice;
    expectedDevice.heldRecipients = {client.identity().fingerprint()};
    QVERIFY(read.payload->device.value() == expectedDevice);
```

### `tests/services/UpdateFileTest.h` and `tests/services/UpdateFileTest.cpp`

- `versionThatLeavesThisDeviceOutChangesNothing` becomes
  `versionThatLeavesThisDeviceOutEndsItsShare`. Its assertions after the take become:

```cpp
    QVERIFY(result.status == TakeStatus::NotNamed);
    QVERIFY(alice.session().isNoLongerShared());
    QVERIFY(alice.session().version() == before.version);
    QVERIFY(alice.session().document() == before.payload.ward);
    QVERIFY(alice.reopen().status == OpenStatus::Opened);
    QVERIFY(alice.session().isNoLongerShared());
```

- `versionUnderAnotherKeyMustNameTheHeldOne` becomes
  `versionUnderAnotherKeyMustShareAKeyWithTheHeldOne`; its body stands, since the refused version
  names a key this copy never held and the taken one names the held key.

### `tests/CMakeLists.txt` and `tests/main.cpp`

Add `services/TestSharing.h`, `services/TestSharing.cpp`, `services/AccessTest.h`,
`services/AccessTest.cpp`, `services/FileSharingTest.h` and `services/FileSharingTest.cpp` to
`WardPlanningTests`. In `tests/main.cpp`, include `AccessTest.h` and `FileSharingTest.h`,
construct `AccessTest accessTest(nullptr);` and `FileSharingTest fileSharingTest(nullptr);` after
`updateFileTest`, and run them after it:

```cpp
    failures += QTest::qExec(&accessTest, argc, argv);
    failures += QTest::qExec(&fileSharingTest, argc, argv);
```

### What the user runs

`build.bat`, then `build\bin\WardPlanningTests.exe`. Every test in `AccessTest` and
`FileSharingTest` is new; `CommandRoundTripTest` runs each new type's samples;
`StrictReadersTest` and `SpecReaderTest` cover the new device fields and command fields;
`UpdateFileTest`'s renamed tests run with their new assertions.

### Commit

```bash
git branch --show-current
git add src/services/Crypto.h src/services/Crypto.cpp src/services/Identity.h src/services/Identity.cpp \
    src/services/JsonReader.h src/services/JsonReader.cpp src/models/AccessList.h src/models/AccessList.cpp \
    src/commands/FieldChangeJson.h src/commands/FieldChangeJson.cpp src/commands/AccessRules.h \
    src/commands/AccessRules.cpp src/commands/AccessCommands.h src/commands/AccessCommands.cpp \
    src/commands/CommandRegistry.cpp src/services/DeviceState.h src/services/DeviceState.cpp \
    src/services/InvitationFile.h src/services/InvitationFile.cpp src/services/WardSession.h \
    src/services/WardSession.cpp src/widgets/MainWindow.h src/widgets/MainWindow.cpp CMakeLists.txt \
    docs/FORMAT.md docs/ARCHITECTURE.md tests/CMakeLists.txt tests/main.cpp tests/spec/Payload.cpp \
    tests/services/TestClient.h tests/services/TestClient.cpp tests/services/TestSharing.h \
    tests/services/TestSharing.cpp tests/services/AccessTest.h tests/services/AccessTest.cpp \
    tests/services/FileSharingTest.h tests/services/FileSharingTest.cpp tests/services/CommandSamples.cpp \
    tests/services/EveryField.h tests/services/EveryField.cpp tests/services/StrictReadersTest.cpp \
    tests/services/SpecReaderTest.cpp tests/services/WardSessionSavingTest.cpp \
    tests/services/UpdateFileTest.h tests/services/UpdateFileTest.cpp
git commit -m "Access changes are commands, a removal rotates the key, and users and devices join through invitation files"
```

---

## STOP

**Stop here for the user's own review of the core. Do not start task 2 until the user says so.**

After the user's review, dispatch the read-only reviewer on task 1 as executing-with-review
describes, then continue with task 2 when the user says so.

---

## Task 2: Settings

*Settings*, *Words the user sees*, *Everything else, from the document* (the first-run screen
asks only for a device name; names change in *Settings*), *If the server goes away* (the line
under the switch).

**Files:**
- Create: `src/services/AppSettings.h`, `src/services/AppSettings.cpp`
- Create: `src/widgets/SettingsDialog.h`, `src/widgets/SettingsDialog.cpp`
- Modify: `src/widgets/MapWidget.h`, `src/widgets/MapWidget.cpp` (`Coordinates center() const`)
- Modify: `src/services/DocumentManager.h` (`WardSession& session() { return m_session; }`)
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`, `src/main.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/services/AppSettingsTest.h`, `tests/services/AppSettingsTest.cpp`; modify
  `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`

**`AppSettings`** holds the app's own settings, app-wide, in `settings.json` in app data (the
path `AppData::settingsFilePath()` already gives): one JSON object, read once when made and
written whole through `QSaveFile` by every setter.

```cpp
/// The app's own settings, app-wide, in one JSON file: read when made, and written whole by
/// every change.
class AppSettings
{
public:
    explicit AppSettings(const QString& path);

    /// Use the Ward Planning server: off unless the user turns it on.
    bool useServer() const;
    void setUseServer(bool use);

    /// This device's name, which the first start asks for; empty until then.
    QString deviceName() const;
    void setDeviceName(const QString& name);

    /// Your name, which fills in the name asked for when the user creates or joins a ward.
    QString userName() const;
    void setUserName(const QString& name);

    std::optional<DocumentId> lastWard() const;
    void setLastWard(const DocumentId& document);

    /// Where the map opens.
    std::optional<Coordinates> mapHome() const;
    void setMapHome(const Coordinates& home);

private:
    void save() const;

    QString m_path;
    QJsonObject m_json;
};
```

Keys: `useServer` (boolean), `deviceName`, `userName`, `lastWardId` (strings), and the map home
as `defaultLat` and `defaultLng` (numbers), the keys `MainWindow` writes now. A missing file, or
one that does not parse as an object, reads as empty, which is every default.

**`MainWindow`** takes `AppSettings*` in place of `settingsFilePath`. `loadSettings`,
`saveSettings`, `loadSetting`, `saveSetting`, `loadDefaultLocation`, `saveDefaultLocation` and
`m_settingsFilePath` are deleted; every caller reads and writes through `AppSettings`.
`initializeDefaultLocation` reads `mapHome()`, and `onLocationReady` calls `setMapHome`. `main`
makes `AppSettings settings(appData.settingsFilePath());` and passes `&settings`.

**`SettingsDialog`** (`SettingsDialog(const AppSettings& settings, const Coordinates& currentView,
QWidget* parent)`), one short form:
- A check box *Use the Ward Planning server*, with the line under it: *Turn this off only if the
  Ward Planning server has shut down. Every member of your wards must turn it off too.*
- *This device's name* and *Your name*, line edits holding the current values.
- *Map home*: a label showing where the map opens, and *Use current view*, which sets the
  dialog's pending map home to `currentView` and shows it.
- OK is enabled while *This device's name* is not empty, and *Your name* is not empty or was
  empty when the dialog opened.
- Accessors for what the user set: `useServer()`, `deviceName()`, `userName()` (each trimmed),
  `mapHome()` (`std::optional<Coordinates>`, set only by *Use current view*).

**`MainWindow::onSettings`**, on *Settings...* in the File menu, above *Exit*. On OK, in order:
- The switch saved through `setUseServer`.
- A changed device name: `setDeviceName`, then `session().renameDeviceEverywhere(name)`.
- A changed, non-empty user name: `setUserName`, then `session().renameUserEverywhere(name)`.
- A map home set: `setMapHome`.

The open ward's renames reach the window through `documentChanged`, as any command's do.

**Following a name set on another device:** in `openUpdateFile`, after a take whose status is
`Taken`, `if (std::optional<QString> name = DocumentManager::instance()->session().followUserName(m_settings->userName()))`
then `m_settings->setUserName(*name)`.

**`MapWidget::center()`** answers `Coordinates(m_centerLat, m_centerLng)`.

**Tests** (`AppSettingsTest`, each over a file in a `QTemporaryDir`):
- `missingFileReadsAsEveryDefault`: `useServer()` false, both names empty, no last ward, no map
  home; anchored by a second `AppSettings` on a file holding `{"deviceName":"Laptop"}`, which
  reads `"Laptop"`.
- `eachValueSurvivesANewSettingsOnTheSameFile`: `true`, `"Laptop"`, `"Alice Smith"`, a generated
  document id and `Coordinates(40.2338, -111.6585)` set on one, read back equal by another.

**Docs:** `docs/ARCHITECTURE.md`'s *DocumentManager* paragraph says `MainWindow` keeps its
settings through `AppSettings` and names the keys; `docs/USER_GUIDE.md` gains a *Settings*
section in the words of *Words the user sees*: the switch and the line under it, the two names
and that changing one renames you or this device in every ward this device holds, and *Map
home* with *Use current view*.

**Commit:** `git commit -m "Settings holds the server switch, this device's name, your name and the map home, and a name change renames it in every ward"`

---

## Task 3: Sending updates, and the notices of what to share

*Update files* (*Sending*, *Changes to share*), *Every edit is a version* (with the server off,
no ward is syncing), *Everything else, from the document* (*The ward is no longer yours*),
*Failure paths*, *Words the user sees*.

**Files:**
- Create: `src/widgets/ShareNotice.h`, `src/widgets/ShareNotice.cpp`
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`
- Modify: `src/services/WardStore.h`, `src/services/FileWardStore.h`,
  `src/services/FileWardStore.cpp`, `tests/services/FakeWardStore.h`,
  `tests/services/FakeWardStore.cpp`
- Modify: `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Modify: `src/services/WardList.h`, `src/services/WardList.cpp`,
  `src/widgets/OpenWardDialog.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/services/FileWardStoreTest.h/.cpp`, `tests/services/WardListTest.h/.cpp`
- Modify: `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`

**Send Update** is a member action, `m_sendUpdateAction`, shown only while
`!m_settings->useServer()`, and enabled while the open ward is still this device's.
`MainWindow::updateSharing()` sets it and the notice, and runs after `documentChanged`, after a
send, a take, an open or a new ward, and after *Settings* closes with OK.

**`ShareNotice`**, a banner above the map built as `EmergencyBanner` is, with a text and one
button, and two signals, `sendUpdateRequested()` and `deleteCopyRequested()`:
- The ward is no longer this device's: *This ward is no longer shared with this device.* with
  *Delete this copy*. It stays until the user deletes the copy.
- Otherwise, with the switch off and `hasChangesToShare()`: *You have changes to share* with
  *Send Update*.
- Otherwise hidden.

**Deleting a copy.** `WardStore` gains `virtual bool remove(const DocumentId& document,
QString* problem) = 0;`: `FileWardStore` removes the ward's file, `FakeWardStore` its entry.
`DocumentManager::removeOpenWard(QString* problem)` removes the open ward's file. *Delete this
copy* asks first (*Delete this copy of Tanglewood Ward from this device?*, **Delete** /
Cancel), removes it, and shows the ward list; closing the list with no ward open closes the
window, which quits the app.

**The ward list** marks wards by the same rules: `WardListEntry::changesWaiting` is replaced by
`changesToShare` (`WardSession::changesToShare` over the local file's journal and landed marks
and this device's fingerprint) and `noLongerShared`. `OpenWardDialog` shows `%1 (changes to
share)` and `%1 (no longer shared)`.

**Messages** (`openUpdateFile`): after `NotNamed` and `InviteUsedOrWithdrawn` tell the user, each
runs `updateSharing()`. `showChangesToRedo` takes the `ReplayResult` and adds,
for each name in `stillHaveAccess`, *%1 still has access until you remove them again.* after the
line that says what stopped the replay.

**Tests:**
- `FileWardStoreTest::removeDeletesOnlyThatWardsFile`: two wards written, one removed; `read`
  answers `Missing` for it and `Read` with its bytes for the other, and `documents()` lists only
  the other.
- `WardListTest::listMarksChangesToShareAndWardsNoLongerShared`: three wards in a
  `FakeWardStore`, one with a journal entry above this device's mark, one whose entry a held
  version's mark covers, one no longer shared; the entries' `changesToShare` are true, false,
  false and `noLongerShared` false, false, true.

**Docs:** `docs/USER_GUIDE.md` gains *Sharing updates*: *Send Update* with the server switched
off, where the file goes, that each member sends to every other member, what *You have changes
to share* means, opening an update, and the no-longer-shared notice. `docs/ARCHITECTURE.md`'s
*DocumentManager* paragraph names `ShareNotice` and `removeOpenWard`.

**Commit:** `git commit -m "Send Update is offered with the server switched off, and a notice says when there are changes to share or the ward is no longer shared"`

---

## Task 4: Who has access

*Everything else, from the document* (*Who has access*), *Invites* (Add a user, Add a device,
Save invitation, the invitation's message), *Replay* (the rules), *Failure paths* (the not-admin
row, the server-off rows for Leave and *Remove this device*), *Words the user sees*, *Threat
model* (roles are a convention).

**Files:**
- Create: `src/widgets/WhoHasAccessDialog.h`, `src/widgets/WhoHasAccessDialog.cpp`
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp` (*Who Has Access...* in the
  File menu, below *Show in Folder*)
- Modify: `CMakeLists.txt`
- Create: `tests/services/WhoHasAccessDialogTest.h`, `tests/services/WhoHasAccessDialogTest.cpp`;
  modify `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `docs/USER_GUIDE.md`

**The dialog** (`WhoHasAccessDialog(WardSession& session, const AppSettings& settings,
QWidget* parent)`) lists the users, each with their role (*admin* / *member*) and devices, this
device marked *this device*, and the open invites by `describeInvite` (*for Bob*, *a device for
Alice*). It rebuilds from the session's document on `documentChanged`, keeping the selection by
key. The actions on the selected row are offered only where `AccessRules` allows them for this
device (the actor is `session.deviceFingerprint()`), and each runs through
`DocumentManager::executeCommand`, so a refusal or conflict is told as any command's is:
- a user: *Remove* (`RemoveUserCommand`), *Make admin* / *Make member* (`SetRoleCommand`);
- another device of this device's user: *Remove device* (`RemoveDeviceCommand`);
- this device: *Remove this device*, which shows *Removing this device needs the Ward Planning
  server, which is turned off in Settings.* with the switch off, and *Connect to the internet to
  remove this device.* with it on;
- an open invite: *Withdraw invite* (`WithdrawInviteCommand`), and *Save invitation* for one in
  `invitesMadeHere()`.

Below the list: *Add a user*, which a member sees replaced by the text *To add or remove users,
ask an admin: names* (`AccessRules::invite`'s refusal); *Add a device*; and *Leave*, which shows
*Leaving a ward needs the Ward Planning server, which is turned off in Settings.* with the switch
off and *Connect to the internet to leave this ward.* with it on. With the switch on, *Add a
user* and *Add a device* show *Connect to the internet to invite someone.* in place of making an
invite. A ward that is no longer this device's shows the list with every action disabled. The
line *Names change in Settings.* sits under the list.

**Add a user** asks who the invitation is for (a line edit) with *Make them an admin*, off by
default, and makes nothing while the answer is empty or the user cancels. Then
`session.invite(InvitedUser{role, label})`, and on `Applied` the *Save invitation* flow for the
new invite. **Add a device** is `session.invite(std::nullopt)` and the same flow.

**Save invitation** writes `session.writeInvitation(fingerprint)` where the user chooses
(`QFileDialog::getSaveFileName`, starting at `UserFolder::standard().path()` with
`InvitationFile::fileName(wardName, describeInvite(invite))`, filter *Ward Planning invitations
(\*.wardinvite)*) through `QSaveFile`, reveals it with `ShowInFolder::reveal`, and shows the
message to send with it, selectable: for a user invite *%1 has invited you to %2. Install Ward
Planning, then open the attached invitation.* (the user's own name, the ward's name); for a
device invite *Send this invitation to yourself and open it on your other device.*

**The dialog's offers are one function**, `QList<AccessAction> offeredActions() const` over the
selection, plus `QString addUserNotice() const` (empty when *Add a user* is offered), which the
buttons follow and the test reads.

**Tests** (`WhoHasAccessDialogTest`, a dialog over a `TestClient`'s session and an
`AppSettings` in a `QTemporaryDir`; nothing is shown):
- `memberSeesAskAnAdminInPlaceOfAddAUser`: for Carol, a member of a ward whose admins are Bob
  and Alice, `addUserNotice()` is exactly *To add or remove users, ask an admin: Alice, Bob*; for
  Alice it is empty.
- `removeAndRolesAreOfferedOnlyWhereTheRulesAllow`: Alice (admin) selecting Carol is offered
  Remove and Make admin; selecting herself, neither; Carol (member) selecting Alice, neither.
- `saveInvitationIsOfferedOnlyForInvitesThisDeviceMade`: Alice's own invite offers Save
  invitation and Withdraw invite; an invite Bob made, taken through an update, offers Withdraw
  invite only.
- `addAUserWithNoAnswerMakesNoInvite`: the dialog's add-user step given an empty label leaves the
  journal empty and `invitesMadeHere()` empty; given *Bob*, one invite.

**Docs:** `docs/USER_GUIDE.md` gains *Who has access*: users, roles and devices; Add a user,
Add a device and the invitation file; Remove, Make admin / Make member, Remove device, Withdraw
invite; that a removal takes effect on each device as the update reaches it; that names change
in *Settings*; and that roles are an agreement among the people who use the ward, which keeps
well-meaning users from over-sharing, and not a security measure.

**Commit:** `git commit -m "Who has access lists users, devices and invites, offers each change its rules allow, and saves invitations"`

---

## Task 5: Receiving an invitation

*Receiving an invitation*, *Invites* (joining), *Failure paths* (the invitation rows), *Words
the user sees*.

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`
- Modify: `src/widgets/OpenWardDialog.h`, `src/widgets/OpenWardDialog.cpp`
- Modify: `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Modify: `installer.iss`
- Modify: `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`

**Opening a file.** `MainWindow::openUpdateFile` and the new `openInvitationFile` sit behind one
slot, `openFile(const QString& path)`, which goes by the suffix: `.wardinvite` opens an
invitation, anything else an update. `onHandedOver`, the drop (`dragEnterEvent` accepts one
`.wardupdate` or `.wardinvite` file), and `main`'s positional argument (renamed `file`, *An
update or invitation file to open.*) all call it. `main`'s message for a launch whose handover
nobody took says *the file was not opened* and names *File > Open Update* and *File > Join a
Ward*.

**`openInvitationFile(path)`** reads the file (*The file %1 could not be read.* when it cannot),
calls `session().readInvitation(bytes)`, and by its status:
- `NotAnInvitation`: *This file is not a Ward Planning invitation.* (the problem goes to the log).
- `Newer`: *This ward was saved by a newer version of Ward Planning. Update to see the latest
  changes and share your work.*
- `AlreadyHasAccess`: when `mayLeaveWard()`, `DocumentManager::openWard(invited.document)`, the
  last ward set to it, and *This device already has access to %1.*
- `Joinable`: asks only *Join %1?* with the ward's own name, with no name field. On OK and
  `mayLeaveWard()`: `DocumentManager::join(invited, m_settings->userName(),
  m_settings->deviceName())`, the last ward set to it, `followUserName` applied as after a
  take, and *You have joined %1. Send an update to every member of the ward
  so they see you.*; the share notice then shows *You have changes to share*.

**`DocumentManager::join(const InvitedWard&, const QString& userName, const QString&
deviceName)`** runs `m_session.join` and then the automatic updates, as `openWard` does.

**Join a Ward...** in the File menu, below *Open Ward...*, and a *Join a Ward...* button in the
ward list (`WardListChoice::Join`), offer a file picker (*Ward Planning invitations
(\*.wardinvite)*) and call `openInvitationFile`. The ward list is what a first start with no
wards shows, so a new user lands on it. Its empty-list line becomes *This device holds no wards
yet. Create one with New Ward, or open an invitation with Join a Ward.*

**`installer.iss`** registers `.wardinvite` as `WardPlanning.Invitation` (*Ward Planning
invitation*), opening with the app, beside the `.wardupdate` entries.

**Tests:** the session's side of every outcome is in task 1's `FileSharingTest`. The handover of
a path is the instance lock's existing test, which passes any path.

**Docs:** `docs/USER_GUIDE.md` gains *Joining a ward*: opening the invitation (double-click, drop
on the window, *Join a Ward*), what *This invite has been used or withdrawn* means, and sending
an update after joining. `docs/ARCHITECTURE.md`'s *DocumentManager* paragraph says the window
opens update and invitation files through `openFile`.

**Commit:** `git commit -m "Opening an invitation joins: by double-click, drop, File > Join a Ward or the ward list"`

---

## Task 6: The independent reader reads invitations

*Testing* (the known-answer vectors for the invite keypairs; an invitation file reads back),
`docs/FORMAT.md` *Invitation files*.

**Files:**
- Modify: `tests/spec/main.cpp`
- Modify: `tests/services/SpecReaderTest.h`, `tests/services/SpecReaderTest.cpp`

**`tests/spec/main.cpp`** gains a mode, `WardPlanningSpecReader --invitation <file>`, which reads
the file as `docs/FORMAT.md`'s *Invitation files* states and nothing of the app's: the line
strictly (`format`, `seed`, nothing else; a `format` above 1 exits 2); the invite's keys from
the seed with `crypto_kdf_derive_from_key` (context `wpinvite`, subkeys 1 and 2) into
`crypto_box_seed_keypair` and `crypto_sign_seed_keypair`; its fingerprint as for a device; the
header's entry for that fingerprint, unsealed; the payload read as a version; header and access
list agreeing. It prints the payload as compact
JSON, then a newline, then the invite's fingerprint in base64url.

**Tests:**
- `specReaderReadsAnInvitationTheAppWrites`: an invitation a `WardSession` writes for a user
  invite; the reader exits 0, its payload equals the version's payload as `WardFile::read` gives
  it to `Identity::fromInviteSeed(seed)`, and its fingerprint equals the invite's. The reader
  derives the keys with libsodium's own seed-keypair calls, so a difference between those and
  `Identity::fromInviteSeed` fails here.
- `specReaderRefusesAnInvitationWhoseSeedOpensNothing`: the same file with its seed replaced by
  another seed exits 1 with a problem naming the recipient; anchored by the unmutated file
  exiting 0 in the same test.

**Commit:** `git commit -m "tests/spec reads invitation files from FORMAT.md alone, deriving the invite's keys from its seed"`

---

## Open questions for the user

The design is silent on these, or two of its sentences pull different ways; each states the
choice this plan makes.

1. **The device state keeps the held version's recipients** (`heldRecipients`), a field the
   design's list of device-local state does not have. Rotation is decided against the recipients
   of the version this device holds, and with update files that version can be days and app
   restarts old, so the set has to survive closing the app.
2. **An invitation carries the next version, which the inviter does not then hold.** The design
   says the invitation is saved at once and the invite still brings up *You have changes to
   share*. Holding the invitation's version would clear the notice, though no member has it. So
   the inviter's next update has the same number and writer as the invitation's version, with
   more in it; the joiner's join outranks both, and each device's replay converges them.
3. **A joined device writes as the invite while the version it holds does not name it**, and as
   itself otherwise. That covers its first join and every rejoin, which go to members who list
   the invite and not the device. A member who never took the join, and gets an update the new
   member wrote while holding a version that names it, refuses it with *This update is from a
   device this copy of the ward does not list yet*; the next update from a member who took the
   join settles it. Accepted the same way: after a rejoin, members who already took the join
   refuse the rejoined device's update, written by the invite they no longer list, with the same
   message; the data converges through another member's next update.
4. **Which message a version naming neither this device nor its invite gives** is decided by
   whether the journal still holds this device's join: while it does, the join is refused, *This
   invite has been used or withdrawn*; once a taken version's landed mark has carried it, the
   version is a removal, *This ward is no longer shared with this device*. A device removed while
   a rejoin of its own is still unsent gets the invite wording. Both end in the no-longer-shared
   state.
5. **Two joins through one invitation: accepted that the second is refused as from an unknown
   device.** Both are written by the invite; a member who has taken the first no longer lists
   the invite, so the second shows *This update is from a device this copy of the ward does not
   list yet. Open the update from whoever added it first.* The second device learns it was too
   late when it takes a later version, which names neither it nor the invite: *This invite has
   been used or withdrawn*, and its copy becomes no longer shared.
6. **The rename commands name only the device that made them**, so renaming another user or their
   device cannot be expressed and has no rule. The design's test "renaming another user or their
   device is refused" has no case; `AccessTest::renamesReachEveryDevice` covers the rest.
7. **With the server switch on, nothing shares**, since there is no server code: *Send Update*
   is hidden, and *Add a user*, *Add a device*, *Leave* and *Remove this device* show the
   connect-to-the-internet notices, the design's words for a ward that is not syncing because it
   is offline. Receiving updates and invitations works with it on or off.
8. **The invitation's message has no download link**, since there is no server to serve
    `/download`: *Alice Smith has invited you to Tanglewood Ward. Install Ward Planning, then
    open the attached invitation.*
9. **The ward list marks "(changes to share)"** by the changes-to-share rule, in place of
    "(changes waiting)" for a non-empty journal, and "(no longer shared)".
10. **`DocumentManager` gains `WardSession& session()`** for the new calls, in place of a
    forwarding function for each. Its existing forwarders (`writeUpdate`, `takeUpdate`, undo and
    redo, and the rest) could go the same way; the plan leaves them.
11. **The invite keypairs' known-answer vectors are not pinned as literal values**, since the
    executor cannot run the code to observe them. Task 6's independent reader derives the keys
    with libsodium's own seed-keypair calls, which stands in. Literal values can be pinned from
    the first run if you want them too.
12. **Joining asks only *Join Tanglewood Ward?*** A user invite takes *Your name* from
    *Settings*; a device invite's device takes its user's name from the ward, which
    `followUserName` then makes *Your name*.
13. **Where the invitation is saved:** a save dialog starting in the *Ward Planning* folder in
    Documents, as the design's "where the user chooses".
