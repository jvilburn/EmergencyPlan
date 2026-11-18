import { Release } from "./download";

/**
 * What an invitation looks like after the # of its link: a ward's document id of 22 characters,
 * a dot and the invite's seed of 43, all base64url. The app writes it (InvitationLink::code) and
 * the installer finds it in its own file name (packaging/windows/invitation.iss).
 */
export const InvitationPattern = "^[A-Za-z0-9_-]{22}\\.[A-Za-z0-9_-]{43}$";

/**
 * What a link carries after its #: the invitation, then a dot and the ward's name,
 * percent-encoded, which is there for the person to read and decides nothing. Null when the
 * invitation does not read. The join page runs this in the browser, from this function's own
 * text, so it uses nothing outside itself.
 */
export function readLink(hash: string, pattern: string): { invitation: string; ward: string } | null
{
    const invitation = hash.slice(0, 66);
    if (!new RegExp(pattern).test(invitation) || (hash.length > 66 && hash[66] !== "."))
    {
        return null;
    }
    let ward = "";
    try
    {
        ward = decodeURIComponent(hash.slice(67));
    }
    catch
    {
        ward = "";
    }
    return { invitation, ward };
}

/**
 * The name the installer is downloaded under: WardPlanning, the ward's name without a closing
 * "Ward", and the invitation, as WardPlanning-Tanglewood-<invitation>.exe. Of the ward's name
 * only letters and digits are kept, so the one dot before .exe is the invitation's own, which is
 * how the installer finds the invitation. The join page runs this in the browser, as readLink.
 */
export function downloadName(invitation: string, ward: string): string
{
    const label = ward.replace(/\s+ward\s*$/i, "").replace(/[^A-Za-z0-9]+/g, "-").slice(0, 40).replace(/^-+|-+$/g, "");
    return "WardPlanning-" + (label === "" ? "Setup" : label) + "-" + invitation + ".exe";
}

/**
 * The page an invitation link opens. The invitation is the part of the link after the #, which
 * a browser keeps to itself, so this page is the same for every invitation and the server never
 * sees one. Its script reads the invitation in the browser and names the installer's download
 * after it, and the installer hands the invitation on to the app from its own file name. A link
 * whose invitation does not read, as one cut short in an email, is told so in place of a
 * download that would join nothing.
 */
export function joinPage(releases: Record<string, Release>): Response
{
    const offer = releases.windows === undefined
        ? "<p>No installer is available yet.</p>"
        : `<p>Ward Planning runs on Windows. Open this link on your Windows computer.</p>
<p><a id="download" href="/installer/windows" download="WardPlanning-Setup.exe">Download Ward Planning for Windows</a></p>
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
<title>A ward shared with you in Ward Planning</title>
<style>
body { font-family: system-ui, sans-serif; max-width: 34em; margin: 2em auto; padding: 0 1em; line-height: 1.5; }
#invited { font-size: 1.2em; font-weight: bold; }
#download { display: inline-block; padding: 0.7em 1.4em; border-radius: 0.4em; background: #1f5fa8; color: #ffffff;
    font-weight: bold; text-decoration: none; }
li { margin-bottom: 0.4em; }
</style>
</head>
<body>
<h1>Ward Planning</h1>
<p id="invited" hidden>A ward has been shared with you in Ward Planning.</p>
<p id="incomplete" hidden>This invitation link is not complete. Ask the sender to send it again.</p>
<div id="offer">
${offer}
</div>
<script>
const readLink = (${readLink.toString()});
const downloadName = (${downloadName.toString()});
const hash = location.hash.slice(1);
const link = readLink(hash, ${JSON.stringify(InvitationPattern)});
const download = document.getElementById("download");
if (link !== null)
{
    const invited = document.getElementById("invited");
    if (link.ward !== "")
    {
        invited.textContent = link.ward + " has been shared with you in Ward Planning.";
    }
    invited.hidden = false;
    if (download !== null)
    {
        download.download = downloadName(link.invitation, link.ward);
        document.getElementById("last").textContent =
            "Ward Planning installs and opens. It asks for a name for this computer, your name and your email, "
            + "then asks whether to open "
            + (link.ward !== "" ? link.ward : "the ward") + ".";
    }
}
else if (hash !== "")
{
    document.getElementById("incomplete").hidden = false;
    document.getElementById("offer").hidden = true;
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
 * would take the place of the one the page gives. 502 when the release's file cannot be had.
 */
export async function installer(release: Release): Promise<Response>
{
    let upstream: Response;
    try
    {
        upstream = await fetch(release.url);
    }
    catch
    {
        return new Response(null, { status: 502 });
    }
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
