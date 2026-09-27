# Hidden: Source on Source SDK 2013

A reimplementation of [Hidden: Source](http://www.hidden-source.com) (Calibre Studios, Beta 4b, 2006) on
the current Source SDK 2013 Multiplayer, reusing the original mod's content.

The original game code was lost; this is a full rebuild of the game on the modern SDK
with the old assets. This repository is a fork of
[ValveSoftware/source-sdk-2013](https://github.com/ValveSoftware/source-sdk-2013) with a `HIDDEN` game
target built on the HL2MP code base. Valve's original README is in [README.sdk.md](README.sdk.md).

## Status

Playable test builds. The Hidden and IRIS play full rounds on the Beta 4b maps: weapons and
equipment, the Hidden's pounce, cling, aura and pigstick, the HUD, menus and spectating, and bots for
both teams (`bot_add`). The goal is to match Beta 4b first; differences are listed in the docs.

## Installing

1. Install **Source SDK Base 2013 Multiplayer** from Steam (Library, Tools).
2. Install **Hidden: Source Beta 4b** into `steamapps/sourcemods/hidden`, as the original installer
   does. Its maps, models, sounds and materials are used from there; this mod ships none of them.
3. Download a build from the [releases](../../releases) (`latest` is the newest `main` build) and
   extract it into `steamapps/sourcemods`, next to `hidden`:

   ```
   steamapps/sourcemods/
     hidden/        Hidden: Source Beta 4b
     hidden2013/    this mod
   ```

4. Restart Steam and start **Hidden: Rebuild** from the library.

The Windows and Linux builds are 64-bit only. The `-symbols` archives hold debug information for
crash reports; players don't need them.

### Dedicated server

Install Source SDK Base 2013 Dedicated Server with SteamCMD (app 244310), then extract the **server
package** (`hidden2013-<version>-windows-server.zip` or `-linux-server.tar.gz`) into its folder and
put Beta 4b's `hidden` folder there too. SteamCMD's server only has 32-bit launchers; the package
brings 64-bit ones, `srcds_win64.exe` and `srcds_linux64`:

```
srcds_win64.exe -console -game hidden2013 +maxplayers 12 +map hdn_docks
./srcds_linux64 -console -game hidden2013 +maxplayers 12 +map hdn_docks
```

On Linux, link SteamCMD's 64-bit `steamclient.so` into `~/.steam/sdk64` first. The
[servers page](https://phit.github.io/hidden-2013/servers/) has the details.

## Layout

| Path | What |
|---|---|
| `src/game/client/client_hidden.vpc`, `src/game/server/server_hidden.vpc` | Client and server projects (HL2MP plus `HIDDEN` define) |
| `src/game/{client,server,shared}/hidden/` | Hidden game code |
| `game/hidden2013/` | The mod folder the game runs from |
| `src/srcds_hidden/` | The 64-bit dedicated server launchers |
| `web/` | The project site (GitHub Pages) |
| `tools/` | Build, packaging and asset scripts |
| `.github/workflows/build.yml` | CI: Windows and Linux builds, packages, releases |

## Building

The Beta 4b content isn't in this repository. The mod mounts `game/hidden` (next to
`game/hidden2013`, as in `sourcemods`), so link that to your Beta 4b install:

```powershell
New-Item -ItemType Junction -Path game\hidden -Target "<Steam>\steamapps\sourcemods\hidden"
```

Or copy the content into `game/hidden2013/legacy/`, which is mounted first:
`py tools/import_legacy.py <source> [<source> ...] --clean`, where `<source>` is an installed mod
folder, the `ianua-base-hsb4b.tar.gz` archive, or a zip of maps (`ghs-patch-assets-v2.zip`,
`ghs-mappack-v1.zip`). Only content folders are copied, never the 2006 binaries.

**Windows:** Source SDK 2013 Multiplayer installed through Steam, Visual Studio 2022 (or its Build
Tools) with MSVC v143 and a Windows 10/11 SDK, and Python 3.13+.

```powershell
tools\build.ps1            # Release; add -Regen after changing .vpc files, -Configuration Debug for debug
```

This writes the DLLs to `game/hidden2013/bin/x64`, the shaders to `game/hidden2013/shaders/fxc` and
the launcher to `game/hidden2013_win64.exe`. Run the launcher with Steam running.

**Linux:** [podman](https://podman.io/) and the Steam Runtime's sniper SDK image, which the script
pulls:

```sh
src/buildhidden [release|debug]
```

This writes the libraries to `game/hidden2013/bin/linux64`. The shaders are compiled on Windows only.

**Packages:** `py tools/package.py windows|linux` puts the release archives in `dist/`, from what
the builds left in `game/hidden2013`.

## License

The code is under Valve's [SOURCE 1 SDK LICENSE](LICENSE). Hidden: Source content belongs to Calibre
Studios and is not distributed here.
