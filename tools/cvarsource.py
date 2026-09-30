#!/usr/bin/env python3
"""The mod's cvars, console commands and ported plugins, read from its source (src/game/*/hidden).

Shared by web/build.py (the site's tables) and tools/server_cfg.py (the example server config).
Standard library only.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME_SRC = ROOT / "src" / "game"
PLUGIN_DIR = GAME_SRC / "server" / "hidden" / "plugins"

# Cvars and commands that are for development, internal, or left over from the SDK's test bot.
HIDDEN_NAMES = {
    "bot_forcefireweapon", "bot_forceattack2", "bot_forceattackon", "bot_flipout", "bot_changeclass",
    "bot_mimic_yaw_offset", "bot_sendcmd", "bot_crouch", "bot_add_test", "cl_showmypanel",
    "ToggleMyPanel", "setposx", "setposy", "blur", "hdn_gameui_unplaced", "-radiomenu", "-visible",
    "sv_motd_unload_on_dismissal", "cl_jimmeh",
}


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
