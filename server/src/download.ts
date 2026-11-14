/** The current release of one platform: the platform's name, its version and its installer's download address. */
export interface Release
{
    name: string;
    version: string;
    url: string;
}

/** A page offering each platform's installer, for people who do not have the app yet. */
export function downloadPage(releases: Record<string, Release>): Response
{
    const links = Object.values(releases)
        .map((release) => `<li><a href="${release.url}">Download for ${release.name}</a> (${release.version})</li>`);
    const offer = links.length > 0 ? `<ul>\n${links.join("\n")}\n</ul>` : "<p>No installer is available yet.</p>";
    const page = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Download Ward Planning</title>
</head>
<body>
<h1>Ward Planning</h1>
${offer}
</body>
</html>
`;
    return new Response(page, { headers: { "Content-Type": "text/html; charset=utf-8" } });
}
