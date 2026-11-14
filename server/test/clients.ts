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

export function newDocument(): string
{
    return encode(random(IdBytes));
}

export function random(length: number): Uint8Array
{
    return crypto.getRandomValues(new Uint8Array(length));
}

/** Version number of document, written by writer and sealed to recipients, with a payload of random bytes. */
export function version(
    document: string,
    number: number,
    writer: Device,
    recipients: Device[],
    payload: Uint8Array = random(64)): Uint8Array
{
    return framed(JSON.stringify({
        format: 1,
        document,
        number,
        writer: writer.fingerprint,
        nonce: encode(random(NonceBytes)),
        recipients: recipients.map((device) => ({ fingerprint: device.fingerprint, sealedKey: encode(random(SealedKeyBytes)) })),
    }), payload);
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

/** A create of body, signed by device. */
export async function post(device: Device, body: Uint8Array): Promise<Response>
{
    const parsed = readVersion(body);
    const document = parsed === null ? newDocument() : parsed.header.document;
    const headers = parsed === null ? {} : await signed(device, writeLine("create", parsed));
    return ward(document).fetch(`${origin}/documents`, { method: "POST", headers, body });
}

/** A push of body to document, signed by device. */
export async function put(device: Device, document: string, body: Uint8Array): Promise<Response>
{
    const parsed = readVersion(body);
    const headers = parsed === null ? {} : await signed(device, writeLine("push", parsed));
    return ward(document).fetch(`${origin}/documents/${document}`, { method: "PUT", headers, body });
}

export function head(document: string): Promise<Response>
{
    return ward(document).fetch(`${origin}/documents/${document}/head`);
}

export function get(document: string): Promise<Response>
{
    return ward(document).fetch(`${origin}/documents/${document}`);
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
        socket.addEventListener("message", (event) =>
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
        });
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
}

/** Opens document's live channel as device, answering the refusal when it is refused. */
export async function open(device: Device, document: string, line: string = channelLine(document)): Promise<Channel | Response>
{
    const response = await ward(document).fetch(`${origin}/documents/${document}/channel`, {
        headers: { ...(await signed(device, line)), Upgrade: "websocket" },
    });
    return response.webSocket === null ? response : new Channel(response.webSocket);
}

/** Opens a channel that must be accepted. */
export async function opened(device: Device, document: string): Promise<Channel>
{
    const channel = await open(device, document);
    if (!(channel instanceof Channel))
    {
        throw new Error(`the channel was refused with ${channel.status}`);
    }
    return channel;
}

/** A ward whose version 2 its first device created, with every device named. */
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
