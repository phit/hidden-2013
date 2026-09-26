#!/usr/bin/env python3
"""Import Hidden: Source Beta 4b content into game/hidden2013/legacy.

The new mod reuses the old mod's assets but none of its code, so only content
folders are copied, plus the stock defaults in cfg/ (default binds, server
configs). bin/, shaders/ (2006 shader DLL output) and per-install state such as
config.cfg, downloads and demos are skipped.

Sources, in any combination and order (later ones win):
  - an installed mod folder, e.g. ...\\steamapps\\sourcemods\\hidden
  - ianua-base-hsb4b.tar.gz (the repackaged official installer)
  - a .zip; only .bsp files are taken from it, into maps/
    (ghs-patch-assets-v2.zip for the fixed maps, ghs-mappack-v1.zip)

Usage:
  py tools/import_legacy.py SOURCE [SOURCE ...] [--clean]
"""
import argparse
import os
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / "game" / "hidden2013" / "legacy"

CONTENT_DIRS = {"materials", "models", "sound", "maps", "scripts", "resource", "media"}

# The files Beta 4b shipped in cfg/. Anything else there is a player's own state.
CFG_FILES = {"config_default.cfg", "listenserver.cfg", "server.cfg", "tutorial.cfg", "valve.rc",
             "settings_default.scr", "user_default.scr"}


def content_relpath(parts):
    """Map an archive/member path to a path under legacy/, or None to skip it.

    Accepts paths with or without a leading 'hidden/' component.
    """
    parts = [p for p in parts if p not in ("", ".")]
    if parts and parts[0].lower() == "hidden":
        parts = parts[1:]
    if len(parts) == 2 and parts[0].lower() == "cfg" and parts[1].lower() in CFG_FILES:
        return Path("cfg", parts[1].lower())
    if len(parts) < 2 or parts[0].lower() not in CONTENT_DIRS:
        return None
    parts[0] = parts[0].lower()
    if parts[0] == "maps":
        # The engine lowercases map lookups; keep names resolvable on Linux too.
        parts[-1] = parts[-1].lower()
    return Path(*parts)


def write(rel, data, stats):
    out = DEST / rel
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    stats[rel.parts[0]] = stats.get(rel.parts[0], 0) + 1


def import_dir(src, stats):
    src = src / "hidden" if (src / "hidden" / "gameinfo.txt").exists() else src
    for dirpath, _, files in os.walk(src):
        for name in files:
            full = Path(dirpath) / name
            rel = content_relpath(full.relative_to(src).parts)
            if rel:
                write(rel, full.read_bytes(), stats)


def import_tar(src, stats):
    with tarfile.open(src) as tf:
        for m in tf:
            if not m.isfile():
                continue
            rel = content_relpath(m.name.split("/"))
            if rel:
                write(rel, tf.extractfile(m).read(), stats)


def import_zip(src, stats):
    with zipfile.ZipFile(src) as zf:
        for info in zf.infolist():
            name = info.filename
            if info.is_dir() or not name.lower().endswith(".bsp"):
                continue
            write(Path("maps", Path(name).name.lower()), zf.read(info), stats)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sources", nargs="+", type=Path)
    ap.add_argument("--clean", action="store_true", help="delete legacy/ before importing")
    args = ap.parse_args()

    if args.clean and DEST.exists():
        shutil.rmtree(DEST)
    DEST.mkdir(parents=True, exist_ok=True)

    for src in args.sources:
        stats = {}
        name = src.name.lower()
        if src.is_dir():
            import_dir(src, stats)
        elif name.endswith((".tar.gz", ".tgz", ".tar")):
            import_tar(src, stats)
        elif name.endswith(".zip"):
            import_zip(src, stats)
        else:
            sys.exit(f"don't know how to import {src}")
        summary = ", ".join(f"{k} {v}" for k, v in sorted(stats.items())) or "nothing"
        print(f"{src}: {summary}")

    print(f"-> {DEST}")


if __name__ == "__main__":
    main()
