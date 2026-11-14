import { env } from "cloudflare:workers";
import { IdBytes, NonceBytes, SealedKeyBytes, channelLine, encode, fingerprintOf, readVersion, writeLine } from "../src/wire";

/** A device as the server sees one: two public keys, a fingerprint, and a signing key to sign with. */
export interface Device
{
    publicKey: Uint8Array;
    verifyKey: Uint8Array;
    signingKey: CryptoKey;
    fingerprint: string;
}

export async function newDevice(): Promise<Device>
{
    const pair = (await crypto.subtle.generateKey({ name: "Ed25519" }, true, ["sign", "verify"])) as CryptoKeyPair;
    const verifyKey = new Uint8Array((await crypto.subtle.exportKey("raw", pair.publicKey)) as ArrayBuffer);
    const publicKey = random(32);
    return { publicKey, verifyKey, signingKey: pair.privateKey, fingerprint: encode(fingerprintOf(publicKey, verifyKey)) };
}

/** Two new devices, the one with the lower fingerprint first. */
export async function newDevicesInOrder(): Promise<[Device, Device]>
{
    const first = await newDevice();
    const second = await newDevice();
    return first.fingerprint < second.fingerprint ? [first, second] : [second, first];
}

export function newDocument(): string
{
    return encode(random(IdBytes));
}

export function random(length: number): Uint8Array
{
    return crypto.getRandomValues(new Uint8Array(length));
}

/** Version number of document, written by writer and sealed to recipients, with a payload of random bytes. */
export function version(document: string, number: number, writer: Device, recipients: Device[]): Uint8Array
{
    return framed(headerLine(document, number, writer, recipients), random(64));
}

/** A deleted ward's final version: a header naming no recipient, and no payload. */
export function finalVersion(document: string, number: number, writer: Device): Uint8Array
{
    return framed(headerLine(document, number, writer, []), new Uint8Array(0));
}

function headerLine(document: string, number: number, writer: Device, recipients: Device[]): string
{
    return JSON.stringify({
        format: 1,
        document,
        number,
        writer: writer.fingerprint,
        nonce: encode(random(NonceBytes)),
        recipients: recipients.map((device) => ({ fingerprint: device.fingerprint, sealedKey: encode(random(SealedKeyBytes)) })),
    });
}

/** A header line and a payload, framed as a body. */
export function framed(header: string, payload: Uint8Array): Uint8Array
{
    const line = new TextEncoder().encode(header + "\n");
    const bytes = new Uint8Array(line.length + payload.length);
    bytes.set(line);
    bytes.set(payload, line.length);
    return bytes;
}

/**
 * The line a create or a push of body signs for document, whatever body's header line holds, so
 * a body the server refuses can still arrive properly signed.
 */
export function lineOver(call: "create" | "push", document: string, body: Uint8Array): string
{
    const header = { document, number: 1, writer: "", recipients: [] };
    return writeLine(call, { bytes: body, headerLength: body.indexOf(0x0a), header });
}

/** The `Ward-Signer` and `Ward-Signature` headers of device signing line. */
export async function signed(device: Device, line: string): Promise<Record<string, string>>
{
    const signature = await crypto.subtle.sign({ name: "Ed25519" }, device.signingKey, new TextEncoder().encode(line));
    return {
        "Ward-Signer": `${encode(device.publicKey)}.${encode(device.verifyKey)}`,
        "Ward-Signature": encode(new Uint8Array(signature)),
    };
}

/** The Ward object holding document, as the Worker reaches it. */
export function ward(document: string): DurableObjectStub
{
    return env.WARD.get(env.WARD.idFromName(document));
}

const origin = "https://ward.test";

/** A create (`POST`) or a push (`PUT`) of body with headers, sent to document's ward. */
export function send(method: "POST" | "PUT", document: string, headers: Record<string, string>, body: Uint8Array): Promise<Response>
{
    const path = method === "POST" ? "/documents" : `/documents/${document}`;
    return ward(document).fetch(`${origin}${path}`, { method, headers, body });
}

/** A create of body, a well-formed version, signed by device. */
export async function post(device: Device, body: Uint8Array): Promise<Response>
{
    const parsed = wellFormed(body);
    return send("POST", parsed.header.document, await signed(device, writeLine("create", parsed)), body);
}

/** A push of body, a well-formed version, to document, signed by device. */
export async function put(device: Device, document: string, body: Uint8Array): Promise<Response>
{
    return send("PUT", document, await signed(device, writeLine("push", wellFormed(body))), body);
}

function wellFormed(body: Uint8Array)
{
    const parsed = readVersion(body);
    if (parsed === null)
    {
        throw new Error("the body is not a well-formed version; send it with send()");
    }
    return parsed;
}

export function head(document: string): Promise<Response>
{
    return ward(document).fetch(`${origin}/documents/${document}/head`);
}

export function get(document: string): Promise<Response>
{
    return ward(document).fetch(`${origin}/documents/${document}`);
}

/** The bytes of the version the server holds. */
export async function stored(document: string): Promise<Uint8Array>
{
    return new Uint8Array(await (await get(document)).arrayBuffer());
}

/** The fingerprints of devices, as a channel message lists them. */
export function everyone(...devices: Device[]): string[]
{
    return devices.map((device) => device.fingerprint).sort();
}

/** What a channel has been told: who is here, and the current version's number. */
export interface Presence
{
    present: string[];
    version: number;
}

/** An open live channel, with the messages it has received in order. */
export class Channel
{
    private messages: Presence[] = [];
    private waiting: ((message: Presence) => void)[] = [];
    readonly closed: Promise<number>;

    constructor(readonly socket: WebSocket)
    {
        socket.addEventListener("message", (event) => this.receive(event));
        this.closed = new Promise((resolve) => socket.addEventListener("close", (event) => resolve(event.code)));
        socket.accept();
    }

    /** The next message, waiting for it when none has arrived. */
    next(): Promise<Presence>
    {
        const message = this.messages.shift();
        if (message !== undefined)
        {
            return Promise.resolve(message);
        }
        return new Promise((resolve) => this.waiting.push(resolve));
    }

    close(): void
    {
        this.socket.close(1000, "Closed by the test");
    }

    private receive(event: MessageEvent): void
    {
        const message = JSON.parse(event.data as string) as Presence;
        const next = this.waiting.shift();
        if (next !== undefined)
        {
            next(message);
        }
        else
        {
            this.messages.push(message);
        }
    }
}

/** A request to open document's live channel, with headers. */
export function connect(document: string, headers: Record<string, string>): Promise<Response>
{
    return ward(document).fetch(`${origin}/documents/${document}/channel`, { headers });
}

/** Opens document's live channel as device, which must be accepted. */
export async function opened(device: Device, document: string): Promise<Channel>
{
    const response = await connect(document, { ...(await signed(device, channelLine(document))), Upgrade: "websocket" });
    if (response.webSocket === null)
    {
        throw new Error(`the channel was refused with ${response.status}`);
    }
    return new Channel(response.webSocket);
}

/** The status that refuses device opening document's live channel with a signature over line. */
export async function refusal(device: Device, document: string, line: string): Promise<number>
{
    const response = await connect(document, { ...(await signed(device, line)), Upgrade: "websocket" });
    if (response.webSocket !== null)
    {
        throw new Error("the channel was opened");
    }
    return response.status;
}

/**
 * A ward on the server, with every device named. Its first device wrote version 1 when it made
 * the ward, so the version it creates the ward on the server with is 2.
 */
export async function created(devices: Device[]): Promise<string>
{
    const document = newDocument();
    const response = await post(devices[0], version(document, 2, devices[0], devices));
    if (response.status !== 201)
    {
        throw new Error(`the ward was not created: ${response.status}`);
    }
    return document;
}
