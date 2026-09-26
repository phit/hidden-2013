#!/usr/bin/env python3
"""Package the built mod for players and server owners.

Makes, in dist/:
  hidden2013-<version>-<platform>.<zip|tar.gz>          the hidden2013/ folder to extract into
                                                        steamapps/sourcemods (or a server's game dir)
  hidden2013-<version>-<platform>-symbols.<zip|tar.gz>  debug information (.pdb, or .dbg split from
                                                        the .so files), for crash stacks

The mod folder holds the files tracked in git under game/hidden2013, the compiled shaders and the
platform's binaries, which a build must have put in game/hidden2013/bin/<x64|linux64> and
game/hidden2013/shaders/fxc (in CI the build jobs' artifacts are unpacked there). No Beta 4b
content: players mount their own install (see gameinfo.txt).

Usage:
  py tools/package.py windows|linux [--version V] [--out DIR]
"""

import argparse
import datetime
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MOD = "hidden2013"
GAME = ROOT / "game" / MOD

PLATFORMS = {
    "windows": {"bin": "x64", "libs": ["client.dll", "server.dll", "game_shader_generic_hidden.dll"],
                "symbols": ".pdb", "archive": "zip"},
    "linux": {"bin": "linux64", "libs": ["client.so", "server.so", "game_shader_generic_hidden.so"],
              "symbols": ".dbg", "archive": "tar.gz"},
}


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()


def default_version():
    date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d")
    return f"{date}-{git('rev-parse', '--short', 'HEAD')}"


def split_debug_info(lib, dbg):
    """Move a .so's debug information into dbg and link the stripped library to it."""
    subprocess.run(["objcopy", "--only-keep-debug", str(lib), str(dbg)], check=True)
    subprocess.run(["objcopy", "--strip-debug", "--strip-unneeded", f"--add-gnu-debuglink={dbg}", str(lib)],
                   check=True, cwd=dbg.parent)


def write_archive(path, kind, root, files):
    """Write files (paths relative to root) into a zip or tar.gz, keeping executable bits in tars."""
    if kind == "zip":
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for rel in files:
                zf.write(root / rel, rel.as_posix())
    else:
        with tarfile.open(path, "w:gz", compresslevel=9) as tf:
            for rel in files:
                info = tf.gettarinfo(root / rel, rel.as_posix())
                info.uid = info.gid = 0
                info.mode = 0o755 if rel.suffix == ".so" else 0o644
                info.uname = info.gname = ""
                with open(root / rel, "rb") as f:
                    tf.addfile(info, f)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("platform", choices=PLATFORMS)
    ap.add_argument("--version", default=None, help="default: <UTC date>-<short commit>")
    ap.add_argument("--out", type=Path, default=ROOT / "dist")
    args = ap.parse_args()

    plat = PLATFORMS[args.platform]
    version = args.version or default_version()
    bindir = GAME / "bin" / plat["bin"]

    missing = [lib for lib in plat["libs"] if not (bindir / lib).exists()]
    shaders = sorted((GAME / "shaders" / "fxc").glob("*.vcs"))
    if missing:
        sys.exit(f"not built: {', '.join(missing)} in {bindir}")
    if not shaders:
        sys.exit("no compiled shaders in shaders/fxc (tools/build_shaders.ps1, on Windows)")

    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp) / MOD
        game_files, symbol_files = [], []

        for rel in git("ls-files", f"game/{MOD}").splitlines():
            src = ROOT / rel
            dst = stage / Path(rel).relative_to(f"game/{MOD}")
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            game_files.append(dst.relative_to(stage.parent))

        for vcs in shaders:
            dst = stage / "shaders" / "fxc" / vcs.name
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(vcs, dst)
            game_files.append(dst.relative_to(stage.parent))

        outbin = stage / "bin" / plat["bin"]
        outbin.mkdir(parents=True, exist_ok=True)
        for lib in plat["libs"]:
            dst = outbin / lib
            shutil.copy2(bindir / lib, dst)
            game_files.append(dst.relative_to(stage.parent))
            if args.platform == "windows":
                pdb = bindir / (Path(lib).stem + ".pdb")
                if pdb.exists():
                    shutil.copy2(pdb, outbin / pdb.name)
                    symbol_files.append((outbin / pdb.name).relative_to(stage.parent))
            else:
                dbg = outbin / (lib + ".dbg")
                split_debug_info(dst, dbg)
                symbol_files.append(dbg.relative_to(stage.parent))

        args.out.mkdir(parents=True, exist_ok=True)
        ext = plat["archive"]
        package = args.out / f"{MOD}-{version}-{args.platform}.{ext}"
        write_archive(package, ext, stage.parent, sorted(game_files))
        print(f"{package} ({len(game_files)} files, {package.stat().st_size / 1e6:.1f} MB)")

        if symbol_files:
            symbols = args.out / f"{MOD}-{version}-{args.platform}-symbols.{ext}"
            write_archive(symbols, ext, stage.parent, sorted(symbol_files))
            print(f"{symbols} ({len(symbol_files)} files, {symbols.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
