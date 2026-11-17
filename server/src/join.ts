import { Release } from "./download";

/**
 * The page an invitation link opens. The invitation is the part of the link after the #, which
 * a browser keeps to itself, so this page is the same for every invitation and the server never
 * sees one. Its script reads the invitation in the browser and names the installer's download
 * after it, and the installer hands the invitation on to the app from its own file name.
 */
export function joinPage(releases: Record<string, Release>): Response
{
    const offer = releases.windows === undefined
        ? "<p>No installer is available yet.</p>"
        : `<p><a id="download" href="/installer/windows" download="WardPlanning-Setup.exe">Download Ward Planning for Windows</a></p>
<ol>
<li>Click the download, then open the file once it has downloaded.</li>
<li>If Windows says "Windows protected your PC", click <b>More info</b>, then <b>Run anyway</b>.</li>
<li id="last">Ward Planning installs and opens.</li>
</ol>`;
    const page = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="referrer" content="no-referrer">
<title>Join a ward in Ward Planning</title>
</head>
<body>
<h1>Ward Planning</h1>
<p id="invited" hidden>You have been invited to share a ward in Ward Planning.</p>
${offer}
<script>
// The invitation: a ward's id of 22 characters, a dot and a key of 43, as the link carries it.
const invitation = location.hash.slice(1);
const download = document.getElementById("download");
if (/^[A-Za-z0-9_-]{22}\\.[A-Za-z0-9_-]{43}$/.test(invitation))
{
    document.getElementById("invited").hidden = false;
    if (download !== null)
    {
        download.download = "WardPlanning-Setup-" + invitation + ".exe";
        document.getElementById("last").textContent =
            "Ward Planning installs, opens, and asks whether to join the ward. Click OK.";
    }
}
</script>
</body>
</html>
`;
    return new Response(page, { headers: { "Content-Type": "text/html; charset=utf-8" } });
}

/**
 * The installer of a platform's current release, passed through from where the release keeps
 * it, so a page of this server can name the download. It sends no file name of its own, which
 * would take the place of the one the page gives.
 */
export async function installer(release: Release): Promise<Response>
{
    const upstream = await fetch(release.url);
    if (!upstream.ok)
    {
        return new Response(null, { status: 502 });
    }
    const headers = new Headers({ "Content-Type": "application/octet-stream" });
    const length = upstream.headers.get("Content-Length");
    if (length !== null)
    {
        headers.set("Content-Length", length);
    }
    return new Response(upstream.body, { headers });
}
