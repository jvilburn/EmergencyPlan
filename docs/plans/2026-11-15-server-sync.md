# Server Sync Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** With *Use the Ward Planning server* turned on, the open ward shares itself through the
server: every change is pushed as it is made, every other member's version is taken as it lands,
the window says who else is here, and it says what stops sharing when something does.

**Architecture:** `WardSession` stays the one place that changes a ward; it gains the two calls
every way of sharing uses, `takeVersion` and `holdPushed`, which update files already go through
under other names. `WardSync` sits beside the session and owns the one orchestration, the *look*:
read the server's head, then take it, push over it, create the ward, or end this device's share,
and settle every unanswered write the same way. It holds the ward's live channel, which says who
is here and when to look. `Server` is the wire: it turns each call into a request and reads the
reply, over a `Transport` that is HTTP in the app (`NetworkTransport`) and an in-process
`FakeServer` in the tests.

**Tech Stack:** Qt 6.12 (Core, Gui, Widgets, Network, WebSockets, Test), C++17, MSVC 2022, CMake +
Ninja via `build.bat`, libsodium from vcpkg, Qt Test. The server is `server/` in this repository,
deployed at `https://ward-planning-dev.ward-planning-server.workers.dev`.

**Design authority:** `docs/SHARED_WARD_DESIGN.md`: *What the server is*, *The live channel*, *Who
is here*, *Every edit is a version*, *Which version wins*, *Saving*, *If the server goes away*,
*Failure paths*, *Testing*. Section names in *italics* are its sections. The server as built is
`server/src/ward.ts` and `server/README.md`.

**What a second plan covers, and this one leaves as it is:** an invitation saved only once the
version naming it is on the server; joining online with its lost-answer cases and *The server has
no such ward*; Leave and *Remove this device*; the update check from `/releases/latest`; the
server address override; and the switch being on by default. After this plan, invitations and
joins work as they do with the server off, and their versions travel through the server by the
ordinary path.

---

## Conventions for every task

- **The executor does not build.** The project's `CLAUDE.md` forbids running cmake or the app from
  this shell. The user builds with `build.bat` and runs `build\bin\WardPlanningTests.exe`. Each
  task's tests are written in that task and run when the user builds. The server's own checks
  (`npm test`, `npm run live` in `server/`) the executor runs itself.
- **Core first.** Task 1 is the whole design-bearing core, then a STOP for the user's own review.
  Every later task is a consequence of it. The tree and the app may be red between tasks.
- **Delete with the last caller**, in the same task.
- **Docs change in the task whose code they describe** (`docs/FORMAT.md`,
  `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`, `docs/SHARED_WARD_DESIGN.md`).
- **Style:** `docs/CODING_STYLE.md` and the project `CLAUDE.md`: Allman braces, braces on every
  control statement, only one-liners in headers, `std::optional` for nullable fields, leading
  operators on continuation lines, `tr()` for every user-visible string. Comments say what is true
  now. ASCII only.
- **Commits:** one per task, with the message the task gives, plain, no attribution lines. Before
  each commit, `git branch --show-current` prints `feature/server-sync`. Stage the files the task
  changed by name.
- **Tests** pin observed values, use inputs that give non-trivial results, and anchor every
  absence assertion to a positive signal.

---

## Task 1: The core: the wire, the look, and the two calls every way of sharing uses

*What the server is*, *The live channel*, *Every edit is a version* (the checks, a head that ranks
below, a ward is syncing), *Which version wins*, *Saving* (the push follows the save).

How it fits together:

- **A look settles everything.** `WardSync::look` reads the head and acts on how it ranks against
  the version this device holds. Every trigger (a local change, a notice on the channel, a retry)
  only asks for a look, and one look runs at a time, so there is one path to reason about.
- **A write is stored or it is not.** A write answered as stored is held. Any other answer, or
  none, is followed by another look, which finds out from the head what happened.
- **`onServer`**, a flag in this device's state, says this device has seen the server hold the
  ward. A `404` for a ward with the flag is the ward gone; without it, the ward is created.

**Files:**
- Create: `src/services/SignedLine.h`, `src/services/SignedLine.cpp`
- Create: `src/services/Transport.h`
- Create: `src/services/Server.h`, `src/services/Server.cpp`
- Create: `src/services/WardSync.h`, `src/services/WardSync.cpp`
- Modify: `src/services/WardSession.h`, `src/services/WardSession.cpp`
- Modify: `src/services/DeviceState.h`, `src/services/DeviceState.cpp`
- Modify: `src/services/UpdateFile.h`, `src/services/UpdateFile.cpp` (its line comes from `SignedLine`)
- Modify: `CMakeLists.txt` (the new sources; `WebSockets` in `find_package` and `Qt6::WebSockets`
  in the link list, which Task 3's `NetworkTransport` uses)
- Modify: `docs/FORMAT.md`, `tests/spec/Payload.cpp`, `tests/services/EveryField.cpp` (`onServer`)

**Step 1: `SignedLine`**

`src/services/SignedLine.h`:

```cpp
#pragma once

#include "Id.h"

#include <QByteArray>

/// What a signed line is for.
enum class SignedCall
{
    Create,  ///< the first version of a ward, sent to the server
    Push,    ///< a later version, sent to the server
    Update   ///< a version in an update file
};

/// The UTF-8 line a device signs, which names what it is for, the ward, and the exact bytes:
/// wardplanning-v1 <call> <document> <header hash> <payload hash>, with single spaces.
class SignedLine
{
public:
    /// The line for version, a version of document in a ward file's framing: each hash is
    /// BLAKE2b-256 of the bytes on its side of the version's first newline, in base64url.
    static QByteArray ofVersion(SignedCall call, const DocumentId& document, const QByteArray& version);

    /// The line that opens document's live channel, with - for each hash it lacks.
    static QByteArray ofChannel(const DocumentId& document);
};
```

`src/services/SignedLine.cpp`:

```cpp
#include "SignedLine.h"

#include "Base64Url.h"
#include "Crypto.h"

namespace
{
QByteArray wordFor(SignedCall call)
{
    switch (call)
    {
    case SignedCall::Create:
        return "create";
    case SignedCall::Push:
        return "push";
    case SignedCall::Update:
        return "update";
    }
    return QByteArray();
}

QByteArray hashOf(const QByteArray& bytes)
{
    return Base64Url::encode(Crypto::hash(bytes, Crypto::HashBytes, QByteArray())).toLatin1();
}
}  // namespace

QByteArray SignedLine::ofVersion(SignedCall call, const DocumentId& document, const QByteArray& version)
{
    qsizetype boundary = version.indexOf('\n');
    return "wardplanning-v1 " + wordFor(call) + ' ' + document.toString().toLatin1() + ' '
        + hashOf(version.left(boundary)) + ' ' + hashOf(version.mid(boundary + 1));
}

QByteArray SignedLine::ofChannel(const DocumentId& document)
{
    return "wardplanning-v1 channel " + document.toString().toLatin1() + " - -";
}
```

`UpdateFile::signedLine` is deleted; `UpdateFile::write` and `UpdateFile::verifies` call
`SignedLine::ofVersion(SignedCall::Update, ...)`.

**Step 2: `src/services/Transport.h`**

Every call is an object that emits its answer once, so the caller connects a slot to it, and
destroying the call drops its answer. Nothing here takes a callback.

```cpp
#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

/// One request to the server: its method, its path from the server's address, its headers and
/// its body.
struct TransportRequest
{
    QByteArray method;
    QString path;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
};

/// The server's reply: its status and its body. A request that got no answer, because the
/// server could not be reached or the connection failed, has the status NoAnswer.
struct TransportReply
{
    static constexpr int NoAnswer = 0;

    int status = NoAnswer;
    QByteArray body;
};

/// One request on its way. It emits answered once, after Transport::send has returned, and
/// then deletes itself. Destroying it first drops the answer.
class TransportCall : public QObject
{
    Q_OBJECT

public:
    explicit TransportCall(QObject* parent) : QObject(parent) {}

signals:
    void answered(const TransportReply& reply);
};

/// An open WebSocket to the server.
class TransportChannel : public QObject
{
    Q_OBJECT

public:
    explicit TransportChannel(QObject* parent) : QObject(parent) {}

    /// Closes the socket now. closed is not emitted for a close asked for here.
    virtual void close() = 0;

signals:
    /// The server sent a text message.
    void messageReceived(const QByteArray& message);

    /// The socket closed, or never opened. Emitted once, after Transport::open has returned.
    void closed();
};

/// What carries requests to the server and replies back: HTTP in the app, and the fake server,
/// in process, in the tests.
class Transport
{
public:
    virtual ~Transport() = default;

    /// Sends request. The call is a child of parent.
    virtual TransportCall* send(const TransportRequest& request, QObject* parent) = 0;

    /// Opens a WebSocket with request, a GET whose headers ride on the upgrade. The channel is
    /// a child of parent.
    virtual TransportChannel* open(const TransportRequest& request, QObject* parent) = 0;
};
```

**Step 3: `Server`**

`src/services/Server.h`:

```cpp
#pragma once

#include "Id.h"
#include "Identity.h"
#include "Transport.h"

#include <QByteArray>
#include <QObject>
#include <QSet>

enum class ServerReadStatus
{
    Found,
    Missing,  ///< the server has no such ward
    NoAnswer  ///< the server could not be reached, or answered with a fault
};

/// What a head or a fetch answered.
struct ServerRead
{
    ServerReadStatus status;
    QByteArray bytes;  ///< when Found: the header line for a head, the whole version for a fetch
};

/// A create or a push on its way. It emits answered once and then deletes itself; destroying
/// it first drops the answer.
class ServerWrite : public QObject
{
    Q_OBJECT

public:
    /// Reads call's reply; call becomes this object's child.
    ServerWrite(TransportCall* call, QObject* parent);

signals:
    /// Whether the server stored the version. Any other answer, or none, is not stored.
    void answered(bool stored);

private slots:
    void onAnswered(const TransportReply& reply);
};

/// A head or a fetch on its way, as ServerWrite is.
class ServerReading : public QObject
{
    Q_OBJECT

public:
    ServerReading(TransportCall* call, QObject* parent);

signals:
    void answered(const ServerRead& read);

private slots:
    void onAnswered(const TransportReply& reply);
};

/// A ward's live channel: the server's messages on it, read.
class ServerChannel : public QObject
{
    Q_OBJECT

public:
    /// Reads channel's messages; channel becomes this object's child.
    ServerChannel(TransportChannel* channel, QObject* parent);

    /// Closes the channel now. closed is not emitted for a close asked for here.
    void close() { m_channel->close(); }

signals:
    /// The server said who is here, each by fingerprint, this device among them, and the
    /// number of the version it holds.
    void told(const QSet<QByteArray>& present, qint64 version);

    /// The channel closed, or never opened.
    void closed();

private slots:
    void onMessageReceived(const QByteArray& message);

private:
    TransportChannel* m_channel;
};

/// The Ward Planning server's calls, as docs/SHARED_WARD_DESIGN.md gives them under What the
/// server is and The live channel: each becomes one request on the transport, signed where the
/// server asks for a signature, and its reply is read down to what the app acts on. What each
/// call returns is a child of parent.
class Server
{
public:
    /// transport carries the requests; the server does not own it.
    explicit Server(Transport* transport);

    /// POST /documents: version becomes the ward's first version on the server. signer is the
    /// version's writer.
    ServerWrite* create(const DocumentId& document, const QByteArray& version, const Identity& signer,
                        QObject* parent);

    /// PUT /documents/{id}: version becomes the ward's current version, when the server keeps
    /// it. signer is the version's writer.
    ServerWrite* push(const DocumentId& document, const QByteArray& version, const Identity& signer,
                      QObject* parent);

    /// GET /documents/{id}/head: the current version's header line.
    ServerReading* head(const DocumentId& document, QObject* parent);

    /// GET /documents/{id}: the current version.
    ServerReading* fetch(const DocumentId& document, QObject* parent);

    /// Opens document's live channel, signed by signer.
    ServerChannel* openChannel(const DocumentId& document, const Identity& signer, QObject* parent);

private:
    ServerWrite* write(const QByteArray& method, const QString& path, const QByteArray& line,
                       const QByteArray& version, const Identity& signer, QObject* parent);

    Transport* m_transport;
};
```

`src/services/Server.cpp`:

```cpp
#include "Server.h"

#include "Base64Url.h"
#include "SignedLine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString pathOf(const DocumentId& document)
{
    return "/documents/" + document.toString();
}

/// The Ward-Signer and Ward-Signature headers of signer signing line.
QList<QPair<QByteArray, QByteArray>> signedBy(const Identity& signer, const QByteArray& line)
{
    QByteArray keys = Base64Url::encode(signer.publicKey()).toLatin1() + '.'
        + Base64Url::encode(signer.verifyKey()).toLatin1();
    return {{"Ward-Signer", keys}, {"Ward-Signature", Base64Url::encode(signer.sign(line)).toLatin1()}};
}
}  // namespace

ServerWrite::ServerWrite(TransportCall* call, QObject* parent)
    : QObject(parent)
{
    call->setParent(this);
    connect(call, &TransportCall::answered, this, &ServerWrite::onAnswered);
}

void ServerWrite::onAnswered(const TransportReply& reply)
{
    emit answered(reply.status == 200 || reply.status == 201);
    deleteLater();
}

ServerReading::ServerReading(TransportCall* call, QObject* parent)
    : QObject(parent)
{
    call->setParent(this);
    connect(call, &TransportCall::answered, this, &ServerReading::onAnswered);
}

void ServerReading::onAnswered(const TransportReply& reply)
{
    switch (reply.status)
    {
    case 200:
        emit answered(ServerRead{ServerReadStatus::Found, reply.body});
        break;
    case 404:
        emit answered(ServerRead{ServerReadStatus::Missing, QByteArray()});
        break;
    default:
        emit answered(ServerRead{ServerReadStatus::NoAnswer, QByteArray()});
        break;
    }
    deleteLater();
}

ServerChannel::ServerChannel(TransportChannel* channel, QObject* parent)
    : QObject(parent)
    , m_channel(channel)
{
    channel->setParent(this);
    connect(channel, &TransportChannel::messageReceived, this, &ServerChannel::onMessageReceived);
    connect(channel, &TransportChannel::closed, this, &ServerChannel::closed);
}

void ServerChannel::onMessageReceived(const QByteArray& message)
{
    QJsonObject json = QJsonDocument::fromJson(message).object();
    QSet<QByteArray> present;
    for (const QJsonValue& fingerprint : json.value("present").toArray())
    {
        present.insert(Base64Url::decode(fingerprint.toString()));
    }
    emit told(present, json.value("version").toInteger());
}

Server::Server(Transport* transport)
    : m_transport(transport)
{
}

ServerWrite* Server::create(const DocumentId& document, const QByteArray& version, const Identity& signer,
                            QObject* parent)
{
    return write("POST", "/documents", SignedLine::ofVersion(SignedCall::Create, document, version), version,
                 signer, parent);
}

ServerWrite* Server::push(const DocumentId& document, const QByteArray& version, const Identity& signer,
                          QObject* parent)
{
    return write("PUT", pathOf(document), SignedLine::ofVersion(SignedCall::Push, document, version), version,
                 signer, parent);
}

ServerReading* Server::head(const DocumentId& document, QObject* parent)
{
    TransportRequest request{"GET", pathOf(document) + "/head", {}, QByteArray()};
    return new ServerReading(m_transport->send(request, nullptr), parent);
}

ServerReading* Server::fetch(const DocumentId& document, QObject* parent)
{
    TransportRequest request{"GET", pathOf(document), {}, QByteArray()};
    return new ServerReading(m_transport->send(request, nullptr), parent);
}

ServerChannel* Server::openChannel(const DocumentId& document, const Identity& signer, QObject* parent)
{
    TransportRequest request{"GET", pathOf(document) + "/channel",
                             signedBy(signer, SignedLine::ofChannel(document)), QByteArray()};
    return new ServerChannel(m_transport->open(request, nullptr), parent);
}

ServerWrite* Server::write(const QByteArray& method, const QString& path, const QByteArray& line,
                           const QByteArray& version, const Identity& signer, QObject* parent)
{
    TransportRequest request{method, path, signedBy(signer, line), version};
    return new ServerWrite(m_transport->send(request, nullptr), parent);
}
```

`Base64Url::decode`'s exact name and return type come from `src/services/Base64Url.h`; the
executor fits the call in `onMessageReceived` to it.

**Step 4: `DeviceState::onServer`**

In `DeviceState.h`, after `noLongerShared`:

```cpp
    bool onServer = false;               ///< this device has seen the server hold the ward
```

`toJson` writes `"onServer": true` only when set, `fromJson` reads it with
`json.optionalFlag("onServer")`, and `operator==` compares it, each beside `noLongerShared`.

`docs/FORMAT.md`, the *device* table, after `noLongerShared`:

```
| `onServer` | `true` | this device has seen the Ward Planning server hold the ward; omitted until then |
```

`tests/spec/Payload.cpp` reads it with `fields.optionalTrue("onServer")` beside
`noLongerShared`, and `tests/services/EveryField.cpp` sets `device.onServer = true`.

**Step 5: `WardSession`**

`NextVersion` moves out of the class, above it, public, with its comment:

```cpp
/// The next version a device writes, and what holding it changes.
struct NextVersion
{
    QByteArray bytes;  ///< in a ward file's framing, with no device part
    VersionRank version;
    QByteArray dataKey;
    QList<QByteArray> formerKeys;
    QHash<QByteArray, qint64> landed;
    QSet<QByteArray> recipients;
    Identity writer;  ///< the header's writer, which signs the update file or the push
};
```

`TakeStatus::UnknownWriter`'s comment gains "(an update file only)". The class's public part
gains, in place of the private declarations of `nextVersion`, `writer` and
`becomeNoLongerShared`:

```cpp
    /// One above the version this device holds, written by writer(), holding the document,
    /// its access list and the landed marks, with this device's set to its journal's last
    /// sequence number and those of devices the access list no longer holds dropped. When
    /// the access list has lost anyone the version this device holds seals to, it rotates: a
    /// fresh data key, and the hash of the key this device holds appended to the former keys.
    NextVersion nextVersion() const;

    /// Who writes this device's next version and signs for it: the invite it joined through,
    /// while the version it holds does not name this device, since a member who has not taken
    /// the join lists the invite and not the device; otherwise this device.
    Identity writer() const;

    /// The server stored next, which nextVersion built from the version this device still
    /// holds: this device now holds it, as after writeUpdate, and the journal drops the
    /// entries it carries, since the server keeps one line of versions and this one is on it.
    /// The ward is on the server from now on. Saves.
    void holdPushed(const NextVersion& next);

    /// Takes version, in a ward file's framing, checked in this order: its format; that it is
    /// a version of the open ward; that it ranks above the version this device holds; that it
    /// names this device, or, for a device that joined through an invite, that invite, which it
    /// then reads with; that the data key unseals and the payload reads as a version whose
    /// access list the header matches; and that it descends from the copy this device holds,
    /// by sharing a key with it: the key either is under, or one either names among its former
    /// keys. A version that names neither ends this device's share: see becomeNoLongerShared.
    /// A version that names the invite and not this device comes from a member who has not
    /// taken the join, so the join runs on it again (JoinCommand::rejoin) before anything
    /// replays, and enters the journal, so it goes out with the next version. Taking it drops
    /// the journal entries its landed mark for this device covers, raising the counter to that
    /// mark, replays the rest onto it as replayOnto does, and makes it the version this device
    /// holds, under its data key, with its landed marks and its recipients. Then it saves, and
    /// starts a run of the automatic updates for what the version lacks.
    TakeResult takeVersion(const QByteArray& version);

    /// True once this device has seen the server hold the ward.
    bool isOnServer() const { return m_device.onServer; }

    /// The server holds the ward: remembered, and saved when it is news.
    void sawOnServer();

    /// The ward is no longer this device's: unsent changes are dropped, the undo stacks
    /// cleared, the copy kept read-only with the flag saved, so a device reopened still knows.
    void becomeNoLongerShared();
```

`takeUpdate`'s comment becomes:

```cpp
    /// Takes the version an update file carries: the file's format, that it is a version of
    /// the open ward, and that its signature verifies under the verify key the access list
    /// this device holds gives its writer; then as takeVersion takes any version.
    TakeResult takeUpdate(const QByteArray& file);
```

A new signal, and a private `hold`:

```cpp
    /// This device changed the ward and saved it: a command, an undo or a redo applied, or
    /// an automatic update changed the document.
    void localChange();
```

```cpp
    /// This device now holds next: its number and writer, its data key, its former keys, its
    /// landed marks and its recipients. Saves nothing.
    void hold(const NextVersion& next);
```

In `WardSession.cpp`:

```cpp
QByteArray WardSession::writeUpdate()
{
    NextVersion next = nextVersion();
    hold(next);
    save();
    return UpdateFile::write(m_documentId, next.bytes, next.writer);
}

void WardSession::holdPushed(const NextVersion& next)
{
    hold(next);
    m_device.onServer = true;
    m_device.journal.dropLanded(next.landed.value(m_identity->fingerprint()));
    save();
}

void WardSession::hold(const NextVersion& next)
{
    AccessList access = m_document.access();
    access.setFormerKeys(next.formerKeys);
    m_document.setAccess(access);
    m_version = next.version;
    m_dataKey = next.dataKey;
    m_landed = next.landed;
    m_device.heldRecipients = next.recipients;
}

void WardSession::sawOnServer()
{
    if (!m_device.onServer)
    {
        m_device.onServer = true;
        save();
    }
}
```

`takeUpdate` keeps its checks down to the signature and hands the rest on:

```cpp
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
    return takeVersion(update.version);
}

TakeResult WardSession::takeVersion(const QByteArray& version)
{
    WardRead read = readVersion(version);
    if (!read.header.has_value())
    {
        return notTaken(read.status == WardReadStatus::Newer ? TakeStatus::Newer : TakeStatus::Unreadable,
                        read.problem);
    }
    const WardHeader& header = *read.header;
    if (header.document() != m_documentId)
    {
        return notTaken(TakeStatus::OtherWard, QString());
    }
    if (!header.version().ranksAbove(m_version))
    {
        return notTaken(TakeStatus::NotNewer, QString());
    }
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
    // ... from "if (!descends(read, m_dataKey, m_document.access()))" to the end, as takeUpdate
    // has it today.
}
```

`WardRead::header` is set "whenever the header line parsed", so a version in a newer format
arrives with no header and the status `Newer`; the executor confirms that against
`WardFile::read` and keeps the first branch true to it.

`run` emits `localChange()` after its save when the command applied, and the constructor's
connection from `AutomaticUpdates::applied` goes to a new private slot that saves and then emits
`localChange()`:

```cpp
void WardSession::onAutomaticUpdatesApplied()
{
    save();
    emit localChange();
}
```

**Step 6: `WardSync`**

`src/services/WardSync.h`:

```cpp
#pragma once

#include "Server.h"
#include "WardSession.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <optional>

/// What stops the open ward's changes from being shared, when something does.
enum class SyncProblem
{
    None,
    Unreachable,  ///< the server did not answer
    NewerFormat,  ///< a newer Ward Planning wrote the version on the server
    Unreadable,   ///< the version on the server fails a check, so it was not taken
    PushFault     ///< the server did not store this device's version, and its head has not moved
};

/// Shares the session's open ward through the server. Its one operation is the look: it reads
/// the server's head and acts on how the head ranks against the version this device holds.
///
/// - The server has no such ward: one this device has seen there is gone, so the ward is no
///   longer this device's; any other is created there from this device's next version.
/// - The head ranks above: its version is fetched and taken, and the journal replays onto it.
/// - The head is the version this device holds: with changes to share, this device's next
///   version is pushed; otherwise there is nothing to do.
/// - The head ranks below: this device's next version is pushed over it.
///
/// A write the server stores is held. After any other answer, or none, it looks again, and the
/// head says what happened: a newer head is taken and replayed onto, and a head that has not
/// moved is a fault, left for the next retry. One look runs at a time, with one call to the
/// server on its way; asking for a look while one runs makes it look once more when it ends.
/// A save that is pending is tried first, and nothing is pushed while it fails.
///
/// It holds the ward's live channel while it runs. Each message says who is here and the
/// server's version number, and a number above this device's asks for a look. A channel that
/// closes asks for one too, which finds a removal when that is why it closed.
class WardSync : public QObject
{
    Q_OBJECT

public:
    /// Shares session's ward through server. It owns neither.
    WardSync(WardSession* session, Server* server, QObject* parent);

    /// Starts sharing the session's open ward: opens its channel and looks. A ward that is no
    /// longer this device's is not started.
    void start();

    /// Stops: the channel closes, and the call on its way is dropped with its answer. Before
    /// the session opens another ward, and when the server is turned off.
    void stop();

    /// Opens the channel when it is closed, and looks, which tries again whatever is pending.
    void retry();

    /// Looks, as the class comment describes. Does nothing while stopped.
    void look();

    bool isRunning() const { return m_running; }

    /// True while the channel is open: the server has answered on it and it has not closed.
    bool isConnected() const { return m_connected; }

    SyncProblem problem() const { return m_problem; }

    /// A ward is syncing while it is connected and nothing stops its pushes: no problem, no
    /// save pending, and the ward still this device's.
    bool isSyncing() const;

    /// The fingerprint of every device with the ward open, this device among them, as the
    /// channel last said; empty while it is closed.
    const QSet<QByteArray>& present() const { return m_present; }

signals:
    /// isConnected, problem or isSyncing changed.
    void stateChanged();

    void presenceChanged();

    /// A version fetched from the server was put to the session, which answered result.
    void versionFetched(const TakeResult& result);

private slots:
    void onLocalChange();
    void onHead(const ServerRead& head);
    void onFetched(const ServerRead& version);
    void onWritten(bool stored);
    void onTold(const QSet<QByteArray>& present, qint64 version);
    void onChannelClosed();
    void onRetryTimer();

private:
    /// A write on its way: the version sent, the version this device held when it was built,
    /// and the head it went over, which is no version when the server had none.
    struct Writing
    {
        NextVersion next;
        VersionRank base;
        VersionRank over;
    };

    void openChannel();

    /// Sends this device's next version over the head over: a create when the server has no
    /// such ward, otherwise a push. A write the server did not store over this same head is
    /// not sent again; that is the fault.
    void write(bool create, const VersionRank& over);

    /// Ends the look with problem, and looks once more when one was asked for meanwhile.
    void finish(SyncProblem problem);

    /// Ends the look and starts another at once.
    void lookAgain();

    void setConnected(bool connected);

    WardSession* m_session;
    Server* m_server;
    ServerChannel* m_channel = nullptr;
    QObject* m_call = nullptr;    ///< the look's call on its way, which deletes itself once answered
    QTimer m_retryTimer;
    bool m_running = false;
    bool m_connected = false;
    bool m_looking = false;
    bool m_lookAsked = false;     ///< a look was asked for while one ran
    bool m_changeUnsent = false;  ///< this device changed the ward since its last write was built
    SyncProblem m_problem = SyncProblem::None;
    QSet<QByteArray> m_present;
    std::optional<Writing> m_writing;

    /// The head a write went over that the server did not store.
    std::optional<VersionRank> m_unstoredOver;

    static constexpr int RetryMilliseconds = 30000;
};
```

`src/services/WardSync.cpp`:

```cpp
#include "WardSync.h"

WardSync::WardSync(WardSession* session, Server* server, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_server(server)
{
    m_retryTimer.setInterval(RetryMilliseconds);
    connect(&m_retryTimer, &QTimer::timeout, this, &WardSync::onRetryTimer);
    connect(m_session, &WardSession::localChange, this, &WardSync::onLocalChange);
}

void WardSync::start()
{
    if (m_running || m_session->isNoLongerShared())
    {
        return;
    }
    m_running = true;
    m_retryTimer.start();
    openChannel();
    look();
}

void WardSync::stop()
{
    m_running = false;
    m_retryTimer.stop();
    delete m_call;
    m_call = nullptr;
    if (m_channel != nullptr)
    {
        // A message on the channel can be what leads here, so the channel is closed now and
        // deleted once its signal has returned.
        m_channel->close();
        m_channel->disconnect(this);
        m_channel->deleteLater();
        m_channel = nullptr;
    }
    m_writing.reset();
    m_unstoredOver.reset();
    m_looking = false;
    m_lookAsked = false;
    m_changeUnsent = false;
    m_problem = SyncProblem::None;
    m_present.clear();
    emit presenceChanged();
    setConnected(false);
    emit stateChanged();
}

void WardSync::retry()
{
    if (!m_running)
    {
        return;
    }
    if (m_channel == nullptr)
    {
        openChannel();
    }
    look();
}

bool WardSync::isSyncing() const
{
    return m_connected
        && m_problem == SyncProblem::None
        && !m_session->savePending()
        && !m_session->isNoLongerShared();
}

void WardSync::look()
{
    if (!m_running)
    {
        return;
    }
    if (m_session->isNoLongerShared())
    {
        stop();
        return;
    }
    if (m_looking)
    {
        m_lookAsked = true;
        return;
    }
    if (!m_session->retryPendingSave())
    {
        emit stateChanged();
        return;
    }
    m_looking = true;
    ServerReading* head = m_server->head(m_session->documentId(), this);
    connect(head, &ServerReading::answered, this, &WardSync::onHead);
    m_call = head;
}

void WardSync::onHead(const ServerRead& head)
{
    m_call = nullptr;
    switch (head.status)
    {
    case ServerReadStatus::NoAnswer:
        finish(SyncProblem::Unreachable);
        return;
    case ServerReadStatus::Missing:
        if (m_session->isOnServer())
        {
            m_session->becomeNoLongerShared();
            finish(SyncProblem::None);
            return;
        }
        write(true, VersionRank());
        return;
    case ServerReadStatus::Found:
        break;
    }

    m_session->sawOnServer();
    HeaderRead read = WardHeader::parse(head.bytes);
    switch (read.status)
    {
    case HeaderStatus::Read:
        break;
    case HeaderStatus::Newer:
        finish(SyncProblem::NewerFormat);
        return;
    case HeaderStatus::Unreadable:
        qWarning("The server's head could not be read: %s", qPrintable(read.problem));
        finish(SyncProblem::Unreadable);
        return;
    }

    const VersionRank& theirs = read.header->version();
    if (theirs.ranksAbove(m_session->version()))
    {
        ServerReading* fetch = m_server->fetch(m_session->documentId(), this);
        connect(fetch, &ServerReading::answered, this, &WardSync::onFetched);
        m_call = fetch;
        return;
    }
    if (theirs == m_session->version() && !m_changeUnsent && !m_session->hasChangesToShare())
    {
        finish(SyncProblem::None);
        return;
    }
    write(false, theirs);
}

void WardSync::onFetched(const ServerRead& version)
{
    m_call = nullptr;
    switch (version.status)
    {
    case ServerReadStatus::NoAnswer:
        finish(SyncProblem::Unreachable);
        return;
    case ServerReadStatus::Missing:
        lookAgain();
        return;
    case ServerReadStatus::Found:
        break;
    }

    TakeResult result = m_session->takeVersion(version.bytes);
    emit versionFetched(result);
    switch (result.status)
    {
    case TakeStatus::Taken:
        m_unstoredOver.reset();
        lookAgain();
        return;
    case TakeStatus::NotNewer:
    case TakeStatus::NotNamed:
    case TakeStatus::InviteUsedOrWithdrawn:
        finish(SyncProblem::None);
        return;
    case TakeStatus::Newer:
        finish(SyncProblem::NewerFormat);
        return;
    case TakeStatus::OtherWard:
    case TakeStatus::UnknownWriter:
    case TakeStatus::Unreadable:
        qWarning("The server's version was not taken: %s", qPrintable(result.problem));
        finish(SyncProblem::Unreadable);
        return;
    }
}

void WardSync::write(bool create, const VersionRank& over)
{
    if (m_unstoredOver.has_value() && *m_unstoredOver == over)
    {
        m_unstoredOver.reset();
        finish(SyncProblem::PushFault);
        return;
    }
    m_changeUnsent = false;
    m_writing = Writing{m_session->nextVersion(), m_session->version(), over};
    const NextVersion& next = m_writing->next;
    ServerWrite* write = create
        ? m_server->create(m_session->documentId(), next.bytes, next.writer, this)
        : m_server->push(m_session->documentId(), next.bytes, next.writer, this);
    connect(write, &ServerWrite::answered, this, &WardSync::onWritten);
    m_call = write;
}

void WardSync::onWritten(bool stored)
{
    m_call = nullptr;
    Writing writing = *m_writing;
    m_writing.reset();
    if (!stored)
    {
        m_changeUnsent = true;
        m_unstoredOver = writing.over;
        lookAgain();
        return;
    }
    m_unstoredOver.reset();
    if (!(m_session->version() == writing.base))
    {
        // An update file was taken while the write was on its way, so this device holds
        // another version now. The head is the stored write, and a look ranks the two.
        lookAgain();
        return;
    }
    m_session->holdPushed(writing.next);
    if (m_changeUnsent || m_session->hasChangesToShare())
    {
        lookAgain();
        return;
    }
    finish(SyncProblem::None);
}

void WardSync::finish(SyncProblem problem)
{
    m_looking = false;
    m_problem = problem;
    emit stateChanged();
    if (m_lookAsked)
    {
        m_lookAsked = false;
        look();
    }
}

void WardSync::lookAgain()
{
    m_looking = false;
    m_lookAsked = false;
    look();
}

void WardSync::openChannel()
{
    m_channel = m_server->openChannel(m_session->documentId(), m_session->writer(), this);
    connect(m_channel, &ServerChannel::told, this, &WardSync::onTold);
    connect(m_channel, &ServerChannel::closed, this, &WardSync::onChannelClosed);
}

void WardSync::onLocalChange()
{
    m_changeUnsent = true;
    look();
}

void WardSync::onTold(const QSet<QByteArray>& present, qint64 version)
{
    if (present != m_present)
    {
        m_present = present;
        emit presenceChanged();
    }
    setConnected(true);
    if (version > m_session->version().number)
    {
        look();
    }
}

void WardSync::onChannelClosed()
{
    m_channel->deleteLater();
    m_channel = nullptr;
    m_present.clear();
    emit presenceChanged();
    setConnected(false);
    look();
}

void WardSync::onRetryTimer()
{
    if (!m_connected || m_problem != SyncProblem::None)
    {
        retry();
    }
}

void WardSync::setConnected(bool connected)
{
    if (m_connected != connected)
    {
        m_connected = connected;
        emit stateChanged();
    }
}
```

Two things the executor checks while writing it, each against the text above it:

- `onLocalChange` is connected for the life of the object, so it runs while stopped; `look`
  returns at once then, and `m_changeUnsent` is cleared by the next `stop` or write.
- `look` calls `stop` when the ward is no longer this device's, which is how a removal found by
  a look, or by an update file, ends sharing.

**Step 7: `CMakeLists.txt`**

Add `WebSockets` to `find_package(Qt6 REQUIRED COMPONENTS ...)` and `Qt6::WebSockets` to the
library's link list, and add `src/services/SignedLine.cpp`, `Server.cpp`, `WardSync.cpp` and
their headers, with `Transport.h`, to the library's sources.

**Step 8: STOP for the user's review**

Hand the user the diff. Nothing is committed, nothing is built, and no agent review has run.
After the user's review, the agent review, then:

```bash
git add src/services/SignedLine.h src/services/SignedLine.cpp src/services/Transport.h src/services/Server.h src/services/Server.cpp src/services/WardSync.h src/services/WardSync.cpp src/services/WardSession.h src/services/WardSession.cpp src/services/DeviceState.h src/services/DeviceState.cpp src/services/UpdateFile.h src/services/UpdateFile.cpp CMakeLists.txt docs/FORMAT.md tests/spec/Payload.cpp tests/services/EveryField.cpp
git commit -m "WardSync shares the open ward through the server: it looks at the head, then takes it, pushes over it or creates the ward, and holds the live channel"
```

---

## Task 2: The fake server, the test client, and the sync tests

*Testing* (the server interface, a test client, *Against the fake server*, *The server switch*).

**Files:**
- Create: `tests/server/FakeServer.h`, `tests/server/FakeServer.cpp`
- Create: `tests/server/ServerSyncTest.h`, `tests/server/ServerSyncTest.cpp`
- Create: `tests/services/SignedLineTest.h`, `tests/services/SignedLineTest.cpp`
- Modify: `tests/services/TestClient.h`, `tests/services/TestClient.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `server/test/wire.test.ts` (the block's title)

**Step 1: `FakeServer`**

A `Transport` that is the server, in memory, keeping the rules `server/src/ward.ts` keeps: it
reads each request as the Worker and the `Ward` object do, with `WardHeader::parse`,
`Identity::fingerprintOf`, `SignedLine` and `Crypto::verify`, and answers the statuses
`server/README.md` lists. Per ward it holds the current version's bytes and the open channels,
each under its signer's fingerprint.

```cpp
/// The Ward Planning server, in process: a Transport that keeps each ward's current version by
/// the server's own rules and holds its channels, and that a test can make misbehave.
class FakeServer : public Transport
{
public:
    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;

    /// While unreachable, every request gets no answer and stores nothing, and every channel
    /// closes and none opens.
    void setReachable(bool reachable);

    /// The next write is stored, and told to the channels, and gets no answer.
    void dropNextWriteReply();

    /// Hands over the oldest reply that is waiting; false when none is. Every reply waits
    /// until a test hands it over, here or through settle.
    bool deliverNext();

    /// Hands over every waiting reply and channel event, and those they lead to, until none
    /// is left.
    void settle();

    /// The ward's current version becomes bytes, whatever the rules say, as a restored or
    /// hostile server would serve it. Tells no channel.
    void serve(const DocumentId& document, const QByteArray& bytes);

    /// Forgets the ward and closes its channels.
    void forget(const DocumentId& document);

    /// The bytes of the ward's current version; empty when it holds none.
    QByteArray version(const DocumentId& document) const;

    /// How many requests and channel opens it has received.
    int calls() const;

    /// The fingerprint of every device with a channel open to the ward.
    QSet<QByteArray> present(const DocumentId& document) const;
};
```

Nothing is answered inside `send` or `open`: each reply and each channel event (a message, a
close) joins one queue in the order the server produced it, and `deliverNext` or `settle` hands
them over. So a test decides what has arrived when, with no timer and no event loop, and a test
that does not care calls `settle` after each step. An `open` by a signer the current header does
not name, or while unreachable, queues a close. A push that drops a device closes its channels,
and a second open by one device closes its first.

**Step 2: `TestClient`**

The client gains a server and a sync, so a multi-user test is two or three clients on one
`FakeServer`:

```cpp
    /// A client of server, which it shares its wards through once startSync is called.
    TestClient(const QString& deviceName, FakeServer* server);

    WardSync& sync() { return *m_sync; }

    /// Starts sharing the open ward, and settles the server.
    void startSync();
```

`reopen()` rebuilds the sync with the session. The existing constructor stays for the tests that
use no server.

**Step 3: `SignedLineTest`**

The known answers `server/test/wire.test.ts` pins, for the same inputs: the document id is the 16
bytes `0x00` to `0x0f`, and the version is the header line `{"format":1}`, a newline, then the
payload.

- `createLine`: payload `abc` gives
  `wardplanning-v1 create AAECAwQFBgcICQoLDA0ODw IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E vd2BPGNCOXIxce8_7phXm5SWTjuxyz5CcmLIwGjVIxk`
- `pushLine`: an empty payload gives
  `wardplanning-v1 push AAECAwQFBgcICQoLDA0ODw IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E DldRwCblQ7Loqy6wYJnaodHl30d3j3eH-qtFzfEv46g`
- `updateLine`: payload `abc` gives the create line with `update` in place of `create`
- `channelLine`: `wardplanning-v1 channel AAECAwQFBgcICQoLDA0ODw - -`

In `server/test/wire.test.ts` the block's title becomes `known answers shared with the app`, and
the fingerprint test's comment names `tests/services/SignedLineTest.cpp` beside
`IdentityTest.cpp`. Run `npm test` in `server/`: 40 pass.

**Step 4: `ServerSyncTest`**

Each test builds its clients on one `FakeServer` and drives them step by step. `alice` creates a
ward of `TestWard`'s content; where a test needs two members, `TestSharing::share` gives both the
ward and both start. Names and what each pins:

*The first look*
- `aNewWardIsCreatedOnTheServer`: after `startSync`, the server's version equals the bytes
  `alice`'s `nextVersion` gave, numbered 2; `isOnServer()` is true; her journal is empty; one
  `POST` and no `PUT` were sent.
- `aCreateWhoseAnswerIsLostIsSettledByTheNextLook`: with `dropNextWriteReply`, the look ends in
  no problem with the ward on the server, the journal emptied by the mark, and exactly one `POST`
  in `calls`.
- `aWardTheServerNoLongerHasIsNoLongerShared`: after `forget`, `retry` makes the ward no longer
  this device's, read-only, its journal dropped, and no `POST` follows.

*Pushing*
- `aCommandIsPushedAsTheNextVersion`: `commitBobsPhone` on `alice` leaves the server one number
  higher, holding that phone when `bob` takes it, and her journal empty.
- `aCommandMadeWhileAPushIsOnItsWayGoesOutWithTheNext`: two commits with nothing delivered between them; after
  `deliverNext` for each reply in turn, the server holds both changes in two versions and the
  journal is empty; the second entry was still in the journal after the first reply.
- `aPushWhoseAnswerIsLostIsNotAppliedTwice`: an attempt command pushed with its reply dropped;
  after the next look the journal is empty by the mark, and the document holds exactly one such
  attempt; repeated with `reopen()` before that look.
- `twoPushesOfOneNumberTheSecondReplaysAndPushesTheNext`: both offline (`setReachable(false)`),
  each edits a different person's phone; back online, `alice` retries then `bob` retries: the
  server's number is two above the start, both phones are in both documents after `alice`
  retries again, and neither replay was abandoned.
- `aConflictAbandonsTheJournalAndTheNextCommandPushes`: both offline edit Bob's phone; `alice`
  lands first; `bob`'s `versionFetched` carries `Abandoned` with the to-redo text naming his
  value; the server keeps `alice`'s; `bob`'s next command pushes.
- `anAutomaticUpdateIsPushedOnce`: a geocode batch for two families pushes one version, and a
  second client that takes it starts no batch and pushes nothing (`calls` unchanged after its
  look but for the head and the fetch).
- `aSaveThatFailsPushesNothing`: with the store failing writes, a commit leaves the server where
  it was and `isSyncing()` false; once writes work, `retry` saves and pushes it.

*Taking*
- `aNoticeMakesTheOtherClientFetch`: `alice` commits; `bob`, connected, holds her change with no
  call from the test but the server's `settle`.
- `aServerAheadWithAnEmptyJournalFastForwards`: `bob` offline while `alice` pushes twice; his
  `retry` takes the head as `FastForwarded`.
- `aHeadThatRanksBelowIsPushedOver`: `serve` an older version of the ward; `alice`'s `retry`
  pushes her next version though her journal is empty, and the server keeps it.
- `aHeadFromBeforeThisDeviceJoinedIsRetriedUntilAMemberPushes`: `serve` a version that lacks
  `bob`; `bob`'s look ends in `PushFault`, the ward still his; after `alice`'s look pushes over
  it, `bob`'s retry ends in no problem.
- `aVersionThatFailsACheckReplacesNothing`: `serve` another ward's version under this id, and
  the same with its header's id rewritten: each look ends in `Unreadable`, `alice`'s document and
  version unchanged, and her next command is not pushed while it lasts; once `serve` puts back a
  version she can read, `retry` clears it and pushes.
- `aNewerFormatOnTheServerStopsPushesAndKeepsEdits`: `serve` a header line with `format` 2: the
  problem is `NewerFormat`, a commit journals and saves and sends no `PUT`; serving a readable
  version again clears it and the journal replays and pushes.
- `aRemovedDeviceIsNoLongerShared`: `alice` removes `bob`'s user and pushes; `bob`'s channel
  closes, his look makes the ward no longer his, his sync stops, and a later `retry` sends
  nothing.

*Unreachable*
- `editsMadeWhileUnreachableGoUpOnReconnect`: unreachable, three commits: the problem is
  `Unreachable`, `isConnected()` false, the journal holds three; reachable and `retry`: one
  version carries all three and the channel is open again.

*The channel*
- `theChannelSaysWhoIsHere`: `alice` alone sees herself; when `bob` starts, both see both; when
  `bob` stops, `alice` sees herself again.
- `aSecondChannelFromOneDeviceKeepsWhoIsHere`: `alice`'s `retry` after her channel is closed
  under her leaves `bob`'s `present()` holding both, with `presenceChanged` not having dropped
  her in between once settled.

*The switch*
- `withTheServerOffNothingIsSent`: a client whose sync is never started creates, edits, writes
  an update and joins through an invitation, and `calls()` stays 0.
- `anUpdateFileAboveTheHeadIsPushedOver`: `bob`, syncing, takes `alice`'s update file written
  with her sync stopped: his next look pushes over the head, and the server then holds her
  change.
- `anUpdateFileBelowTheHeadIsReplayedOntoTheHead`: the file is taken, and the next look takes the
  head and replays onto it.
- `membersWhoTradedFilesTurnTheServerOn`: `alice` and `bob` share by files only, the ward never
  on the server; `bob` starts first and creates it there; `alice` starts and takes it or pushes
  over it; both end holding equal documents.

Register `SignedLineTest` and `ServerSyncTest` in `tests/main.cpp` and add the sources to
`tests/CMakeLists.txt`.

**Step 5: Commit**

```bash
git add tests/server tests/services/SignedLineTest.h tests/services/SignedLineTest.cpp tests/services/TestClient.h tests/services/TestClient.cpp tests/CMakeLists.txt tests/main.cpp server/test/wire.test.ts
git commit -m "A fake server keeps the real one's rules in process, and the sync tests run clients against it"
```

---

## Task 3: `NetworkTransport`: HTTP and the WebSocket

*The live channel* (the ping, a closed channel), *Hosting notes*.

**Files:**
- Create: `src/services/NetworkTransport.h`, `src/services/NetworkTransport.cpp`
- Modify: `CMakeLists.txt`

`NetworkTransport` is a `QObject` and a `Transport` over one `QNetworkAccessManager`:

```cpp
/// The app's transport: HTTPS and WebSocket to the Ward Planning server.
class NetworkTransport : public QObject, public Transport
{
    Q_OBJECT

public:
    /// The server the app is built to use.
    static constexpr char BuiltInAddress[] = "https://ward-planning-dev.ward-planning-server.workers.dev";

    /// Sends to the server at address, such as BuiltInAddress.
    NetworkTransport(const QString& address, QObject* parent);

    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;
};
```

- **`send`** returns a `NetworkCall`, a `TransportCall` that owns its `QNetworkReply`. It builds
  a `QNetworkRequest` for `address + path` with the request's headers and a transfer timeout of
  30 seconds (`setTransferTimeout`), sends it with `sendCustomRequest`, and on `finished` emits
  `answered` with the HTTP status and the body, or `NoAnswer` when the reply has no status, then
  deletes itself. Destroying the call aborts the request.
- **`open`** returns a `NetworkChannel`, a `TransportChannel` over a `QWebSocket` it owns. It
  opens `wss://` (or `ws://` for an `http://` address) with a `QNetworkRequest` carrying the
  request's headers. `textMessageReceived` becomes `messageReceived`; `disconnected` and
  `errorOccurred` become one `closed`, emitted once. Every 30 seconds it sends a ping, and a pong
  that has not arrived when the next ping is due closes the socket, as *The live channel* asks.

Qt 6.12's `QWebSocket` is newer than the executor's training data: read its documentation for
`open(const QNetworkRequest&)`, `ping`, `pong` and `errorOccurred` before writing against them.

No unit test: Task 5's conformance suite runs it against the live server.

**Commit:**

```bash
git add src/services/NetworkTransport.h src/services/NetworkTransport.cpp CMakeLists.txt
git commit -m "NetworkTransport carries the server's calls over HTTPS and its live channel over a WebSocket"
```

---

## Task 4: The window: the switch, the notices, who is here

*If the server goes away* (the switch), *Who is here*, *Failure paths*, *Saving* (what is pending
is retried).

**Files:**
- Modify: `src/main.cpp`, `src/services/DocumentManager.h`, `src/services/DocumentManager.cpp`
- Create: `src/services/Presence.h`, `src/services/Presence.cpp`
- Modify: `src/widgets/MainWindow.h`, `src/widgets/MainWindow.cpp`
- Modify: `src/widgets/ShareNotice.h`, `src/widgets/ShareNotice.cpp`
- Modify: `src/widgets/WardSharing.h`, `src/widgets/WardSharing.cpp`,
  `src/widgets/WhoHasAccessDialog.cpp`
- Create: `tests/services/PresenceTest.h`, `tests/services/PresenceTest.cpp`
- Modify: `tests/services/WhoHasAccessDialogTest.cpp`, `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `docs/ARCHITECTURE.md`, `docs/USER_GUIDE.md`, `docs/SHARED_WARD_DESIGN.md`

**`DocumentManager`** takes a `Transport*` after the store, and owns a `Server` on it and a
`WardSync` on its session and that server.

- `WardSync& sync()` for the window.
- `void setUseServer(bool use)`: starts the sync when a ward is open and `use` is set, stops it
  otherwise. `createWard`, `openWard` and `join` stop the sync before they change the session and
  start it after, when the server is in use and the ward opened.
- `bool retryPending()` replaces `retryPendingSave` for the window's callers: it retries the save
  and, when nothing is left unsaved, calls `sync().retry()`; it answers what `retryPendingSave`
  answered.
- It forwards `WardSync::versionFetched` as its own signal of the same name.

`main` builds a `NetworkTransport` on `NetworkTransport::BuiltInAddress` and hands it to the
manager, then calls `setUseServer(settings.useServer())` once the window has its settings.

**`Presence`** turns the channel's fingerprints into the line above the ward:

```cpp
/// Who else has the ward open, as the window says it.
class Presence
{
    Q_DECLARE_TR_FUNCTIONS(Presence)

public:
    /// One name for each other device in present, as access names it: its user's name; for an
    /// open invite, "someone with Bob's invitation" by its label, or "a device invitation";
    /// and for a fingerprint access does not know, "a device that just joined". A user with
    /// two devices here is named once. own is this device, which is left out.
    static QStringList others(const AccessList& access, const QSet<QByteArray>& present, const QByteArray& own);

    /// "Alice Smith is also here", "Alice Smith and Bob Jones are also here", or with three
    /// or more, "Alice Smith, Bob Jones and Carol Lee are also here"; empty for no one.
    static QString line(const QStringList& others);
};
```

`PresenceTest`: `othersLeavesOutThisDeviceAndNamesAUserOnce` (own device, a user with two
devices here, a user invite labelled Bob, a device invite, an unknown fingerprint: exactly four
names, in sorted order), and `lineJoinsNames` (the three wordings and the empty one).

**`ShareNotice`** gains `void showMessage(const QString& message)`, a notice with no button, and
`MainWindow` a presence label above the map, hidden while it is empty.

**`MainWindow`**
- `updateSharing` also runs on `WardSync::stateChanged`, and with the server on shows the first
  that applies, after *no longer shared*:

  | State | Notice |
  |---|---|
  | `SyncProblem::NewerFormat` | *This ward was saved by a newer version of Ward Planning. Update to see the latest changes and share your work.* |
  | `SyncProblem::Unreadable` | *The ward on the server could not be read. Your copy is safe and you can keep working; sharing resumes when the server has a readable copy.* |
  | `SyncProblem::PushFault` | *Your changes could not be shared right now. They are saved here.* |
  | not connected, or `SyncProblem::Unreachable` | *Working offline. Your changes are saved here and will go up when the connection returns.* |
  | otherwise | no notice |

- `WardSync::presenceChanged` sets the presence label from `Presence`.
- `WardSync::versionFetched` runs what `openUpdateFile` runs after a take, without its message
  boxes for a version that was taken: `followUserDetails()` when `Taken`, and
  `showChangesToRedo` when the replay was `Abandoned`. The two share one private function,
  `afterTake(const TakeResult&)`.
- `onSettings` calls `DocumentManager::setUseServer` with the new value.
- `onApplicationStateChanged` and `unsavedChangesMayBeLost` call `retryPending()`.
- `executeCommand` needs no change: the session's `localChange` asks for the look.

**`WardSharing`** and **`WhoHasAccessDialog`**: with the server on, *Add a User* and *Add a
Device* run as they do with it off while the ward is syncing; otherwise they show the notice of
what stops it, from one function that `updateSharing`'s table and these share,
`WardSharing::whatStopsSharing(const WardSync&)`, which answers *Connect to the internet to
invite someone.* only when the cause is no connection. Leave stays as it is. Both take the
`WardSync` beside the settings; `WhoHasAccessDialogTest` builds one on a `FakeServer`.

**Docs:** `docs/ARCHITECTURE.md` gains `Transport`, `Server`, `WardSync`, `NetworkTransport`,
`Presence` and the manager's part in them. `docs/USER_GUIDE.md` gains what the switch does when
on, the four notices and the presence line. In `docs/SHARED_WARD_DESIGN.md`, *Testing*'s
server-interface bullet says what was built (`Server` is one class over a `Transport`; the
app's is `NetworkTransport`, the tests' `FakeServer`), and its test-client bullet says sync
lives in `WardSync` beside `WardSession`.

**Commit:**

```bash
git commit -m "With the server on, the open ward syncs: the window says who else is here and what stops sharing"
```

(after staging, by name, each file this task changed)

---

## Task 5: The conformance suite, against the fake server and the real one

*Testing* (the fake and the real server are checked against each other).

**Files:**
- Create: `tests/server/ConformanceTest.h`, `tests/server/ConformanceTest.cpp`
- Modify: `tests/CMakeLists.txt`, `tests/main.cpp`
- Modify: `server/README.md`, `docs/BUILDING.md`

One suite written against `Server`, run twice by `tests/main.cpp`: on a `FakeServer`, always,
and on a `NetworkTransport` at `WARD_PLANNING_SERVER_URL` when that is set. Each case waits for its
answers through one helper, which settles the fake server and, for the real one, runs the
event loop until the answer arrives or ten seconds pass. The versions are real ones, written by `WardFile::write`
for identities the test makes.

The suite makes three wards in all, so a run stays inside the server's limit of 10 creates a
minute from one address:

- `createAndRead`: a create is stored; a second create of the id is not; `head` is the header
  line and `fetch` the exact bytes; a `head` and a `fetch` of an unknown id are `Missing`.
- `whoMayWrite`: a push by a device the header does not name is not stored; one signed by a
  named device that is not its `writer` is not stored; one whose header names another ward is
  not stored; after each, `fetch` gives the bytes from before; the named writer's push is stored.
- `onlyAHigherNumberIsKept`: a push of the current number by another named device is not stored
  and the first stays; a lower number is not stored; a number two above is stored.
- `removalAndDeletion`: a push that drops a device is stored, and that device's next push is
  not; a final version with no recipients is stored, after which a push and a create of the id
  are not.
- `theChannel`: an open by a named device is told who is here and the number; a second device's
  open is told to both; a stored push is told to both with its number; a push that drops a
  device closes its channel; an open by a device the header does not name closes without a
  message.

Run it against the live server:

```
set WARD_PLANNING_SERVER_URL=https://ward-planning-dev.ward-planning-server.workers.dev
build\bin\WardPlanningTests.exe
```

A disagreement is fixed in whichever of `FakeServer` and `server/` is wrong.

`server/README.md` and `docs/BUILDING.md` each gain the two lines above, and what the suite
leaves on the server: three wards, one of them deleted.

**Commit:**

```bash
git add tests/server/ConformanceTest.h tests/server/ConformanceTest.cpp tests/CMakeLists.txt tests/main.cpp server/README.md docs/BUILDING.md
git commit -m "One conformance suite runs against the fake server and, when its address is set, the real one"
```

---

## Open questions for the user

Each states the choice this plan makes.

1. **Add a user and Add a device with the server on** save the invitation at once while the ward
   is syncing, as with the server off. The second plan holds the invitation back until its
   version is on the server.
2. **A look reads the head before every push**, so each edit is two small requests. It keeps one
   path for a first push, a retry and a lost answer.
3. **A fingerprint on the channel that the access list does not know** shows as *a device that
   just joined*, the design's "a device that joined since this copy was last current".
4. **The channel is signed as the invite** while the version this device holds does not name the
   device, as its pushes are.
5. **The conformance suite's wards stay on the development server**: three per run, since the
   server has no call that removes a ward.
