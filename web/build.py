#!/usr/bin/env python3
"""Build the project site (GitHub Pages) into web/_out.

After NEOTOKYO;REBUILD's site: Markdown pages under web/src with a few "key: value" metadata lines
on top (title; summary, the description in link previews; image, their picture under img/),
wrapped in _header.html and _footer.html. Blog posts are web/src/blog/<YYYY-MM-DD>/index.md; the
home page lists them and atom.xml carries the newest.

Placeholders filled in at build time, each alone on its line:
  <!-- LATEST_RELEASE -->  the newest release and its packages (GitHub API; a link if offline)
  <!-- BLOG_LIST -->       every blog post, newest first
  <!-- CVARS -->           the mod's server and client cvars, read from the source
  <!-- COMMANDS -->        the mod's console commands, read from the source
  <!-- PLUGINS -->         the ported SourceMod plugins and their cvars, read from the source

Needs the markdown package (py -m pip install markdown).

Usage:
  py web/build.py [--serve]      --serve: then serve _out on http://localhost:8000
"""

import argparse
import datetime
import html
import json
import os
import re
import shutil
import sys
import urllib.request
from pathlib import Path

import markdown

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from cvarsource import GAME_SRC, read_cvars, read_commands, read_plugin, hidden_sources  # noqa: E402

WEB = Path(__file__).resolve().parent
ROOT = WEB.parent
SRC = WEB / "src"
OUT = WEB / "_out"

SITE_URL = "https://phit.github.io/hidden-2013/"
SITE_TITLE = "Hidden: Rebuild"
SITE_DESCRIPTION = "Hidden: Source Beta 4b, rebuilt on the current Source SDK 2013: the original game with native Linux servers, bots and the popular plugins built in."
DEFAULT_IMAGE = "docks.jpg"
THEME_COLOR = "#cc0000"
PREVIEW = "preview.jpg"
REPO = "phit/hidden-2013"
FEED_LIMIT = 5

# ---------------------------------------------------------------------------------------------
# Generated blocks (HTML, since they go straight into the Markdown)

def esc(s):
    return html.escape(s, quote=False)


def cvar_table(cvars, with_side=True):
    rows = ["<table>", "<tr><th>Cvar</th><th>Default</th>" + ("<th>Side</th>" if with_side else "") + "<th>What it does</th></tr>"]
    for c in cvars:
        where = {"client": "client", "server": "server", "shared": "server"}[c["side"]]
        rows.append(f'<tr><td><code>{esc(c["name"])}</code></td><td><code>{esc(c["default"])}</code></td>'
                    + (f"<td>{where}</td>" if with_side else "") + f'<td>{esc(c["help"])}</td></tr>')
    rows.append("</table>")
    return "\n".join(rows)


def command_table(commands):
    rows = ["<table>", "<tr><th>Command</th><th>Side</th><th>What it does</th></tr>"]
    for c in commands:
        where = {"client": "client", "server": "server", "shared": "server"}[c["side"]]
        rows.append(f'<tr><td><code>{esc(c["name"])}</code></td><td>{where}</td><td>{esc(c["help"])}</td></tr>')
    rows.append("</table>")
    return "\n".join(rows)


def cvars_block():
    plugin_dir = GAME_SRC / "server" / "hidden" / "plugins"
    files = [p for p in hidden_sources() if plugin_dir not in p.parents]
    # A plugin cvar the client predicts with has a copy there; it's listed with its plugin.
    plugin_names = {c["name"] for c in read_cvars(plugin_dir.glob("hidden_plugin_*.cpp"))}
    return cvar_table([c for c in read_cvars(files) if c["name"] not in plugin_names])


def commands_block():
    plugin_dir = GAME_SRC / "server" / "hidden" / "plugins"
    files = [p for p in hidden_sources() if plugin_dir not in p.parents]
    return command_table(read_commands(files))


def plugins_block():
    out = []
    plugin_dir = GAME_SRC / "server" / "hidden" / "plugins"
    for path in sorted(plugin_dir.glob("hidden_plugin_*.cpp")):
        p = read_plugin(path)
        anchor = path.stem.replace("hidden_plugin_", "")
        out.append(f'<h3 id="{anchor}">{esc(p["name"])}</h3>')
        origin = esc(p["origin"])
        if p["url"]:
            origin = f'<a href="{esc(p["url"])}">{origin}</a>'
        out.append(f'<p>{esc(p["description"])}<br><small>Ported from {origin}.</small></p>')
        if p["cvars"]:
            out.append(cvar_table(p["cvars"], with_side=False))
        if p["commands"]:
            out.append(command_table(p["commands"]))
    return "\n".join(out)


def latest_release_block():
    fallback = (f'<p>See the <a href="https://github.com/{REPO}/releases">releases on GitHub</a>; '
                f'<a href="https://github.com/{REPO}/releases/tag/latest">latest</a> is the newest test build.</p>')
    try:
        headers = {"Accept": "application/vnd.github+json", "User-Agent": "hidden-2013-site"}
        if os.environ.get("GITHUB_TOKEN"):  # CI: avoids the anonymous rate limit
            headers["Authorization"] = f"Bearer {os.environ['GITHUB_TOKEN']}"
        req = urllib.request.Request(f"https://api.github.com/repos/{REPO}/releases/latest", headers=headers)
        release = json.load(urllib.request.urlopen(req, timeout=20))
    except Exception as e:  # no release yet (404), or offline
        print(f"latest release: {e}; linking the releases page", file=sys.stderr)
        return fallback
    date = (release.get("published_at") or "")[:10]
    links = []
    for asset in release.get("assets", []):
        if "-symbols" in asset["name"]:
            continue
        platform = "Windows" if asset["name"].endswith(".zip") else "Linux"
        links.append(f'{platform}: <a href="{esc(asset["browser_download_url"])}">{esc(asset["name"])}</a>')
    return (f'<p>Current release: <a href="{esc(release["html_url"])}">{esc(release["tag_name"])}</a>, published {date}<br>\n'
            + "<br>\n".join(links) + "</p>\n" + fallback)


# ---------------------------------------------------------------------------------------------
# Pages

def split_meta(text):
    """lowdown-style metadata: "key: value" lines up to the first blank line."""
    meta, lines = {}, text.splitlines()
    i = 0
    while i < len(lines) and re.match(r"^[\w()-]+:\s", lines[i]):
        key, value = lines[i].split(":", 1)
        meta[key.strip().lower()] = value.strip()
        i += 1
    return meta, "\n".join(lines[i:])


def blog_posts():
    posts = []
    for md in sorted((SRC / "blog").glob("*/index.md"), reverse=True):
        meta, _ = split_meta(md.read_text(encoding="utf-8"))
        posts.append({"date": md.parent.name, "title": meta.get("title", md.parent.name),
                      "summary": meta.get("summary", ""), "path": md})
    return posts


def blog_list_block():
    items = [f'<li><a href="{SITE_PATH}blog/{p["date"]}/">{p["date"]} - {esc(p["title"])}</a></li>' for p in blog_posts()]
    return "<ul>\n" + "\n".join(items) + "\n</ul>"


BLOCKS = {
    "LATEST_RELEASE": latest_release_block,
    "BLOG_LIST": blog_list_block,
    "CVARS": cvars_block,
    "COMMANDS": commands_block,
    "PLUGINS": plugins_block,
}

# The site's path below the host ("/hidden-2013/" on GitHub Pages), for absolute links.
SITE_PATH = "/" + SITE_URL.split("://", 1)[1].split("/", 1)[1]


def render(md_text):
    cache = {}

    def fill(m):
        name = m.group(1)
        if name not in cache:
            cache[name] = BLOCKS[name]()
        return cache[name]

    md_text = re.sub(r"^<!-- (" + "|".join(BLOCKS) + r") -->$", fill, md_text, flags=re.M)
    md_text = md_text.replace("{{root}}", SITE_PATH)
    return markdown.markdown(md_text, extensions=["tables", "fenced_code", "toc", "md_in_html"])


def social_meta(meta, url, date, image):
    """Open Graph and Twitter tags for link previews (Discord, Steam, ...) and JSON-LD for search
    engines. A page's "summary:" is its description; image is the preview picture's URL."""
    title = meta.get("title", SITE_TITLE)
    if title == "Home":
        title = SITE_TITLE
    description = meta.get("summary", SITE_DESCRIPTION)
    tags = [
        ("name", "description", description),
        ("name", "theme-color", THEME_COLOR),
        ("property", "og:site_name", SITE_TITLE),
        ("property", "og:type", "article" if date else "website"),
        ("property", "og:title", title),
        ("property", "og:description", description),
        ("property", "og:url", url),
        ("property", "og:image", image),
        ("name", "twitter:card", "summary_large_image"),
    ]
    if date:
        tags.append(("property", "article:published_time", date))
    lines = [f'<meta {kind}="{key}" content="{html.escape(value)}">' for kind, key, value in tags]
    lines.append(f'<link rel="canonical" href="{url}">')

    org = {"@type": "Organization", "name": SITE_TITLE, "url": SITE_URL, "logo": SITE_URL + "img/logo.png"}
    if date:
        data = {"@context": "https://schema.org", "@type": "BlogPosting", "headline": title,
                "description": description, "datePublished": date, "url": url, "mainEntityOfPage": url,
                "image": image, "author": org, "publisher": org}
    elif url == SITE_URL:
        data = {"@context": "https://schema.org", "@type": "WebSite", "name": SITE_TITLE, "url": url,
                "description": description, "publisher": org}
    else:
        data = {"@context": "https://schema.org", "@type": "WebPage", "name": title, "url": url,
                "description": description, "isPartOf": {"@type": "WebSite", "name": SITE_TITLE, "url": SITE_URL}}
    # "</" can't appear inside the script element.
    lines.append('<script type="application/ld+json">' + json.dumps(data).replace("</", "<\\/") + "</script>")
    return "\n    ".join(lines)


def page(md_path, header, footer, url):
    meta, body = split_meta(md_path.read_text(encoding="utf-8"))
    title = meta.get("title", SITE_TITLE)
    is_post = md_path.parent.parent == SRC / "blog"
    # A post's own screenshot, preview.jpg next to it, heads its link previews and follows its intro
    # (before the first section); other pages use an image: under img/, or the default.
    if is_post and (md_path.parent / PREVIEW).exists():
        image = url + PREVIEW
        figure = f"![{title}]({PREVIEW})\n\n"
        m = re.search(r"^## ", body, re.M)
        body = body[:m.start()] + figure + body[m.start():] if m else body.rstrip() + "\n\n" + figure
    else:
        image = SITE_URL + "img/" + meta.get("image", DEFAULT_IMAGE)
    head = (header.replace("{{title}}", esc(title))
            .replace("{{meta}}", social_meta(meta, url, md_path.parent.name if is_post else None, image))
            .replace("{{root}}", SITE_PATH))
    return head + render(body) + footer.replace("{{root}}", SITE_PATH)


def atom_feed():
    posts = blog_posts()[:FEED_LIMIT]
    updated = posts[0]["date"] if posts else datetime.date.today().isoformat()
    out = ['<?xml version="1.0" encoding="utf-8"?>', '<feed xmlns="http://www.w3.org/2005/Atom">',
           f'<title type="text">{SITE_TITLE}</title>', f"<updated>{updated}T00:00:00Z</updated>",
           f"<id>{SITE_URL}blog/</id>", f"<author><name>{SITE_TITLE}</name></author>",
           f'<link rel="alternate" type="text/html" href="{SITE_URL}"/>',
           f'<link rel="self" type="application/atom+xml" href="{SITE_URL}atom.xml"/>']
    for p in posts:
        url = f'{SITE_URL}blog/{p["date"]}/'
        _, body = split_meta(p["path"].read_text(encoding="utf-8"))
        out += ["<entry>", f'<title>{esc(p["title"])}</title>', f'<link rel="alternate" type="text/html" href="{url}"/>',
                f'<updated>{p["date"]}T00:00:00Z</updated>', f'<published>{p["date"]}T00:00:00Z</published>',
                f'<summary>{esc(p["summary"])}</summary>', f'<content type="html">{html.escape(render(body))}</content>',
                f"<id>{url}</id>", "</entry>"]
    out.append("</feed>")
    return "\n".join(out) + "\n"


def build():
    if OUT.exists():
        shutil.rmtree(OUT)
    header = (SRC / "_header.html").read_text(encoding="utf-8")
    footer = (SRC / "_footer.html").read_text(encoding="utf-8")
    for src in sorted(SRC.rglob("*")):
        if src.is_dir() or src.name.startswith("_"):
            continue
        rel = src.relative_to(SRC)
        if src.suffix == ".md":
            # page.md and dir/index.md both become .../index.html, for clean URLs.
            dst = OUT / (rel.parent / "index.html" if src.name == "index.md" else rel.with_suffix("") / "index.html")
            dst.parent.mkdir(parents=True, exist_ok=True)
            rel_dir = dst.parent.relative_to(OUT).as_posix()
            url = SITE_URL if rel_dir == "." else f"{SITE_URL}{rel_dir}/"
            dst.write_text(page(src, header, footer, url), encoding="utf-8")
        else:
            dst = OUT / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
        print(f"  {dst.relative_to(OUT)}")
    (OUT / "atom.xml").write_text(atom_feed(), encoding="utf-8")
    (OUT / ".nojekyll").write_text("")
    print(f"built {OUT}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--serve", action="store_true", help="serve _out on http://localhost:8000 after building")
    args = ap.parse_args()
    build()
    if args.serve:
        import functools, http.server
        # Serve so that SITE_PATH works locally: _out appears at /hidden-2013/.
        root = OUT.parent / "_serve"
        if root.exists():
            shutil.rmtree(root)
        shutil.copytree(OUT, root / SITE_PATH.strip("/"))
        handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
        print(f"serving http://localhost:8000{SITE_PATH}")
        http.server.ThreadingHTTPServer(("localhost", 8000), handler).serve_forever()


if __name__ == "__main__":
    main()
