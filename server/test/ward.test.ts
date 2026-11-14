import { describe, expect, it } from "vitest";
import { channelLine, encode, fingerprintOf, readVersion, writeLine } from "../src/wire";
import {
    Device,
    connect,
    created,
    everyone,
    finalVersion,
    framed,
    get,
    head,
    lineOver,
    newDevice,
    newDevicesInOrder,
    newDocument,
    opened,
    post,
    put,
    random,
    refusal,
    send,
    signed,
    stored,
    version,
} from "./clients";

/** The number of the version the server holds, read from its head. */
async function numberOf(document: string): Promise<number>
{
    return (await (await head(document)).json<{ number: number }>()).number;
}

/** How long ago, in milliseconds, the answer says the current version was published. */
function ageOf(answer: Response): number
{
    const published = answer.headers.get("Ward-Published")!;
    expect(published).toMatch(/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/);
    return Date.now() - Date.parse(published);
}

describe("create", () =>
{
    it("stores the first version, signed by the writer its header names, and serves it back byte for byte", async () =>
    {
        const alice = await newDevice();
        const document = newDocument();
        const body = version(document, 2, alice, [alice]);

        expect((await post(alice, body)).status).toBe(201);

        const whole = await get(document);
        expect(whole.status).toBe(200);
        expect(new Uint8Array(await whole.arrayBuffer())).toEqual(body);
        expect(ageOf(whole)).toBeGreaterThanOrEqual(0);
        expect(ageOf(whole)).toBeLessThan(5000);

        const top = await head(document);
        expect(top.status).toBe(200);
        expect(await top.text()).toBe(new TextDecoder().decode(body.subarray(0, body.indexOf(0x0a))));
        expect(top.headers.get("Ward-Published")).toBe(whole.headers.get("Ward-Published"));
    });

    it("refuses an id that exists with 409, before checking who signed it", async () =>
    {
        const alice = await newDevice();
        const mallory = await newDevice();
        const document = await created([alice]);

        expect((await post(alice, version(document, 9, alice, [alice]))).status).toBe(409);
        expect((await post(mallory, version(document, 9, mallory, [mallory]))).status).toBe(409);
        expect((await post(mallory, version(document, 9, alice, [alice]))).status).toBe(409);
        expect(await numberOf(document)).toBe(2);
    });

    it("refuses a signer its header does not name, a signer that is not its writer, and a signature over another line, with 403", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = newDocument();
        const body = version(document, 2, alice, [alice]);

        expect((await post(bob, body)).status).toBe(403);
        expect((await post(bob, version(document, 2, bob, [alice]))).status).toBe(403);
        expect((await post(bob, version(document, 2, alice, [alice, bob]))).status).toBe(403);
        const pushLine = await signed(alice, writeLine("push", readVersion(body)!));
        expect((await send("POST", document, pushLine, body)).status).toBe(403);
        expect((await head(document)).status).toBe(404);
        expect((await post(alice, body)).status).toBe(201);
    });

    it("refuses a verify key that is not a point on the curve with 403", async () =>
    {
        const stray: Device = { ...(await newDevice()), verifyKey: new Uint8Array(32).fill(0xff) };
        stray.fingerprint = encode(fingerprintOf(stray.publicKey, stray.verifyKey));
        const document = newDocument();

        expect((await post(stray, version(document, 2, stray, [stray]))).status).toBe(403);
        expect((await head(document)).status).toBe(404);
    });

    it("refuses a body that is not a version with 400, though it is signed", async () =>
    {
        const alice = await newDevice();
        const document = newDocument();
        const good = version(document, 2, alice, [alice]);
        const goodLine = new TextDecoder().decode(good.subarray(0, good.indexOf(0x0a)));
        const newerFormat = framed(goodLine.replace('"format":1', '"format":2'), random(64));
        const extraKey = framed(goodLine.replace('"format":1', '"format":1,"extra":true'), random(64));
        const withoutNewline = new TextEncoder().encode(goodLine);

        for (const body of [newerFormat, extraKey, withoutNewline])
        {
            const headers = await signed(alice, lineOver("create", document, body));
            expect((await send("POST", document, headers, body)).status).toBe(400);
        }
        expect((await head(document)).status).toBe(404);
        expect((await post(alice, good)).status).toBe(201);
    });

    it("refuses a malformed or missing signer with 400", async () =>
    {
        const alice = await newDevice();
        const document = newDocument();
        const body = version(document, 2, alice, [alice]);
        const headers = await signed(alice, writeLine("create", readVersion(body)!));

        for (const broken of [
            { ...headers, "Ward-Signer": encode(alice.verifyKey) },
            { ...headers, "Ward-Signature": headers["Ward-Signature"].slice(1) },
            { "Ward-Signer": headers["Ward-Signer"] },
        ])
        {
            expect((await send("POST", document, broken, body)).status).toBe(400);
        }
        expect((await head(document)).status).toBe(404);
        expect((await send("POST", document, headers, body)).status).toBe(201);
    });
});

describe("push", () =>
{
    it("stores a version whose number is above the current one, by one or by more, and publishes it then", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const createdAt = (await head(document)).headers.get("Ward-Published")!;
        const third = version(document, 3, bob, [alice, bob]);

        await new Promise((resolve) => setTimeout(resolve, 5));
        expect((await put(bob, document, third)).status).toBe(200);
        expect(await stored(document)).toEqual(third);
        const pushedAt = (await head(document)).headers.get("Ward-Published")!;
        expect(Date.parse(pushedAt)).toBeGreaterThan(Date.parse(createdAt));

        expect((await put(alice, document, version(document, 8, alice, [alice, bob]))).status).toBe(200);
        expect(await numberOf(document)).toBe(8);
    });

    it("stores one of two pushes of the same number sent at once, and refuses the other with 409", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const bodies = [version(document, 3, alice, [alice, bob]), version(document, 3, bob, [alice, bob])];

        const answers = await Promise.all([put(alice, document, bodies[0]), put(bob, document, bodies[1])]);
        const statuses = answers.map((answer) => answer.status);
        expect([...statuses].sort()).toEqual([200, 409]);
        expect(await stored(document)).toEqual(bodies[statuses.indexOf(200)]);
    });

    it("refuses the current number from a writer with a higher fingerprint, so the first one kept stays", async () =>
    {
        const [lower, higher] = await newDevicesInOrder();
        const document = await created([lower, higher]);
        const first = version(document, 3, lower, [lower, higher]);
        expect((await put(lower, document, first)).status).toBe(200);

        expect((await put(higher, document, version(document, 3, higher, [lower, higher]))).status).toBe(409);
        expect(await stored(document)).toEqual(first);
    });

    it("refuses a number that is the current one or below it with 409, and keeps what it holds", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const third = version(document, 3, alice, [alice]);
        expect((await put(alice, document, third)).status).toBe(200);

        expect((await put(alice, document, version(document, 3, alice, [alice]))).status).toBe(409);
        expect((await put(alice, document, version(document, 1, alice, [alice]))).status).toBe(409);
        expect(await stored(document)).toEqual(third);
    });

    it("lets only a signer the current header names write, so a version cannot admit its own signer", async () =>
    {
        const alice = await newDevice();
        const mallory = await newDevice();
        const document = await created([alice]);

        expect((await put(mallory, document, version(document, 3, mallory, [alice, mallory]))).status).toBe(403);
        expect(await numberOf(document)).toBe(2);
    });

    it("refuses a version signed by a device that is not its writer", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);

        expect((await put(alice, document, version(document, 3, bob, [alice, bob]))).status).toBe(403);
        expect(await numberOf(document)).toBe(2);
        expect((await put(bob, document, version(document, 3, bob, [alice, bob]))).status).toBe(200);
    });

    it("refuses a signature over another line, or over the same header with another payload", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const body = version(document, 3, alice, [alice]);
        const longer = new Uint8Array(body.length + 1);
        longer.set(body);

        const createLine = await signed(alice, writeLine("create", readVersion(body)!));
        expect((await send("PUT", document, createLine, body)).status).toBe(403);
        const pushLine = await signed(alice, writeLine("push", readVersion(body)!));
        expect((await send("PUT", document, pushLine, longer)).status).toBe(403);
        expect(await numberOf(document)).toBe(2);
        expect((await send("PUT", document, pushLine, body)).status).toBe(200);
    });

    it("refuses a device the last version removed", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        expect((await put(alice, document, version(document, 3, alice, [alice]))).status).toBe(200);

        expect((await put(bob, document, version(document, 4, bob, [alice, bob]))).status).toBe(403);
        expect(await numberOf(document)).toBe(3);
    });

    it("refuses a header naming another document, and a body that is not a version though it is signed, with 400", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const good = version(document, 3, alice, [alice]);
        const goodLine = new TextDecoder().decode(good.subarray(0, good.indexOf(0x0a)));
        const newerFormat = framed(goodLine.replace('"format":1', '"format":2'), random(64));

        expect((await put(alice, document, version(newDocument(), 3, alice, [alice]))).status).toBe(400);
        const headers = await signed(alice, lineOver("push", document, newerFormat));
        expect((await send("PUT", document, headers, newerFormat)).status).toBe(400);
        expect(await numberOf(document)).toBe(2);
        expect((await put(alice, document, good)).status).toBe(200);
    });

    it("answers 404 for a document the server does not have, and stores nothing", async () =>
    {
        const alice = await newDevice();
        const document = newDocument();

        expect((await put(alice, document, version(document, 3, alice, [alice]))).status).toBe(404);
        expect((await head(document)).status).toBe(404);
        expect((await get(document)).status).toBe(404);
    });

    it("gives the first of 400, 404, 403, 409 when several apply", async () =>
    {
        const alice = await newDevice();
        const mallory = await newDevice();
        const missing = newDocument();
        const document = await created([alice]);

        expect((await put(alice, missing, version(newDocument(), 3, alice, [alice]))).status).toBe(400);
        expect((await put(mallory, missing, version(missing, 3, mallory, [mallory]))).status).toBe(404);
        expect((await put(mallory, document, version(document, 1, mallory, [mallory]))).status).toBe(403);
        expect((await put(alice, document, version(document, 1, alice, [alice]))).status).toBe(409);
    });

    it("takes a deleted ward's final version, closes its channels, then refuses every write to its id", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const channel = await opened(alice, document);
        await channel.next();

        expect((await put(alice, document, finalVersion(document, 3, alice))).status).toBe(200);
        expect(await channel.closed).toBe(1000);
        expect((await put(alice, document, version(document, 4, alice, [alice]))).status).toBe(403);
        expect((await post(alice, version(document, 4, alice, [alice]))).status).toBe(409);
        expect(await numberOf(document)).toBe(3);
    });
});

describe("the live channel", () =>
{
    it("tells a device opening it who is here and the current version's number", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        expect((await put(alice, document, version(document, 5, alice, [alice]))).status).toBe(200);

        const channel = await opened(alice, document);
        expect(await channel.next()).toEqual({ present: everyone(alice), version: 5 });
    });

    it("tells everyone, in fingerprint order, when someone arrives and when they leave", async () =>
    {
        const [lower, higher] = await newDevicesInOrder();
        const document = await created([lower, higher]);
        const higherChannel = await opened(higher, document);
        await higherChannel.next();

        const lowerChannel = await opened(lower, document);
        const both = { present: [lower.fingerprint, higher.fingerprint], version: 2 };
        expect(await lowerChannel.next()).toEqual(both);
        expect(await higherChannel.next()).toEqual(both);

        lowerChannel.close();
        expect(await higherChannel.next()).toEqual({ present: [higher.fingerprint], version: 2 });
        expect(await lowerChannel.closed).toBe(1000);
    });

    it("replaces a device's open channel when it opens another, with no change in who is here", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const aliceChannel = await opened(alice, document);
        await aliceChannel.next();
        const first = await opened(bob, document);
        await first.next();
        await aliceChannel.next();

        const second = await opened(bob, document);
        expect(await first.closed).toBe(1000);
        const both = everyone(alice, bob);
        expect(await second.next()).toEqual({ present: both, version: 2 });

        // Whatever the old channel's close makes the server send, every message still lists both.
        expect((await put(alice, document, version(document, 3, alice, [alice, bob]))).status).toBe(200);
        let message = await aliceChannel.next();
        while (message.version !== 3)
        {
            expect(message).toEqual({ present: both, version: 2 });
            message = await aliceChannel.next();
        }
        expect(message).toEqual({ present: both, version: 3 });
    });

    it("tells every channel when a version is stored", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const aliceChannel = await opened(alice, document);
        await aliceChannel.next();
        const bobChannel = await opened(bob, document);
        await bobChannel.next();
        await aliceChannel.next();

        expect((await put(alice, document, version(document, 5, alice, [alice, bob]))).status).toBe(200);
        const stored = { present: everyone(alice, bob), version: 5 };
        expect(await aliceChannel.next()).toEqual(stored);
        expect(await bobChannel.next()).toEqual(stored);
    });

    it("closes the channel of a device a stored version drops", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const aliceChannel = await opened(alice, document);
        await aliceChannel.next();
        const bobChannel = await opened(bob, document);
        await bobChannel.next();
        await aliceChannel.next();

        expect((await put(alice, document, version(document, 3, alice, [alice]))).status).toBe(200);
        expect(await bobChannel.closed).toBe(1000);
        expect(await aliceChannel.next()).toEqual({ present: everyone(alice), version: 3 });
    });

    it("refuses a device the header does not name and a signature over another line with 403, and a missing ward with 404", async () =>
    {
        const alice = await newDevice();
        const mallory = await newDevice();
        const document = await created([alice]);
        const missing = newDocument();

        expect(await refusal(mallory, document, channelLine(document))).toBe(403);
        expect(await refusal(alice, document, channelLine(missing))).toBe(403);
        expect(await refusal(alice, missing, channelLine(missing))).toBe(404);
        (await opened(alice, document)).close();
    });

    it("refuses an open that is not a WebSocket upgrade, or has a malformed signer, with 400", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const headers = await signed(alice, channelLine(document));

        expect((await connect(document, headers)).status).toBe(400);
        expect((await connect(document, { Upgrade: "websocket" })).status).toBe(400);
        expect((await connect(document, { ...headers, "Ward-Signer": "x", Upgrade: "websocket" })).status).toBe(400);
        (await opened(alice, document)).close();
    });
});
