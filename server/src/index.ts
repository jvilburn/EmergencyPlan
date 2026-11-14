import releases from "./releases.json";
import { IdBytes, Route, decode, readVersion, routeOf, status } from "./wire";

export { Ward } from "./ward";

/** The current release of one platform: its version and its installer's download address. */
interface Release
{
    version: string;
    url: string;
}

/** The name each platform in `releases.json` has on the download page. */
const PlatformNames: Record<string, string> = { windows: "Windows", macos: "macOS" };

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
                if (decode(route.document, IdBytes) === null)
                {
                    // A push's header cannot name a malformed id, so a push to one is malformed.
                    return status(route.kind === "push" ? 400 : 404);
                }
                return wardOf(env, route.document).fetch(request);
        }
    },
} satisfies ExportedHandler<Env>;

/** The limit a call counts against: creating a document, or reading one. Pushes and channels are signed by key holders. */
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

/** A page offering each platform's installer, for people who do not have the app yet. */
function downloadPage(platforms: Record<string, Release>): Response
{
    const links = Object.entries(platforms)
        .map(([platform, release]) => `<li><a href="${release.url}">Download for ${PlatformNames[platform]}</a> (${release.version})</li>`)
        .join("\n");
    const page = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Download Ward Planning</title>
</head>
<body>
<h1>Ward Planning</h1>
<ul>
${links}
</ul>
</body>
</html>
`;
    return new Response(page, { headers: { "Content-Type": "text/html; charset=utf-8" } });
}
