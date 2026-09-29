#!/usr/bin/env python3
"""Package the built mod for players and server owners.

Makes, in dist/:
  hidden2013-<version>-<platform>.<zip|tar.gz>          the hidden2013/ folder to extract into
                                                        steamapps/sourcemods
  hidden2013-<version>-<platform>-standalone.<...>      the same with the mod's own launcher, for a
                                                        folder outside sourcemods (the launcher app
                                                        installs this, next to Beta 4b's hidden/)
  hidden2013-<version>-<platform>-server.<zip|tar.gz>   for the folder of Source SDK Base 2013
                                                        Dedicated Server (SteamCMD 244310): the
                                                        64-bit launcher it lacks and hidden2013/
                                                        with the dedicated server library
  hidden2013-<version>-<...>-symbols.<zip|tar.gz>       debug information (.pdb, or .dbg split from
                                                        the .so files), for crash stacks

The mod folder holds the files tracked in git under game/hidden2013, the compiled shaders and the
platform's binaries, which a build must have put in game/hidden2013/bin/<x64|linux64> and
game/hidden2013/shaders/fxc (in CI the build jobs' artifacts are unpacked there). No Beta 4b
content: players mount their own install (see gameinfo.txt).

Usage:
  py tools/package.py windows|linux|windows-server|linux-server [--version V] [--out DIR]
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

# "root": other files, (source, destination relative to the folder the package is extracted into).
PLATFORMS = {
    "windows": {"bin": "x64", "libs": ["client.dll", "server.dll", "game_shader_generic_hidden.dll"],
                "symbols": ".pdb", "archive": "zip", "shaders": True, "root": []},
    "linux": {"bin": "linux64", "libs": ["client.so", "server.so", "game_shader_generic_hidden.so"],
              "symbols": ".dbg", "archive": "tar.gz", "shaders": True,
              "root": [(ROOT / "tools" / "lowercase_beta4b.sh", "hidden2013/lowercase_beta4b.sh")]},
    # Started from its own folder rather than by Steam, the launcher keeps its -steam and the game is
    # VAC-secured without launch options. It finds SDK Base through the Steam API. The symbols are
    # the same as the sourcemods package's.
    "windows-standalone": {"bin": "x64", "libs": ["client.dll", "server.dll", "game_shader_generic_hidden.dll"],
                           "symbols": None, "archive": "zip", "shaders": True,
                           "root": [(ROOT / "game" / "hidden2013_win64.exe", "hidden2013_win64.exe"),
                                    (ROOT / "game" / "bin" / "x64" / "steam_api64.dll", "bin/x64/steam_api64.dll")]},
    "linux-standalone": {"bin": "linux64", "libs": ["client.so", "server.so", "game_shader_generic_hidden.so"],
                         "symbols": None, "archive": "tar.gz", "shaders": True,
                         "root": [(ROOT / "game" / "hidden2013_linux64", "hidden2013_linux64"),
                                  (ROOT / "game" / "bin" / "linux64" / "libsteam_api.so", "bin/linux64/libsteam_api.so"),
                                  (ROOT / "tools" / "lowercase_beta4b.sh", "hidden2013/lowercase_beta4b.sh")]},
    # SteamCMD's dedicated server has 64-bit engine libraries but only 32-bit launchers; on Linux it
    # also lacks the 64-bit Steamworks library, and loads the game's server_srv.so.
    "windows-server": {"bin": "x64", "libs": ["server.dll"], "symbols": ".pdb", "archive": "zip",
                       "shaders": False, "root": [(ROOT / "game" / "srcds_win64.exe", "srcds_win64.exe")]},
    "linux-server": {"bin": "linux64", "libs": ["server_srv.so"], "symbols": ".dbg", "archive": "tar.gz",
                     "shaders": False,
                     "root": [(ROOT / "game" / "srcds_linux64", "srcds_linux64"),
                              (ROOT / "src" / "lib" / "public" / "linux64" / "libsteam_api.so",
                               "bin/linux64/libsteam_api.so"),
                              (ROOT / "tools" / "lowercase_beta4b.sh", "hidden2013/lowercase_beta4b.sh"),
                              (ROOT / "tools" / "hidden_update.sh", "hidden_update.sh")]},
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


# Files other than libraries that get the executable bit in tars.
EXECUTABLES = {"srcds_linux64", "hidden2013_linux64", "lowercase_beta4b.sh", "hidden_update.sh"}


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
                info.mode = 0o755 if rel.suffix == ".so" or rel.name in EXECUTABLES else 0o644
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

    missing = [str(bindir / lib) for lib in plat["libs"] if not (bindir / lib).exists()]
    missing += [str(src) for src, _ in plat["root"] if not src.exists()]
    shaders = sorted((GAME / "shaders" / "fxc").glob("*.vcs")) if plat["shaders"] else []
    if missing:
        sys.exit(f"not built: {', '.join(missing)}")
    if plat["shaders"] and not shaders:
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

        for src, rel in plat["root"]:
            dst = stage.parent / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            game_files.append(dst.relative_to(stage.parent))

        outbin = stage / "bin" / plat["bin"]
        outbin.mkdir(parents=True, exist_ok=True)
        for lib in plat["libs"]:
            dst = outbin / lib
            shutil.copy2(bindir / lib, dst)
            game_files.append(dst.relative_to(stage.parent))
            if plat["symbols"] == ".pdb":
                pdb = bindir / (Path(lib).stem + ".pdb")
                if pdb.exists():
                    shutil.copy2(pdb, outbin / pdb.name)
                    symbol_files.append((outbin / pdb.name).relative_to(stage.parent))
            elif lib.endswith(".so"):
                # Stripped either way; only the platforms that name ".dbg" keep what was split off.
                dbg = outbin / (lib + ".dbg")
                split_debug_info(dst, dbg)
                if plat["symbols"] == ".dbg":
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
