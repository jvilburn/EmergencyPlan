# Encrypted Shared Document Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use executing-with-review to implement this plan task-by-task.

**Goal:** Encrypt the ward document, hold the authoritative copy on a Ward Planning server the app talks to directly, and keep a complete local file that works with no network at all.

**Architecture:** Envelope encryption. Each document holds one random data key, sealed separately to each device's X25519 public key via libsodium `crypto_box_seal`. The payload - today's document JSON, compressed - is encrypted with XChaCha20-Poly1305 under that data key, as one payload, with the plaintext header bound as associated data. The roster lists people, each with linked devices; every device holds two keypairs, X25519 for sealing and Ed25519 for signing, generated silently on first run and held in the OS keychain through qtkeychain. A signed header chain proves membership changes; each person is an admin or a member, and any admin grants and removes alone. Saving writes the local file; publishing uploads it conditionally on the version this session started from. All local writes funnel through one `DocumentSession` path, and every server call answers through a callback.

**Tech Stack:** Qt 6.11 (Widgets, Network, Test), C++17, MSVC 2022, Ninja. libsodium from vcpkg; qtkeychain built from source via `FetchContent` against the system Qt.

**Design document:** [2026-08-30-encrypted-shared-document-design.md](2026-08-30-encrypted-shared-document-design.md) - read it before starting. This plan implements it and does not restate its rationale.

---

## Before you start

**The server is separate work.** Its design, implementation, hosting, and operation
have their own document. This plan builds the client against a **fake server** - an
in-process test double implementing the contract in the design's *The server* section -
written in Task 1.8. Every sync task is testable before a real server exists, and
end-to-end verification waits on that companion work.

**You do not build or run anything.** The MSVC environment is only set up inside
`build.bat`. Ask the user to build with **Ctrl+Shift+B** in VS Code and to run the
test executable, then paste the output. Never invoke cmake, ninja, or the compiler
from a Claude shell - Git Bash and MSYS lack the MSVC environment and produce
misleading errors like `Cannot open include file: 'type_traits'`.

**`build.bat` only reconfigures when the top-level `CMakeLists.txt` changes.** It byte-compares it against `build/windows-debug/.cmake_marker` (the `NEED_CONFIGURE`
block in `build.bat`). A task
touching only `tests/CMakeLists.txt` will appear to build successfully and produce no new
test binary. If a build produces no new test binary, have the user delete
`build/windows-debug/.cmake_marker` and build again.

**Each preset builds into its own directory,** `build/<presetName>`, so the debug
executable and test binary are under `build/windows-debug/bin/` and the release build
under `build/windows-release/bin/`. The vcpkg dependency tree is shared at
`build/vcpkg_installed/`. MuPDF and its dependencies arrive through
`find_package(unofficial-libmupdf CONFIG REQUIRED)`, which carries per-configuration
library locations, so a Debug build links the Debug libraries.

**Line numbers cite the tree as of commit `6266c5e`.** Code moves as tasks land, so find
each site by the symbol named beside it and treat the number as a hint.

**Standing rule: every new `.cpp`/`.h` must be added to the source lists in
`CMakeLists.txt`.** `WardPlanningLib` is built from hand-maintained
`set(MODEL_SOURCES ...)` / `SERVICE_SOURCES` / `WIDGET_SOURCES` lists above line 350 - 
there is no globbing. This plan creates roughly thirty files and does not repeat the
instruction in each task.

**Standing rule: core first.** After Task 0's rename, Task 1 is the whole behavioural core
of the format, membership, and server rules, and Task 2 is a stop for the user's review of
it. Task 6 is the second core - the runtime orchestration that saves, publishes, and
reconciles - and Task 7 is the stop for its review; every task after it is a consequence.
The tree may not build between tasks while a core and its consequences land; that is
expected. Within a related set of files, migrate the
whole unit to the new design in one step so it never reads as two designs at once, and
delete code in the same task that removes its last caller.

**Coding style** is in [CODING_STYLE.md](../CODING_STYLE.md): Allman braces, only
one-line functions in headers, always brace control structures, leading operators on
continuation lines, `std::optional<>` for nullable fields, the local in
`toJson`/`fromJson` is named `json`, and every user-visible string goes through
`tr()`.

**Commit after every task**, except that each core is committed at the end of its review
stop - Task 1 at Task 2, Task 6 at Task 7 - once the user's review is applied. Plain messages, no attribution lines.

**Terminology:**

| Term | Meaning |
|---|---|
| Data key | 32-byte random key encrypting one document's payload |
| Sealed key | The data key encrypted to one device's public key |
| Person | Someone with access: member id, display name, role, and one or more devices |
| Role | **Admin** - grants, removes, unlinks another's device, and changes roles, alone - or **member**. Admins are listed in the header; every version lists at least one |
| Device | One installation's keypairs and device name; its private keys never leave it |
| Roster | People, each with their devices, stored **inside** the payload |
| Link | Adding a device to your own person, authorised from a device you already use |
| Recipient entry | `{member id, fingerprint, public key, verify key, sealed key}` in the **plaintext** header; the fingerprint covers both keys |
| Header chain | The kept headers - those whose membership changed - each signed by a device of the one before; lets a removed member prove removal without decrypting |
| Leave | A person publishing the version that drops them; it does not rotate the key |
| Key generation | Counter incremented on every data key rotation |
| Save counter | Counter incremented on every local write |
| Document id | 128 random bits generated by the app at creation; names the local file, keys local records, and is registered unchanged when shared |
| Base copy | The local file as it stood at the base version, kept for three-way merge |
| Server address | Base URL of the Ward Planning server, supplied by the app from `AppSettings`; documents carry only their id |
| Base version | The server version this local file was last reconciled with |
| Publish | Upload the local file, conditional on the base version |
| Lease | Server-held edit token; while it is held elsewhere, this session is read-only |
| Allow-list | The devices the server permits to read and write one document - exactly the current accepted header's recipients, never updated separately |

---

## Phase 0 - Rename

### Task 0: Rename the product to Ward Planning

First, so every later task - the core, the test harness, the libsodium build - is
written against final names, and because `setOrganizationName` keys where the storage
and identity tasks store their records.

> **Done.** Commit `428f047`. The notes below record what the task covered.
> Beyond the table: the file-dialog filters (`MainWindow.cpp:287,351`), the tile
> service user agent, the stylesheet header comment, `LICENSE.txt`, and the four
> `docs/*.md` title lines - which settles Task 28's rename item. The version is
> single-sourced from `project(VERSION ...)` through `WARD_PLANNING_VERSION` and
> `QCoreApplication::applicationVersion()`. Two build fixes landed with it: the
> MuPDF config package, and per-preset build directories. `vcpkg.json` also gained
> the `builtin-baseline` Task 4 revisits.
>
> **Accepted limitation:** the rename moves `QStandardPaths::CacheLocation`, so the
> map tile cache under the old name is orphaned rather than migrated. Tiles
> re-download on first use.

**Files:** `CMakeLists.txt`, `src/main.cpp:12-13`, `packaging/macos/Info.plist`,
`installer.iss`, `vcpkg.json`, `tests/CMakeLists.txt`, **`build.bat:51`**,
`build.sh`, `.vscode/launch.json`, `.vscode/tasks.json`, `README.md`

**`build.bat:51` is the one that will bite immediately:**

```
C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe --qmldir src\qml build\bin\EmergencyPlan.exe
if errorlevel 1 exit /b 1
```

Renaming the executable target leaves that path missing. And because this task edits
`CMakeLists.txt`, `build.bat:23` sets `NEED_CONFIGURE=1`, which deletes
`build\.deployed` at `:36`, which forces the deploy branch at `:48` to run - so the
failure is guaranteed on the very build this task asks the user to perform. Check
`build.sh` for the same path; it is referenced from `.vscode/tasks.json:12,16,34,38`.

**Sites:**

| What | Where |
|---|---|
| CMake project and targets `EmergencyPlan`/`EmergencyPlanLib` -> `WardPlanning`/`WardPlanningLib` | `CMakeLists.txt:3`, target definitions, `:602` status message |
| Project `DESCRIPTION` | `CMakeLists.txt:5` |
| `MACOSX_BUNDLE_GUI_IDENTIFIER`, `_BUNDLE_NAME`, `_ICON_FILE` | `CMakeLists.txt:525,528,537` |
| Commented `.rc` path | `CMakeLists.txt:513` |
| `setApplicationName`, `setOrganizationName` | `src/main.cpp:12-13` |
| `CFBundleExecutable`, `CFBundleIconFile`, `CFBundleIdentifier`, `CFBundleName`, `CFBundleTypeName`, `CFBundleTypeIconFile`, `NSLocationUsageDescription` | `Info.plist:9,12,15,21,52,63,69` |
| `MyAppExeName`, `OutputBaseFilename`, the `WardEmergencyPlan.Document` ProgID | `installer.iss:7,18,74-77` |
| Port name | `vcpkg.json:2` |
| Test executable, its link line, and `add_test` registration | `tests/CMakeLists.txt:33,37,38,48` |
| Executable target, its resources and bundle properties | `CMakeLists.txt:430-541` |
| Launch configurations and paths | `.vscode/launch.json:5,8,22,25,39,42,55` |
| Run tasks | `.vscode/tasks.json:61,63,67` |
| Stale Qt 6.10.1 paths (project builds with 6.11.0, `CMakePresets.json:39`) | `.vscode/launch.json:15,32,62,79,96`, `.vscode/tasks.json:97,110` |
| Window title | `src/widgets/MainWindow.cpp:530` |
| `exportedBy` metadata | `src/services/JsonService.cpp:110` |

The file **extension** is not part of this task - `Info.plist:59` and the others are
Task 12.

**`setOrganizationName("")` must become a real value.** It is currently empty and it
keys per-user storage locations; Task 8 puts a security control there. It is
`"Church"`, matching the `com.church.wardplanning` bundle identifier, which puts
per-user storage under `Church/Ward Planning/` - the application name alone would
repeat itself there.

**Pick one canonical version number** and apply it everywhere: `CMakeLists.txt:3-4`
says 1.0.0, `installer.iss:5` says 1.0.1, `Info.plist:27,30` say 1.0.0. This release
is 1.1.0.

**Settings from earlier versions are not carried forward.** Changing the application
and organization names moves `QStandardPaths` directories, so the old settings file at
`QStandardPaths::CacheLocation` (`MainWindow.cpp:630-634`) is no longer read, and Task 8
replaces it outright. The map asks for a default location once, and no document is
reopened at startup. (There is no window geometry
persistence in this app; `MainWindow` hard-codes `resize(1400, 900)` at `:86`.)

Build, run, commit as `refactor: rename product to Ward Planning`.

---

## Phase 1 - The core

### Task 1: The encrypted document core

> **Done.** Commit `1a222a2` and the review fixes after it. Where the code differs from
> the sections below, the code is the reference:
> - `Reconcile::validateDownload` takes a `Reconcile::Self` (the sealing keypair and the
>   verify key) in place of a bare `Keypair`, since checking this device's entry needs
>   both public keys. `Reconcile::readHeaders` parses fetched headers and returns
>   `NeedsNewerVersion` when any is a later format, so a caller never has to.
> - `DeviceKeys` lives in `CryptoTypes.h`, shared by `Pairing` and `ServerContract`.
>   `Pairing::newPairingCode` is the one generator; `FakeServer` issues codes with it.
>   `Pairing::Responder` and `Pairing::Initiator` are the pure steps of the exchange,
>   each keeping the first value it reads; the pairing driver (Task 23) feeds them what
>   the server returns, and `PairingTest` drives them through `FakeServer`'s scripted
>   reads to show a responder that re-read would be fooled.
> - `validateDownload` and `checkRemoval` take the document id the local file names,
>   and `validateDownload` a `HighWaterMark` of key generation and save counter.
> - `isCoherent` holds the format version, canonical document ids, and a generation
>   of at least 1, as `parse` does, so the server judges an in-memory header the same.
> - `ResponseMerge`: a record or task deleted on one side and edited on the other keeps
>   the edit, matching the response-level rule; a directory changed identically on both
>   sides is not a conflict.
> - The save counter is capped at 2^53 - 1, and the server refuses an upload that
>   raises it by more than 2^20 (`CounterJump`) or comes faster than 600 an hour from
>   one device, so no single device can pin it at the cap. `verifyLink` holds
>   `originDocumentId` and `formatVersion` fixed across a link.
> - Every parser refuses keys it does not know (`StrictJson::hasOnlyKeys`), and a
>   recipient kind below 1 is `Malformed`.
> - `FormatVectorsTest` pins the associated data, signed bytes, fingerprint, payload
>   hash, pairing values, signature tags, and a request message against vectors
>   computed independently.
> - Every mutating `ServerApi` call takes a request signature (`ServerContract.cpp`
>   holds the endpoints, canonical bodies, and `signRequest`/`requestVerifies`), and
>   `FakeServer` checks each one. Tests sign through `ServerTestSupport`'s `signed...`
>   wrappers, which sign with the device that owns the session.
> - `ServerApi` returns `Result<T>` from each call as written here; Task 6.1 changes every
>   method to answer through a callback, and changes the head, download, and header
>   chain shapes.
> - Task 4a removed the recipient `kind` and the payload version; 1.4, 1.6, and 1.9 below
>   still describe them.

**Everything that decides whether this design works, written as one artifact.** The
document format and its cryptography, the membership chain and its admin rules,
access control, the server contract and its fake, the reconciliation decisions that
protect the local file, the emergency response merge, and the device pairing that
authenticates linking and invites. Each has its tests.

It is pure computation over values: no UI, no settings, no keychain, no network, no
`DocumentManager`. Every later task is a consequence - plumbing, storage, transport,
wiring, and UI around these pieces.

**The tree is not expected to build during this task.** libsodium and the test harness
arrive in Tasks 3 and 4, after review. Write the sections in order; each builds on the
ones above it.

#### 1.1 Crypto value types

**Files:** `src/services/CryptoTypes.h/.cpp`, `tests/services/CryptoTypesTest.h/.cpp`

Fixed-size byte wrappers so the compiler catches a public key passed where a data key
belongs.

**Keep `<sodium.h>` out of the header** - the library target's
`target_include_directories(... PUBLIC ...)` at `CMakeLists.txt:370` would propagate it into every widget translation
unit. Use literals in the header and `static_assert` in the `.cpp`:

```cpp
// CryptoTypes.h
struct PublicKey
{
    static constexpr int Size = 32;

    QByteArray bytes;

    bool isValid() const { return bytes.size() == Size; }
    bool operator==(const PublicKey& other) const { return bytes == other.bytes; }
};

// SecretKey (32), DataKey (32), Fingerprint (16), MemberId (16) follow the same shape.
// MemberId is not the directory's PersonId (src/models/Id.h:60): a MemberId identifies a
// person on the access roster, a PersonId a member of the ward directory.
// VerifyKey (32) and SigningKey (64) are the Ed25519 pair used for the server.
// Fingerprint also declares toHex(), toGroupedHex(), fromHex().
```

```cpp
// CryptoTypes.cpp
static_assert(PublicKey::Size == crypto_box_PUBLICKEYBYTES);
static_assert(SecretKey::Size == crypto_box_SECRETKEYBYTES);
static_assert(DataKey::Size == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
static_assert(VerifyKey::Size == crypto_sign_PUBLICKEYBYTES);
static_assert(SigningKey::Size == crypto_sign_SECRETKEYBYTES);
```

**`SigningKey::Size` is 64, not 32.** libsodium's Ed25519 secret key carries the seed
and the public key together. Writing 32 there compiles and then fails at run time
inside `crypto_sign_detached`.

`Fingerprint::Size = 16` is 128 bits, meeting the design's floor, and is a legal
BLAKE2b output length. The fingerprint is taken over **both** public keys - X25519 then
Ed25519 - so neither key of a device can change while its identity stays the same.

**Tests:** `fromHex(fp.toHex()) == fp`; `toGroupedHex` produces four-character groups
separated by `-`; `fromHex` on malformed input yields an invalid fingerprint. (The `static_assert`s are
checked by the build, not by a test.)

#### 1.2 Key generation, signing, and fingerprints

**Files:** `src/services/DocumentCrypto.h/.cpp`,
`tests/services/DocumentCryptoTest.h/.cpp`

**Step 1: Tests**

```cpp
void DocumentCryptoTest::generatesDistinctKeypairs()
{
    const auto first = DocumentCrypto::generateKeypair();
    const auto second = DocumentCrypto::generateKeypair();

    QVERIFY(first.publicKey.isValid());
    QVERIFY(first.secretKey.isValid());
    QVERIFY(!(first.publicKey == second.publicKey));
}

void DocumentCryptoTest::fingerprintIsStableAndDistinct()
{
    const auto first = DocumentCrypto::generateKeypair();
    const auto second = DocumentCrypto::generateKeypair();

    const auto signing = DocumentCrypto::generateSigningKeypair();
    const auto otherSigning = DocumentCrypto::generateSigningKeypair();

    QCOMPARE(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey),
             DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey));
    QVERIFY(!(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey)
              == DocumentCrypto::fingerprintOf(second.publicKey, signing.verifyKey)));
    QVERIFY(!(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey)
              == DocumentCrypto::fingerprintOf(first.publicKey, otherSigning.verifyKey)));
    QCOMPARE(DocumentCrypto::fingerprintOf(first.publicKey, signing.verifyKey).bytes.size(),
             qsizetype(Fingerprint::Size));
}

void DocumentCryptoTest::signatureVerifiesAndRejectsTampering()
{
    const auto keys = DocumentCrypto::generateSigningKeypair();
    const QByteArray message = "challenge";
    const QByteArray signature = DocumentCrypto::sign(SignatureTag::Auth, message, keys.signingKey);

    QVERIFY(DocumentCrypto::verify(SignatureTag::Auth, message, signature, keys.verifyKey));
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Header, message, signature, keys.verifyKey));
    QVERIFY(!DocumentCrypto::verify(SignatureTag::Auth, "challenqe", signature, keys.verifyKey));
}

void DocumentCryptoTest::signatureUnderOneTagFailsUnderEveryOther()
{
    // A signature the server can influence (Auth) must never verify as document
    // evidence (Header), and no tag may stand in for another.
}
```

**`DocumentCrypto::initialise()` must run before any other call** - libsodium's failure
mode without `sodium_init()` is platform-dependent, not a clean crash. The test runner
calls it in Task 4, where the core tests first run; the app calls it at the top of
`main()`.

**Step 2: Implement**

```cpp
struct Keypair          { PublicKey publicKey; SecretKey secretKey; };
struct SigningKeypair   { VerifyKey verifyKey; SigningKey signingKey; };

class DocumentCrypto
{
public:
    static bool initialise();               ///< wraps sodium_init(); once at startup

    static Keypair generateKeypair();               ///< crypto_box_keypair
    static SigningKeypair generateSigningKeypair(); ///< crypto_sign_keypair
    static DataKey generateDataKey();               ///< randombytes_buf
    static Fingerprint fingerprintOf(const PublicKey& publicKey,
                                     const VerifyKey& verifyKey);   ///< crypto_generichash over both

    static QByteArray sign(SignatureTag tag, const QByteArray& message, const SigningKey& key);
    static bool verify(SignatureTag tag,
                       const QByteArray& message,
                       const QByteArray& signature,
                       const VerifyKey& key);
};
```

```cpp
enum class SignatureTag
{
    Auth,       ///< "auth"    - login challenge, the one message the server chooses
    Request,    ///< "request" - mutating request: document id and body hash
    Header,     ///< "header"  - DocumentHeader::signedBytes()
    Pairing     ///< "pairing" - the responder's handover to a device it just paired with
};
```

**Every signature is tagged.** `sign` and `verify` prepend the tag's ASCII bytes and one
0x00 byte to the message before `crypto_sign_detached` / `_verify_detached`. The tag is
never stored or transmitted. There is no untagged overload, so no code path can sign raw
bytes. This is what stops a hostile server from choosing login challenge bytes equal to
a header and collecting a real admin's signature over it.

**The two keypairs stay independent.** libsodium offers
`crypto_sign_ed25519_pk_to_curve25519`, and the design declines it: two keys cost
nothing and keep the sealing key and the signing key each doing one job. Do not add
the conversion "for convenience".

Expect PASS.

#### 1.3 Seal and unseal the data key

**Files:** `src/services/DocumentCrypto.h/.cpp`, `tests/services/DocumentCryptoTest.cpp`

```cpp
static QByteArray sealDataKey(const DataKey& dataKey, const PublicKey& recipient);
static std::optional<DataKey> unsealDataKey(const QByteArray& sealed, const Keypair& keys);
```

`crypto_box_seal` / `crypto_box_seal_open`. Output is
`DataKey::Size + crypto_box_SEALBYTES`. A failed open returns `std::nullopt` - the
ordinary "not addressed to me" case, not an error to report.

Sealed boxes use XSalsa20-Poly1305 internally rather than the payload's
XChaCha20-Poly1305. Immaterial for a 32-byte key; comment it so nobody later "fixes"
it.

**Tests:** the holder unseals; a different keypair does not; two seals of the same key
to the same recipient differ (ephemeral sender key).

#### 1.4 The document header

**Files:** `src/services/DocumentHeader.h/.cpp`, `tests/services/DocumentHeaderTest.h/.cpp`

```cpp
enum class RecipientKind
{
    X25519Sealed = 1        ///< a new kind is a new format version
};

struct RecipientEntry
{
    RecipientKind kind = RecipientKind::X25519Sealed;
    MemberId member;                ///< 128 random bits; groups a person's devices
    Fingerprint fingerprint;        ///< this device: fingerprintOf(publicKey, verifyKey)
    PublicKey publicKey;            ///< public; lets any reader recompute the fingerprint
    VerifyKey verifyKey;            ///< public; lets the header chain verify without a roster
    QByteArray sealedKey;

    bool operator==(const RecipientEntry& other) const;
};

struct DocumentHeader
{
    static constexpr int CurrentFormatVersion = 2;

    int formatVersion = CurrentFormatVersion;
    QString documentId;                     ///< 128 random bits, hex; set at creation, never changed
    QString originDocumentId;               ///< set only on a copy: the document it came from
    int keyGeneration = 1;
    qint64 saveCounter = 0;
    Fingerprint lastWriter;
    PublicKey writerPublicKey;              ///< lastWriter's keys; fingerprintOf them == lastWriter
    VerifyKey writerVerifyKey;
    QDateTime lastWriteTime;                ///< UTC
    QByteArray nonce;                       ///< 24 bytes
    QList<RecipientEntry> recipients;
    QList<MemberId> admins;                 ///< the admins' member ids, ascending by hex
    QByteArray payloadHash;                 ///< BLAKE2b of the ciphertext; outside the AD
    QByteArray writerSignature;             ///< Ed25519 by lastWriter; outside the AD

    QJsonObject toJson() const;

    QByteArray associatedData() const;
    QByteArray signedBytes() const;         ///< associatedData() + payloadHash
    std::optional<RecipientEntry> entryFor(const Fingerprint& fingerprint) const;

    bool operator==(const DocumentHeader& other) const;
};
```

**`documentId` is generated once, at creation, and bound in the associated data.**
Only someone already holding the data key can change it, and a modified file fails to
decrypt. Sharing registers the same id with the server (Task 6), so the id never
changes and nothing is re-encrypted. A copy is re-encrypted under a fresh id, which
makes it a genuinely different document. Whether a document is shared is not a header
property: it is shared once `AppSettings` holds a base version for it (Task 6).

**`payloadHash` and `writerSignature` are the two fields outside the associated
data.** The hash is computed from the ciphertext the associated data produces, and the
signature covers both. `signedBytes` is the associated data followed by the
length-prefixed payload hash; Task 1.5 signs and verifies it. Carrying the hash in the
header is what lets a header verify without its payload, which the membership chain
needs (Task 1.5).

**The header carries no server address.** The app supplies it (Task 8), so moving the
service to a new domain is an app release and existing documents follow unchanged.

**Parsing is strict.** `DocumentHeader::parse` is the only reader, and it distinguishes two refusals:

```cpp
enum class HeaderParse { Ok, NeedsNewerVersion, Malformed };
static HeaderParse parse(const QJsonObject& json, DocumentHeader& out);
```

`NeedsNewerVersion` for a `formatVersion` above `CurrentFormatVersion` or any recipient
`kind` this build does not know; `Malformed` for anything missing or ill-formed in a
version it does know. Nothing is skipped and nothing is carried through unread - a
tolerant reader mostly misreads. A new recipient kind ships as a new format version.

**`associatedData()` must be canonical, stable, and computed from the serialized field
values** - the same strings `toJson` writes. Two traps:

- **Do not use `QJsonDocument::toJson`.** Key ordering is unspecified across Qt
  versions, and a reordering would make every existing file undecryptable after a Qt
  upgrade.
- **Do not mix in-memory and parsed values.** `QDateTime` carries milliseconds;
  `Qt::ISODate` drops them and `Qt::ISODateWithMs` keeps them, and `fromString` can
  return a different timespec. If the writer builds AD from
  `QDateTime::currentDateTimeUtc()` and the reader builds it from the parsed value,
  every file fails authentication. Serialize first, build AD from the serialized
  strings.

Build the byte string explicitly and in fixed order: format version, document id,
origin document id (empty when absent),
key generation, save counter, last-writer hex, writer public key hex, writer verify key
hex, write time string, nonce hex,
then each recipient's kind, member id hex, fingerprint hex, public key hex, verify key hex, and base64 sealed key **in stored order**,
then the number of admins and each admin's member id hex in ascending order,
with a separator byte that cannot occur in any field or an explicit length prefix.

**A header verifies its own signature.** It carries its writer's two public keys, and
`parse` refuses one whose `fingerprintOf(writerPublicKey, writerVerifyKey)` is not
`lastWriter`. Checking a signature therefore never needs the writer looked up anywhere
first - which matters for a leave, whose writer has just left the roster. **A header may
list no recipients** only as the last person's leave (1.5, rule 7); `parse` accepts it,
and nobody can open or follow it.

**The admin list is canonical.** It is written in ascending order of member id hex, and
`parse` refuses one out of order or with a repeat, so one admin set has exactly one
serialisation and one associated data. It is read beside the recipients because a
removed device proves its removal from headers alone, and that proof has to show the
remover was an admin.

**Tests:** full round-trip including recipients, their order, the admin list, and the writer's
keys; `parse` refuses writer keys whose fingerprint is not `lastWriter`, and an admin list out
of order, with a repeat, or with a malformed id; it accepts a header listing no one; and returns
`Malformed` for a missing version, a non-object, a wrong-size nonce, a wrong-size
signature or payload hash, a wrong-size recipient verify key, and a malformed fingerprint;
`NeedsNewerVersion` for a higher `formatVersion` and for an unknown `kind`;
`associatedData()` differs when any single field differs, **including the document
id**, and including recipient order and kind, **but not when only `payloadHash` or
`writerSignature` differs**; `signedBytes()` differs when `payloadHash` differs; **AD computed from a header
that has been through `toJson`/`parse` equals AD from the original** - this is the
test that catches the timestamp trap, and "byte-identical across two constructions"
does not.

#### 1.5 Encrypt and decrypt the payload

**Files:** `src/services/DocumentCrypto.h/.cpp`, `tests/services/DocumentCryptoTest.cpp`

```cpp
static QByteArray generateNonce();          ///< 24 bytes, randombytes_buf, EVERY write
static QByteArray encryptPayload(const QByteArray& plaintext,
                                 const DataKey& key,
                                 const DocumentHeader& header);
static std::optional<QByteArray> decryptPayload(const QByteArray& sealed,
                                                const DataKey& key,
                                                const DocumentHeader& header);

static QByteArray payloadHash(const QByteArray& ciphertext);   ///< crypto_generichash
static QByteArray signHeader(const DocumentHeader& header, const SigningKey& key);  ///< SignatureTag::Header
static bool verifyHeader(const DocumentHeader& header);   ///< against header.writerVerifyKey

/// The writer is a device in this version's roster, or this version is a leave by a
/// device of the person it drops.
static bool writerIsAccountedFor(const DocumentHeader& header, const AccessRoster& roster);

enum class ChainResult { Removed, StillMember, Broken };

/// Walks headers forward from an already-authenticated anchor.
static ChainResult verifyChain(const DocumentHeader& anchor,
                               const QList<DocumentHeader>& later,
                               const Fingerprint& self);
```

**Every write is signed.** The writer sets its own keys and `payloadHash` in the header,
then `signHeader` signs `header.signedBytes()`. `verifyHeader(header)` checks the
signature against `header.writerVerifyKey` - one path everywhere. *Who* the writer is
is a separate question with one answer: a device in the version's own roster, or, for a
leave, a device of the person the version drops, proven by the chain or, with no base
copy, by the payload decrypting at all. Task 6's load path and Task 1.9 both use
`DocumentCrypto::writerIsAccountedFor(header, roster)` for it. Any mismatch between
header and roster fails too (`AccessControl::headerMatchesRoster`, Task 1.7).

**`verifyChain` is what a removed device's app relies on.** It cannot decrypt, so it
walks the headers after its base copy's header (Task 1.9). A link from `previous` to
`next` holds when all of these do, in the design's numbering:

1. `next.lastWriter` is a device among `previous`'s recipients, `next`'s writer keys are
   that entry's keys, and the signature verifies. The writer must be found in
   `previous`, never only in `next` - a server could otherwise mint a header listing a
   key of its own and sign with it.
2. The save counter rises. The key generation rises by **exactly one when a device or
   person is dropped by an unlink or a removal, and is unchanged otherwise.** The counter
   is capped at `DocumentHeader::MaxSaveCounter` (2^53 - 1); the server alone bounds one
   upload's rise to `MaxSaveCounterStep` (2^20), since a chain walked with headers
   missing sees larger gaps. A rotation
   without a drop is refused, so a header skipped because its membership did not change
   can never have hidden a rotation, and a jump - which would let one member pin the
   generation at the maximum - is impossible.
3. `next.lastWriter` is also among `next`'s recipients - except in a leave (rule 7).
4. Within each header, every entry's fingerprint equals `fingerprintOf(publicKey,
   verifyKey)`, appears under exactly one person, and no two entries share a public key
   or a verify key; a device in both headers has the **same member id, public key, and
   verify key** in both; every admin is a person in that header; and a header listing
   anyone lists at least one admin.
5. A device added under a person present in `previous` requires `lastWriter` to be a
   device of that person. An admin cannot attach a device to someone else.
6. A device dropped while its person remains requires `lastWriter` to be another device
   of that person **or** a device of an admin, and the key generation to increase. A
   person's last device is never dropped this way; that removes the person.
7. A person absent from `next` entirely requires either that `lastWriter`'s person is an
   admin, with the key generation increasing - a removal - or that `lastWriter` is a
   device of that very person, with nothing else in the membership changed and the key
   generation unchanged - a leave.
8. A person present only in `next` requires `lastWriter`'s person to be an admin.
9. A change to the admin list requires `lastWriter`'s person to be an admin, apart from a
   leaver dropping off it.

*An admin* always means a person on `previous`'s admin list, never `next`'s, so no one
can promote themselves and act on it in the same version.

**A leave does not rotate the key.** The leaver would generate the new key and so know
it; they are leaving by choice, and the server stops serving them the moment the leave
lands. The next removal or unlink rotates.

```cpp
static bool verifyLink(const DocumentHeader& previous, const DocumentHeader& next);
```

`verifyLink` is the single statement of the rule; `verifyChain` applies it pairwise,
and the fake server (Task 1.8) applies it to every upload. If
every link holds and the last header has no entry for `self`, the result is `Removed`;
any failure is `Broken`. Only `Removed` deletes anything (Task 6).

`crypto_aead_xchacha20poly1305_ietf_encrypt` / `_decrypt`, with
`header.associatedData()` as AD and `header.nonce` as the nonce. `qCompress` before
encrypting, `qUncompress` after decrypting; an empty `qUncompress` result is a failure,
not an empty document.

**`generateNonce()` is called on every single save**, never derived from the save
counter and never carried over from a loaded header. Comment it on the declaration - a
future reader will want to make it deterministic for testing, and that would XOR two
versions of the ward directory against each other and expose the Poly1305 one-time key.

**Tests:** `payloadRoundTrips`; `tamperedHeaderFailsDecryption` (keyGeneration,
saveCounter, lastWriteTime, a dropped recipient, and the admin list, each separately);
`truncatedPayloadFailsDecryption`; `wrongKeyFailsDecryption`;
`signatureVerifiesWithWriterKey`; `signatureFailsOnChangedHeaderField`;
`signatureFailsOnChangedPayloadHash`; `signatureFailsWithAnotherMembersKey`;
`chainProvesRemovalByAdminGrantedAfterAnchor` (anchor A+B+C with A an admin; A grants D
and makes D an admin; D removes C; C's chain is `Removed`);
`chainWithWriterOnlyInOwnHeaderIsBroken`; `chainWithWithheldGrantHeaderIsBroken`;
`chainWithWithheldPromotionHeaderIsBroken`; `chainWithBadSignatureIsBroken`;
`chainSkippingHeadersWithUnchangedMembershipVerifies`;
`chainWithHeaderSignedUnderAuthTagIsBroken`;
`chainWithDecreasingGenerationIsBroken`; `chainWithNonIncreasingSaveCounterIsBroken`;
`linkBeyondTheSaveCounterCapIsBroken`;
`chainStillListingSelfIsStillMember`; `chainProvesRemovalForEveryDeviceOfTheRemovedPerson`;
`chainProvesUnlinkForTheUnlinkedDevice`; `chainProvesALeaveToTheLeaversOtherDevices`;
`linkWithWriterAbsentFromOwnRecipientsIsBroken`;
`linkAddingDeviceToOwnPersonHolds`; `linkAddingDeviceToAnotherPersonIsBroken`;
`linkAdminAddingDeviceToAnotherPersonIsBroken`;
`linkUnlinkingOwnDeviceWithRotationHolds`; `linkAdminUnlinkingAnotherPersonsDeviceHolds`;
`linkMemberUnlinkingAnotherPersonsDeviceIsBroken`; `linkUnlinkingWithoutRotationIsBroken`;
`droppingAPersonsLastDeviceIsARemovalNotAnUnlink`;
`linkAdminRemovingAPersonHolds`; `linkMemberRemovingAPersonIsBroken`;
`linkAdminRemovingAnotherAdminHolds`; `linkRemovingWithoutRotationIsBroken`;
`linkRemovingPersonDropsAllTheirDevices`;
`linkAdminGrantingHolds`; `linkMemberGrantingIsBroken`;
`linkAdminPromotingAndDemotingHolds`; `linkAdminSteppingDownHolds`;
`linkMemberPromotingIsBroken`; `linkMemberDemotingIsBroken`;
`linkPromotedInThisVersionCannotActInIt`;
`linkWithNoAdminIsBroken` (covering the last admin leaving or stepping down while others
remain); `linkWithAdminNotInTheHeaderIsBroken`;
`linkRotatingWithoutADropIsBroken`; `linkRemovingWithoutRotatingIsBroken` (also covers
unlinking); `linkWriterKeysNotMatchingPreviousEntryIsBroken`;
`linkLeaveHolds`; `linkLeaveByTheLeaversOtherDeviceHolds`; `linkLeaveThatRotatesIsBroken`;
`linkAdminLeaveThatRotatesIsBroken`; `linkLeaveThatAlsoChangesTheAdminListIsBroken`;
`linkLastPersonLeavingToAnEmptyHeaderHolds`; `linkAfterAnEmptyHeaderIsBroken`;
`linkLeaveThatAlsoRemovesSomeoneElseIsBroken`; `linkLeaveThatAlsoGrantsIsBroken`;
`linkLeavingSomeoneElseIsBroken`;
`linkWithDeviceUnderTwoPeopleIsBroken`; `linkMovingDeviceBetweenPeopleIsBroken`;
`linkChangingVerifyKeyOfExistingDeviceIsBroken`; `linkChangingPublicKeyOfExistingDeviceIsBroken`;
`linkSkippingAKeyGenerationIsBroken`;
`linkWithFingerprintNotMatchingKeysIsBroken`; `linkWithDuplicateVerifyKeyIsBroken`;
`linkAcrossDocumentsIsBroken`.

The nonce-freshness assertions live at the `DocumentFile` level (Task 6.4), where they
can observe what the writer actually did.

#### 1.6 The access roster

**Files:** `src/models/RosterDevice.h/.cpp`, `src/models/RosterPerson.h/.cpp`,
`src/models/AccessRoster.h/.cpp`, `src/models/Document.h/.cpp`,
`tests/models/AccessRosterTest.h/.cpp`

The roster lives **inside** the payload, so it is a document model. It is what makes
revocation possible: a header entry holds only a fingerprint, and you cannot seal to a
fingerprint.

**The roster lists people, each with their devices.** Adding Bob gives Bob access;
Bob's laptop and phone are two devices under one person, added by Bob himself.

`RosterDevice` holds device name, X25519 public key, and Ed25519 verify key; derives
its fingerprint from both public keys, once, when made or read. `RosterPerson` holds
member id, display name, **role** (`Admin` or `Member`), and a non-empty list of devices.
Both have `toJson` / `fromJson` (the latter returning `std::optional`), and `isValid()`
requires exactly what `fromJson` requires, so anything valid reads back from the payload.

`AccessRoster` wraps a hash of people keyed by member id hex, offering person add,
remove, contains; device add and remove under a person; lookup of a device and its
person by fingerprint; person count; setting a person's role; `admins()`, the admins'
member ids ascending by hex, exactly as the header lists them; a `people()` accessor
sorted by display name, each person's devices sorted by device name; and JSON. **A
fingerprint may appear under only one person**, and **no two devices may share a public
key or a verify key** - the header forbids both, and every write reseals from the roster,
so a roster the header could not represent would stop the ward publishing.

**The roster parses strictly, as the header does.** A person or device with a missing or
ill-formed required field fails the whole payload; nothing is dropped and nothing is
written back partially understood. `Document::fromJson` has no failure channel
(`Document.h:158`), so add a strict parser **beside** it:

```cpp
enum class DocumentParse { Ok, NeedsNewerVersion, Malformed };
static DocumentParse parse(const QJsonObject& json, Document& out);
```

`NeedsNewerVersion` for a payload version above the one this build writes; `Malformed`
for any missing or ill-formed required field, roster included. `fromJson` stays as it is
for now - `JsonService.cpp:94` still calls it, and Task 4's build must pass - and Task 4a
replaces it when it gives every model a strict reader.

**Tests:** `parse` returns `Ok` for a document written by `toJson`, `NeedsNewerVersion`
for a higher payload version, and `Malformed` for a roster device with a wrong-size public
key or a missing member id.

Add `accessRoster()` and `setAccessRoster()` to `Document`, serialised like any other
collection. This puts the roster into `Document::operator==` (`Document.cpp:642-656`);
dirty state is unaffected, since that is `m_undoStack.size() != m_savedPosition`
(`CommandHistory.cpp:114-117`) and unrelated to document equality.

**Tests:** person add, remove, and contains; device add and remove under a person;
adding the same device twice leaves one; adding a device already held by another person
is refused; removing a person's last device is refused (that is removing the person);
two people sharing a display name are distinct and sort stably; a person's role
round-trips and `admins()` lists exactly the admins in ascending order; a person with an
unknown role fails the whole roster; round-trip through `Document::toJson` and
`Document::parse`.

#### 1.7 Grant, link, unlink, remove, roles, and leave

**Files:** `src/services/AccessControl.h/.cpp`, `tests/services/AccessControlTest.h/.cpp`

Pure functions over a header plus a roster, testable before any file, server, or UI
exists.

```cpp
struct AccessChange
{
    DocumentHeader header;
    AccessRoster roster;
    DataKey dataKey;        ///< new on remove and unlink; unchanged otherwise
};

class AccessControl
{
public:
    // Every operation takes the writer - the device that will sign the result - 
    // and acts as that device's person. Each returns nullopt rather than a change
    // the chain would refuse.

    /// Adds a new person, as a member, with exactly one device. The writer's person
    /// is an admin; data key and generation unchanged.
    static std::optional<AccessChange> grant(const DocumentHeader& header,
                                             const AccessRoster& roster,
                                             const DataKey& dataKey,
                                             const Fingerprint& writer,
                                             const RosterPerson& newPerson);

    /// Adds a new device under the writer's own person; data key and generation unchanged.
    static std::optional<AccessChange> linkDevice(const DocumentHeader& header,
                                                  const AccessRoster& roster,
                                                  const DataKey& dataKey,
                                                  const Fingerprint& writer,
                                                  const RosterDevice& newDevice);

    /// Drops one device, never the writer itself and never a person's last: one of the
    /// writer's own, or anyone's when the writer's person is an admin. Rotates the key.
    static std::optional<AccessChange> unlinkDevice(const DocumentHeader& header,
                                                    const AccessRoster& roster,
                                                    const Fingerprint& writer,
                                                    const Fingerprint& device);

    /// Removes a person and every one of their devices. The writer's person is an admin
    /// and not the target, so an admin always remains. Rotates the key.
    static std::optional<AccessChange> remove(const DocumentHeader& header,
                                              const AccessRoster& roster,
                                              const Fingerprint& writer,
                                              const MemberId& target);

    /// Makes a person an admin or a member, the writer's own person included. The
    /// writer's person is an admin, and an admin remains. Data key and generation unchanged.
    static std::optional<AccessChange> setRole(const DocumentHeader& header,
                                               const AccessRoster& roster,
                                               const DataKey& dataKey,
                                               const Fingerprint& writer,
                                               const MemberId& person,
                                               RosterPerson::Role role);

    /// Drops the writer's own person. Refused for the last admin while anyone else
    /// remains; the last person's leave produces a header listing no one. Does not
    /// rotate: the leaver would know the new key anyway.
    static std::optional<AccessChange> leave(const DocumentHeader& header,
                                             const AccessRoster& roster,
                                             const DataKey& dataKey,
                                             const Fingerprint& writer);

    /// Rebuilds header.recipients and header.admins from the roster, sealing dataKey
    /// to every device. nullopt when the key is invalid or any device cannot be sealed to.
    static std::optional<DocumentHeader> resealAll(const DocumentHeader& header,
                                                   const AccessRoster& roster,
                                                   const DataKey& dataKey);

    /// True only when every recipient entry has a roster device with the same
    /// fingerprint, member id, and keys, every roster device has exactly one entry, and
    /// the header's admin list is exactly the roster's admins.
    static bool headerMatchesRoster(const DocumentHeader& header, const AccessRoster& roster);
};
```

**Every operation takes the writer's fingerprint, not a member id.** Tying a change to the
device that will sign it is what lets each operation refuse exactly what `verifyLink`
refuses - including a device unlinking itself - and it sets `lastWriter`, so the save
path cannot sign as a different device. Every operation also refuses a header and roster
that already disagree, and a person or device that would not read back from the payload
exactly as written: one granted request carrying an empty device name would otherwise
make the ward unreadable for everyone.

**The returned header is the start of a write.** Its recipients, admin list, generation,
and `lastWriter` are set; the nonce, write time, payload hash, and signature are cleared,
so a save path that forgets to produce one fails rather than reusing the previous write's
nonce under an unchanged key.

**One change per published version.** Remove and unlink each advance the generation by
exactly one, and the chain refuses a step of two. A change is therefore built on the
server's accepted version and published before the next is built - which is what the
`RosterChangeQueue` (Task 6) does.

**There is no allow-list to compute or send.** The server's allow-list is the accepted
header's recipients (design, *The server*), so every operation above takes effect on the
server when its header is uploaded. The one agreement the server cannot check - header
recipients and admin list against the payload roster - is `headerMatchesRoster`, checked
on every open and download (Task 6, Task 1.9); a mismatch is refused, never shown and
opened.

**Tests:** each operation, finished and signed by its writer, passes `verifyLink` from the
header it started from; and each refusal is isolated, so deleting the check it targets
fails the test.

```cpp
void grantLetsTheNewPersonsDeviceOpenTheDocument();
void grantLeavesTheDataKeyAndGenerationAlone();
void grantAddsAMember();
void grantRefusesAMemberWriter();
void grantRefusesAMemberIdAlreadyInTheRoster();
void grantRefusesAPersonWithMoreThanOneDevice();
void grantRefusesADeviceWithNoNameOrInvalidKeys();
void grantRefusesADeviceSharingAKey();
void linkDeviceLetsTheNewDeviceOpenTheDocumentAndLinkVerifies();
void linkDeviceRefusesADeviceAlreadyLinked();
void unlinkDeviceRotatesTheKeyAndTheUnlinkedDeviceCannotUnseal();
void unlinkDeviceRefusesTheWritingDevice();
void adminUnlinksAnotherPersonsLostDevice();
void memberCannotUnlinkAnotherPersonsDevice();
void removeDropsEveryDeviceOfThePerson();
void removeRotatesTheDataKeyAndBumpsTheGeneration();
void removedMemberCannotUnsealTheNewKey();
void remainingMembersCanUnsealTheNewKey();
void removeRefusesAMemberWriter();
void removeRefusesTheWritersOwnPerson();
void adminRemovesAnotherAdmin();
void setRolePromotesAndDemotes();
void setRoleRefusesAMemberWriter();
void setRoleLetsAnAdminStepDown();
void setRoleRefusesDemotingTheLastAdmin();
void leaveDropsTheWritersPersonWithoutRotating();
void remainingMemberOpensVersionWrittenByALeave();
void leaveRefusesTheLastAdminWhileOthersRemain();
void theLastPersonsLeaveListsNoOne();
void resealAllGivesEveryMemberAnEntryAndListsTheAdmins();
void headerEntryWithNoRosterDeviceFailsTheMatch();
void rosterDeviceWithNoHeaderEntryFailsTheMatch();
void entryWhoseMemberOrKeysDisagreeWithRosterFailsTheMatch();
void adminListThatDisagreesWithRosterFailsTheMatch();
void everyAccessChangeProducesAMatchingHeaderAndRoster();
void operationsRefuseAHeaderThatDisagreesWithItsRoster();
void changesClearWhatAWriteMustProduceAfresh();
```

`removedMemberCannotUnsealTheNewKey` proves the model: seal to three keypairs, remove
one, assert the removed keypair opens no entry in the new header **and** that the *old
data key* fails to decrypt the new payload. A leave deliberately makes no such claim.

#### 1.8 The server contract and a fake server

**Files:** `src/services/ServerContract.h`, `tests/services/FakeServer.h/.cpp`,
`tests/services/ServerConformanceTest.h/.cpp`, `tests/services/FakeServerTest.h/.cpp`

`ServerContract.h` holds only types and no transport: the request and response shapes
for each endpoint in the design's *The server* section - including the pairing endpoints
and a pairing code on an access request - plus a `ServerError` enum
covering unreachable, unauthenticated, not-allow-listed, unknown document, version
conflict, lease held, a header the chain refuses, a payload hash that does not match its
blob, a device added without an invitation, a taken document id, rate
limiting, and the pairing refusals.

`FakeServer` implements the contract in memory and enforces the rules the real server
must enforce, because a client written against a permissive fake will not work against
a strict server:

- version and key generation never decrease
- writes are conditional on the client's starting version
- **the allow-list is the current accepted header's recipients**, with no separate
  endpoint to change it: only those devices read the blob or write, and an accepted
  upload changes it in the same commit
- an accepted header is kept only when its recipient set (member id, fingerprint, and
  verify key) or its admin list differs from the previous header's; the current header is always available; and
  `headers?after={base}&through={version}` returns the kept headers newer than `base`,
  then the header of `version`,
  in order, to devices in the current header, and to a **former** device only up to and
  including the header that removed it. There is no age limit.
  `verifyLink` gives the same result across a skipped header whose membership did not
  change, which is what makes dropping those headers safe.
- the first header of a newly registered document (`POST /documents`) has no previous to
  link from: it must list exactly one person, its writer must be one of that person's
  devices, and its signature must verify against that device's verify key in the same
  header
- the first header's one person is an admin
- every uploaded header must pass `DocumentCrypto::verifyLink` against the current one,
  so a grant, removal, or role change by a writer who is not an admin, a device added to
  another person, or another person's device dropped by a writer who is not an admin, is
  refused
- a header listing no one - the last person's leave - is accepted like any leave, and
  afterwards the document is listed for no device and accepts no upload
- the device uploading a header is the device that wrote it
- a header's payload hash matches the blob uploaded with it - the server hashes the blob
  but never reads it
- a person who left or was removed may be granted again under the same member id
- a header that **adds a device** is accepted only when that device has a pending access
  request for this document (a grant; the request is consumed) or has completed a pairing
  as initiator with a device of the person it is added under (a link). A completed pairing
  is recorded as lasting permission to add that device under that person in any document
  the person belongs to, so the `RosterChangeQueue` can reach every ward however long it
  takes. A newly registered document's first header may list its creator's other
  devices only when each has completed a pairing as initiator with the registering
  device itself; anything else is `DeviceNotInvited`. A document never appears in
  `/me/documents` for a device that did not ask
- a development-only hook moves the server's clock forward, so session, lease, and
  pairing expiry can be tested
- `POST /documents` is rate-limited per device key, and one key may register at most a
  fixed number of documents (a configuration value on the server)
- `POST /documents/{id}/requests` is rate-limited per device key: it is the one write
  open to a non-member
- every mutating request carries a signature by the calling device's own key, under the
  `request` tag, over the route, the session token, the document id, and the
  `wp-request` hash of the request's canonical body (`ServerContract::signRequest`), so a
  captured signature is good only in its own session; anything else is
  `Unauthenticated`. The HTTP adapter (Task 13) sends the signature in a header and the
  server rebuilds the canonical body from the request it parsed
- an upload that raises the save counter by more than `DocumentHeader::MaxSaveCounterStep`
  over the current version is refused (`CounterJump`), and uploads are limited to 600 an
  hour per device key, so no single device can push the counter to its cap
- a device has one access request per document: a new one replaces its earlier
  request, and a request naming a sealing key already held by a device in the document
  or in another device's request is `InvalidRequest`
- an access request's status is readable by the device that posted it through
  `requests/mine`: pending, granted, or withdrawn
- **two authentication levels**: *device* - a signed challenge proving possession of a key,
  enough for `/me/documents`, `requests/mine`, and posting an access or link request;
  *member* - the device is also in the document's current header, required for every
  other document endpoint
- access requests store the pairing code as posted, and return it to members
- pairings belong to no document and expire after 10 minutes; `POST /pairings` needs
  device authentication and returns a fresh code; the responder step needs member
  authentication on some document; the responder, reveal, and handover steps are each
  written once, in that order; until a responder is recorded any member may read a
  pairing - the responder has to read and keep the commitment before posting its keys - 
  and afterwards only its initiator and its responder; and completing the handover is
  what permits the initiator's device to be added under the responder's person
- `GET /documents/{id}` returns version, header, and blob from one consistent state
- a write set is committed whole or rejected whole
- signatures are verified for real, using `DocumentCrypto::verify`

It also offers test-only controls: go offline, force a conflict, hold the lease as
another member, - for Task 6's negative tests - withhold or substitute a header in the
chain it returns, and - for Task 23's pairing tests - serve different pairing contents to
successive reads.

**The payload is opaque to the fake, as it is to the real server.** An upload carries
the plaintext header line and the encrypted payload; the fake stores the header for the
chain and never looks inside the payload. If a test ever needs it to, the design has
been violated somewhere.

**One interface, two servers.** `ServerContract.h` declares an abstract `ServerApi` - 
one method per endpoint, each returning `Result<T>` - and `FakeServer` implements it. The
real server is written separately, in TypeScript on Cloudflare (design, *Hosting notes*),
so the rules exist twice; what keeps the two in agreement is running the same tests
against both.

**Tests, in two suites:**

- **`ServerConformanceTest`** - the rule list above, one test each, written against
  `ServerApi&` and never against `FakeServer` directly. This is the conformance spec: it
  runs against the fake here, and against the real server over HTTP once Task 13 provides
  the adapter. It uses no fake-only control. The expiry tests move the clock through a
  `ServerApi` hook that the real server honours only in local development (design,
  *Companion work*).
- **`FakeServerTest`** - the adversarial controls the real server must never offer:
  offline, forced conflicts, a lease held by another member, withheld and substituted
  headers, scripted pairing reads. These exist for the client's negative tests and are
  tested here only to show the controls themselves work.

#### 1.9 Reconciliation decisions

**Files:** `src/services/Reconcile.h/.cpp`, `tests/services/ReconcileTest.h/.cpp`

The decisions that keep a local file safe when the server moves, as pure functions over
headers, ciphertext, keys, and rosters. No network, no settings, no files: the
sync-and-save core (Task 6) supplies the inputs and carries out the result. Writing them here, beside the crypto and the fake server, is what lets the
whole safety story be read and tested in one place.

```cpp
namespace Reconcile
{
    enum class Verdict { Accept, Removed, NeedsNewerVersion, ServerFault };

    struct DownloadCheck
    {
        Verdict verdict = Verdict::ServerFault;
        QString reason;                         ///< for the sync indicator on ServerFault
        std::optional<DocumentHeader> header;   ///< set on Accept
        std::optional<DataKey> dataKey;         ///< set on Accept
        std::optional<Document> document;       ///< set on Accept
    };

    /// Proves removal from headers alone - no payload, so it also serves a key the
    /// server now refuses. Removed only with a baseHeader and a chain that holds and
    /// ends without this identity; otherwise ServerFault.
    Verdict checkRemoval(const QString& documentId,
                         const std::optional<DocumentHeader>& baseHeader,
                         const QList<DocumentHeader>& laterHeaders,
                         const Fingerprint& self);

    /// Decides whether a server version may replace the local file.
    /// baseHeader is the base copy's header - the last version this machine and the
    /// server agreed on - or nullopt when no base copy passes its recorded hash.
    DownloadCheck validateDownload(const QString& documentId,
                                   const std::optional<DocumentHeader>& baseHeader,
                                   const QList<DocumentHeader>& laterHeaders,
                                   const QByteArray& ciphertext,
                                   const Self& self,
                                   const HighWaterMark& highWaterMark);

}
```

**`validateDownload`**, in order, stopping at the first failure:

0. **Headers arrive as parsed by `DocumentHeader::parse`.** If any fetched header parsed
   as `NeedsNewerVersion`, the caller passes that straight through as
   `Verdict::NeedsNewerVersion` without calling here; `Malformed` is `ServerFault`.
   `checkRemoval` follows the same rule, so a newer-format header never leads to
   deleting anything.
1. The last header in `laterHeaders` has no entry for this identity -> return
   `checkRemoval(baseHeader, laterHeaders, self)`. **Removal is never concluded
   without a trusted anchor**, and this is the only place it is concluded.
2. With a `baseHeader`, `verifyChain` from it returns `StillMember` -> otherwise
   `ServerFault`. With none, this step is skipped: this identity can decrypt, so
   steps 3-4 check the version directly.
3. The downloaded header's entry for this device's fingerprint carries exactly this
   device's own public and verify keys - otherwise `ServerFault`. The key unseals,
   `payloadHash` matches `ciphertext`, the payload decrypts,
   `AccessControl::headerMatchesRoster` holds, `verifyHeader` passes, and
   `writerIsAccountedFor` holds - so a version written by someone who has just left is
   accepted like any other. A
   payload whose version is newer than this build's is `NeedsNewerVersion`; a payload or
   roster that fails strict parsing is `ServerFault`.
4. The version is not below the high-water mark: not a lower key generation, and not
   a lower save counter in the same generation.

**The document id is checked first,** against the one the local file names: without a
base header nothing else ties a version to this document, and another ward that also
lists this device would pass every other check.

**`laterHeaders` is never empty.** It always ends with the header of the downloaded
version, which the session fetched in the same snapshot as `ciphertext`. An empty list is
`ServerFault`. When the server is still at the base version there is nothing newer, and
the caller does not call `validateDownload` at all.

**The anchor is the base header, never the local file's header.** Local saves bump the
save counter and are never uploaded, so the local header is not in the server's chain
and its counter can exceed the next member's; anchoring there would read every conflict
after offline edits as a broken chain.

`Accept` carries the decrypted header, key, and document so the caller writes exactly
what was checked. Only `Removed` ever leads to deleting anything (Task 6).

**There is no roster merge.** After a refused publish the caller takes their header, key,
and roster exactly as `validateDownload` returned them. A session's local file never holds
an unpublished roster change - every grant, link, unlink, removal, role change, and leave
goes through the `RosterChangeQueue` (Task 6), built on the server's version and
published on its own - so there is nothing of ours to carry over. Two admins granting at
once both succeed because the queue re-applies the refused one on top of the other.

**Tests:** each `validateDownload` failure in turn - unsealable, hash mismatch, bad
signature, broken chain, generation below the mark - yields `ServerFault` with nothing
else set; a chain-proven removal yields `Removed`, **including when the remover was
granted after the base header**; a withheld or substituted header yields `ServerFault`; a
valid newer version yields `Accept` with the decrypted document; **after several local
saves that push the local save counter above the other writer's, a valid newer version
still yields `Accept`**; with no base header, a valid newer version yields `Accept` and a
header without this identity yields `ServerFault`, never `Removed`; a newer format version or an unknown
recipient kind anywhere in the fetched headers yields `NeedsNewerVersion` and never
`Removed`; a payload with a newer version yields `NeedsNewerVersion`; a header whose entry for this device carries a
different verify key yields `ServerFault`; an empty `laterHeaders` yields
`ServerFault`; `checkRemoval`
reaches the same verdicts from headers alone, with no ciphertext; a version written by
someone who has just left yields `Accept`, with and without a base header; a header whose
writer is neither in its roster nor leaving yields `ServerFault`; the leaver's other
device reaches `Removed` from the leave.

#### 1.10 Emergency response records on reconnect

**Files:** `src/services/ResponseMerge.h/.cpp`, `tests/services/ResponseMergeTest.h/.cpp`

The one case the conflict dialog must never handle. An emergency is when several
people record work at once and when the network is least likely to be there, so
responders edit offline in parallel by design - and telling one of them to "choose a
version" would discard their record of who they contacted during a disaster.

**Reconnect merges three ways against the base copy (Task 6), silently.** Nothing for a
user to decide at two in the morning, and no model changes: no timestamps, no
tombstones.

```cpp
namespace ResponseMerge
{
    /// True when doc differs from base outside the emergency response and roster.
    bool directoryChanged(const Document& doc, const Document& base);

    /// Three-way merge; single values changed differently on both sides take mine,
    /// because mine is the device publishing second. Absence is a value: an emergency
    /// started on one side is kept; started on both becomes one response (name and
    /// startedAt from theirs); ended on one side while the other changed records keeps
    /// the response with those records.
    std::optional<EmergencyResponse> merge(const std::optional<EmergencyResponse>& base,
                                           const std::optional<EmergencyResponse>& mine,
                                           const std::optional<EmergencyResponse>& theirs);

    /// The whole decision: nullopt when both sides changed the directory (conflict
    /// dialog); otherwise the directory from whichever side changed it (theirs when
    /// neither did) with the response merged. The roster is theirs, taken as validated.
    std::optional<Document> mergeDocuments(const Document& base,
                                           const Document& mine,
                                           const Document& theirs);
}
```

**Routing, carried out by Task 6.** After taking their key and roster (Task 1.9): load the base
copy. With it, `mergeDocuments(base, mine, theirs)` either returns the merged document,
which is published silently, or `nullopt`, which raises Task 17's conflict dialog. With
no usable base copy, the conflict dialog.

**The same routing serves online and offline work.** Response edits need no lease
(Task 6), so while one member holds the lease and edits a family, a responder may
publish a contact record first. The lease holder's refused publish then finds the
directory changed only on its own side and merges silently - the conflict dialog is
reserved for the directory changing on both sides.

**Rules**, per family record, per task field, per contact attempt:

| Case | Result |
|---|---|
| Changed on one side only | That side's value, including a deletion |
| Changed on both sides to the same value | That value |
| Contact attempts | Keyed by `ContactAttemptId`; the union of both sides, each attempt merged field by field |
| A single value changed differently on both sides - including the response's `name` and `startedAt` | Mine |
| `taskCategories` | A set: added on either side kept, removed on either side removed |
| Response absent in base, present on one side | That side's response |
| Response absent in base, present on both | One response: family records merged as above with an empty base, `name` and `startedAt` from theirs |
| Response present in base, absent on one side, records changed on the other | The changed side's response is kept |
| Response present in base, absent on one side, unchanged on the other | Absent |

**Every attempt survives, so the record of who was contacted and when survives overlap
intact.** That is the part a ward has to be able to trust afterwards, and it is worth
an explicit test rather than an assumption. A task field - category, description,
assignment, notification, resolution - is merged as a unit where its parts belong
together: assignment (team, person, notes) is one value, and resolution (resolved,
notes, time) is one value.

**Work is partitioned by convention, not enforcement.** A response is organised by
`ResponseArea` and `Team`, so responders are already working different families; the
app does not restrict a device to its assigned area, which would block the person
stepping in to cover someone else.

**Directory and ministering changes made offline take the ordinary conflict path** of
Task 17. Nobody imports a ward directory during a disaster.

**Tests:** two responders' offline records both survive reconnect; overlapping contact
attempts are all kept; an attempt's notes edited on one side keep the edit; a contact
status changed differently on both sides takes mine, and changed only on theirs takes
theirs; a task resolved on one side and reassigned on the other keeps both changes; a
task removed offline stays removed and one removed on theirs is not restored; a family
record added on either side survives; `mergeDocuments` with a response-only change on
one side and a family edit on the other returns a document holding both, whichever
side is mine; with a family edit on both sides it returns `nullopt`; with no directory
change on either side it keeps theirs and merges the response; an emergency started
only on mine is kept; started on both yields one response with theirs' name; ended on
theirs while mine recorded an attempt keeps the response and the attempt; ended on theirs
with mine unchanged is ended; a category added on one side and another removed on the
other gives both changes.

#### 1.11 Device pairing

**Files:** `src/services/Pairing.h/.cpp`, `tests/services/PairingTest.h/.cpp`

**Two devices authenticate each other's public keys by comparing a six-digit number**,
with the server relaying. Used for linking a device and for in-person invites. It never
carries the data key.

```cpp
namespace Pairing
{
    struct DeviceKeys { PublicKey publicKey; VerifyKey verifyKey; };

    QByteArray newNonce();                                  ///< 32 bytes, randombytes_buf
    QByteArray commit(const DeviceKeys& initiator, const QByteArray& nonce);
    bool revealMatches(const QByteArray& commitment,
                       const DeviceKeys& initiator,
                       const QByteArray& nonce);            ///< constant-time
    QString comparisonNumber(const DeviceKeys& initiator,
                             const DeviceKeys& responder,
                             const QByteArray& nonce);      ///< six digits, "482 193"
    QString newPairingCode();                               ///< lookup handle, not a secret

    /// What the responder tells the initiator once the numbers match, signed under
    /// SignatureTag::Pairing with the responder's signing key.
    struct Handover
    {
        MemberId member;
        QString displayName;
        QList<QString> documentIds;     ///< the wards the initiator is being added to
        QByteArray signature;
    };

    QByteArray signHandover(const Handover& handover,
                            const QByteArray& nonce,
                            const SigningKey& responderKey);
    bool handoverVerifies(const Handover& handover,
                          const QByteArray& nonce,
                          const VerifyKey& responderVerifyKey);
}
```

**The pairing carries the identity, not the server.** After the numbers match, the
responder posts a `Handover` signed with the key the initiator just authenticated, over
the pairing's nonce as well as its contents, so it belongs to this pairing only. The
initiator calls `IdentityService::joinPerson` with that member id and display name, and
accepts only the wards it names. Nothing about who this device belongs to comes from the
server.

**The order is what makes it safe, and each side must bind what it read.**

1. The initiator posts `commit(initiator, nonce)`.
2. The responder **fetches that commitment and keeps it**, and only then posts its own
   keys.
3. The initiator **fetches the responder's keys and keeps them**, and only then reveals
   its keys and nonce.
4. The responder checks `revealMatches` **against the commitment it kept**, never against
   one re-read afterwards; the initiator computes its number from **the responder keys it
   kept**.
5. Both display `comparisonNumber` from those kept values.

A relay substituting the initiator's keys must commit before it sees the responder's
keys, and learns the nonce only after choosing any substitute responder keys, so it
cannot search for a matching number - one chance in a million per attempt, and a failure
shows two different numbers. **Re-reading either value after the fact is what would break
this:** a relay that waits until it holds the commitment, the responder keys, the nonce,
and the number can then search for a substitute commitment and nonce that reproduce the
same number, and hand the responder that pair instead. Each side therefore keeps the
bytes it first read and verifies against those. Both `commit` and `comparisonNumber` hash
under their own BLAKE2b personalisation, so neither value can stand in for the other. The
pairing code is only a handle for finding the pairing and carries no security.

**Tests:** `revealMatches` accepts the committed keys and nonce and rejects a changed
public key, a changed verify key, and a changed nonce; `comparisonNumber` is identical on
both sides for the same inputs, differs when either side's keys change, and is always six
digits; two nonces and two pairing codes differ; a simulated relay that swaps the
initiator's keys after commitment cannot make the responder's number match without
breaking `revealMatches`; **a relay that serves one commitment before the responder posts
its keys and a different commitment-and-reveal pair afterwards is caught, because the
responder verifies against the commitment it kept** - the same test with the responder
re-reading instead must fail, so the test pins the ordering and not just the maths. `handoverVerifies` accepts a handover signed by the paired responder and
rejects one signed by another key, one for a different nonce, and one whose member id,
display name, or ward list was changed.

---

### Task 2: STOP - user review of the core

**Stop here and hand the core to the user for review.** Do not start Task 3.

Present Task 1 as one artifact: the source files and their tests, in the order of its
sections. The tree does not need to build yet - libsodium and the test harness arrive in
Tasks 3 and 4 - so review is of the design as code, not of a passing run.

Ask the user specifically to look at:

- the header layout, `associatedData()` and `signedBytes()` field order
- `verifyLink` and `verifyChain`, including the admin rules and the leave
- `AccessControl::remove`, `setRole`, `leave`, `resealAll`, and strict parsing (`NeedsNewerVersion`, `Malformed`)
- `FakeServer`'s enforced rules, which the companion server must also satisfy
- `Reconcile::checkRemoval` and `validateDownload`, including a version written by someone who has just left
- `ResponseMerge::merge`, `directoryChanged`, and `mergeDocuments`
- `Pairing`, and the commit-respond-reveal order

Apply the user's changes to the core before anything else. After the user's review,
run the agent code review on the core (user review first, then agent). Commit the core
as `feat: encrypted document core` once both are settled.

---

## Phase 2 - Make the core build and pass

### Task 3: Stand up the test harness

> **Done.** Commit `1a222a2`, together with Tasks 1 and 4. The smoke test was seen
> failing before its assertion was corrected. `tests/CMakeLists.txt` adds `tests/models`
> and `tests/services` to the include path so suites include their headers by name.

Every entry in `TEST_SOURCES` is commented out (`tests/CMakeLists.txt:14-29`), so
`if(TEST_SOURCES)` at line 32 is false and nothing is built.

Build the consolidated runner immediately - `QTEST_MAIN` generates a `main()`, so a
second test file would be a duplicate-symbol link error.

**Files:** `tests/TestMain.cpp`, `tests/services/SmokeTest.h/.cpp`,
`tests/CMakeLists.txt`

```cpp
// tests/TestMain.cpp
#include <QApplication>
#include <QStandardPaths>
#include <QtTest>
#include "services/SmokeTest.h"

// Each suite is qExec'd in turn. Passing argc/argv through means a command-line
// test-function filter applies to every suite, and a suite with no matching
// function returns non-zero. Run without arguments for the full pass.
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("Church");
    app.setApplicationName("Ward Planning Tests");

    // Keep every settings write out of the developer's real profile, so no test
    // depends on state left by a previous run. AppSettings arrives in Task 8; until
    // then this also keeps QStandardPaths-based paths in a test location.
    QStandardPaths::setTestModeEnabled(true);

    int status = 0;

    {
        SmokeTest test;
        status |= QTest::qExec(&test, argc, argv);
    }

    return status;
}
```

`QApplication` is required, not optional: `QTest::qExec` does not create one, and
without it `QSettings` has no organization, `QStandardPaths` misbehaves, and
qtkeychain's asynchronous jobs have no event loop to complete on.

`SmokeTest.h` declares a `QObject` subclass with `Q_OBJECT` and a private slot;
`SmokeTest.cpp` defines it. AUTOMOC handles the header - `CMAKE_AUTOMOC` is set at
`CMakeLists.txt:19`, before `add_subdirectory(tests)` at `:577`, and is
directory-inherited.

Write the slot to fail first (`QCOMPARE(1 + 1, 3);`). In `tests/CMakeLists.txt`,
replace the commented `TEST_SOURCES` block; the executable and `add_test` registration
are already `WardPlanningTests` from Task 0.

**If the build produces no test binary**, have the user delete `build/windows-debug/.cmake_marker`
and build again (see *Before you start*).

Fix the assertion, confirm PASS, commit as `test: add Qt Test harness`.

---

### Task 4: Add libsodium and qtkeychain

> **Done.** Commit `1a222a2`. vcpkg baseline `07f4812` (vcpkg tool re-bootstrapped to
> 2026-07-27), libsodium 1.0.22 port-version 1. The static port writes `SODIUM_STATIC`
> into its own installed `export.h`, so no compile definition is needed. qtkeychain is
> 0.17.0, chosen over 0.15.0 by the user. Moving the baseline changed zlib's static
> library name, so the first build needed a fresh configure (delete `CMakeCache.txt`).

**libsodium comes from vcpkg; qtkeychain does not.** The vcpkg port `qtkeychain-qt6`
declares `{"name": "qtbase", "default-features": false}` and `qttools`, so taking it
would build a second Qt 6 from source and link it beside the system Qt at
`C:/Qt/6.11.0/msvc2022_64` - hours of build time and an ABI hazard.

**Files:** `vcpkg.json`, `CMakeLists.txt`, `tests/services/CryptoLinkTest.h/.cpp`,
`tests/TestMain.cpp`

**Step 1: Translations off.** qtkeychain's own CMake uses `Qt6LinguistTools` for its
`.ts` files unless translations are disabled; with them off (Step 3) the system Qt needs
no Linguist Tools. If the fetched `CMakeLists.txt` has no such option, confirm
`C:/Qt/6.11.0/msvc2022_64/lib/cmake/Qt6LinguistTools` exists before building - a missing
one fails at *configure* time, which under `build.bat` looks like a total build failure.

**Step 2: libsodium**

```json
{
    "name": "wardplanning",
    "version": "1.1.0",
    "builtin-baseline": "<vcpkg commit sha>",
    "dependencies": [
        "libmupdf",
        { "name": "libsodium", "version>=": "1.0.22" }
    ]
}
```

A bare `"libsodium"` string pins nothing, and the current baseline does not help: the
local vcpkg clone at `245eb91` (Dec 22 2025, already in `vcpkg.json`) builds libsodium
1.0.20 port-version 3 from the `1.0.20-RELEASE` tag, which predates the CVE-2025-69277
fix commit `ad3004e`. Upstream vcpkg ships libsodium 1.0.22 (checked 2026-09-13).

1. Ask the user to update the local vcpkg clone (`git -C $VCPKG_ROOT pull`), then set
   `builtin-baseline` to `git -C $VCPKG_ROOT rev-parse HEAD` and confirm
   `$VCPKG_ROOT/ports/libsodium/vcpkg.json` reports 1.0.22 or later.
2. **Confirm the fix is in that release.** Verified 2026-09-27: `ad3004e` itself sits
   on a side branch and is not an ancestor of `1.0.22-RELEASE`, but the same patch
   landed as `f2da4cd8`, which is an ancestor of both `1.0.21-RELEASE` and
   `1.0.22-RELEASE`. The source agrees: `ge25519_is_on_main_subgroup` in 1.0.21 and
   1.0.22 adds the `Y == Z` check that 1.0.20 lacks. The user accepted 1.0.22 on that
   evidence. `CryptoLinkTest` asserts the linked library is at least 1.0.22.
3. **Warn the user before they build:** moving the baseline re-resolves `libmupdf` and
   its whole transitive tree (freetype, harfbuzz, openjpeg, libjpeg, libpng, zlib,
   jbig2dec, gumbo, brotli, bz2). That can be a long rebuild. After it, have the user
   import a ward directory PDF and a ministering PDF to confirm `PdfExtractor` still
   works.

**Step 3: qtkeychain via FetchContent**

```cmake
include(FetchContent)
FetchContent_Declare(qtkeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG 875f77d9f61bd97fd84cca47ce3bc71186dfbd09  # 0.17.0
    EXCLUDE_FROM_ALL
)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TRANSLATIONS OFF)
set(BUILD_TEST_APPLICATION OFF)
FetchContent_MakeAvailable(qtkeychain)
target_compile_definitions(qt6keychain PUBLIC QTKEYCHAIN_NO_EXPORT)
```

Checked against the fetched source: the tag has no `v` prefix; 0.17.0 builds for Qt 6 by
default; `QTKEYCHAIN_STATIC` is deprecated in favour of `BUILD_SHARED_LIBS`, which
qtkeychain's `cmake_dependent_option` writes to the cache; `BUILD_TRANSLATIONS OFF`
skips `LinguistTools`. The generated export header marks a static build's classes
`dllimport` unless told otherwise, which `QTKEYCHAIN_NO_EXPORT` does. On Linux it needs
Qt DBus and the libsecret development headers.

**Step 4: Find and link**

```cmake
find_package(unofficial-sodium CONFIG REQUIRED)
```

then add `unofficial-sodium::sodium` and `qt6keychain` to
`target_link_libraries(WardPlanningLib PUBLIC ...)`. On the `x64-windows-static-md`
triplet static libsodium needs:

```cmake
target_compile_definitions(WardPlanningLib PUBLIC SODIUM_STATIC)
```

or MSVC emits `dllimport` references. Correct the imported target names from the
actual vcpkg and FetchContent output.

**Step 5: Run the core.** `CryptoLinkTest` asserts `sodium_init() >= 0`. Add the Task 1
sources to `SERVICE_SOURCES`/`MODEL_SOURCES`, register every Task 1 test suite in
`TestMain.cpp` (with `DocumentCrypto::initialise()` right after `QApplication`, per
Task 1.2), build, and have the user run the tests. **Every core test must pass before
Task 5 starts**; a failure here is a core defect, fixed in the core and shown to the
user, not worked around in a later task. Commit as `build: add libsodium and qtkeychain`.

---

### Task 4a: Strict model parsers

> **Done**, with these differences from the text below:
> - `StrictJson` holds the shared readers (required and optional fields, range-checked
>   integers, exact times and dates, id sets); `DocumentHeader` uses them too.
> - An optional field present but empty is refused, since the writer omits it when
>   empty; an address line is refused unless `Address` would keep it unchanged.
> - **No cross-reference checks.** The app has always tolerated a team, tag, or group
>   naming someone who is no longer in the document, and removing a member produces
>   exactly that, so `parse` checks each collection's own ids and nothing between them.
>   The one real defect behind them, the ward-directory import giving two members who
>   share a first name the same id, is fixed at its source.
> - **One version.** The recipient `kind` and the payload's `payloadVersion` are gone:
>   every change to the sealing or the payload raises the header's `formatVersion`,
>   which is read before decrypting, so neither ever decided anything on its own.
>   `Document::parse` returns `std::optional<Document>`.
> - A task's assignment (assignee, notes, notification) and its resolution (notes,
>   time) are each one optional value, so a merge can no longer leave a notification
>   without an assignee or a resolution time on an unresolved task.
> - Emergency-response times are written with milliseconds (`StrictJson::writeTime`), so
>   a record read back from disk equals the one in memory.
> - `JsonService` reads through `Document::parse` and writes the bare payload. Until the
>   roster bootstrap exists, the app cannot reopen what it saves.
> - The tests are two suites, `ModelParsersTest` and `DocumentParseTest`, over shared
>   samples in `ModelSamples` whose written form must equal the sample exactly.

**Every model type gets a strict reader, so `Document::parse` refuses a damaged payload
rather than loading part of it.** Task 1.6 made the payload version and the access
roster strict; the directory, ministering, and emergency collections still go through
the lenient `fromJson` readers, so a damaged family record loads as whatever `fromJson`
makes of it. That contradicts the rule the header and roster already follow - nothing
skipped, nothing defaulted, nothing carried through unread.

Placed here, immediately after the suite first runs, because this is exactly the code
where one field spelled differently between writer and reader makes every real document
`Malformed`, and a round-trip test catches that at once - but only when tests run.

**Files:** every model with a `fromJson` - `Family`, `Person`, `Birthday`, `Gender`,
`Team`, `Tag`, `EmergencyAsset`, `MinisteringDistrict`, `MinisteringGroup`,
`DocumentMetadata`, `Ward`, `Stake`, and `EmergencyResponse` with its nested records - 
plus `Document.cpp`, and a test file per type under `tests/models/`.

**Each parser accepts exactly what the current `toJson` writes, and nothing else.** This
is a clean break, so none of `fromJson`'s tolerances carry over: no default fallbacks
(`tagLevelFromJson`), no case-insensitive enum strings, no reading of retired fields.
Fields `toJson` omits when empty are optional; everything else is required. Each type
gets:

```cpp
static std::optional<Family> fromJson(const QJsonObject& json);   // replacing the lenient one
```

matching `RosterDevice` and `RosterPerson` - `nullopt` on any missing or ill-formed
field, and a collection fails whole when any element fails.

**`Document::parse` then reads every collection strictly** and stops calling
`Document::fromJson`. Cross-references are checked too: a family naming a person id that
is not in it, a tag or team naming an unknown person or family, and a ministering group
naming an unknown district are `Malformed`, because each would otherwise be a dangling
id the UI has to guess about.

**`JsonService` keeps working until Task 6** by switching its one call from
`Document::fromJson` to `Document::parse` here, reporting `Malformed` as its existing
load error. With no legacy files to honour, nothing it currently reads is lost: the
current build's own documents are exactly what `toJson` writes. The core then has no
`fromJson` left to delete.

**Tests:** per type, a round trip through `toJson` and `fromJson`; every required field
removed in turn is refused; a wrong JSON type for each field is refused; an optional
field's absence is accepted. For `Document::parse`, a damaged element in each collection
refuses the whole payload, and each dangling cross-reference is refused. The existing
suites must still pass.

Commit as `feat: strict model parsers`.

---

## Phase 3 - Clear the ground

### Task 5: Remove the emergency archive feature

Before the sync-and-save core, because it deletes the archive's paths into
`JsonService` and `DocumentManager`, so the core replaces one write design rather than
two. About 180 references across 17 files - recount before starting.

**Files:**
- Delete: `src/widgets/ArchiveBrowserDialog.h`, `src/widgets/ArchiveBrowserDialog.cpp`
- Modify: `src/services/EmergencyManager.h/.cpp`, `src/services/DocumentManager.h/.cpp`,
  `src/widgets/MainWindow.h/.cpp`, `src/widgets/EmergencyBanner.h/.cpp`,
  `src/widgets/ActionButtonsWidget.cpp`, `src/widgets/TeamsView.cpp`,
  `src/widgets/WardListView.cpp`, `src/listmodels/FamilyTreeModel.cpp`,
  `src/listmodels/MinisteringModel.cpp`, `src/listmodels/TeamsTreeModel.cpp`,
  `src/models/EmergencyResponse.h/.cpp`, `src/services/ReportGenerator.h/.cpp`,
  `src/services/ResponseMerge.cpp`, `tests/models/ModelSamples.cpp`,
  `tests/models/ModelParsersTest.cpp`, `CMakeLists.txt`

**Step 1: `EmergencyManager`**

Delete `saveArchive()`, `loadArchive()`, `reopenArchive()`, `closeArchive()`,
`isViewingArchive()`, the `archiveViewOpened`/`archiveViewClosed` signals, and the
`m_savedResponse` stash. Change `endEmergency(bool archive)` to `endEmergency()`.

Note `reopenArchive` currently deletes the archive file after a successful save
(`EmergencyManager.cpp:187-199`); that goes with it, including the
`persistResponseData()` call at `:187`.

**`endedAt` goes too.** An ended response is cleared from the document
(`endEmergency`), so with archives gone no stored response is ever ended. Delete
`m_endedAt`, `endedAt()`, `setEndedAt`, and `clearEndedAt` from `EmergencyResponse`,
with its `endedAt` reading, writing, and comparison (`EmergencyResponse.cpp:699-701,
728,745-752,772`), the merge line (`ResponseMerge.cpp:301`), and the sample field and
its optional-field entry (`ModelSamples.cpp:190`, `ModelParsersTest.cpp:242`). The end
time reaches the report as a parameter (Step 2).

**Step 2: The End Emergency dialog**

`MainWindow.cpp:930-998` is a hand-built `QMessageBox` with "Archive && End"
(`:944`), "Discard && End" (`:945`), a "Generate summary report (PDF)" checkbox
(`:940-942`), a `bool archive` threaded through `:951-972`, and two status messages at
`:990-997`.

Collapse it to a confirmation. **The summary report stays and becomes more
prominent** - with archives gone it is the only thing that outlives the response, so
the dialog should present it as the way to keep a record rather than as an
afterthought.

**The final report takes the end time as a parameter.** `ReportGenerator`'s emergency
report takes `std::optional<QDateTime> endedAt` beside the response
(`ReportGenerator.cpp:191-193` reads it from the response today). The end dialog passes
the current time; the mid-emergency **Generate report** action (`:1000-1003`) passes
none. The live response ends only when the user confirms, so cancelling the report -
`generateEmergencyReportWithConfirm` returning false at `MainWindow.cpp:1024-1033` -
leaves the emergency active.

Note in the dialog copy that the report is a plaintext PDF saved wherever the user
chooses.

**Step 3: The rest of `MainWindow`**

More sites than a quick sweep finds: `:2` the include; `:214-216` **two** menu actions
(`m_openArchiveAction`, `m_closeArchiveAction`); `:237-240` two connections;
`:1075-1082` and `:1084-1102` two slots; `:1104-1188` `onOpenArchive()` (85 lines);
`:1193` and `:1198-1199` enablement; and `MainWindow.h:62-63,66,120-121`.

**Step 4: The remaining views**

`DocumentManager`: delete `setArchiveDocument()`, `clearArchiveDocument()`,
`isViewingArchive()`, and `m_archiveDocument`; `document()` returns `m_document`
unconditionally; remove the guards at `:89,106,122` (Task 6 replaces them).

`EmergencyBanner.h:14` and `EmergencyBanner.cpp:42-52` (`setArchiveName`).
`WardListView.cpp:94,96` (the `archiveViewOpened`/`archiveViewClosed` connections) as
well as `:573`. `ActionButtonsWidget.cpp:22`, `TeamsView.cpp:141,225`,
`MainWindow.cpp:1193`, `FamilyTreeModel.cpp:54,56`, `MinisteringModel.cpp:35,37`,
`TeamsTreeModel.cpp:51,53`. Remove `ArchiveBrowserDialog.cpp` from `WIDGET_SOURCES`.

**Step 5: Build and check by hand**

The harness does not exist yet, so confirm by running: the app starts, a document
opens, an emergency starts and ends with the new confirmation, the summary PDF
generates, and no archive menu item remains.

Existing `*_archives/` folders are left on disk; the app stops reading them.

**Step 6:** `git commit -am "refactor: remove emergency archive feature"`

---

## Phase 4 - The sync-and-save core

### Task 6: The sync-and-save core

> **Done.** Committed at Task 7. Where the code differs from the sections below, the
> code is the reference:
> - **`ServerVersion.h/.cpp`** holds what the session and the queue share: reading and
>   writing the base copy, forgetting a ward, fetching and validating the server's
>   version, and the removal proof. `DocumentSession::forget` is `ServerVersion::forget`.
> - **The high-water mark is the base version's header.** It rises when the local file
>   takes in a server version or a publish is accepted, and only then; the queue does not
>   raise it, since a closed ward's local file would then fail to open after a rotation.
>   "Unpublished changes" is the local counter above the mark, which needs no base copy.
> - **A session owns its timers**, passed to `create` and `open`, and the queue owns its
>   own. The backoff constants are `SessionTimers::FirstRetryMilliseconds` and
>   `LastRetryMilliseconds`.
> - **Destroying a session closes it** - the scheduled save is written and the lease
>   released, and every reply still on its way is dropped - so there is no `close()`.
>   `flush()` reports whether that save succeeds; a failed save stays scheduled.
> - `documentReplaced` is emitted in one place: whenever the local file takes in a
>   document other than the one in memory.
> - Every head check asked while another operation is in flight runs when it ends.
>   Registration is one of those operations.
> - `syncState()` comes with `faultReason()`; `open` returns a refusal's reason.
> - The write-path backstop is gone: `m_document` changes only through `edit()`, which
>   refuses first.
> - **The queue keeps the open ward current.** `setOpenSession` names it; after a publish
>   to it the session checks the server, and when this device is proven off it the
>   session finishes removed and writes nothing more. `shareOpenWard` shares it with the
>   pending Links, which then finish through `changeFinished`; `DocumentSession::share`
>   takes the devices to link.
> - `RosterChange` has no document id; the record that keeps it, and the queue's calls,
>   name the ward. `DocumentRecords::documentIds()` lets a queue resume.
> - A ward that cannot publish now waits for the next Retry while the others go ahead. A
>   fetch that got no answer to judge is retried; `ValidationFailed` is a version that did
>   not validate, or one the server refused as malformed.
> - `download(after)` runs from 0 to the current version; the current version itself
>   serves the current header alone, and a snapshot at the base version is judged by
>   itself and must equal the held base header. `FakeServer::withholdHeader` never drops
>   the last header a read serves, and `substituteHeader` reaches downloads too.

**Everything that decides how a document is saved, published, and kept safe at run time,
written as one artifact.** Task 1 made the decisions as pure functions over values. This
task is the orchestration that carries them out: the file on disk, the open ward's state
machine, the publish queue, the lease, the refused-publish route, removal, and the roster
change queue. Every later task is storage, transport, wiring, or UI around it.

It runs against small seams rather than the real services. Identity is a value; the
per-document records sit behind an interface that tests keep in memory; files go to paths
the caller names; the server is `ServerApi`, with `FakeServer` in the tests; and timers
fire when the test says so. No settings file, keychain, network, `DocumentManager`, or
widget is touched.

**The tree is not expected to build while this lands.** Write the sections in order; each
builds on the ones above it.

#### 6.1 The server contract answers through callbacks

**Files:** `src/services/ServerContract.h/.cpp`, `tests/services/FakeServer.h/.cpp`,
`tests/services/ServerTestSupport.h/.cpp`, `tests/services/ServerConformanceTest.cpp`,
`tests/services/FakeServerTest.cpp`, `tests/services/PairingTest.cpp`

The real server answers over the network, and the window must not freeze while it does.
Every `ServerApi` method therefore takes a completion instead of returning its result:

```cpp
template<typename T>
using Reply = std::function<void(const Result<T>&)>;

class ServerApi
{
public:
    virtual ~ServerApi() = default;

    virtual void challenge(Reply<QByteArray> reply) = 0;
    virtual void head(const Session& session, const QString& documentId, Reply<Head> reply) = 0;
    virtual void upload(const Session& session,
                        const QString& documentId,
                        qint64 baseVersion,
                        const Upload& upload,
                        const QByteArray& requestSignature,
                        Reply<qint64> reply) = 0;
    // ...every other endpoint the same way: its parameters, then the reply.
};
```

**Two rules every implementation keeps.** The reply is called exactly once, and never
before the call returns. The second is what lets a caller start a request from inside its
own state change without being re-entered halfway through it; the HTTP implementation
(Task 13) keeps it naturally, and the fake keeps it by queueing.

**Reads carry headers as the server stored them, and the chain has no upper bound to
guess.** A reader parses what it fetched with `Reconcile::readHeaders`, so a header in a
later format reaches the client as `NeedsNewerVersion` rather than being lost in transit,
and a removed device can fetch the proof of its removal without knowing the current
version, which the server no longer tells it:

```cpp
/// GET /documents/{id}/head - member authentication.
struct Head
{
    qint64 version = 0;
    std::optional<Fingerprint> leaseHolder;
};

/// GET /documents/{id}?after={base} - member authentication. One consistent state: the
/// kept headers newer than base, then the current header, and the current blob.
struct Snapshot
{
    qint64 version = 0;
    QList<QJsonObject> headers;     ///< never empty; ends with the header of version
    QByteArray blob;
};

/// GET /documents/{id}/headers?after={base} - device authentication. The kept headers
/// newer than base, then the current header; for a former device, up to and including
/// the header that removed it.
struct HeaderChain
{
    QList<QJsonObject> headers;
};
```

`download` gains the `after` parameter, `headers` loses `through`, and `Head` loses the
header and key generation, which only the download needs. `Upload` keeps its parsed
`DocumentHeader`, since the client uploads what it wrote.

**`FakeServer` decides at call time and replies later.** The server's state changes when
the call is made, as when a request reaches a real server, and the reply is queued and
delivered on a later turn of the event loop. Three controls join the others:

```cpp
void holdReplies(bool hold);    ///< queued replies wait for deliverNext()
bool deliverNext();             ///< delivers the oldest held reply; false when none
void loseReplies(bool lose);    ///< calls still take effect; their replies arrive as Unreachable
```

Holding replies is how a session test puts a publish in flight and then saves again, or
closes the ward, before the server's answer arrives; losing them is how it shows a publish
the server accepted but whose answer never came. `substituteHeader` with a raised
`formatVersion` serves a later-format header for the `NeedsNewerVersion` tests.

**The server suites keep what they assert.** `ServerTestSupport` gains one helper that
makes a call and waits for its reply:

```cpp
/// Runs call with a reply that stores the result, then spins a QEventLoop until it
/// arrives. Ten seconds without one is ServerError::Unreachable.
template<typename T>
ServerContract::Result<T> wait(const std::function<void(ServerContract::Reply<T>)>& call);
```

The `signed...` wrappers keep their signatures and wait internally, and each direct
`server().method(...)` call in the suites becomes a `wait`. Waiting on the event loop
serves the fake and a real server alike, so the conformance suite still runs unchanged
against both. The chain tests call `headers` with only a base, as a client does.

**Tests (`FakeServerTest`):** a reply never arrives before the call returns; held replies
arrive in call order, one per `deliverNext()`; an upload's effect is visible to a second
call while the first call's reply is still held; a lost reply's upload is still the
server's current version. **Conformance:** a removed device's `headers(after = its base)`
ends with the header that removed it, although it cannot read the head; a snapshot's last
header is the header of its version. Every existing server and pairing test passes.

#### 6.2 The seams

**Files:** `src/services/DeviceIdentity.h/.cpp`, `src/services/DocumentRecords.h/.cpp`,
`src/services/SessionTimers.h/.cpp`, `tests/services/SessionTestSupport.h/.cpp`

```cpp
/// This device, as the core sees it: a value, fixed once a person exists.
/// IdentityService (Task 10) produces it.
struct DeviceIdentity
{
    Keypair sealing;
    SigningKeypair signing;
    MemberId member;
    QString displayName;
    QString deviceName;

    Fingerprint fingerprint() const;
    Reconcile::Self self() const;
    RosterDevice asRosterDevice() const;
    RosterPerson asNewPerson() const;   ///< this person, an admin, with only this device
};
```

```cpp
/// What this device keeps about one document besides its files. AppSettings keeps it
/// in the app (Task 8).
struct DocumentRecord
{
    std::optional<qint64> baseVersion;      ///< the server version the base copy holds; set once shared
    QByteArray baseHash;                    ///< the base copy's payload hash
    Reconcile::HighWaterMark highWaterMark;
    QList<RosterChange> pendingChanges;     ///< 6.6
};

class DocumentRecords
{
public:
    virtual ~DocumentRecords() = default;

    /// A default record when none is held.
    virtual DocumentRecord record(const QString& documentId) const = 0;
    virtual void setRecord(const QString& documentId, const DocumentRecord& record) = 0;
    virtual void removeRecord(const QString& documentId) = 0;
};
```

```cpp
/// Where one document's files live. DocumentLibrary (Task 11) names them.
struct DocumentPaths
{
    QString file;
    QString base;
};
```

```cpp
/// A session's timers, or the roster change queue's. The app runs them on QTimer; tests
/// fire them by hand, so no test waits on the clock and no assertion depends on
/// event-loop timing.
class SessionTimers
{
public:
    enum class Timer { Save, Retry, Heartbeat, HeadCheck };

    virtual ~SessionTimers() = default;

    virtual void start(Timer timer, int milliseconds) = 0;  ///< restarts one already running
    virtual void stop(Timer timer) = 0;

    std::function<void(Timer)> onTimeout;
};
```

`SessionTestSupport` holds the test implementations: `MemoryRecords`; `ManualTimers`,
which records each running timer and its interval and fires one on `fire(Timer)`; and a
`TestWard` of up to three devices - each a `DeviceIdentity`, `MemoryRecords`,
`ManualTimers`, `ServerConnection`, and `QTemporaryDir` - over one `FakeServer`, with
helpers to open a session for any of them and to let every pending reply arrive.

#### 6.3 `ServerConnection`: signing in and signing requests

**Files:** `src/services/ServerConnection.h/.cpp`,
`tests/services/ServerConnectionTest.h/.cpp`

One per device, over a `ServerApi&`. It holds the session token, signs in on first use -
fetch a challenge, sign it under `SignatureTag::Auth`, exchange it for a token - and signs
every mutating request under `SignatureTag::Request` with `ServerContract::signRequest`.
Everything above it calls the server without seeing sessions or signatures.

```cpp
class ServerConnection
{
public:
    ServerConnection(ServerContract::ServerApi& server, const DeviceIdentity& identity);

    // Every reply is delivered only while its context is alive, as with QTimer::singleShot:
    // a session destroyed with a call outstanding never hears the answer.

    void registerDocument(const ServerContract::Upload& upload,
                          QObject* context,
                          ServerContract::Reply<qint64> reply);
    void head(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Head> reply);
    void download(const QString& documentId,
                  qint64 after,
                  QObject* context,
                  ServerContract::Reply<ServerContract::Snapshot> reply);
    void headers(const QString& documentId,
                 qint64 after,
                 QObject* context,
                 ServerContract::Reply<ServerContract::HeaderChain> reply);
    void upload(const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                QObject* context,
                ServerContract::Reply<qint64> reply);
    void takeLease(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Lease> reply);
    void releaseLease(const QString& documentId, QObject* context, ServerContract::Reply<ServerContract::Done> reply);
    void myDocuments(QObject* context, ServerContract::Reply<QStringList> reply);
};
```

Tasks 21-23 add the access request and pairing calls in the same shape.

**An expired session is signed into again once.** On `Unauthenticated` the connection
signs in afresh and repeats the call once; a second `Unauthenticated` is the answer. Calls
made while a sign-in is in flight wait for it, so a burst of calls costs one sign-in.

**Never log a token, a challenge response, or a request body.** The bodies are
ciphertext, but the habit of logging them survives longer than the ciphertext does.

**Tests, against `FakeServer`:** the first call signs in and succeeds; after the clock
moves past the session length, the next call signs in again and succeeds; a device whose
sign-in the server refuses gets one `Unauthenticated` reply and no retry loop; three calls
made before the first sign-in completes fetch one challenge; every mutating call reaches
the server with a signature `requestVerifies` accepts; a reply whose context was destroyed
while it was held is dropped.

#### 6.4 The file on disk

**Files:** `src/services/DocumentFile.h/.cpp`, `tests/services/DocumentFileTest.h/.cpp`,
`src/services/JsonService.h/.cpp`

**Layout:** a single-line JSON header, a `'\n'` byte, then the raw encrypted payload. The
same bytes are the local file and the base copy, and split at the first newline they are an
upload's header and blob. Reading the header is one `readLine()`, which keeps every version
check cheap.

```cpp
namespace DocumentFile
{

/// One version of a document as written: its header and its ciphertext.
struct Sealed
{
    DocumentHeader header;
    QByteArray ciphertext;
};

/// The header line, '\n', and the ciphertext.
QByteArray encode(const Sealed& sealed);

enum class Read { Ok, NeedsNewerVersion, Malformed };

/// Splits at the first newline and parses the header with DocumentHeader::parse.
Read decode(const QByteArray& bytes, Sealed& out);

/// Builds the next version on previous: recipients and admins resealed from the
/// document's roster by AccessControl::resealAll, a fresh nonce, saveCounter + 1, the
/// writer's keys, the time, the payload hash, and the signature. nullopt when resealing
/// fails.
std::optional<Sealed> seal(const Document& document,
                           const DocumentHeader& previous,
                           const DataKey& dataKey,
                           const DeviceIdentity& writer);

std::optional<QByteArray> readAll(const QString& path);
bool write(const QString& path, const QByteArray& bytes, QString* errorMessage);

} // namespace DocumentFile
```

**`seal` is the only place the save counter increments.** Callers keep the returned
header as their current one. Recipients are rebuilt from the roster on every seal rather
than carried forward, so a mis-sealed entry cannot lock its owner out of every later
version. `generateNonce()` is called on every seal (Task 1.5).

**Serialize the header with `QJsonDocument::Compact`.** The old `JsonService` code uses
`Indented`, which is the natural thing to copy and would break the layout.

**Open files without `QIODevice::Text`.** On Windows it translates `\n` to `\r\n` on write
and back on read; applied to raw XChaCha20-Poly1305 output every 0x0A byte becomes 0x0D
0x0A, and the reverse translation is not an inverse for pre-existing pairs. Every save
would produce a file that fails authentication. `write` uses `QSaveFile`, so a crash
mid-write leaves the previous file intact.

**Opening a local file is a download with no base copy.** The session checks it with
`Reconcile::validateDownload(documentId, std::nullopt, {header}, ciphertext, self, mark)`,
so one statement of the rules - this device's entry and keys, the payload hash, decryption,
`headerMatchesRoster`, the signature, `writerIsAccountedFor`, and the high-water mark -
serves the local file and every download alike. What the user can act on is small:

```cpp
enum class LoadFailure
{
    None,
    NotFound,
    NeedsNewerVersion,
    Refused         ///< failed a check; the reason is validateDownload's
};
```

A shared ward whose local file is missing or refused is downloaded with
`DocumentSession::download` (6.5) and then opened, so a damaged or rolled-back file costs
nothing while the server is reachable.

**`JsonService` becomes plaintext-only.** `toJsonString` and `parseJson` serve the
developer export (Task 26); `loadDocument` and `saveDocument` are deleted with their last
callers in `DocumentManager` (Task 14).

**Tests:** a round trip with two recipients opens for either; a third keypair is refused; a
flipped payload byte is refused; the document id survives a round trip and is covered by
authentication; a header whose signature was replaced by another member's is refused; **two
consecutive seals produce different nonces**, and **two seals from the same previous header
also produce different nonces** - the second catches a counter-derived nonce, which the
first would pass; a ciphertext holding both 0x0A and 0x0D bytes reads back byte-identical
through `write` and `readAll`; `encode` writes exactly one newline before the ciphertext;
`decode` reports `NeedsNewerVersion` for a later header format and `Malformed` for bytes
with no newline.

#### 6.5 `DocumentSession`

**Files:** `src/services/DocumentSession.h/.cpp`, `tests/services/DocumentSessionTest.h/.cpp`

**One open ward: its document, header, data key, and sync state.** `DocumentManager`
(Task 14) owns the one session the window shows and keeps commands, undo, and geocoding.

```cpp
enum class SyncState
{
    Local,              ///< not shared
    UpToDate,
    PendingUpload,
    Unreachable,
    ServerFault         ///< a download failed validation; the local file is untouched
};

class DocumentSession : public QObject
{
    Q_OBJECT

public:
    enum class Edit { Directory, Response };
    enum class Refusal { None, DirectoryReadOnly, RequiresUpdate };
    enum class Resolution { KeepMine, KeepTheirs };
    enum class Download { Done, Unreachable, NeedsNewerVersion, Removed, ServerFault };

    struct Context
    {
        DeviceIdentity identity;
        DocumentRecords& records;
        ServerConnection& server;
        SessionTimers& timers;
    };

    struct Conflict
    {
        QString writerName;         ///< their roster, then the base copy's, then "a former member"
        QDateTime writeTime;
    };

    /// A new, unshared ward, written at once: a first data key, generation 1, and a
    /// roster holding only this person, as admin, with only this device.
    static std::unique_ptr<DocumentSession> create(const Context& context,
                                                   const QString& documentId,
                                                   const DocumentPaths& paths,
                                                   QString* errorMessage);

    /// Opens the local file, which must exist; a shared ward then checks the server.
    static std::unique_ptr<DocumentSession> open(const Context& context,
                                                 const QString& documentId,
                                                 const DocumentPaths& paths,
                                                 LoadFailure& failure);

    /// Fetches a shared ward whose local file is missing or refused, writes it and its
    /// base once it validates, and reports; the caller then opens it.
    static void download(const Context& context,
                         const QString& documentId,
                         const DocumentPaths& paths,
                         QObject* receiver,
                         std::function<void(Download)> done);

    /// Deletes the local file, the base copy, and the record.
    static void forget(DocumentRecords& records, const QString& documentId, const DocumentPaths& paths);

    const Document& document() const;
    const DocumentHeader& header() const;
    bool isShared() const;

    /// Applies change unless refused, then schedules a save. A refusal changes nothing.
    Refusal edit(Edit kind, const std::function<void(Document&)>& change);

    bool flush(QString* errorMessage);  ///< writes a scheduled save now
    void publishNow();                  ///< flush, then publish without waiting on the retry timer
    void checkServer();                 ///< a head check now
    void close();                       ///< flush, release the lease, stop every timer

    bool isDirectoryReadOnly() const;
    bool requiresUpdate() const;
    bool mayWriteDirectory() const;     ///< unshared, or this device holds the lease
    SyncState syncState() const;
    void setEmergencyActive(bool active);

    void share();
    void resolve(Resolution resolution);

signals:
    void documentReplaced();            ///< fast-forward, merge, Keep theirs: undo no longer applies
    void versionAdopted(const DocumentHeader& previousBase, const QList<DocumentHeader>& laterHeaders);
    void syncStateChanged(SyncState state);
    void readOnlyChanged();
    void requiresUpdateChanged();
    void conflictRaised(const Conflict& conflict);
    void saveFailed(const QString& errorMessage);   ///< a local write; the one modal failure
    void shareFailed(ServerContract::ServerError error);
    void removed(const QString& byWhom, bool left); ///< files and record gone; the session is finished
};
```

**Saving and publishing are separate operations on separate clocks.** An accepted `edit`
restarts the `Save` timer; its timeout writes the local file through `DocumentFile::seal`
and `write`. `flush` does the same at once and reports a failure. Every write reaches disk
through this one path - directory commands, response edits, geocoding results, ward
lookups - so the read-only check and the debounce cover all of them. A written save of a
shared ward then publishes.

**Refuse before changing anything.** `edit` checks first and returns a `Refusal` without
calling `change`:

| State | `Directory` | `Response` |
|---|---|---|
| Ordinary | Allowed | Allowed |
| Lease held by another device | `DirectoryReadOnly` | Allowed, saved, published |
| `requiresUpdate()` | `RequiresUpdate` | Allowed, saved locally, not published |

`DocumentManager` routes `undo` and `redo` through `edit(Directory, ...)`, so they are
refused the same way. The write path checks again as a backstop.

**The base version is the whole mechanism.** It is what tells "the server moved" from "I
moved" with certainty rather than by timestamp. **The base moves whenever the local file
takes in a server version, and only then:** a fast-forward, a silent merge, a `resolve`, an
accepted publish, and the registration that shares the ward. The base copy is the exact
bytes of that version, written with `QSaveFile`, and its payload hash goes into the record
beside the base version. Loading it returns `nullopt` when the file is missing or its hash
does not match; it is read without the high-water mark, since it is older than the
document by design.

**"Unpublished changes" is exact:** the local header's save counter is above the base
copy header's. A publish pending when the app closed is therefore pending again when it
opens, and a base copy that fails its hash leaves the local file counted as unpublished,
which at worst publishes it once more.

**On open**, a shared ward checks the head, then:

| Server | Local | Action |
|---|---|---|
| At base | No unpublished changes | Open the local file |
| Ahead of base | No unpublished changes | Download, validate, then replace and move the base |
| At base | Unpublished changes | Open, publish when able |
| Ahead of base | Unpublished changes | The refused-publish route below |
| Unreachable | Either | Open the local file, state `Unreachable` |
| Refuses this device (`NotAllowListed`) | Either | The *Removed* check below |

**Nothing replaces the local file until the download validates**: download the snapshot
after the base version, parse its headers with `Reconcile::readHeaders`, then call
`Reconcile::validateDownload` with the base copy's header and the record's high-water mark.

- `Accept`: write the downloaded file, then move the base and raise the high-water mark;
  emit `versionAdopted` with the old base header and the fetched headers, then
  `documentReplaced`.
- `Removed`: the *Removed* path below.
- `NeedsNewerVersion`: the local file and record stay untouched and `requiresUpdate()`
  becomes true. **It is re-evaluated on every head check**, and clears as soon as the
  server serves a version this build understands; the response records saved meanwhile
  then publish through the ordinary route.
- `ServerFault`: the local file and record stay untouched, the local copy opens, and the
  state is `ServerFault`.

`download` runs the same steps for a ward with no usable local file, with `validateDownload`
checking against the base copy when one passes its hash and directly otherwise.

**Publishing.** One pending upload, never a list: a later save supersedes an earlier one.
A publish is the local file's bytes, uploaded conditional on the base version. A publish
that cannot reach the server is retried on the `Retry` timer, starting at 5 seconds and
doubling to at most 5 minutes, reset by any success - a ward on a phone tether during an
emergency should not lose its connection to a retry storm. Nothing publishes while
`requiresUpdate()` is set or a conflict awaits `resolve`. **A failed publish is a state,
never an error**; only a failed local write emits `saveFailed`.

**Every refused publish takes one route.** The server refuses only when its version moved
past the base, so theirs is newer; a revocation is itself a new version, so a moved key
generation always arrives this way too.

1. **Fetch and validate theirs** as on open. `ServerFault` keeps everything untouched;
   `Removed` goes to *Removed*.
2. **Take theirs** - data key, key generation, and roster exactly as validated. Nothing of
   this session's roster needs carrying over, since every roster change goes through the
   queue (6.6).
3. **Route the content** with `ResponseMerge::mergeDocuments(base, mine, theirs)`:
   - **Equal to theirs** - including when theirs is this device's own publish whose answer
     was lost, since a directory changed identically on both sides merges: adopt theirs as
     a fast-forward and publish nothing.
   - **Merged**: move the base to theirs, seal the merged document on their header, write
     it, and publish it conditional on their version.
   - **`nullopt`, or no usable base copy**: a conflict, below. Nothing publishes this
     session's payload over theirs directly.

**While a conflict waits, the base does not move.** The session seals this session's
document on their header at once, so the local file holds this session's work under the
adopted roster and key, and every later save builds on it; the base stays where it was, so a
crash before `resolve` meets the same conflict on reopen rather than publishing mine over
theirs unasked. Head checks and publishing pause until `resolve`, which moves the base to
theirs and then:

- **Keep theirs**: their directory with the merged response; published only when that
  differs from theirs; `documentReplaced`.
- **Keep mine**: the local file as it stands - this session's directory, the merged
  response, and the adopted roster and key - published conditional on their version. It
  was sealed on their header, so it passes every server rule and never restores anyone
  they removed.

Save mine as a copy is Task 17's: it exports `document()`, then resolves Keep theirs.

**Removed.** Reached from `validateDownload`, and whenever the server refuses this device
as `NotAllowListed` - on open, on a head check, or on a publish. A refused device cannot
read the head or the blob, so the session fetches `headers(after = base version)`, whose
chain ends with the header that removed it, and calls `Reconcile::checkRemoval` with the
base copy's header; only a chain-proven removal is acted on, and anything else is
`ServerFault`. On proof: `forget` the ward, stop every timer, and emit `removed`, naming the
removing member when the held roster has them, with `left` set when the proving header was
this person's own leave from another of their devices. Recorded copies are the library's
(Task 11). No recovery copy is written.

**The lease.** Opening a shared ward takes the lease, refreshed on the `Heartbeat` timer
at a third of its lifetime and released by `close`. `LeaseHeld` makes the directory
read-only (`readOnlyChanged`); the session then tries again on each head check, and a lease
whose heartbeat stopped is taken over when the server lets it expire. **The emergency
response is outside the lease:** response edits are always allowed and always publish, and
a refusal merges through the route above. **The lease cannot speak for a disconnected
device:** while the server is unreachable there is no lease to take, the session is
editable, and reconciliation happens on reconnect.

**Head checks** run on the `HeadCheck` timer, every 5 minutes, and every 30 seconds while
`setEmergencyActive(true)`, so connected responders see each other's records within about
half a minute; `checkServer` runs one at once. A head check is skipped while a publish is in
flight, since the publish's own answer settles the same question. A server ahead of the base
takes the open table's route. **A directory-read-only session fast-forwards** to what the
lease holder publishes, and every replacement of the in-memory document emits
`documentReplaced`.

**Automatic directory updates follow the lease.** `mayWriteDirectory()` is true for an
unshared ward, or a shared one whose lease this device holds. `DocumentManager` starts
geocoding and unit lookups only while it is true, and drops their results when it has
since become false (Task 14).

**Sharing.** `share` first applies the ward's pending `Link` changes (6.6) to the roster,
then registers the local file with `registerDocument` under its existing id; its header,
listing this person's devices, becomes the first allow-list. The returned version becomes
the base, which is what makes the ward shared. The id and paths are unchanged. A refusal
emits `shareFailed`; Task 20 handles `IdTaken`.

**Tests** are in 6.7.

#### 6.6 The roster change queue

**Files:** `src/services/RosterChangeQueue.h/.cpp`, `src/services/RosterChange.h/.cpp`,
`tests/services/RosterChangeQueueTest.h/.cpp`

**Every change to who has access goes through this queue** - grant (Task 22), link and
unlink (Task 23), removal, role changes, and leaving (Task 24) - **and none needs the
lease.** A roster change is never blocked by whoever holds the lease, so a member cannot
hold off their own removal and a thief cannot hold off an unlink.

```cpp
struct RosterChange
{
    enum class Kind { Grant, Link, Unlink, Remove, SetRole, Leave };

    Kind kind;
    QString documentId;
    std::optional<RosterPerson> person;         ///< Grant: the new person
    std::optional<RosterDevice> device;         ///< Link, Unlink
    std::optional<MemberId> target;             ///< Remove, SetRole
    std::optional<RosterPerson::Role> role;     ///< SetRole

    QJsonObject toJson() const;
    static std::optional<RosterChange> fromJson(const QJsonObject& json);
};

enum class RosterChangeOutcome { Published, AlreadyDone, NoLongerApplies, ValidationFailed };

class RosterChangeQueue : public QObject
{
    Q_OBJECT

public:
    RosterChangeQueue(const DeviceIdentity& identity,
                      DocumentRecords& records,
                      ServerConnection& server,
                      SessionTimers& timers,
                      std::function<DocumentPaths(const QString&)> pathsFor,
                      QObject* parent = nullptr);

    void enqueue(const RosterChange& change);
    void enqueueForEveryWard(const RosterChange& change, const QStringList& documentIds);
    QList<RosterChange> pending() const;

signals:
    void pendingChanged();
    void published(const QString& documentId);
    void changeFinished(const RosterChange& change, RosterChangeOutcome outcome);
    void removed(const QString& documentId, bool left);
};
```

**The queue works on the server's version directly and opens no session.** For each
change, one at a time: download the snapshot after the ward's base version, validate it
with `validateDownload` against the base copy (or directly when it has none), apply the
matching `AccessControl` call to *their* document with this device's fingerprint as
writer, seal it on their header, and upload it conditional on their version. On
acceptance it raises the record's high-water mark and emits `published`;
`DocumentManager` then calls `checkServer()` on the open session when it is that ward, and
every other ward's local file catches up by the ordinary route the next time it opens - a
fast-forward, or a silent merge when only its side changed the directory. The queue's own
`Retry` timer runs its backoff.

- **Persisted** in each ward's `DocumentRecord::pendingChanges`, and removed only once its
  publish is accepted or it is dropped. A queue constructed over existing records resumes
  them.
- **One change per publish.** Each change is applied to the server's accepted version and
  published before the next is built, because a version may advance the key generation by
  only one.
- **An unshared ward keeps its changes pending.** Only a `Link` can reach one - the queue
  is how a device joins every ward of its person - and `share` applies it before
  registering, which is the one moment it matters: until then only this device reads the
  file. An `Unlink` of a device with a pending `Link` there drops both.
- **`AlreadyDone`** when the server's version already reflects the change - the person
  already removed, the device already linked.
- **`NoLongerApplies`** when the `AccessControl` call returns `nullopt` against the
  server's version - this person is no longer an admin, or is now the last admin trying to
  leave. Not retried; dropped, and reported so the UI can say why (Task 24).
- **`NotAllowListed`** runs the removal check a session runs. Proven, the ward is
  forgotten and `removed` is emitted; a pending `Leave` finishes as `AlreadyDone` - it is
  how a leave whose answer was lost comes back - and any other change as
  `NoLongerApplies`. Unproven, `ValidationFailed`.
- **Retried** on backoff when the server is unreachable, the upload is refused as a
  conflict, or the ward's current version needs a newer build. **`ValidationFailed`**
  stops that ward's change and reports it.
- **A leave, once accepted,** forgets the ward and emits `removed` with `left` set.

#### 6.7 Tests

**Files:** `tests/services/DocumentSessionTest.h/.cpp`,
`tests/services/RosterChangeQueueTest.h/.cpp`, `tests/TestMain.cpp`

Every test runs devices of a `TestWard` against one `FakeServer`, fires timers by hand,
and holds or loses replies where ordering matters. Each asserts on disk bytes, records, and
the server's state, not only on signals.

**Creating and opening.** A created ward reopens with the same keys, its roster holds
exactly one person, an admin, with exactly this device, and its header carries the given
id; `NotFound`, `NeedsNewerVersion`, and `Refused` each come from a file built to produce
it; a file rolled back to a lower generation, or to a lower save counter in the same
generation, is `Refused` by the high-water mark; a shared ward whose local file was deleted
or corrupted is fetched by `download` and then opens; each row of the open table.

**Saving.** Several edits then one `Save` timeout advance the on-disk save counter by
exactly 1; `flush` writes a scheduled save at once; a `Directory` edit and a `Response` edit
each advance the counter through the same path; a failed write emits `saveFailed` and leaves
the previous file intact.

**Publishing.** A save while the server is unreachable leaves one pending upload, and three
saves still leave one; coming back online publishes once and reaches `UpToDate`; the retry
interval grows from 5 seconds and caps at 5 minutes; a publish pending at close is pending
again when the ward reopens from the same records; with a publish's reply held, a further
edit publishes after it rather than beside it, and a head check fired meanwhile does
nothing; **a publish whose reply is lost is retried, refused, and adopted as this device's
own version, with no conflict and no second publish**; a successful fetch and a successful
publish each write a base copy whose hash matches the record, and a tampered or missing
base copy loads as `nullopt`.

**Downloads.** A server ahead with nothing local fast-forwards and emits `versionAdopted`
and `documentReplaced`; a server serving a version that fails each validation step in turn
- unsealable, hash mismatch, bad signature, broken chain, older generation - leaves the
local file byte-identical, the record unchanged, and the state `ServerFault`.

**The refused-publish route.** A refused publish adopts their key, generation, and roster;
a response-only change on one side and a family edit on the other merge and publish
silently, conditional on their version; **with three devices publishing, two refusals in a
row both merge silently**; a family edit on both sides raises a conflict without publishing
mine, and no head check runs until `resolve`; a member they removed is not restored;
reopening before `resolve` meets the conflict again; **Keep mine** after they granted a
person publishes a version `FakeServer` accepts, holding this session's directory and
still listing their new person; **Keep theirs** holds their directory and the merged
response.

**Removal.** A chain-proven revocation deletes the file, base copy, and record - mid-session,
on open, and on a publish refused as `NotAllowListed`, **including when the revoker was
granted after the base header** - with the proof fetched by `headers(after = base)` alone; a
withheld header, a substituted header, or an allow-list refusal without a proving chain
deletes nothing; a leave from this person's other device reports `left`.

**Read-only and updates.** With a foreign lease, a `Directory` edit is refused without
calling its change and leaves the document and on-disk counter unchanged, while a
`Response` edit saves and publishes; a lease whose holder stops its heartbeat is taken over
after expiry; going unreachable while holding the lease leaves the session editable; a
read-only session fast-forwards to the holder's publish; the head check runs at 30 seconds
while an emergency is active and at 5 minutes otherwise; `close` releases the lease and stops
the heartbeat; a later-format header served by `substituteHeader` sets `requiresUpdate`,
after which a `Directory` edit is refused, a `Response` edit saves locally, and nothing
publishes; a later head check serving an understood version clears it and publishes the
saved response records; `mayWriteDirectory` is false with a foreign lease and true once the
lease is taken.

**Lifetime.** With replies held, destroying the session and then delivering every reply
crashes nothing and writes no file or record.

**Sharing.** Sharing leaves the document id and paths unchanged, records a base version,
and the server holds the ward; a second device granted access opens it; a `Link` queued
while the ward was unshared puts that device in the first header.

**The queue.** With three wards, one open, an unlink reaches all three and the server
refuses the unlinked device on each, and the open session fast-forwards after `published`;
a ward whose lease is held elsewhere still takes a removal; a change the server already
reflects is `AlreadyDone`; a removal by a person demoted meanwhile is `NoLongerApplies` and
not retried; a leave whose reply was lost finishes as `AlreadyDone` and forgets the ward; a
pending change survives recreating the queue over the same records; a ward whose local file
has unpublished directory changes keeps them and holds the new roster when next opened; two
admins granting at once both end up with their person on the roster.

Register `ServerConnectionTest`, `DocumentFileTest`, `DocumentSessionTest`, and
`RosterChangeQueueTest` in `TestMain.cpp`.

---

### Task 7: STOP - user review of the core

**Stop here and hand the core to the user for review.** Do not start Task 8.

Present Task 6 as one artifact: the source files and their tests, in the order of its
sections. The tree does not need to build yet.

Apply the user's changes to the core before anything else. After the user's review, run
the agent code review on the core. Then have the user build and run the tests; **every test
must pass before Task 8 starts**, and a failure is a core defect, fixed in the core and
shown to the user. Commit as `feat: sync-and-save core` once all of this is settled.

---

## Phase 5 - Storage and identity

### Task 8: Settings store

> **Done**, with these differences from the text below:
> - `AppSettings::open(path, errorMessage)` makes the one instance `main()` owns, at
>   `<AppDataLocation>/settings.json`; tests open their own over a `QTemporaryDir`, so
>   there is no global path to set.
> - The library's per-document entry is stored now - ward name, last opened, copy paths -
>   beside the record, and `removeRecord` forgets all of it.
> - The app no longer reopens a document at startup; `lastDocumentId` is kept for Task 11.
> - A failed settings write is logged and the values in memory stand.
> - The built-in server address is empty until the companion server has one.

Before identity, because identity stores a display name in it.

**Files:** `src/services/AppSettings.h/.cpp`, `src/services/SingleInstance.h/.cpp`,
`src/main.cpp`, `src/widgets/MainWindow.cpp:607-668`, `tests/services/AppSettingsTest.h/.cpp`

Settings today live in a hand-rolled JSON file under `QStandardPaths::CacheLocation`
holding `defaultLat` and `defaultLng` (`:644-645`) and `lastDocumentPath` (`:660`). This
task replaces it; **nothing is migrated** from earlier versions, and the old file and its
`MainWindow` helpers (`:607-668`) are deleted.

**`AppSettings` is a JSON file at an explicit path**, `<AppDataLocation>/settings.json`.
It loads the file once into memory, and every change rewrites it through `QSaveFile`, so a
crash mid-write leaves the previous file intact - the high-water mark is security-relevant and must never be half-written. JSON fits the data:
sets of fingerprints and ids, and a record per document id with a list of copy paths, map
directly onto objects and arrays, and the rest of the codebase already speaks
`QJsonDocument`. Nothing goes in `CacheLocation`, which the OS may clear - silently
discarding the high-water mark would disable the rollback defence with no signal.

**`AppSettings` implements `DocumentRecords` (Task 6.2)**, so the core's records persist
without the core knowing where.

**Only one instance of the app runs per user.** The settings file, documents, base copies,
and lease heartbeat all assume a single writer, and JSON has no cross-process locking. At
the top of `main()`, before settings or identity load, take a `QLockFile` in
`AppDataLocation`. If it is held, connect to the running instance through a
`QLocalSocket` named for the application, send the command-line arguments (so a
double-clicked `.wardplanning` file opens there), and exit. The running instance listens
on a `QLocalServer` created with `QLocalServer::UserAccessOption`, so only the same user's
processes can connect, raises and activates its window, and handles the forwarded
arguments. A stale lock left by a crash is taken over (`QLockFile::setStaleLockTime`, plus
`removeStaleLockFile` when the owning process is gone).

**The path is set in one place.** `AppSettings::setFilePath` is called once at startup
with the real path, and once by the test runner with a file inside a fresh
`QTemporaryDir`, so every run starts empty and nothing touches the developer's own
settings.

Three groups of state:

| Group | Holds |
|---|---|
| Device | Display name, the developer-export toggle, the server address override |
| Per document, keyed by **document id** | The `DocumentRecord`; cached ward-list entry; **paths of copies written from it** |
| Map | `defaultLat`, `defaultLng` |

**Per-document state is keyed by document id, not by file path.** Documents move between
machines and are named by id in app data (Task 11), and a path-keyed record would lose the
high-water mark the first time either changed.

`lastDocumentPath` becomes `lastDocumentId`.

**The server address comes from here.** `AppSettings::serverAddress()` returns the
override when one is set and the built-in default otherwise. The default is a compile-time
constant in `AppSettings.cpp`, filled in with the companion server work; the override is an
advanced setting for testing and self-hosting, with no everyday UI. `HttpServerApi`
(Task 13) resolves its base URL through this one call on every request.

**Tests:** a value written is read back through a second `AppSettings` on the same path;
numbers round-trip as numbers; two runs of the suite each start from an empty file; records
for two ids stay separate; a `DocumentRecord` with pending roster changes round-trips; a
malformed settings file is reported, not silently replaced with defaults. By hand:
launching the app a second time raises the first window and exits, and double-clicking a
`.wardplanning` file while the app runs hands it to the running window.

Commit as `refactor: unify settings storage`.

---

### Task 9: KeyStore

**Files:** `src/services/KeyStore.h/.cpp`, `tests/services/KeyStoreTest.h/.cpp`

```cpp
class KeyStore : public QObject
{
    Q_OBJECT

public:
    explicit KeyStore(const QString& account, QObject* parent = nullptr);

    void store(const QByteArray& secret);
    void load();
    void remove();

signals:
    void stored();
    void loaded(const QByteArray& secret);
    void notFound();
    void failed(const QString& errorMessage);
};
```

Service string `"Ward Planning"`, account passed in so tests use their own.
`ReadPasswordJob`, `WritePasswordJob`, and `DeletePasswordJob`, secret as base64.

**One keychain entry holds both keypairs.** The identity is an X25519 pair and an Ed25519
pair, serialised together as one JSON object and stored as a single secret, together with the member id (Task 10), which must outlive cleared application data so a
reinstall keeps the device attached to its person.
Two entries could get out of step - a machine holding a sealing key it cannot authenticate
with is a state with no recovery path and no useful error.

**Distinguish `notFound` from `failed`.** The first is a first run; the second is a locked
keychain or a Linux box with no Secret Service. Conflating them makes a locked keychain
look like a first run and silently mints a second identity, orphaning the user's
documents.

**Tests** need a real keychain: use account `"identity-test"`, remove it in `cleanup()`,
`QSKIP` on `failed()`. A stored secret loads back; a missing secret reports `notFound`; a
stored identity record round-trips both keypairs.

Commit as `feat: KeyStore over qtkeychain`.

---

### Task 10: IdentityService

**Files:** `src/services/IdentityService.h/.cpp`, `src/main.cpp:24-33`,
`tests/services/IdentityServiceTest.h/.cpp`

Owns this device's two keypairs and device name, and the person this device belongs to -
member id and display name - and resolves **before any document exists**. Its product is
the `DeviceIdentity` value the core takes (Task 6.2).

```cpp
class IdentityService : public QObject
{
    Q_OBJECT

public:
    bool initialiseBlocking();          ///< true when an identity is loaded or created

    bool isReady() const;
    std::optional<DeviceIdentity> identity() const;     ///< empty until ready and a person exists

        QString defaultDeviceName() const;          ///< QSysInfo::machineHostName()

    /// A fresh random member id.
    void createPerson(const QString& displayName, const QString& deviceName);
    /// On link: the member id and display name from the responder's handover.
    void joinPerson(const MemberId& memberId, const QString& displayName, const QString& deviceName);

    QString lastError() const;

};
```

**`KeyStore` is signal-based, so `main()` cannot call it and continue.**
`initialiseBlocking()` runs a local `QEventLoop`, quit by whichever of `loaded`,
`notFound`, or `failed` arrives, with a timeout so a wedged keychain cannot hang startup.
On `notFound` it generates **both** keypairs and stores them as one record.

On failure `main()` shows a modal naming the platform store and exits - there is no
document without an identity. Task 14 asks the first-screen question below before
building any service that takes the identity.

**The member id is stored in the keychain record with the keypairs**, so a reinstall or
cleared application data keeps the device attached to its person. Display name and device name live in `AppSettings`. The device name defaults to the
system host name and is offered for editing on the first screen, since "DESKTOP-4F2K9" is
not how Bob knows his laptop.

**Names are fixed once a person exists.** The display name is set when the person is
created or joined, and the device name when the device is; both reach other members only
through the roster, and a rename would be a roster change that no chain rule carries.

**The first screen asks one question** when no person exists yet: *Do you already use Ward
Planning on another device?* **No** calls `createPerson` after asking for a display name.
**Yes** starts linking (Task 23), which calls `joinPerson` once the existing device
accepts. This order is what keeps Bob from becoming two people by tapping Request access on
his phone.

**Tests** use a test constructor, `IdentityService(const DeviceIdentity&)`, that is ready
at once and never reads or writes `KeyStore`: `identity()` is empty before initialisation
and before a person exists; `createPerson` yields a fresh member id; `joinPerson` keeps the
given id; the member id survives a reload of the keychain record. Only `KeyStore`'s own tests use a keychain.

Commit as `feat: device identity service`.

---

### Task 11: The document library

**Files:** `src/services/DocumentLibrary.h/.cpp`, `src/widgets/OpenWardDialog.h/.cpp`,
`src/widgets/MainWindow.h/.cpp` (File menu, `:287,343,351`, `openFile` at `:306`),
`tests/services/DocumentLibraryTest.h/.cpp`

**The app owns the file, and the user never chooses a path.** Documents live in
`QStandardPaths::AppDataLocation`, named by document id. "Open" becomes a list of wards,
not a file dialog.

```cpp
struct LibraryEntry
{
    QString documentId;
    QString wardName;           ///< display label, from the document
    QDateTime lastOpened;
};

class DocumentLibrary
{
public:
    QList<LibraryEntry> entries() const;        ///< sorted by ward name
    QStringList documentIds() const;
    DocumentPaths pathsFor(const QString& documentId) const;
    QString newDocumentId() const;              ///< 128 random bits, hex
    void add(const QString& documentId, const QString& wardName);
    void setWardName(const QString& documentId, const QString& wardName);
    void forget(const QString& documentId);     ///< an unshared ward: its files and entry

    /// Writes a copy under a fresh id and a fresh data key, with originDocumentId set,
    /// and records its path.
    bool exportCopy(const Document& document,
                    const QString& originDocumentId,
                    const QString& targetPath,
                    QString* errorMessage);
        /// Opens a copy read-only.
    std::optional<Document> openCopy(const QString& sourcePath);
    /// After a session's removed(): deletes every recorded copy still at its path and
    /// removes the entry.
    void forgetRemoved(const QString& documentId);
};
```

`ensureFilePath()` (`DocumentManager.cpp:392-407`) and the untitled-document dance go
away: a new ward gets its id and paths up front, then `DocumentSession::create`, so there is
no window in which a document has no home.

**`maybeRenameForWard()` (`:409-443`) is deleted too.** There is no file to rename. In its
place, `DocumentManager` sets the entry's `wardName` from `Document::suggestedFilename()`
whenever the document changes and that is non-empty and differs, so the ward list reads *Tanglewood Ward* at the moment the file
used to be renamed, and the list never has to decrypt a document to show its name.

**Two escape hatches, and no more.** `exportCopy` writes a `.wardplanning` file wherever
the user likes, **sealed with `DocumentFile::seal` under a fresh document id and a fresh
data key, with the original's id as `originDocumentId`** - a fresh key so nobody who once
held the ward's key, such as someone who has since left, can read it - so a copy can never
publish itself into the ward's document; its path is recorded in `AppSettings` under the
original's id. A "Show in folder" action reveals the app data directory.

`openCopy` opens an exported file **read-only** and reports whose roster it carries; it
never becomes the live document and is never added to the library. This replaces Save As entirely,
including its different-roster block: a copy cannot be written back over a shared document
because that operation no longer exists.

**The File menu this task builds**, replacing the old file dialogs at `MainWindow.cpp:287,
343,351`:

| Action | Does |
|---|---|
| **New ward** | A fresh id, then `DocumentSession::create` |
| **Open ward...** | `OpenWardDialog`: the wards from `entries()`, sorted by name, each showing whether it is shared; double-click or Open loads by id |
| **Save a copy...** | File dialog for a target path, then `exportCopy` |
| **Open a copy...** | File dialog for a `.wardplanning` file, then `openCopy`, shown read-only with a banner naming whose roster it carries |
| **Show in folder** | Reveals the app data directory |

`MainWindow::openFile(path)` (`:306`) is removed with its callers; opening by path is
Task 12's routing.

**Tests:** `newDocumentId` produces distinct 128-bit ids and `pathsFor` paths under the test
app data directory; `entries()` sorts by ward name; `exportCopy` output has a document id
different from the original's and from a second export's, the original as its origin, opens
with the same device keys, and is recorded; `forgetRemoved` deletes recorded copies still at
their paths and leaves a moved copy alone;
`forget` removes both the entry and the files; `pathsFor` an unknown id is empty rather than a
guess; a change that identifies the ward changes the entry's `wardName` from *Untitled* to
the suggested name.

Commit as `feat: document library in app data`.

---

### Task 12: File extension routing

> **The extension itself was done alongside Task 0**, at the user's request, and committed
> in `1a222a2`. `src/services/DocumentConstants.h` holds `FileExtension` and `FileGlob`, and
> the installer retires the 1.0.x `.emergencyplan` association. **What remains here is
> `FileOpenRouter`.**

**Files:** `src/services/FileOpenRouter.h/.cpp`, `src/main.cpp:38`,
`src/services/SingleInstance.cpp`, `tests/services/FileOpenRouterTest.h/.cpp`

With documents held in app data the extension governs **exported copies, recovery copies,
and file association** rather than an everyday Open dialog. Files with any other extension,
`.emergencyplan` included, are not the app's files and get no special handling.

**Opening a `.wardplanning` file by path** - a double-click, or a path forwarded by a second
launch (Task 8) - goes through one router that reads the header with `DocumentFile::decode`:

| Header | Action |
|---|---|
| `documentId` is in the ward list | Open that ward by id |
| `originDocumentId` is set (a copy) | `DocumentLibrary::openCopy`, read-only |
| Neither - a document this device does not have, such as a managed file handed over on a USB stick | Offer Request access for its `documentId` (Task 21) |
| Not a readable Ward Planning header | Say the file is not a Ward Planning document |

The same router serves `main.cpp:38` at startup and the running instance's forwarded
arguments.

The bundle identifiers at `CMakeLists.txt:525` and `Info.plist:15` are product identity,
handled in Task 0 - do not touch them here.

**Tests:** the router picks each row above for a file built to match it.

Build, create a document, export a copy, double-click it, and confirm it opens read-only.
Commit as `feat: route documents opened by path through their header`.

---

## Phase 6 - Transport

### Task 13: HTTP server

**Files:** `src/services/HttpServerApi.h/.cpp`, `tests/services/HttpServerApiTest.h/.cpp`,
`tests/services/ServerConformanceTest.cpp`, `CMakeLists.txt`

`HttpServerApi` implements `ServerApi` (Task 6.1) over `QNetworkAccessManager`, in the same
shape as the existing geocoding services (`Qt6::Network` is already linked,
`CMakeLists.txt:30,392`). Each reply arrives from the network's own signal, which keeps
6.1's rule that a reply never comes before the call returns. The base URL comes from
`AppSettings::serverAddress()` (Task 8) on each request, so an override takes effect without
restarting. Signing in and signing requests are `ServerConnection`'s (Task 6.3), so this
class is transport only: routes, bodies, the request signature in a header, and HTTP status
mapped onto `ServerError`, with a failed connection as `Unreachable`.

**Never log a token, a challenge response, or a request body.**

**Tests:** each request and response body round-trips through its JSON form; each HTTP
status the server uses maps to its `ServerError`; a connection refused is `Unreachable`.

**The conformance run.** `ServerConformanceTest` (Task 1.8) runs through `HttpServerApi`
against a real server URL taken from an environment variable - locally, the companion
server under `wrangler dev` with its development clock override enabled. With the variable
unset the run is skipped, so the ordinary suite needs no server. **Every rule the client
depends on is thereby checked against the server that will actually enforce it**, and the
companion server's plan uses the same run as its acceptance gate. A test that passes against
the fake and fails against the real server is a disagreement to resolve in one of them,
never a test to loosen.

Commit as `feat: HTTP server transport`.

---

## Phase 7 - The app on the core

### Task 14: Wire the app to the core

**Files:** `src/main.cpp`, `src/services/DocumentManager.h/.cpp`,
`src/services/EmergencyManager.h/.cpp`, `src/services/QtSessionTimers.h/.cpp`,
`src/widgets/MainWindow.h/.cpp`, `tests/services/DocumentManagerTest.h/.cpp`

**Wiring in `main.cpp`**, in order, after the `DocumentCrypto::initialise()` already at
the top of `main()`: single-instance lock (Task 8), `AppSettings`,
`IdentityService::initialiseBlocking()` (Task 10), **the first-screen question when no
person exists yet**, then `HttpServerApi`, `ServerConnection`, `DocumentLibrary`,
`RosterChangeQueue`, `DocumentManager`, `EmergencyManager`, `TileService`, `MainWindow`,
and the startup file argument through Task 12's router. Every service that takes the
identity is built after it exists, and takes it by value. `DocumentManager` is constructed
before `MainWindow`, both stack locals, so `MainWindow` is destroyed first and
`DocumentManager` outlives it.

`QtSessionTimers` implements `SessionTimers` (Task 6.2) with four single-shot `QTimer`s.

**`DocumentManager` holds the open `DocumentSession`** and keeps what is its own: commands,
undo, geocoding, and unit lookups.

- `executeCommand`, `undo`, and `redo` go through `session.edit(Edit::Directory, ...)`; a
  refusal leaves the undo stack untouched and shows *Another member is editing the
  directory.* or the update notice. `DocumentManager.cpp:97-101` mutates before saving
  today, which is exactly what this replaces.
- **`EmergencyManager` keeps no copy of the response.** Today it holds `m_response`,
  refreshes it only on a full `documentChanged` (`EmergencyManager.cpp:onDocumentChanged`,
  `syncFromDocument`), and every mutation writes all of it back through
  `persistResponseData()`. A replaced document would then be overwritten by the stale copy
  at the next click, and the merge would read the other side's contact attempts as
  deletions. Instead each mutator reads the response from `DocumentManager::document()`
  and changes it in place through `DocumentManager::editResponse`, which replaces
  `setEmergencyResponse` and forwards to `edit(Edit::Response, ...)`, so there is one copy.
  `m_response`, `syncFromDocument`, and `persistResponseData` are deleted, and a
  `wasActive` flag compared on each `documentChanged` emits `emergencyStarted` and
  `emergencyEnded`. `syncFamilies()` goes through the same `edit`.
- `documentReplaced` clears the undo stack, resets the saved position, and emits
  `documentChanged(DocumentChange::full())`, as `setDocument` does today.
- `onFamilyGeocoded`, `onWardLookupComplete`, and `onStakeLookupComplete` apply through
  `edit(Edit::Directory, ...)`. `startBatchGeocoding` and the lookups start only while
  `mayWriteDirectory()` is true, their results are dropped when it has since become false,
  and taking the lease starts a batch for families still lacking coordinates.
- `newDocument`, `openDocument(id)`, `saveDocument`, and `filePath` move onto the library
  and session. `saveDocumentAs`, `saveDocumentOnly`, `autoSave`, `ensureFilePath`,
  `maybeRenameForWard`, `buildUniquePath`, and `JsonService::loadDocument`/`saveDocument`
  are deleted with their last callers. `openDocument` carries `LoadFailure` out rather than
  a `bool` and message; `MainWindow.cpp:59` passes `nullptr` for the message today. A
  shared ward whose file is `NotFound` or `Refused` is fetched with
  `DocumentSession::download` and opened when it arrives.
- The queue's `published` calls `checkServer()` on the open session when it is that
  ward. A session's or the queue's `removed` runs `DocumentLibrary::forgetRemoved` and
  tells the user their access was removed, naming who removed them, or that they left
  from another device.

**`MainWindow::closeEvent`** - there is none in `src/widgets/` today and no `aboutToQuit`
connection - calls `session.close()`, which flushes the pending save and releases the lease.
Opening another ward or a new one closes the current session the same way.

**Ctrl+S** (`MainWindow.cpp:322-328`, a no-op when a path exists) calls `publishNow()`.
Report only a local failure modally: if the flush succeeds and the publish cannot reach the
server, the work is saved, and the sync indicator says the rest.

**Tests:** `DocumentManager` needs a test constructor that takes its `DocumentSession` and
creates no network services, and a reset for `s_instance` (`DocumentManager.cpp:36-37`) in
`cleanup()`. With a foreign lease, a command, `undo`, and `redo` are each refused and leave
the document and undo stack unchanged while an `EmergencyManager` change saves; a
fast-forward clears the undo stack; **after a merged document arrives, an
`EmergencyManager` contact change still holds the other side's attempts**; batch geocoding
does not start without the lease; a geocoding result arriving after the lease is lost
leaves the document unchanged.

Commit as `feat: run the app on the sync-and-save core`.

---

### Task 15: Restore the ward list and missing files

**Files:** `src/services/DocumentLibrary.cpp`, `src/widgets/OpenWardDialog.cpp`,
`tests/services/DocumentLibraryTest.cpp`

When the local ward list is empty, or application data was cleared, fetch
`/me/documents` through `ServerConnection::myDocuments` and add each document this device
is listed on to the library, downloading it with `DocumentSession::download` - with no base copy, so
`validateDownload` checks it directly. When a ward in the list
has no local file, offer *Fetch Tanglewood Ward from the server* and open it, which
downloads it.

**Tests:** clearing the library and settings, then starting, restores every document the
fake server lists for this device and each opens; a listed document whose local file was
deleted is fetched on request.

Commit as `feat: restore the ward list from the server`.

---

### Task 16: The sync indicator

**Files:** `src/widgets/SyncStatusWidget.h/.cpp`, `src/widgets/MainWindow.cpp`

**Quiet and persistent**, in the status bar, following `DocumentSession::syncState()` and
`isDirectoryReadOnly()`: *Up to date*, *Changes not yet uploaded*, *Server unreachable*,
*Could not check the server's copy*, and, while the lease is held elsewhere, *Another member
is editing the directory; you can still record emergency responses.* Nothing for an
unshared ward. It is never a dialog and never steals focus; banners belong to Task 19's
container.

Write the copy to promise only what holds - *while you are connected, one person edits the
directory at a time* - rather than implying a lock that spans an outage.

Commit as `feat: sync status indicator`.

---

### Task 17: Conflict resolution dialog

**Files:** `src/widgets/ConflictDialog.h/.cpp`, `src/widgets/MainWindow.cpp`,
`src/services/DocumentLibrary.cpp`, `tests/services/DocumentLibraryTest.cpp`

**The roster is already settled when this dialog opens.** The session took their key,
generation, and roster exactly (Task 6.5); nothing here touches the roster.

On `conflictRaised`, present three options, naming who saved and when from
`Conflict::writerName` and `writeTime`:

1. **Save mine as a copy** - default, destroys nothing: write the session's `document()` as a
   recovery copy, then `resolve(KeepTheirs)`
2. **Keep theirs**: their directory, with the merged response
3. **Keep mine**: this session's directory, the merged response, and the adopted roster and
   key, published on top of their version

**Recovery copies** go to `QStandardPaths::DocumentsLocation`, named
`<Ward> (unsaved changes yyyy-MM-dd HHmm).wardplanning`, written by `exportCopy` under a
fresh document id with the original as `originDocumentId`, their path recorded like an
exported copy's. `exportCopy` seals to the **full current roster** - after a refused publish,
the adopted one - since a copy is never published, so sealing to one person would buy nothing
and would make the copy unreadable if that machine is what failed.

They land in Documents, where the user can see them, precisely because ordinary documents do
not: a recovery copy is an artifact to deal with, not a document the app manages.
`DocumentLibrary::openCopy` opens one read-only; merging it back is manual and the wording
says so.

**Tests:** a recovery copy opens with every current roster device's key, has a document id
different from the original's with the original as origin, and its path is recorded.

Commit as `feat: conflict resolution dialog`.

---

## Phase 8 - Access UI

Prefer visible buttons over context menus; the audience is casual users.

### Task 18: Access roster dialog

**Files:** `src/widgets/AccessDialog.h/.cpp`, `src/widgets/MainWindow.cpp`,
`src/services/AccessEvents.h/.cpp`, `tests/services/AccessEventsTest.h/.cpp`

Lists **people** with their roles, the current person marked, each expandable to their
devices by device name, each device showing **which device added it and when** - read
from the kept header that linked it (its `lastWriter` and `lastWriteTime`).

**What an admin sees on another person:** **Remove**, **Make admin** or **Make member**,
**Unlink** on each of their devices except their last, and Details (revealing device
fingerprints). A member sees Details only, and a note naming the ward's admins.

**On your own person:** **Link a device**, **Unlink** on each of your devices except this
one (unavailable when you have only one device), **Make member** for an admin stepping
down (disabled for the last admin while anyone else remains: *Make someone else an admin
first*), and **Leave** (Task 24). Invite at the top, for admins.

**People sharing a display name are flagged** in every member's access list - *Two people
are named Bob Wilson* - since the admin who granted the second one may be the person the
warning is meant to catch.

**Every change to who has access is announced, once per sync.** At every
`versionAdopted` (Task 6.5), a pure function, `AccessEvents::between(previousBase,
laterHeaders)`, turns the kept headers since the old base into people-level events - added, removed, left, made
admin, made member - and the banner container (Task 19) shows **one** dismissible notice,
*3 changes to who has access*, expanding to the list: *Bob Wilson added Tom Hale*, *Tom
Hale left*. Names come from the current roster, then the base copy's, then *a former
member*. Linking and unlinking a person's own devices is not announced; it shows in the
list above. This is what makes a hostile admin's actions visible within minutes.

Every version the local file takes in passes through `versionAdopted` exactly once, so
nothing is announced twice and nothing needs a separate record of what was.

**Tests** (`tests/services/AccessEventsTest.cpp`): each event kind is produced from the
header pair that makes it; a leave is `left`, not `removed`; a grant followed by a removal
between two syncs yields both; a device link or unlink yields nothing; a person no roster
names is *a former member*.

Commit as `feat: access roster dialog`.

---

### Task 19: Single key holder notice

**Files:** `src/widgets/MainWindow.h/.cpp`

**In the main window, not the dialog.** The design calls for a persistent notice; a
user with one key holder has no reason to open the access dialog, so putting it there
means it is never seen. Follow the `EmergencyBanner` pattern.

`MainWindow.cpp:97` adds exactly one banner widget above the splitter, so **decide the
stacking** before writing it: an emergency and a single-holder notice can be true at
once. Simplest workable answer is a vertical container holding both, emergency on top.

Persistent and dismissible, not a per-save interruption: *Only you can open this
document. If you lose your devices, it cannot be recovered. Add someone now.* Every ward
starts in this state and some stay there, so a warning firing on every save would be
trained away exactly where it matters most. A person with a single device also gets a
gentler, dismissible suggestion to link a second one.

**While the roster holds exactly one admin and anyone else**, admins see a second
persistent, dismissible notice: *You are the only admin. If you lose your devices, nobody
can add or remove people. Make someone else an admin.*

**While the session's `requiresUpdate()` is set**, a persistent notice: *This ward was
saved by a newer version of Ward Planning. Update to see the latest changes and share your
work.* It follows `requiresUpdateChanged`, and the update check (Task 27) runs at once.

Commit as `feat: ward notices`.

---

### Task 20: Share a document

**Files:** `src/widgets/ShareDialog.h/.cpp`, `src/services/DocumentManager.cpp`,
`src/services/DocumentLibrary.cpp`, `tests/services/DocumentLibraryTest.cpp`

A new document is local and private. **Share** calls `DocumentSession::share` (Task 6.5),
which registers it under its existing id. **If the server refuses the id as taken** -
someone holding a copy of the managed file registered it first - the app gives the
document a fresh id: it seals the current document under that id with the same data key
and roster, writes it at the library's paths for the new id, moves the library entry and
records across, opens the new session, and shares that instead; the squatted id is
abandoned.

What the user gets back is an **invite link** carrying the document id, with a copy
button. It is not secret and grants no access on its own - say so in the dialog, or
someone will treat it as one and be afraid to send it.

An already-shared document shows the link rather than offering to share again.

**Test:** a share refused as `IdTaken` ends with the ward shared under a fresh id, one
library entry for it, and nothing left under the old id.

Commit as `feat: share a document`.

---

### Task 21: Request access

**Files:** `src/widgets/RequestAccessDialog.h/.cpp`, `src/services/ServerConnection.h/.cpp`

Someone opens an invite link, or opens an `.wardplanning` file that carries a document
id. Offer "Request access". One click posts a request to the server carrying this
device's public keys, the member id and display name, and the device name. A device
that already belongs to a person of this document never offers it; that is linking.

**Pending state, with a concrete mechanism:** poll `GET /documents/{id}/requests/mine` -
authenticated by the requesting device, answering *pending*, *granted*, or
*withdrawn* - on a slow interval and when the app regains focus. A non-member cannot read
the head, so without this endpoint pending, refused, and unknown would look the same. The grant can arrive minutes or hours later,
and a volunteer told "install this and you'll see the ward list" needs to see *waiting
for someone to grant access*, not a bare failure.

The request goes to the server rather than beside a file, so a read-only shared folder
is no longer a failure mode. A server that cannot be reached is, and it gets its own
message: the request has not been sent yet, and will be when the connection returns.

**On *granted***, the ward is added to the library and fetched and opened through
Task 15's path.

Commit as `feat: request document access`.

---

### Task 22: Grant access

**Files:** `src/widgets/GrantAccessDialog.h/.cpp`, `src/services/DocumentManager.cpp`

When a ward opens, and each time the app regains focus, an admin's app fetches pending
requests and presents them. A
member's app does not offer to grant; the access dialog names the ward's admins instead.

**Remote requests are not verified** (design, *Granting access*) - anyone holding an
invite link can post one under any name. The dialog shows the name, device name, and when
the request arrived; the device fingerprint is under Details for anyone who wants it. It
does not claim the request is verified.

**Flag a display name that matches a current person:** *Bob Wilson already has access.
If this is Bob's new phone, Bob should link it from his other device instead.* Granting
anyway is allowed, but the default button is Cancel - a second Bob is a separate person,
with none of the first Bob's devices.

**Grant is disabled, with the reason, only while the app is waiting for an update.** Granting
enqueues a `Grant` change (Task 6.6) for a new person holding the requesting device; the server marks the
request granted when it accepts the upload. The accepted upload is what puts the new
device on the server's allow-list, so the grant is complete exactly when the publish is;
until then it shows as pending.

**Test:** granting adds a person, as a member, with one device and a header entry, the server admits
that device as soon as the upload is accepted, and the new device can then fetch and open the document from `FakeServer`; a
request whose display name matches a current person shows the warning with Cancel as
default; a member's app offers no grant, and `FakeServer` refuses one written by a member.

Commit as `feat: grant document access`.

---

### Task 23: Device linking and in-person invite

**Files:** `src/widgets/LinkDeviceDialog.h/.cpp`, `src/widgets/PairingDialog.h/.cpp`,
`src/widgets/InviteDialog.h/.cpp`, `src/widgets/AccessDialog.cpp`,
`src/services/ServerConnection.h/.cpp`, `tests/services/LinkingTest.h/.cpp`

**Pairing is the core's `Pairing` (Task 1.11).** This task builds the flows on it. One
`PairingDialog` serves both roles and both uses: the initiator shows the pairing code and
then the number; the responder takes the code and then shows the number; each has
**Numbers match** and **Numbers differ**. Nothing proceeds until the responder confirms.

**The dialog keeps what it read** (Task 1.11): the responder fetches and stores the
commitment before posting its keys and verifies the later reveal against that stored
copy; the initiator fetches and stores the responder's keys before revealing and computes
its number from that stored copy. Neither re-reads the pairing to check what it already
has - that is what stops a relay searching for matching values once it has seen
everything.

**Linking.** On the new device, the first screen's **Yes** (Task 10) starts a pairing as
initiator: *On a device where you already use Ward Planning, choose Link a device and
enter this code.* On the existing device, **Link a device** takes the code and runs the
responder side. Once the numbers match and its handover is accepted - the server permits the link only
from then - it adds a **link** change for every ward this device belongs to to the
`RosterChangeQueue` (Task 6.6), with the device built from the initiator keys it kept. The new device
receives the responder's signed `Handover` (Task 1.11), checks it against the responder
keys it kept, and calls `IdentityService::joinPerson` with the member id and display name
it carries. It then **polls `/me/documents`**, adding only the wards the handover names
and **only when the header that added its device was written by the device it paired
with** - the writer's verify key equals the responder keys it kept. Nothing about its
identity comes from the server. **Numbers differ**, a failed `revealMatches`, or expiry abandons the
pairing and says to start again.

**Unlinking.** **Unlink** on one of your devices first finds, in the kept headers, every
device that device linked - each header that added a device under this person records its
writer - and asks: *This phone also added "Dell Laptop" on 3 September. Unlink it too?*,
defaulting to **Unlink both**. It then adds an **unlink** change for every chosen device and
every ward to the `RosterChangeQueue`. The unlinked device deletes its copies when its chain check
proves it (Task 6).

**Visible while pending:** the access dialog shows *Adding Pixel 8 to 2 more wards...* or
*Removing Surface Laptop from 1 more ward...* until the queue's `pending()` is empty.

**In-person invite.** The invitee's app starts a pairing as initiator and includes the
pairing code in its access request. The granter picks the request, runs the responder
side in `PairingDialog`, and **Grant** stays disabled until the numbers are confirmed to
match. The grant's device is built from the initiator keys the responder kept, never from
the request's: a request whose keys were swapped in transit then names a device with no
pending request, and the server refuses the grant as `DeviceNotInvited`.

Nothing self-adds: a person is added by an admin, and a device only by its own person.

**Tests:** linking adds the device under the linking device's own person in every document and the
new device opens each; a link to a device already held by another person is refused;
unlinking rotates the key and the unlinked device cannot unseal the new generation;
unlinking a device that itself linked another offers both, and choosing both leaves
neither able to open the next generation; a relay that changes the pairing between
reads is refused by both dialogs; a ward whose adding header was written by a device other
than the paired one is not accepted by the new device; a ward listed by the fake but absent from the
handover is not added; a handover signed by any other key is refused; a grant after a
matching pairing whose request carried substituted keys is refused by the server.

Commit as `feat: device linking and pairing`.

---

### Task 24: Remove, roles, and leave

**Files:** `src/widgets/AccessDialog.cpp`, `src/services/DocumentManager.cpp`,
`tests/services/RemovalTest.h/.cpp`

**Remove.** An admin's Remove on another person confirms, then enqueues a `Remove` change
(Task 6.6), which, on top of the server's current version, runs `AccessControl::remove` - 
the person's roster record and every one of their header entries removed, the data key
rotated and re-sealed for every remaining device - and publishes. The accepted header is
the new allow-list, so the removed person's devices are refused from that moment. **It
does not need the lease**, so the person being removed cannot delay it by holding one.
Nobody else approves and nothing waits.

**Unlink another person's device.** An admin's Unlink on someone else's device - a phone
reported stolen while its owner keeps their laptop - enqueues an `Unlink` change, which
rotates the key the same way.

**Roles.** An admin's Make admin and Make member enqueue a `SetRole` change. Neither
rotates the key: who may read is unchanged.

**Leave.** On your own person, Leave warns that your copies on every device are deleted,
**hides the ward at once**, and enqueues a `Leave` change. The queued change runs
`AccessControl::leave` on top of the server's current version and publishes the version
that drops this person, without rotating the key. **Once it is accepted**, the queue forgets
the ward and `DocumentLibrary::forgetRemoved` deletes its copies - the same deletion a
proven removal does - and your other devices do the same when their chain check proves
the leave. While the server is unreachable the ward stays hidden and the change waits.
If the change no longer applies (`NoLongerApplies`, Task 6.6), the ward reappears.

**Every leave is the same, including the last person's**: with nobody else remaining, the
published version lists no one, so the server lists the ward for no device and it does
not come back after a reinstall. Leave therefore replaces the separate "Remove from this
computer" action for a shared document (`DocumentLibrary::forget` still serves an
unshared one).

**The last admin names a successor.** When the last admin presses Leave while anyone else
remains, the app asks *Before you leave, choose who will be admin*, listing the others; one
confirmation enqueues a `SetRole` promoting them and then the `Leave`. The same picker
appears when a leave is dropped because this person has meanwhile become the last admin.

**Until the upload is accepted the change is shown as pending** and retried through the
queue; acceptance is the moment it takes effect on the server.

**The dialog says what removal does and does not do.** Future versions are closed to
that person the moment the upload is accepted, and their app deletes its local
copy the next time it connects (Task 6). A copy they took out of the app beforehand,
or a device that never connects again, keeps what it had.

**Test:** after an admin removes a person, every device of that person is refused by
`FakeServer` and cannot unseal the new generation, while every remaining device can, and
`verifyChain` from each removed device's base header returns `Removed`; a removal written
while the target holds the lease still publishes; a member's app offers no Remove, and a
removal written by a member is refused by `FakeServer`; an admin removes another admin; an
admin unlinks another person's device and that device's chain returns `Removed` while the
person's other device stays a member; promoting, demoting, and stepping down publish and
change nothing about who can read; the last admin's Make member is disabled while others
remain; Leave hides the ward at once and deletes its files only once the leave is
accepted, keeping the key generation, and every remaining member opens the result; a
leave queued while the server is unreachable publishes when it returns; two admins
leaving at once leave one ward whose second leaver sees the successor picker; the last
admin's Leave with others remaining promotes the chosen successor, then leaves; the last
person's Leave publishes a header listing no one, after which `FakeServer` lists the ward
for nobody.

Commit as `feat: remove, roles, and leave`.

---

## Phase 9 - Messages, update notice, and documentation

### Task 25: Failure path messages

**Files:** `src/widgets/MainWindow.cpp:56-69,592-595`

One message per `LoadFailure` and per `ServerError`, per the design's table. Load
failures are modal; sync states are not.

Three behaviour changes:

- `MainWindow` auto-loads the last document at startup and on failure **silently clears
  the stored path** (`:68`), so the user finds an empty window and is told nothing.
  Startup now restores the last document from the ward list by id; a failure is
  reported and the record left alone.
- A failed local save (`DocumentSession::saveFailed`, relayed by `DocumentManager` in
  place of `autoSaveFailed`) reaches a transient `statusBar()->showMessage(...)`
  (`:592-595`) that the next status message overwrites. A failed **local** save becomes a modal
  error - a user must not keep working while nothing is being written down.
- A failed **publish** is never modal. It is the sync indicator's job.

Commit as `fix: explain document load and save failures`.

---

### Task 26: Developer JSON export

**Files:** `src/widgets/MainWindow.cpp`

A menu action, off by default behind an `AppSettings` toggle, writing plaintext JSON
via the existing `toJsonString`. It warns that the output contains unencrypted member
data and defaults to a location outside any synced folder. This is how you inspect a
document when helping a user.

Commit as `feat: developer decrypted JSON export`.

---

### Task 27: Update notice

**Files:** `src/services/UpdateChecker.h/.cpp`, `src/main.cpp`, `CMakeLists.txt`,
`src/widgets/MainWindow.h/.cpp`, `tests/services/UpdateCheckerTest.h/.cpp`

The server address is built into the app, so a move to a new domain reaches members
through a release. Members on an old version need to hear about it.

**The check reads GitHub Releases, independent of the Ward Planning server.** An app
pointed at a server that has gone away still learns a new version exists, which is the
case the notice is for. `GET https://api.github.com/repos/<owner>/<repo>/releases/latest`,
comparing `tag_name` (leading `v` stripped) against the running version with
`QVersionNumber`. Draft and prerelease entries are excluded by that endpoint already.
Use the repository's post-rename name from Task 0; GitHub redirects a renamed
repository, but the check should not depend on that.

**The running version comes from CMake:** `main.cpp` already sets it from
`WARD_PLANNING_VERSION`.

**Cadence:** on startup and then at most once a day while running, the last check time
kept in `AppSettings`. Silent on any failure - no network, rate limiting, malformed
JSON - since the next check tries again and a failed update check is never the user's
problem.

**The notice is a banner**, in the container Task 19 introduced, below the emergency
banner: *A new version of Ward Planning is available.* with **Download**, opening the
release page through `QDesktopServices::openUrl`, and **Not now**, which hides it until
the next newer version. The app installs nothing itself; the installer the user already
knows does that.

**When the server has been unreachable for a day or more and a newer version is
known**, the sync indicator's *Server unreachable* state adds *a newer version is
available* - the pairing that most likely means the service moved.

Give `UpdateChecker` an injectable fetch - a function from URL to reply bytes or
failure - so its tests need no network.

**Tests:** a newer tag raises the notice; an equal or older tag does not; `v1.10.0` is
newer than `v1.9.0`, which is the comparison string ordering gets wrong; a malformed
response and a network failure are silent; a check within a day of the last one is
skipped; "Not now" on 1.2.0 stays hidden for 1.2.0 and reappears for 1.3.0.

Commit as `feat: notify when a newer version is available`.

---

### Task 28: Documentation

**Files:** `docs/USER_GUIDE.md`, `docs/ARCHITECTURE.md`, `docs/BUILDING.md`,
`docs/DESIGN_GUIDE.md`, `README.md`, `CLAUDE.md`

- **USER_GUIDE**: sharing a document and sending the invite link; granting and removing
  access; admins and members, and that any admin can grant, remove, and change roles
  alone; linking your phone or another computer, and unlinking a lost one; why every
  roster change is announced; what the single-holder and single-admin notices mean; what the sync indicator's states mean; that the app keeps
  working with no network and uploads when it returns; that during an outage responders
  cannot see each other's work until it returns; and - prominently - **that access must
  be granted before an emergency, because granting needs the server.** Also the update
  notice, and that installing new versions keeps the app reaching the service.
- **ARCHITECTURE**: the new services, the single write path, save versus publish, the
  removal of the archive overlay.
- **README**: new product name, extension, and dependencies.
- **CLAUDE.md**: remove the Active Tasks row.

Also state in the user guide that `ReportGenerator` writes plaintext PDFs to a
user-chosen path, and that emergency archives are gone with the summary PDF as the
record.

Commit as `docs: encrypted shared documents`.

---

## Verification before calling it done

`FakeServer` exists only inside the test executable, so it covers the automated tests,
not these checks in the running app.

- **No server needed** - check as the phases land: items 1, 8, and 11 (item 11 reaches
  only GitHub).
- **Companion server needed:** items 2-7, 4a, 4b, 4c, 9, and 10. Their behaviour is already
  covered against `FakeServer` by the automated tests; these runs confirm it end to end.

Ask the user to confirm, in the running app:

1. A new document is created, saved, and reopened from the ward list.
2. Sharing a document produces an invite link.
3. A second identity (a second Windows account) opens the link, requests access, and is
   granted it; both can then edit.
4. An admin removes a person; the removed person can no longer open the document, and
   their local copy is deleted with a message the next time that account's app connects.
   A member has no Remove. An admin makes a member an admin, and the new admin can then
   grant. Every member sees the changes announced in one notice.
4b. A member leaves; their copy disappears at once and is gone once the leave lands, the
   others still open the ward, and the leaver's second device says they left. The last
   admin leaving is asked to choose a successor. The single-admin notice shows while one
   admin remains.
4c. An admin unlinks another person's lost device; that device's copy is deleted on its
   next connection and the person's other device keeps working.
4a. A person links a second device (a second Windows account) by comparing the pairing
   number;
   both open the document; unlinking the second from the first deletes its copy on its
   next connection and leaves the first working.
5. Two identities editing at once produce a conflict dialog, and "Save mine as a copy"
   leaves both sets of work intact.
6. Two admins granting access at the same time both end up with their person on the
   roster, and both
   grantees can open the document.
7. With the network disconnected, both identities record contact results during an
   emergency; reconnecting keeps every contact attempt from both.
8. Starting an emergency, ending it, and generating the summary PDF all work with no
   archive prompt.
9. Clearing application data and reopening restores the ward list from the server and
    downloads the documents again.
10. Uninstalling and reinstalling keeps the identity and its access.
11. A build whose version is below the latest GitHub release shows the update notice,
    and Download opens the release page.

Do not claim the feature works until the user reports these results - this is exactly
the kind of feature whose failures are invisible in unit tests.

---

## Known limits carried from the design

- Document content is never merged. Ordinary conflicts are resolved by choosing a
  version; roster grants merge; emergency response records are inserted on reconnect.
- Responders cannot see each other's work while the network is down. Local-network peer
  sync would close this and is separate future work.
- Revocation governs future versions and cleans up the app's copy. The removed
  member's app deletes its local copy once the header chain proves the removal; copies
  taken out of the app, and devices that never reconnect, are beyond reach.
- One admin can remove anyone. A compromised admin device can remove every other person;
  every removal is announced, and recovery is a new ward built from the directory PDFs - 
  tags, teams, assets, special needs, notes, and past emergencies are lost.
- Remote access requests are not verified; an admin can grant one sent by whoever holds a
  forwarded invite link. In-person grants compare a number.
- A stolen, unlocked device acts as its owner, with its owner's role; unlinking it offers
  to unlink what it added, and otherwise any admin removes it.
- A leaver knows the data key until the next rotation, since leaving does not rotate; the
  server refuses them every later version, and a leaver colluding with the operator is
  outside what the design defends.
- Access must be granted before an emergency, because granting needs the server.
- Rollback marks live in application data. After it is cleared, the first version the
  server serves is accepted as it stands, so a malicious server could serve one older
  version once.
- A copy exported before a removal still opens read-only on the removed device; removal
  deletes the copies at their recorded paths, and a moved copy is out of reach anyway.
- Reports stay plaintext.
- A hostile server operator could keep members on separate copies without them noticing;
  nothing is exposed, and each member keeps their own work.
- Emergency archives are removed; the summary PDF is the surviving record.
- There is no folder-based sharing mode. Continuity if the server stops rests on the
  project being open source, the format being documented, and every member holding a
  complete file. A successor restores the service at the same address, or ships a
  release with a new one; documents carry only their id and follow either way.
- The design stays classical rather than post-quantum; a hybrid entry kind would ship as
  a new format version.
- A version written by a newer app is refused; until the server again serves an understood
  version, or the app is updated, the directory stays read-only and nothing publishes,
  while emergency response edits save locally.
- The rollback check follows the server's versions: a local file rolled back to an
  unpublished save above the base goes unnoticed, and an unshared ward has no rollback
  check at all.
