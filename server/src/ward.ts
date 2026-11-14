import { DurableObject } from "cloudflare:workers";
import {
    Version,
    channelLine,
    readSigner,
    readVersion,
    routeOf,
    status,
    verifies,
    writeLine,
} from "./wire";

/** The current version of one ward, which is all the server keeps of it, and when it was published. */
interface Stored extends Version
{
    published: string;
}

const CurrentKey = "current";

/**
 * One ward: its current version, replaced only by a version whose number is above it, and the
 * live channel of every device holding the ward open.
 *
 * A Durable Object runs one event at a time, but another request can run while this one awaits.
 * Each call finishes its awaiting, which ends with the signature check, before it reads the
 * current version. From that read to storing the next version nothing awaits, so no other request
 * runs in between, and two writes of one number cannot both be kept.
 */
export class Ward extends DurableObject<Env>
{
    async fetch(request: Request): Promise<Response>
    {
        const route = routeOf(request.method, new URL(request.url).pathname);
        switch (route?.kind)
        {
            case "create":
                return this.create(request);
            case "push":
                return this.push(request, route.document);
            case "head":
            case "version":
                return this.read(route.kind);
            case "channel":
                return this.open(request, route.document);
            default:
                return status(404);
        }
    }

    /** Clients send nothing on the channel; their pings are protocol frames the platform answers. */
    webSocketMessage(): void
    {
    }

    /** A device closed its channel: the close is answered, which completes it, and the others are told. */
    webSocketClose(socket: WebSocket, code: number, reason: string): void
    {
        socket.close(code, reason);
        this.leave(socket);
    }

    webSocketError(socket: WebSocket): void
    {
        this.leave(socket);
    }

    /**
     * The first version, stored when no version exists and its signer is the writer its header
     * names among its recipients. The Worker hands a create to the ward its header names.
     */
    private async create(request: Request): Promise<Response>
    {
        const version = readVersion(new Uint8Array(await request.arrayBuffer()));
        const signer = readSigner(request.headers);
        if (version === null || signer === null)
        {
            return status(400);
        }
        const signed = await verifies(signer, writeLine("create", version));
        if (this.current() !== undefined)
        {
            return status(409);
        }
        if (!signed
            || version.header.writer !== signer.fingerprint
            || !version.header.recipients.includes(signer.fingerprint))
        {
            return status(403);
        }
        this.store(version);
        return status(201);
    }

    /**
     * A later version, stored when its signer is its writer, the current header names that
     * signer, and its number is above the current one.
     */
    private async push(request: Request, document: string): Promise<Response>
    {
        const version = readVersion(new Uint8Array(await request.arrayBuffer()));
        const signer = readSigner(request.headers);
        if (version === null || signer === null || version.header.document !== document)
        {
            return status(400);
        }
        const signed = await verifies(signer, writeLine("push", version));
        const current = this.current();
        if (current === undefined)
        {
            return status(404);
        }
        if (!signed
            || version.header.writer !== signer.fingerprint
            || !current.header.recipients.includes(signer.fingerprint))
        {
            return status(403);
        }
        if (version.header.number <= current.header.number)
        {
            return status(409);
        }
        this.store(version);
        for (const socket of this.ctx.getWebSockets())
        {
            if (!version.header.recipients.includes(this.deviceOf(socket)))
            {
                socket.close(1000, "This device is no longer named by the ward");
            }
        }
        this.tellEveryone(version.header.number, null);
        return status(200);
    }

    /** The current version's header line, or the whole of it, with when it was published. */
    private read(part: "head" | "version"): Response
    {
        const current = this.current();
        if (current === undefined)
        {
            return status(404);
        }
        const bytes = part === "head" ? current.bytes.subarray(0, current.headerLength) : current.bytes;
        return new Response(bytes, { status: 200, headers: { "Ward-Published": current.published } });
    }

    /** Opens a live channel for a device the current header names, replacing any it already has open. */
    private async open(request: Request, document: string): Promise<Response>
    {
        const signer = readSigner(request.headers);
        if (signer === null || request.headers.get("Upgrade")?.toLowerCase() !== "websocket")
        {
            return status(400);
        }
        const signed = await verifies(signer, channelLine(document));
        const current = this.current();
        if (current === undefined)
        {
            return status(404);
        }
        if (!signed || !current.header.recipients.includes(signer.fingerprint))
        {
            return status(403);
        }
        for (const old of this.ctx.getWebSockets(signer.fingerprint))
        {
            old.close(1000, "Replaced by a newer channel from this device");
        }
        const pair = new WebSocketPair();
        this.ctx.acceptWebSocket(pair[1], [signer.fingerprint]);
        this.tellEveryone(current.header.number, null);
        return new Response(null, { status: 101, webSocket: pair[0] });
    }

    private leave(socket: WebSocket): void
    {
        const current = this.current();
        if (current !== undefined)
        {
            this.tellEveryone(current.header.number, socket);
        }
    }

    /**
     * Sends every open channel who is here, in sorted order, and the current version's number.
     * A channel that is leaving is neither told nor listed.
     */
    private tellEveryone(number: number, leaving: WebSocket | null): void
    {
        const open = this.ctx.getWebSockets().filter((socket) => socket !== leaving && socket.readyState === WebSocket.OPEN);
        const present = [...new Set(open.map((socket) => this.deviceOf(socket)))].sort();
        const message = JSON.stringify({ present, version: number });
        for (const socket of open)
        {
            socket.send(message);
        }
    }

    /** The fingerprint of the device a channel was opened by, which is the channel's one tag. */
    private deviceOf(socket: WebSocket): string
    {
        return this.ctx.getTags(socket)[0];
    }

    private current(): Stored | undefined
    {
        return this.ctx.storage.kv.get<Stored>(CurrentKey);
    }

    private store(version: Version): void
    {
        const stored: Stored = { ...version, published: new Date().toISOString() };
        this.ctx.storage.kv.put(CurrentKey, stored);
    }
}
