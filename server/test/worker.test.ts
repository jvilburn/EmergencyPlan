import { exports } from "cloudflare:workers";
import { describe, expect, it } from "vitest";
import { readVersion, writeLine } from "../src/wire";
import { framed, newDevice, newDocument, random, signed, version } from "./clients";

const origin = "https://ward.test";

/** A request to the Worker from address, as Cloudflare's edge passes it on. */
function fromAddress(address: string, path: string, init: RequestInit = {}): Promise<Response>
{
    return exports.default.fetch(new Request(`${origin}${path}`, {
        ...init,
        headers: { ...(init.headers as Record<string, string>), "CF-Connecting-IP": address },
    }));
}

async function create(address: string, document: string): Promise<Response>
{
    const alice = await newDevice();
    const body = version(document, 2, alice, [alice]);
    const headers = await signed(alice, writeLine("create", readVersion(body)!));
    return fromAddress(address, "/documents", { method: "POST", headers, body });
}

describe("routing", () =>
{
    it("hands a create to the ward its header names, and reads reach the same ward", async () =>
    {
        const document = newDocument();
        expect((await create("192.0.2.1", document)).status).toBe(201);
        const head = await fromAddress("192.0.2.1", `/documents/${document}/head`);
        expect(head.status).toBe(200);
        expect((await head.json<{ document: string }>()).document).toBe(document);
        expect((await fromAddress("192.0.2.1", `/documents/${document}`)).status).toBe(200);
        expect((await fromAddress("192.0.2.1", `/documents/${newDocument()}/head`)).status).toBe(404);
    });

    it("answers a create whose body cannot name a ward with 400", async () =>
    {
        const answer = await fromAddress("192.0.2.2", "/documents", { method: "POST", body: framed("{}", random(8)) });
        expect(answer.status).toBe(400);
    });

    it("answers a malformed id with 404 for a read or a channel and 400 for a push, and an unknown route with 404", async () =>
    {
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id/head")).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id")).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id/channel")).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id", { method: "PUT", body: "x" })).status).toBe(400);
        expect((await fromAddress("192.0.2.3", "/documents", { method: "GET" })).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/")).status).toBe(404);
    });
});

describe("limits by address", () =>
{
    it("refuses an address's eleventh create in a minute with 429, and not another address's", async () =>
    {
        for (let i = 0; i < 10; ++i)
        {
            expect((await create("198.51.100.1", newDocument())).status).toBe(201);
        }
        const document = newDocument();
        expect((await create("198.51.100.1", document)).status).toBe(429);
        expect((await fromAddress("198.51.100.1", `/documents/${document}/head`)).status).toBe(404);
        expect((await create("198.51.100.2", newDocument())).status).toBe(201);
    });

    it("refuses an address's six hundred and first read in a minute with 429, and still takes its pushes", async () =>
    {
        const document = newDocument();
        const reads = await Promise.all(Array.from({ length: 600 }, () => fromAddress("203.0.113.1", `/documents/${document}/head`)));
        expect(reads.every((read) => read.status === 404)).toBe(true);
        expect((await fromAddress("203.0.113.1", `/documents/${document}`)).status).toBe(429);
        expect((await fromAddress("203.0.113.1", `/documents/${document}`, { method: "PUT", body: "x" })).status).toBe(400);
        expect((await fromAddress("203.0.113.2", `/documents/${document}`)).status).toBe(404);
    });
});

describe("releases", () =>
{
    it("serves each platform's current version and installer, and a download page linking them", async () =>
    {
        const latest = await fromAddress("192.0.2.9", "/releases/latest");
        expect(latest.status).toBe(200);
        const platforms = await latest.json<Record<string, { version: string; url: string }>>();
        expect(platforms.windows.version).toMatch(/^\d+\.\d+\.\d+$/);
        expect(platforms.windows.url).toMatch(/^https:\/\//);

        const page = await (await fromAddress("192.0.2.9", "/download")).text();
        expect(page).toContain(`<a href="${platforms.windows.url}">Download for Windows</a>`);
    });
});
