import { blake2b } from "@noble/hashes/blake2.js";

/** Byte lengths of the binary values on the wire, each sent as base64url without padding. */
export const IdBytes = 16;
export const NonceBytes = 24;
export const FingerprintBytes = 16;
export const SealedKeyBytes = 80;
export const KeyBytes = 32;
export const SignatureBytes = 64;

/** The largest integer the format writes. */
const MaxInteger = 2 ** 53;

/** The one header format this server stores. A new format is supported here before any app writes it. */
export const Format = 1;

const encoder = new TextEncoder();
const FingerprintPersonalization = personalization("wp-fingerprint");

/** Base64url without padding, as every binary value on the wire is. */
export function encode(bytes: Uint8Array): string
{
    let binary = "";
    for (const byte of bytes)
    {
        binary += String.fromCharCode(byte);
    }
    return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

/** The bytes text spells when it is the canonical base64url of exactly length bytes; null otherwise. */
export function decode(text: unknown, length: number): Uint8Array | null
{
    if (typeof text !== "string"
        || text.length !== Math.ceil(length * 4 / 3)
        || !/^[A-Za-z0-9_-]*$/.test(text))
    {
        return null;
    }
    const binary = atob(text.replace(/-/g, "+").replace(/_/g, "/"));
    const bytes = Uint8Array.from(binary, (character) => character.charCodeAt(0));
    return encode(bytes) === text ? bytes : null;
}

/** What the server reads from a header: its document, the version's number and writer, and the fingerprints it seals to. */
export interface Header
{
    document: string;
    number: number;
    writer: string;
    recipients: string[];
}

/** The header line read strictly as format 1: exactly its six keys, each well formed. */
export function readHeader(line: string): Header | null
{
    let json: unknown;
    try
    {
        json = JSON.parse(line);
    }
    catch
    {
        return null;
    }
    if (!hasExactly(json, ["format", "document", "number", "writer", "nonce", "recipients"])
        || json.format !== Format
        || decode(json.document, IdBytes) === null
        || !Number.isInteger(json.number)
        || (json.number as number) < 1
        || (json.number as number) > MaxInteger
        || decode(json.writer, FingerprintBytes) === null
        || decode(json.nonce, NonceBytes) === null
        || !Array.isArray(json.recipients))
    {
        return null;
    }
    const recipients: string[] = [];
    for (const recipient of json.recipients)
    {
        if (!hasExactly(recipient, ["fingerprint", "sealedKey"])
            || decode(recipient.fingerprint, FingerprintBytes) === null
            || decode(recipient.sealedKey, SealedKeyBytes) === null)
        {
            return null;
        }
        recipients.push(recipient.fingerprint as string);
    }
    return {
        document: json.document as string,
        number: json.number as number,
        writer: json.writer as string,
        recipients,
    };
}

/** A version as it travels: the header line, one `\n`, then the payload. */
export interface Version
{
    bytes: Uint8Array;
    headerLength: number;
    header: Header;
}

/** The version bytes hold, when they are framed and the header is well formed; null otherwise. */
export function readVersion(bytes: Uint8Array): Version | null
{
    const headerLength = bytes.indexOf(0x0a);
    if (headerLength < 0)
    {
        return null;
    }
    let line: string;
    try
    {
        line = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(bytes.subarray(0, headerLength));
    }
    catch
    {
        return null;
    }
    const header = readHeader(line);
    if (header === null)
    {
        return null;
    }
    return { bytes, headerLength, header };
}

/** Who signed a request, from its `Ward-Signer` and `Ward-Signature` headers. */
export interface Signer
{
    fingerprint: string;
    verifyKey: Uint8Array;
    signature: Uint8Array;
}

/** The signer, when both headers are well formed; null otherwise. */
export function readSigner(headers: Headers): Signer | null
{
    const keys = (headers.get("Ward-Signer") ?? "").split(".");
    if (keys.length !== 2)
    {
        return null;
    }
    const publicKey = decode(keys[0], KeyBytes);
    const verifyKey = decode(keys[1], KeyBytes);
    const signature = decode(headers.get("Ward-Signature"), SignatureBytes);
    if (publicKey === null || verifyKey === null || signature === null)
    {
        return null;
    }
    return { fingerprint: encode(fingerprintOf(publicKey, verifyKey)), verifyKey, signature };
}

/** A device's fingerprint: 16-byte BLAKE2b of its X25519 public key then its Ed25519 verify key, personalized `wp-fingerprint`. */
export function fingerprintOf(publicKey: Uint8Array, verifyKey: Uint8Array): Uint8Array
{
    const keys = new Uint8Array(publicKey.length + verifyKey.length);
    keys.set(publicKey);
    keys.set(verifyKey, publicKey.length);
    return blake2b(keys, { dkLen: FingerprintBytes, personalization: FingerprintPersonalization });
}

/** The line a `create` or a `push` signs: the document, then the hashes of the header line and the payload. */
export function writeLine(call: "create" | "push", version: Version): string
{
    const header = version.bytes.subarray(0, version.headerLength);
    const payload = version.bytes.subarray(version.headerLength + 1);
    return `wardplanning-v1 ${call} ${version.header.document} ${hash(header)} ${hash(payload)}`;
}

/** The line opening the live channel signs, with `-` for the hashes it lacks. */
export function channelLine(document: string): string
{
    return `wardplanning-v1 channel ${document} - -`;
}

/** Whether the signer's signature over line verifies. A key that is not a valid point does not verify. */
export async function verifies(signer: Signer, line: string): Promise<boolean>
{
    try
    {
        const key = await crypto.subtle.importKey("raw", signer.verifyKey, { name: "Ed25519" }, false, ["verify"]);
        return await crypto.subtle.verify({ name: "Ed25519" }, key, signer.signature, encoder.encode(line));
    }
    catch
    {
        return false;
    }
}

/** Where a request goes, read from its method and path alone. */
export type Route =
    | { kind: "create" }
    | { kind: "head"; document: string }
    | { kind: "version"; document: string }
    | { kind: "push"; document: string }
    | { kind: "channel"; document: string }
    | { kind: "releases" }
    | { kind: "download" };

export function routeOf(method: string, path: string): Route | null
{
    const parts = path.split("/");
    if (method === "POST" && path === "/documents")
    {
        return { kind: "create" };
    }
    if (method === "GET" && path === "/releases/latest")
    {
        return { kind: "releases" };
    }
    if (method === "GET" && path === "/download")
    {
        return { kind: "download" };
    }
    if (parts.length === 3 && parts[1] === "documents" && method === "GET")
    {
        return { kind: "version", document: parts[2] };
    }
    if (parts.length === 3 && parts[1] === "documents" && method === "PUT")
    {
        return { kind: "push", document: parts[2] };
    }
    if (parts.length === 4 && parts[1] === "documents" && parts[3] === "head" && method === "GET")
    {
        return { kind: "head", document: parts[2] };
    }
    if (parts.length === 4 && parts[1] === "documents" && parts[3] === "channel" && method === "GET")
    {
        return { kind: "channel", document: parts[2] };
    }
    return null;
}

/** An answer with no body. */
export function status(code: number): Response
{
    return new Response(null, { status: code });
}

function hash(bytes: Uint8Array): string
{
    return encode(blake2b(bytes, { dkLen: 32 }));
}

/** A BLAKE2b personalization: the name zero-padded to 16 bytes. */
function personalization(name: string): Uint8Array
{
    const bytes = new Uint8Array(16);
    bytes.set(encoder.encode(name));
    return bytes;
}

function hasExactly(value: unknown, keys: string[]): value is Record<string, unknown>
{
    if (typeof value !== "object" || value === null || Array.isArray(value))
    {
        return false;
    }
    const actual = Object.keys(value);
    return actual.length === keys.length && keys.every((key) => actual.includes(key));
}
