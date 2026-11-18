import { exports } from "cloudflare:workers";
import { afterEach, describe, expect, it, vi } from "vitest";
import { downloadPage } from "../src/download";
import { InvitationPattern, downloadName, joinPage, readLink } from "../src/join";
import releases from "../src/releases.json";
import { IdBytes, KeyBytes, channelLine, encode, readVersion, writeLine } from "../src/wire";
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

    it("refuses an address's reads past its limit, fifty a minute as the tests run, with 429, and still takes its pushes", async () =>
    {
        const document = newDocument();
        const reads = await Promise.all(Array.from({ length: 50 }, () => fromAddress("203.0.113.1", `/documents/${document}/head`, {})));
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

describe("joining through a link", () =>
{
    afterEach(() =>
    {
        vi.restoreAllMocks();
    });

    it("serves one join page for every invitation, which names the download after the invitation in the browser", async () =>
    {
        const answer = await fromAddress("192.0.2.20", "/open", {});
        expect(answer.status).toBe(200);
        expect(answer.headers.get("Content-Type")).toBe("text/html; charset=utf-8");
        const page = await answer.text();
        expect(page).toBe(await joinPage(releases).text());

        // The page reads the invitation from the part of the link the browser keeps to itself.
        expect(page).toContain("location.hash");
        expect(page).toContain(`readLink(hash, ${JSON.stringify(InvitationPattern)})`);
        expect(page).toContain(`const readLink = (${readLink.toString()});`);
        expect(page).toContain(`const downloadName = (${downloadName.toString()});`);
        expect(page).toContain("download.download = downloadName(link.invitation, link.ward);");
        expect(page).toContain('link.ward + " has been shared with you in Ward Planning."');
        expect(page).toContain("It asks for a name for this computer, your name and your email, ");
        expect(page).toContain('"then asks whether to open "');
        expect(page).toContain('href="/installer/windows"');
        expect(page).toContain("Run anyway");
        expect(page).toContain("Open this link on your Windows computer");
        expect(page).toContain('<p id="incomplete" hidden>This invitation link is not complete.');
    });

    it("reads as an invitation exactly a ward's id, a dot and a seed, as the app writes them", () =>
    {
        const pattern = new RegExp(InvitationPattern);
        const invitation = `${encode(random(IdBytes))}.${encode(random(KeyBytes))}`;
        expect(invitation.length).toBe(66);
        expect(pattern.test(invitation)).toBe(true);

        expect(pattern.test(invitation.slice(0, 65))).toBe(false);
        expect(pattern.test(invitation.slice(1))).toBe(false);
        expect(pattern.test(invitation + "A")).toBe(false);
        expect(pattern.test(invitation.replace(".", "_"))).toBe(false);
        expect(pattern.test(invitation.replace(/.$/, "+"))).toBe(false);
        expect(pattern.test("")).toBe(false);
    });

    it("reads a link's invitation and its ward's name, and names the download after both", () =>
    {
        const invitation = `${encode(random(IdBytes))}.${encode(random(KeyBytes))}`;

        expect(readLink(`${invitation}.Tanglewood%20Ward`, InvitationPattern)).toEqual({ invitation, ward: "Tanglewood Ward" });
        expect(readLink(invitation, InvitationPattern)).toEqual({ invitation, ward: "" });
        // A name that does not decode is left out; the invitation still reads.
        expect(readLink(`${invitation}.Tanglewood%2`, InvitationPattern)).toEqual({ invitation, ward: "" });
        expect(readLink(invitation.slice(0, 65), InvitationPattern)).toBeNull();
        expect(readLink(`${invitation}A`, InvitationPattern)).toBeNull();
        expect(readLink("", InvitationPattern)).toBeNull();

        expect(downloadName(invitation, "Tanglewood Ward")).toBe(`WardPlanning-Tanglewood-${invitation}.exe`);
        expect(downloadName(invitation, "Maple 2nd ward ")).toBe(`WardPlanning-Maple-2nd-${invitation}.exe`);
        expect(downloadName(invitation, "Maple Branch")).toBe(`WardPlanning-Maple-Branch-${invitation}.exe`);
        expect(downloadName(invitation, "St. George (YSA) Ward")).toBe(`WardPlanning-St-George-YSA-${invitation}.exe`);
        expect(downloadName(invitation, "")).toBe(`WardPlanning-Setup-${invitation}.exe`);
        expect(downloadName(invitation, "Ward")).toBe(`WardPlanning-Ward-${invitation}.exe`);
        // The installer finds the invitation by its dot, so the name brings none of its own.
        const long = downloadName(invitation, "A.B.C ".repeat(30));
        expect(long.split(".").length).toBe(3);
        expect(long.length).toBeLessThanOrEqual("WardPlanning-".length + 40 + 1 + 66 + 4);
        expect(long.endsWith(`-${invitation}.exe`)).toBe(true);
    });

    it("offers no download while there is no Windows release", async () =>
    {
        const page = await joinPage({}).text();
        expect(page).toContain("No installer is available yet.");
        expect(page).not.toContain("/installer/windows");
    });

    it("passes a platform's installer through, with no file name of its own", async () =>
    {
        const bytes = new Uint8Array([77, 90, 1, 2, 3]);
        // The answer is made when the Worker asks, so its body belongs to the Worker's request.
        const upstream = vi.spyOn(globalThis, "fetch").mockImplementation(async () =>
            new Response(bytes, { headers: { "Content-Length": "5", "Content-Disposition": "attachment; filename=other.exe" } }));

        const answer = await fromAddress("192.0.2.21", "/installer/windows", {});

        expect(answer.status).toBe(200);
        expect(new Uint8Array(await answer.arrayBuffer())).toEqual(bytes);
        expect(answer.headers.get("Content-Type")).toBe("application/octet-stream");
        expect(answer.headers.get("Content-Length")).toBe("5");
        expect(answer.headers.get("Content-Disposition")).toBeNull();
        expect(upstream).toHaveBeenCalledWith((releases as Record<string, { url: string }>).windows.url);
    });

    it("answers 404 for a platform with no release, and 502 when the release's file cannot be had", async () =>
    {
        expect((await fromAddress("192.0.2.22", "/installer/amiga", {})).status).toBe(404);
        expect((await fromAddress("192.0.2.22", "/installer/constructor", {})).status).toBe(404);
        const upstream = vi.spyOn(globalThis, "fetch").mockImplementation(async () => new Response(null, { status: 404 }));
        expect((await fromAddress("192.0.2.22", "/installer/windows", {})).status).toBe(502);
        upstream.mockImplementation(async () =>
        {
            throw new Error("the connection was reset");
        });
        expect((await fromAddress("192.0.2.22", "/installer/windows", {})).status).toBe(502);
    });
});
