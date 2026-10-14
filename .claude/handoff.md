# Starting point: the shared ward design

A design document, not code. Nothing has been implemented.

## State

- Branch `design/shared-ward`, off master at `e7a9faa`. Nothing committed.
- The design: `docs/SHARED_WARD_DESIGN.md`, about 1080 lines, ASCII only. Untracked.
- Master holds none of the encrypted-document work: no `DocumentCrypto`, `DocumentSession`,
  `ServerContract`, `Reconcile`, `DocumentLibrary`, `IdentityFile`, `InviteLink`,
  `UpdateChecker`. What exists is `Command`, `CommandHistory`, `DocumentManager`,
  `EmergencyManager`, the import and family commands, and the models.
- Branch `docs/encrypted-shared-document` holds an earlier implementation of a different
  design. It is **not** the starting point. It is readable as reference and nothing more;
  the owner judged it too convoluted and chose to start from master.
- `.claude/settings.local.json` has two uncommitted allow rules. Leave them out of commits.

## What the design says

A ward is one encrypted document that carries its own access list, and the people holding
it open see each other and each other's work.

- **The server** stores the current version of each ward plus a version number that rises
  with it, relays a notice when it changes, and reports who holds it open. It knows
  fingerprints, sizes and publish times, and nothing about people. Five calls plus a live
  channel. A write is signed and conditional on the version it was built from; the server
  accepts it only if the current header names the signer's fingerprint.
- **The access list** lives inside the encrypted payload: people, their devices' public
  keys, roles, and open invites. It is also the set of keys the data key is sealed to,
  which is why it must hold public keys - the header outside holds only one-way
  fingerprints, so nothing could be sealed to anyone without it.
- **Every change is a command.** Content edits, imports and access changes alike. Commands
  enter a journal on disk and leave it when the version carrying one is on the server, so
  "there are unpushed changes" is "the journal is not empty."
- **Replay is the only answer to two people at once**, on the `git rebase` model: fetch the
  current version, replay each journal entry onto it, push. A command applies when the
  values it saw are still there, or when its result already holds. Anything else is a
  conflict.
- **One conflict abandons the whole replay.** The server's version is taken as it stands,
  the journal is kept, and the person redoes their work from it. No partial application.
- **Joining** is a one-time invite link. **Removing** is one write without that person's
  devices, which rotates the data key.

## Decisions reached in discussion, which the document states but does not argue

Do not re-propose these. Each was settled deliberately, several after the owner corrected
a worse proposal of mine.

- **Replay, never reconciliation.** Comparing two finished versions field by field is
  rejected: it produces a state nobody authored and needs a rule per pair of shapes. I
  drifted into designing that twice and was pulled back twice.
- **Emergency responses are a keyed collection**, not one optional value. Two responders
  who each start one offline get two events, and the app does not pretend to know they were
  the same emergency.
- **Rotation belongs to publishing, not saving.** A local save seals to the current access
  list without rotating. This is what lets the base copy go: the local file is always under
  the key of the last version this device took from or gave to the server, so there is one
  trusted key and an incoming version descends from it.
- **No base copy, no save counter, no debounce.** Each had exactly one job that something
  else now does.
- **A command is immutable once constructed.** Start condition, result and sentence are
  fixed by the constructor. Three existing commands read their previous state inside
  `execute` and must stop: `SetFamiliesCommand`, `ImportWardDirectoryCommand`,
  `ImportMinisteringCommand`.
- **A refusal is not a conflict.** Replay runs the app's own rules (the only admin cannot
  step down, the last person cannot leave) and reports them in the app's own wording, so
  the person is told to promote someone rather than to redo work that would be refused
  again.
- **Automatic updates are not journalled.** Geocoding and unit lookup are derived data,
  never a person's work to redo, so a lost push means the next batch recomputes them.
- **The term is "access list."** "Sealing set" and "roster" were both dropped.
- **No delete call on the server.** The operator cannot read the ciphertext, so holding it
  is already accepted; pruning is an operator action outside this interface.

## Where the review stands

Two read-only reviews have run against the document. The first was discarded after the
design changed under it. The second produced 24 items, worked through one at a time.

**Settled:** C1 (replay must not re-execute a command that captures its own expectation),
C2 (the descent check without a base copy), C3 (making every write a command is the core of
the work, and the document now says so), I4, I5 and S2 (moot - nothing exists to remove),
I6, I7, I8, I9, I10, I11, I12, M7, S1.

**Open:** I13, and M1, M2, M3, M4, M5, M6, M8, M9.

## The live thread: I13, paused mid-discussion

I13 is test gaps. Four are agreed and unwritten:

- pin the journal's sentences exactly, since the document claims it reads as the person's
  own list
- state the empty-journal fast-forward in the Replay prose, not only in the fake-server list
- add a push attempt from an exported copy, since the document claims a copy can never push
  into the ward
- add the lost-reply case: the push succeeded but its answer never arrived, so the entry
  must leave the journal when the device next sees that version

What stopped us is the fifth. The reviewer wanted the "a journal of only already-satisfied
commands applies in full" test to assert more, and three shapes were tried and rejected:
byte equality (impossible - every write draws a fresh nonce), a ladder of pre- and
post-assertions (the owner: "this is getting confusing"), and a no-push-when-unchanged rule
to make the assertion meaningful (the owner: a replay that changes nothing is exceptionally
rare, so the rule is not worth it).

Where it landed, and what to resume from: **the oracle is that the local document equals
the server's document**, which holds after any sync, whether the replay applied or was
abandoned. Each test then needs that plus whether the server's document holds this person's
work with the journal empty, or does not with the journal intact. The owner asked to discuss
this carefully rather than apply it, so **do not rewrite the test list without taking that
discussion up first.**

## How the owner works

He reads for structure and readability: "if the code is simple and readable it is more
likely to be correct." Prefer deletion to mechanism. The data is names, addresses and birth
dates; the threat is a lost phone and a mistaken invite, and "if this were credit card
processing, then every hole needs to be patched. It isn't."

He will stop you when a fix starts growing. Three things to avoid, each of which he had to
call out in this session:

- **Layering.** One wrong word in a failure message grew into a server call, a new link
  shape, a recovery flow and a new accepted limit before he said "sounds like another
  layer." The fix was one line.
- **Invented examples.** I justified a debounce with "a responder clicking through fifty
  families," which modifies nothing and runs no commands. He said "get real about your
  examples."
- **Claiming the code already does something.** I told him the journal needed nothing new
  because the write paths were already commands. Three of four were not.

Discuss one item at a time, lead with a one-line summary of the issue and your own proposed
fix rather than the reviewer's, and wait. Use Opus 5.5 for subagents and the read-only
reviewer for reviews. The rest of his preferences load from CLAUDE.md and its memory files.

## Next step

Resume the I13 discussion from the local-equals-server oracle above. Then the remaining
minor items. Nothing in the document is committed yet.
