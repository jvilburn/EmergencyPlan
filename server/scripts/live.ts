// Runs one ward through a deployed server: npm run live -- <server address>
// It creates a ward, pushes to it, holds a channel through a push, and ends by deleting the ward.
import { IdBytes, NonceBytes, SealedKeyBytes, channelLine, encode, fingerprintOf, readVersion, writeLine } from "../src/wire.ts";

const server = process.argv[2];
const encoder = new TextEncoder();

function random(length: number): Uint8Array
{
    return crypto.getRandomValues(new Uint8Array(length));
}

function check(what: string, actual: unknown, expected: unknown): void
{
    const passed = JSON.stringify(actual) === JSON.stringify(expected);
    console.log(`${passed ? "ok  " : "FAIL"} ${what}: ${JSON.stringify(actual)}`);
    if (!passed)
    {
        console.log(`     expected ${JSON.stringify(expected)}`);
        process.exitCode = 1;
    }
}

const pair = (await crypto.subtle.generateKey({ name: "Ed25519" }, true, ["sign", "verify"])) as CryptoKeyPair;
const verifyKey = new Uint8Array(await crypto.subtle.exportKey("raw", pair.publicKey));
const publicKey = random(32);
const fingerprint = encode(fingerprintOf(publicKey, verifyKey));
const document = encode(random(IdBytes));

async function signed(line: string): Promise<Record<string, string>>
{
    const signature = await crypto.subtle.sign({ name: "Ed25519" }, pair.privateKey, encoder.encode(line));
    return {
        "Ward-Signer": `${encode(publicKey)}.${encode(verifyKey)}`,
        "Ward-Signature": encode(new Uint8Array(signature)),
    };
}

function version(number: number, recipients: string[], payload: Uint8Array): Uint8Array
{
    const header = JSON.stringify({
        format: 1,
        document,
        number,
        writer: fingerprint,
        nonce: encode(random(NonceBytes)),
        recipients: recipients.map((recipient) => ({ fingerprint: recipient, sealedKey: encode(random(SealedKeyBytes)) })),
    });
    const line = encoder.encode(header + "\n");
    const bytes = new Uint8Array(line.length + payload.length);
    bytes.set(line);
    bytes.set(payload, line.length);
    return bytes;
}

async function write(method: "POST" | "PUT", call: "create" | "push", body: Uint8Array): Promise<number>
{
    const path = method === "POST" ? "/documents" : `/documents/${document}`;
    const headers = await signed(writeLine(call, readVersion(body)!));
    return (await fetch(server + path, { method, headers, body })).status;
}

const second = version(2, [fingerprint], random(40000));
check("create", await write("POST", "create", second), 201);
check("create again", await write("POST", "create", second), 409);

const whole = await fetch(`${server}/documents/${document}`);
check("fetch", whole.status, 200);
check("fetch returns the bytes sent", encode(new Uint8Array(await whole.arrayBuffer())) === encode(second), true);
console.log(`     published ${whole.headers.get("Ward-Published")}`);

const messages: string[] = [];
const socket = new WebSocket(`${server.replace(/^http/, "ws")}/documents/${document}/channel`, {
    headers: await signed(channelLine(document)),
} as unknown as string[]);
const closed = new Promise<number>((resolve) => socket.addEventListener("close", (event) => resolve(event.code)));
socket.addEventListener("message", (event) => messages.push(event.data as string));
await new Promise((resolve, reject) =>
{
    socket.addEventListener("open", resolve);
    socket.addEventListener("error", reject);
});
await new Promise((resolve) => setTimeout(resolve, 1500));
check("channel opens with who is here", messages, [JSON.stringify({ present: [fingerprint], version: 2 })]);

check("push 3", await write("PUT", "push", version(3, [fingerprint], random(40000))), 200);
check("push 3 again", await write("PUT", "push", version(3, [fingerprint], random(64))), 409);
await new Promise((resolve) => setTimeout(resolve, 1500));
check("channel hears the push", messages[1], JSON.stringify({ present: [fingerprint], version: 3 }));

check("delete the ward", await write("PUT", "push", version(4, [], new Uint8Array(0))), 200);
check("channel closed by the delete", await closed, 1000);
check("push after delete", await write("PUT", "push", version(5, [fingerprint], random(64))), 403);
check("head after delete", (await (await fetch(`${server}/documents/${document}/head`)).json() as { number: number }).number, 4);
