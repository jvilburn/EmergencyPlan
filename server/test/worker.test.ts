import { exports } from "cloudflare:workers";
import { describe, expect, it } from "vitest";
import { downloadPage } from "../src/download";
import releases from "../src/releases.json";
import { channelLine, readVersion, writeLine } from "../src/wire";
import { Channel, Device, framed, newDevice, newDocument, random, signed, version } from "./clients";

const origin = "https://ward.test";

/** A request to the Worker from address, as Cloudflare's edge passes it on. */
function fromAddress(address: string, path: string, init: RequestInit): Promise<Response>
{
    return exports.default.fetch(new Request(`${origin}${path}`, {
        ...init,
        headers: { ...(init.headers as Record<string, string>), "CF-Connecting-IP": address },
    }));
}

/** A create, from address, of document's first version by device. */
async function create(address: string, device: Device, document: string): Promise<Response>
{
    const body = version(document, 2, device, [device]);
    const headers = await signed(device, writeLine("create", readVersion(body)!));
    return fromAddress(address, "/documents", { method: "POST", headers, body });
}

describe("routing", () =>
{
    it("hands a create to the ward its header names, and reads, pushes and channels reach the same ward", async () =>
    {
        const alice = await newDevice();
        const document = newDocument();
        expect((await create("192.0.2.1", alice, document)).status).toBe(201);

        const head = await fromAddress("192.0.2.1", `/documents/${document}/head`, {});
        expect(head.status).toBe(200);
        expect((await head.json<{ document: string }>()).document).toBe(document);
        expect((await fromAddress("192.0.2.1", `/documents/${newDocument()}/head`, {})).status).toBe(404);

        const third = version(document, 3, alice, [alice]);
        const pushed = await fromAddress("192.0.2.1", `/documents/${document}`, {
            method: "PUT",
            headers: await signed(alice, writeLine("push", readVersion(third)!)),
            body: third,
        });
        expect(pushed.status).toBe(200);
        const whole = await fromAddress("192.0.2.1", `/documents/${document}`, {});
        expect(new Uint8Array(await whole.arrayBuffer())).toEqual(third);

        const upgrade = await fromAddress("192.0.2.1", `/documents/${document}/channel`, {
            headers: { ...(await signed(alice, channelLine(document))), Upgrade: "websocket" },
        });
        expect(upgrade.status).toBe(101);
        const channel = new Channel(upgrade.webSocket!);
        expect(await channel.next()).toEqual({ present: [alice.fingerprint], version: 3 });
        channel.close();
    });

    it("answers a create whose body cannot name a ward with 400", async () =>
    {
        const answer = await fromAddress("192.0.2.2", "/documents", { method: "POST", body: framed("{}", random(8)) });
        expect(answer.status).toBe(400);
    });

    it("answers an id that is not one as the ward it would name does, and an unknown route with 404", async () =>
    {
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id/head", {})).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id", {})).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/documents/not-an-id", { method: "PUT", body: "x" })).status).toBe(400);
        expect((await fromAddress("192.0.2.3", "/documents", { method: "GET" })).status).toBe(404);
        expect((await fromAddress("192.0.2.3", "/", {})).status).toBe(404);
    });
});

describe("limits by address", () =>
{
    it("refuses an address's eleventh create in a minute with 429, well formed or not, and not another address's", async () =>
    {
        const alice = await newDevice();
        for (let i = 0; i < 10; ++i)
        {
            expect((await create("198.51.100.1", alice, newDocument())).status).toBe(201);
        }
        const document = newDocument();
        expect((await create("198.51.100.1", alice, document)).status).toBe(429);
        expect((await fromAddress("198.51.100.1", "/documents", { method: "POST", body: "x" })).status).toBe(429);
        expect((await fromAddress("198.51.100.1", `/documents/${document}/head`, {})).status).toBe(404);
        expect((await create("198.51.100.2", alice, newDocument())).status).toBe(201);
    });

    it("refuses an address's reads past three thousand in a minute with 429, and still takes its pushes", async () =>
    {
        const document = newDocument();
        const reads = await Promise.all(Array.from({ length: 3000 }, () => fromAddress("203.0.113.1", `/documents/${document}/head`, {})));
        expect(reads.every((read) => read.status === 404)).toBe(true);
        expect((await fromAddress("203.0.113.1", `/documents/${document}`, {})).status).toBe(429);
        expect((await fromAddress("203.0.113.1", `/documents/${document}`, { method: "PUT", body: "x" })).status).toBe(400);
        expect((await fromAddress("203.0.113.2", `/documents/${document}`, {})).status).toBe(404);
    });
});

describe("releases", () =>
{
    it("serves the releases file, and a download page as HTML", async () =>
    {
        const latest = await fromAddress("192.0.2.9", "/releases/latest", {});
        expect(latest.status).toBe(200);
        expect(await latest.json()).toEqual(releases);

        const page = await fromAddress("192.0.2.9", "/download", {});
        expect(page.status).toBe(200);
        expect(page.headers.get("Content-Type")).toBe("text/html; charset=utf-8");
        expect(await page.text()).toBe(await downloadPage(releases).text());
    });

    it("links each platform's installer on the download page, or says there is none", async () =>
    {
        const page = await downloadPage({
            windows: { name: "Windows", version: "2.0.0", url: "https://example.test/setup.exe" },
            macos: { name: "macOS", version: "2.0.1", url: "https://example.test/app.dmg" },
        }).text();
        expect(page).toContain('<li><a href="https://example.test/setup.exe">Download for Windows</a> (2.0.0)</li>');
        expect(page).toContain('<li><a href="https://example.test/app.dmg">Download for macOS</a> (2.0.1)</li>');
        expect(page).not.toContain("No installer");

        const empty = await downloadPage({}).text();
        expect(empty).toContain("<p>No installer is available yet.</p>");
        expect(empty).not.toContain("<li>");
    });
});
