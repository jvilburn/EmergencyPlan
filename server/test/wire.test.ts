import { describe, expect, it } from "vitest";
import { channelLine, decode, encode, fingerprintOf, readVersion, readHeader, writeLine } from "../src/wire";
import { framed } from "./clients";

/** The id the known answers use: the 16 bytes 0x00 to 0x0f. */
const fixedId = encode(Uint8Array.from({ length: 16 }, (_, i) => i));
const fingerprint = "A".repeat(22);

function header(fields: Record<string, unknown>): string
{
    return JSON.stringify({
        format: 1,
        document: fixedId,
        number: 7,
        writer: fingerprint,
        nonce: "A".repeat(32),
        recipients: [{ fingerprint, sealedKey: "A".repeat(107) }],
        ...fields,
    });
}

describe("known answers", () =>
{
    it("fingerprint", () =>
    {
        // The app's IdentityTest pins the same value for the same keys.
        const publicKey = Uint8Array.from({ length: 32 }, (_, i) => i);
        const verifyKey = Uint8Array.from({ length: 32 }, (_, i) => 32 + i);
        expect(encode(fingerprintOf(publicKey, verifyKey))).toBe("YwoyV2kctuI1OuIvdMVNUQ");
    });

    it("signed lines", () =>
    {
        // The lines hash the header line {"format":1}, which readVersion refuses, so these bodies
        // are framed by hand. The hashes were computed with Python's hashlib.blake2b(digest_size=32).
        const known = { document: fixedId, number: 1, writer: fingerprint, recipients: [] };
        const abc = { bytes: framed('{"format":1}', new TextEncoder().encode("abc")), headerLength: 12, header: known };
        const none = { bytes: framed('{"format":1}', new Uint8Array(0)), headerLength: 12, header: known };
        expect(writeLine("create", abc)).toBe(
            "wardplanning-v1 create AAECAwQFBgcICQoLDA0ODw "
            + "IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E "
            + "vd2BPGNCOXIxce8_7phXm5SWTjuxyz5CcmLIwGjVIxk");
        expect(writeLine("push", none)).toBe(
            "wardplanning-v1 push AAECAwQFBgcICQoLDA0ODw "
            + "IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E "
            + "DldRwCblQ7Loqy6wYJnaodHl30d3j3eH-qtFzfEv46g");
        expect(channelLine(fixedId)).toBe("wardplanning-v1 channel AAECAwQFBgcICQoLDA0ODw - -");
    });
});

describe("base64url", () =>
{
    it("round-trips every length the wire uses", () =>
    {
        for (const length of [16, 24, 32, 64, 80])
        {
            const bytes = crypto.getRandomValues(new Uint8Array(length));
            expect(decode(encode(bytes), length)).toEqual(bytes);
        }
    });

    it("refuses padding, the other alphabet, a wrong length and a non-canonical last character", () =>
    {
        expect(decode(fixedId, 16)).not.toBeNull();
        expect(decode(fixedId + "==", 16)).toBeNull();
        expect(decode("AAECAwQFBgcICQoLDA0OD+", 16)).toBeNull();
        expect(decode(fixedId.slice(1), 16)).toBeNull();
        expect(decode("AAECAwQFBgcICQoLDA0ODx", 16)).toBeNull();
        expect(decode(16, 16)).toBeNull();
    });
});

describe("header", () =>
{
    it("reads the document, the number, the writer and the fingerprints sealed to", () =>
    {
        expect(readHeader(header({}))).toEqual({ document: fixedId, number: 7, writer: fingerprint, recipients: [fingerprint] });
        expect(readHeader(header({ recipients: [] }))).toEqual({ document: fixedId, number: 7, writer: fingerprint, recipients: [] });
    });

    it("refuses anything but format 1 with exactly its six keys, each well formed", () =>
    {
        expect(readHeader("not json")).toBeNull();
        expect(readHeader("[]")).toBeNull();
        expect(readHeader(header({ format: 2 }))).toBeNull();
        expect(readHeader(header({ format: "1" }))).toBeNull();
        expect(readHeader(header({ extra: true }))).toBeNull();
        expect(readHeader(header({ document: "short" }))).toBeNull();
        expect(readHeader(header({ writer: "A".repeat(21) }))).toBeNull();
        expect(readHeader(header({ nonce: "A".repeat(22) }))).toBeNull();
        expect(readHeader(header({ recipients: {} }))).toBeNull();
        expect(readHeader(header({ recipients: [{ fingerprint }] }))).toBeNull();
        expect(readHeader(header({ recipients: [{ fingerprint, sealedKey: "A".repeat(106) }] }))).toBeNull();
        expect(readHeader(header({ recipients: [{ fingerprint: "A".repeat(21), sealedKey: "A".repeat(107) }] }))).toBeNull();
        expect(readHeader(header({ recipients: [{ fingerprint, sealedKey: "A".repeat(107), extra: true }] }))).toBeNull();
        expect(readHeader(header({ recipients: [null] }))).toBeNull();
        for (const key of ["format", "document", "number", "writer", "nonce", "recipients"])
        {
            const without = JSON.parse(header({}));
            delete without[key];
            expect(readHeader(JSON.stringify(without))).toBeNull();
        }
    });

    it("takes a number that is an integer of at least 1", () =>
    {
        expect(readHeader(header({ number: 1 }))?.number).toBe(1);
        expect(readHeader(header({ number: 2 ** 53 }))?.number).toBe(2 ** 53);
        for (const number of [0, -1, 1.5, "7", null, 2 ** 54])
        {
            expect(readHeader(header({ number }))).toBeNull();
        }
    });

    it("splits a body at its first newline, and refuses one without a newline or with a header that is not UTF-8", () =>
    {
        const payload = new TextEncoder().encode("line one\nline two");
        const body = readVersion(framed(header({}), payload));
        expect(body?.headerLength).toBe(header({}).length);
        expect(readVersion(new TextEncoder().encode(header({})))).toBeNull();
        expect(readVersion(Uint8Array.of(0xff, 0x0a))).toBeNull();
        expect(readVersion(framed("\uFEFF" + header({}), payload))).toBeNull();
    });
});
