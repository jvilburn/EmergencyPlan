# Ward Planning server

The server the app shares wards through: a Cloudflare Worker with one SQLite-backed Durable Object
per ward. Its wire is specified in `docs/SHARED_WARD_DESIGN.md` under *What the server is* and
*The live channel*, and the header it reads in `docs/FORMAT.md`. The server holds ciphertext and
fingerprints only.

## Layout

- `src/index.ts`: the Worker. Limits each address, serves `/releases/latest` and `/download`, and
  hands each `/documents` call to the ward's object.
- `src/ward.ts`: the `Ward` Durable Object, one per ward: the current version, replaced only by a
  higher-numbered one, and the live channels.
- `src/wire.ts`: base64url, the header, a version's framing, the signer, the signed lines, the
  routes.
- `src/download.ts`: the download page.
- `src/releases.json`: each platform's current release.
- `test/`: Vitest tests that run inside workerd.

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
- `npm run deploy` publishes it at its `workers.dev` address.
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

## Where the data is

Each ward is one `Ward` object, named by its document id (`idFromName`), in the Worker's `WARD`
namespace. It keeps one storage value, `current`: the version exactly as written (header line,
`\n`, payload), the header's length, what the server read from the header (document, number,
writer and the fingerprints sealed to) and the publish time.

## Releasing the app

Add or edit the platform's entry in `src/releases.json`, then `npm run deploy`:

```json
{
    "windows": { "name": "Windows", "version": "2.0.0", "url": "https://.../setup.exe" }
}
```

`/releases/latest` serves the file as it is, and `/download` links each entry by its `name`. With
no entries the page says no installer is available.

## Moving to another host

A server that takes over at the same domain implements the wire in *What the server is* and *The
live channel*, and holds every ward's `current` value. Reading those values out of this server
takes an export, which is still to be built.
