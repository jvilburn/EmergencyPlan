import { downloadPage } from "./download";
import releases from "./releases.json";
import { Route, readVersion, routeOf, status } from "./wire";

export { Ward } from "./ward";

/**
 * The Worker in front of the wards: it limits each address, answers the release calls itself,
 * and hands every document call to the Ward object named by its document id.
 */
export default {
    async fetch(request: Request, env: Env): Promise<Response>
    {
        const route = routeOf(request.method, new URL(request.url).pathname);
        if (route === null)
        {
            return status(404);
        }
        const limit = limitOf(route, env);
        if (limit !== null && !(await limit.limit({ key: request.headers.get("CF-Connecting-IP") ?? "" })).success)
        {
            return status(429);
        }
        switch (route.kind)
        {
            case "releases":
                return Response.json(releases);
            case "download":
                return downloadPage(releases);
            case "create":
            {
                const bytes = new Uint8Array(await request.arrayBuffer());
                const version = readVersion(bytes);
                if (version === null)
                {
                    return status(400);
                }
                return wardOf(env, version.header.document).fetch(request.url, { method: "POST", headers: request.headers, body: bytes });
            }
            default:
                return wardOf(env, route.document).fetch(request);
        }
    },
} satisfies ExportedHandler<Env>;

/**
 * The limit a call counts against: creating a document, or reading one. A push or a channel open
 * stores and reveals nothing unless the Ward verifies its signer, so neither is limited.
 */
function limitOf(route: Route, env: Env): RateLimit | null
{
    switch (route.kind)
    {
        case "create":
            return env.CREATE_LIMIT;
        case "head":
        case "version":
            return env.READ_LIMIT;
        default:
            return null;
    }
}

function wardOf(env: Env, document: string): DurableObjectStub
{
    return env.WARD.get(env.WARD.idFromName(document));
}
