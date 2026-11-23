# Ward Planning server

The server the app shares wards through: a Cloudflare Worker with one SQLite-backed Durable Object
per ward. Its wire is specified in `docs/SHARED_WARD_DESIGN.md` under *What the server is* and
*The live channel*, and the header it reads in `docs/FORMAT.md`. The server holds ciphertext and
fingerprints only.

## Layout

- `src/index.ts`: the Worker. Limits each address, serves `/releases/latest`, `/download`, `/open`
  and `/installer/{platform}`, and hands each `/documents` call to the ward's object.
- `src/ward.ts`: the `Ward` Durable Object, one per ward: the current version, replaced only by a
  higher-numbered one, and the live channels.
- `src/wire.ts`: base64url, the header, a version's framing, the signer, the signed lines, the
  routes.
- `src/download.ts`: the download page.
- `src/join.ts`: the page an invitation link opens, and the installer it downloads. The
  invitation is the part of the link after the `#`, which no browser sends, so the page is the
  same for every invitation: its script names the download after the ward and the invitation,
  as `WardPlanning-Tanglewood-<invitation>.exe`, and the installer reads the invitation from
  its own file name. The ward's name follows the invitation in the link, for the person to
  read. `/installer/{platform}` passes the release's file through from where `releases.json`
  says it is, so this site can name the download.
- `src/releases.json`: each platform's current release.
- `test/`: Vitest tests that run inside workerd.
- `scripts/live.ts`: a check of a deployed server, run with `npm run live`.

## Developing

From `server/`:

- `npm install`
- `npm run check`: generates `worker-configuration.d.ts` from `wrangler.jsonc`, then type-checks.
- `npm test`: runs every test locally; nothing reaches Cloudflare.
- `npm run dev`: serves the Worker at `http://localhost:8787`, with local storage under
  `.wrangler/`.

## Deploying

`wrangler.jsonc` configures one Worker, the development Worker `ward-planning-dev`.

- `npx wrangler login`, once per machine, as an account member.
- `npm run deploy` publishes it at `https://ward-planning-dev.oss-storage.workers.dev`.
- `npm run live -- https://ward-planning-dev.oss-storage.workers.dev` runs one ward
  through the deployed server: a create, a fetch, a channel held through a push, and a delete. It
  prints a line per check, and leaves that one deleted ward on the server.
- `npx wrangler tail` streams its logs.

## Statuses

The app asks only whether a call succeeded; the codes are for the logs. Each call gives the first
status in its list that applies.

Every call:

- `404`: the method and path are not a call.
- `429`: a create or a read from an address over its limit, which is 10 creates and 3000 reads a
  minute.

A create (`POST /documents`):

- `400`: the body is not a version, or a signing header is malformed.
- `409`: the id exists.
- `403`: the signature does not verify, the signer is not the header's `writer`, or the header
  does not name the signer.
- `201`: stored.

A push (`PUT /documents/{id}`):

- `400`: the body is not a version, a signing header is malformed, or the header names another
  document.
- `404`: the server has no such ward.
- `403`: the signature does not verify, the signer is not the header's `writer`, or the current
  header does not name the signer.
- `409`: the number is not above the current one.
- `200`: stored.

A read (`GET /documents/{id}` and `GET /documents/{id}/head`):

- `404`: the server has no such ward.
- `200`: the version, or its header line, with `Ward-Published`.

A channel open (`GET /documents/{id}/channel`):

- `400`: the request is not a WebSocket upgrade, or a signing header is malformed.
- `404`: the server has no such ward.
- `403`: the signature does not verify, or the current header does not name the signer.
- `101`: opened.

The pages, the release list and the installer:

- `GET /releases/latest` and `GET /download`: `200`. Neither counts against a limit.
- `GET /open`: `200`, the same page for every invitation.
- `GET /installer/{platform}`: `404` for a platform `releases.json` does not list, `502` when the
  release's file cannot be had, `200` with the file. It counts against the address's read limit.

## Where the data is

Each ward is one `Ward` object, named by its document id (`idFromName`), in the Worker's `WARD`
namespace. It keeps one storage value, `current`: the version exactly as written (header line,
`\n`, payload), the header's length, what the server read from the header (document, number,
writer and the fingerprints sealed to) and the publish time.

## Releasing the app

A release is stated once, as its platform's entry in `src/releases.json`. The app's build reads
its version and number from the `windows` entry (`CMakeLists.txt`), so a release goes out in
this order: edit the entry, build the app and its installer, publish the installer where `url`
points, then `npm run deploy`.

```json
{
    "windows": { "name": "Windows", "version": "2.0.0", "number": 7, "url": "https://.../setup.exe" }
}
```

`number` counts the releases: each release's is one above the last. A running app offers the
listed release as an update when its number is above the app's own.

`/releases/latest` serves the file as it is, and `/download` links each entry by its `name`. With
no entries the page says no installer is available.

The `windows` entry is also the installer the join page hands to people opening an invitation
link, and the page promises what that release's installer does: it reads the invitation from
its own file name. So a release and its `releases.json` entry are deployed together.

## The conformance suite

The app's tests hold one suite of what a server must do, `tests/server/ConformanceTest.cpp`. It
runs against the app's in-process fake server in every test run, and against a real server when
`WARD_PLANNING_SERVER_URL` names one. For the local server `npm run dev` serves, whose wards stay
on this machine under `.wrangler/`:

```
set WARD_PLANNING_SERVER_URL=http://localhost:8787
build\bin\WardPlanningTests.exe
```

For the deployed development Worker, set the address to
`https://ward-planning-dev.oss-storage.workers.dev`.

Each run makes eight create calls and leaves three wards on the server, one of them deleted. A
deployed server allows ten creates a minute from one address, so a second run within the minute
is refused with `429`. A disagreement between the two servers is fixed in whichever is wrong.

## Moving to another host

A server that takes over at the same domain implements the wire in *What the server is* and *The
live channel*, which the conformance suite checks, and holds every ward's `current` value. Reading those values out of this server
takes an export, which is still to be built.
