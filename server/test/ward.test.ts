import { describe, expect, it } from "vitest";
import { channelLine, encode, readVersion, writeLine } from "../src/wire";
import {
    Channel,
    created,
    everyone,
    framed,
    get,
    head,
    newDevice,
    newDocument,
    open,
    opened,
    post,
    put,
    random,
    signed,
    version,
    ward,
} from "./clients";

/** The number of the version the server holds, read from its head. */
async function numberOf(document: string): Promise<number>
{
    return (await (await head(document)).json<{ number: number }>()).number;
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
        expect(whole.headers.get("Ward-Published")).toMatch(/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$/);

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
        const wrongLine = await signed(alice, writeLine("push", readVersion(body)!));
        const answer = await ward(document).fetch("https://ward.test/documents", { method: "POST", headers: wrongLine, body });
        expect(answer.status).toBe(403);
        expect((await head(document)).status).toBe(404);
        expect((await post(alice, body)).status).toBe(201);
    });

    it("refuses a malformed body, header or signer with 400", async () =>
    {
        const alice = await newDevice();
        const document = newDocument();
        const unframed = new TextEncoder().encode(JSON.stringify({ format: 1 }));
        expect((await post(alice, unframed)).status).toBe(400);
        expect((await post(alice, framed('{"format":2}', random(8)))).status).toBe(400);

        const body = version(document, 2, alice, [alice]);
        const headers = await signed(alice, writeLine("create", readVersion(body)!));
        for (const broken of [
            { ...headers, "Ward-Signer": encode(alice.verifyKey) },
            { ...headers, "Ward-Signature": headers["Ward-Signature"].slice(1) },
            { "Ward-Signer": headers["Ward-Signer"] },
        ])
        {
            const answer = await ward(document).fetch("https://ward.test/documents", { method: "POST", headers: broken, body });
            expect(answer.status).toBe(400);
        }
        expect((await head(document)).status).toBe(404);
    });
});

describe("push", () =>
{
    it("stores a version whose number is above the current one, by one or by more", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const third = version(document, 3, bob, [alice, bob]);

        expect((await put(bob, document, third)).status).toBe(200);
        expect(new Uint8Array(await (await get(document)).arrayBuffer())).toEqual(third);

        expect((await put(alice, document, version(document, 8, alice, [alice, bob]))).status).toBe(200);
        expect(await numberOf(document)).toBe(8);
    });

    it("stores one of two pushes of the same number sent at once, and refuses the other with 409", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);

        const answers = await Promise.all([
            put(alice, document, version(document, 3, alice, [alice, bob])),
            put(bob, document, version(document, 3, bob, [alice, bob])),
        ]);
        expect(answers.map((answer) => answer.status).sort()).toEqual([200, 409]);
        expect(await numberOf(document)).toBe(3);
    });

    it("refuses a number that is the current one or below it with 409, and keeps what it holds", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const third = version(document, 3, alice, [alice]);
        expect((await put(alice, document, third)).status).toBe(200);

        expect((await put(alice, document, version(document, 3, alice, [alice]))).status).toBe(409);
        expect((await put(alice, document, version(document, 1, alice, [alice]))).status).toBe(409);
        expect(new Uint8Array(await (await get(document)).arrayBuffer())).toEqual(third);
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

    it("refuses a signature over another line", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);
        const body = version(document, 3, alice, [alice]);

        const wrongLine = await signed(alice, writeLine("create", readVersion(body)!));
        const answer = await ward(document).fetch(`https://ward.test/documents/${document}`, { method: "PUT", headers: wrongLine, body });
        expect(answer.status).toBe(403);
        expect(await numberOf(document)).toBe(2);
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

    it("refuses a header naming another document, and a body that is not a version, with 400", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);

        expect((await put(alice, document, version(newDocument(), 3, alice, [alice]))).status).toBe(400);
        expect((await put(alice, document, random(64).fill(0x41))).status).toBe(400);
        expect(await numberOf(document)).toBe(2);
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

    it("takes a deleted ward's final version, then refuses every write to its id", async () =>
    {
        const alice = await newDevice();
        const document = await created([alice]);

        expect((await put(alice, document, version(document, 3, alice, [], new Uint8Array(0)))).status).toBe(200);
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

        const channel = await opened(alice, document);
        expect(await channel.next()).toEqual({ present: everyone(alice), version: 2 });
    });

    it("tells everyone when someone arrives and when they leave", async () =>
    {
        const alice = await newDevice();
        const bob = await newDevice();
        const document = await created([alice, bob]);
        const aliceChannel = await opened(alice, document);
        await aliceChannel.next();

        const bobChannel = await opened(bob, document);
        const both = { present: everyone(alice, bob), version: 2 };
        expect(await bobChannel.next()).toEqual(both);
        expect(await aliceChannel.next()).toEqual(both);

        bobChannel.close();
        expect(await aliceChannel.next()).toEqual({ present: everyone(alice), version: 2 });
        expect(await bobChannel.closed).toBe(1000);
    });

    it("replaces a device's open channel when it opens another, and keeps who is here", async () =>
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
        const both = { present: everyone(alice, bob), version: 2 };
        expect(await second.next()).toEqual(both);
        expect(await aliceChannel.next()).toEqual(both);
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

    it("refuses a device the header does not name, a bad signature, and a missing ward", async () =>
    {
        const alice = await newDevice();
        const mallory = await newDevice();
        const document = await created([alice]);

        const stranger = await open(mallory, document);
        expect(stranger).not.toBeInstanceOf(Channel);
        expect((stranger as Response).status).toBe(403);

        const wrongLine = await open(alice, document, channelLine(newDocument()));
        expect((wrongLine as Response).status).toBe(403);

        const missing = newDocument();
        expect(((await open(alice, missing)) as Response).status).toBe(404);
    });
});
