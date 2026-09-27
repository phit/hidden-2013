#!/usr/bin/env python3
"""Build the project site (GitHub Pages) into web/_out.

After NEOTOKYO;REBUILD's site: Markdown pages under web/src with a few "key: value" metadata lines
on top (title, and summary for blog posts), wrapped in _header.html and _footer.html. Blog posts are
web/src/blog/<YYYY-MM-DD>/index.md; the home page lists them and atom.xml carries the newest.

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

WEB = Path(__file__).resolve().parent
ROOT = WEB.parent
SRC = WEB / "src"
OUT = WEB / "_out"
GAME_SRC = ROOT / "src" / "game"

SITE_URL = "https://phit.github.io/hidden-2013/"
SITE_TITLE = "Hidden: Rebuild"
REPO = "phit/hidden-2013"
FEED_LIMIT = 5

# Cvars and commands that are for development, internal, or left over from the SDK's test bot.
HIDDEN_NAMES = {
    "bot_forcefireweapon", "bot_forceattack2", "bot_forceattackon", "bot_flipout", "bot_changeclass",
    "bot_mimic_yaw_offset", "bot_sendcmd", "bot_crouch", "bot_add_test", "cl_showmypanel",
    "ToggleMyPanel", "setposx", "setposy", "blur", "hdn_gameui_unplaced", "-radiomenu", "-visible",
    "sv_motd_unload_on_dismissal", "cl_jimmeh",
}


# ---------------------------------------------------------------------------------------------
# Reading the source

STRING = r'"(?:[^"\\]|\\.)*"'
# ConVar name( "name", default, flags, "help", ... ): the default may be a #define.
CONVAR = re.compile(
    r'\bConVar\s+\w+\s*\(\s*(' + STRING + r')\s*,\s*(' + STRING + r'|\w+)\s*(?:,\s*([^,")]+))?'
    r'(?:,\s*((?:' + STRING + r'\s*)+))?', re.S)
CON_COMMAND = re.compile(r'\bCON_COMMAND(?:_F)?\s*\(\s*(\w+)\s*,\s*((?:' + STRING + r'\s*)+)(?:,\s*([^)]+))?\)')
# ConCommand name( "name", callback, "help", flags ); on one line; the callback may be a lambda.
CONCOMMAND = re.compile(r'\bConCommand\s+\w+\s*\(\s*(' + STRING + r')(.*)\);\s*$', re.M)
DEFINE = re.compile(r'^#define\s+(\w+)\s+(' + STRING + r')', re.M)


def c_string(literals):
    """The value of one or more adjacent C string literals."""
    parts = re.findall(STRING, literals or "")
    return "".join(bytes(p[1:-1], "utf-8").decode("unicode_escape") for p in parts)


def side(path):
    rel = path.relative_to(GAME_SRC).parts[0]
    return {"client": "client", "server": "server", "shared": "shared"}[rel]


def hidden_sources(subdir=""):
    for d in ("server/hidden", "shared/hidden", "client/hidden"):
        base = GAME_SRC / d / subdir if subdir else GAME_SRC / d
        if base.is_dir():
            yield from sorted(base.rglob("*.cpp"))


def read_cvars(files):
    cvars = []
    for path in files:
        text = path.read_text(encoding="utf-8", errors="replace")
        defines = {k: c_string(v) for k, v in DEFINE.findall(text)}
        for m in CONVAR.finditer(text):
            raw_default = m.group(2)
            default = c_string(raw_default) if raw_default.startswith('"') else defines.get(raw_default, raw_default)
            name, flags, help_text = c_string(m.group(1)), (m.group(3) or "").strip(), c_string(m.group(4))
            if name in HIDDEN_NAMES or "FCVAR_CHEAT" in flags:
                continue
            cvars.append({"name": name, "default": default, "help": help_text, "side": side(path), "file": path})
    return sorted(cvars, key=lambda c: c["name"])


def read_commands(files):
    commands = []
    for path in files:
        text = path.read_text(encoding="utf-8", errors="replace")
        found = [(m.group(1), c_string(m.group(2)), m.group(3) or "") for m in CON_COMMAND.finditer(text)]
        for m in CONCOMMAND.finditer(text):
            literals = re.findall(STRING, m.group(2))
            found.append((c_string(m.group(1)), c_string(literals[-1]) if literals else "", m.group(2)))
        for name, help_text, flags in found:
            if name in HIDDEN_NAMES or "FCVAR_CHEAT" in flags:
                continue
            commands.append({"name": name, "help": help_text, "side": side(path), "file": path})
    return sorted(commands, key=lambda c: c["name"].lstrip("+-"))


def read_plugin(path):
    """A plugin's name, origin and original thread, from its file header."""
    header = path.read_text(encoding="utf-8", errors="replace").split("#include", 1)[0]
    lines = [re.sub(r"^//\s?", "", l).strip() for l in header.splitlines()]
    purpose = " ".join(l for l in lines if l and not l.startswith("=")).split("Original:")[0]
    purpose = re.sub(r"^Purpose:\s*", "", purpose).replace("See docs/spec/plugins.md.", "").strip()
    m = re.match(r"(.+?) \((.+?)\):\s*(.*)", purpose)
    name, origin, description = (m.group(1), m.group(2), m.group(3)) if m else (path.stem, "", purpose)
    description = description[:1].upper() + description[1:]
    url = re.search(r"Original:\s*(\S+)", header)
    return {"name": name, "origin": origin, "description": description, "url": url.group(1) if url else None,
            "cvars": read_cvars([path]), "commands": read_commands([path])}


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
    return cvar_table(read_cvars(files))


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


def page(md_path, header, footer):
    meta, body = split_meta(md_path.read_text(encoding="utf-8"))
    title = meta.get("title", SITE_TITLE)
    head = header.replace("{{title}}", esc(title)).replace("{{root}}", SITE_PATH)
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
            dst.write_text(page(src, header, footer), encoding="utf-8")
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
