#!/usr/bin/env python3
"""Write cfg/server_example.cfg: every server cvar of the mod and its plugins with its default and
help text, read from the source, for server owners to copy to cfg/server.cfg. The packages carry it
(tools/package.py); it isn't in git, so it can't go stale.

Usage:
  py tools/server_cfg.py [OUT]      default: game/hidden2013/cfg/server_example.cfg
"""

import sys
import textwrap
from pathlib import Path

from cvarsource import GAME_SRC, PLUGIN_DIR, ROOT, hidden_sources, read_cvars, read_plugin

HEADER = """\
// Hidden: Rebuild example server config, generated from the source of this release.
//
// Copy it to cfg/server.cfg and change what you need; it runs at every map start. Updates replace
// this file but never your server.cfg. Without a server.cfg of its own the server runs the Beta 4b
// install's cfg/server.cfg.
//
// Only the basics below are set. Every other line is commented out at its default: remove the //
// in front of a cvar to change it.
"""

BASICS = [
    ("hostname", '"Hidden: Rebuild"', "Server name in the server browser"),
    ("sv_password", '""', "Password players need to join; empty for none"),
    ("sv_lan", "0", "0 for an internet server, 1 for LAN only"),
    ("sv_region", "255", "0 US East, 1 US West, 2 South America, 3 Europe, 4 Asia, 5 Australia, "
                         "6 Middle East, 7 Africa, 255 world"),
    ("mp_timelimit", "20", "Minutes on a map before the next one in cfg/mapcycle.txt (Beta 4b's server.cfg: 20)"),
    ("mp_friendlyfire", "0", "Marines can hurt each other"),
    ("sv_alltalk", "0", "The Hidden and the marines hear each other's voice chat"),
]


def wrap(text):
    return "\n".join("// " + line for line in textwrap.wrap(text, 96)) if text else ""


def cvar_lines(cvars):
    out = []
    for c in cvars:
        if wrap(c["help"]):
            out.append(wrap(c["help"]))
        value = c["default"] if c["default"].replace(".", "").lstrip("-").isdigit() else f'"{c["default"]}"'
        out.append(f'//{c["name"]} {value}')
        out.append("")
    return out


def server_cvars(files):
    # Client cvars belong in players' configs; debug output isn't for a server's config.
    return [c for c in read_cvars(files) if c["side"] != "client" and "debug" not in c["name"]]


def generate():
    lines = [HEADER, "// ---- Basics", ""]
    for name, value, help_text in BASICS:
        lines += [wrap(help_text), f"{name} {value}", ""]

    plugin_names = {c["name"] for c in read_cvars(PLUGIN_DIR.glob("hidden_plugin_*.cpp"))}
    game = [c for c in server_cvars(p for p in hidden_sources() if PLUGIN_DIR not in p.parents)
            if c["name"] not in plugin_names]
    lines += ["", "// ---- Game", ""] + cvar_lines(game)

    lines += ["", "// ---- Plugins (ported SourceMod plugins; each is off until its cvar turns it on)", ""]
    for path in sorted(PLUGIN_DIR.glob("hidden_plugin_*.cpp")):
        plugin = read_plugin(path)
        cvars = [c for c in plugin["cvars"] if "debug" not in c["name"]]
        if cvars:
            lines += [f"// -- {plugin['name']}", ""] + cvar_lines(cvars)

    return "\n".join(lines).rstrip() + "\n"


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "game" / "hidden2013" / "cfg" / "server_example.cfg"
    out.write_bytes(generate().encode("utf-8"))
    print(out)


if __name__ == "__main__":
    main()
