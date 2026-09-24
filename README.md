# Hidden: Source on Source SDK 2013

A reimplementation of [Hidden: Source](http://www.hidden-source.com) (Calibre Studios, Beta 4b, 2006) on
the current Source SDK 2013 Multiplayer, reusing the original mod's content.

The original game code was lost; a Calibre developer suggested rebuilding the game on the modern SDK
with the old assets. This repository is a fork of
[ValveSoftware/source-sdk-2013](https://github.com/ValveSoftware/source-sdk-2013) with a `HIDDEN` game
target built on the HL2MP code base. Valve's original README is in [README.sdk.md](README.sdk.md).

## Status

Phase 1, skeleton. The mod builds, the dedicated server boots, and the Beta 4b maps load. There is no
Hidden gameplay yet: the game plays as HL2MP, and the Hidden map entities (`info_hidden_spawn`,
`info_marine_spawn`, `info_spectator`, `location_brush`, `extraction_point`, …) are still unknown to
the server.

## Layout

| Path | What |
|---|---|
| `src/game/client/client_hidden.vpc`, `src/game/server/server_hidden.vpc` | Client and server projects (HL2MP plus `HIDDEN` define) |
| `src/game/{client,server,shared}/hidden/` | Hidden game code (to come) |
| `game/mod_hidden/` | The mod folder the game runs from |
| `game/mod_hidden/legacy/` | Beta 4b content, imported locally and **not in git** |
| `tools/` | Build and asset scripts |

## Getting the content

The original assets aren't in this repository. Import them from a Beta 4b copy:

```powershell
py tools/import_legacy.py <source> [<source> ...] --clean
```

`<source>` can be an installed mod folder (`...\steamapps\sourcemods\hidden`), the
`ianua-base-hsb4b.tar.gz` archive, or a zip of maps such as `ghs-patch-assets-v2.zip` (fixed
`hdn_decay`/`hdn_origin`) or `ghs-mappack-v1.zip`. Only content folders are copied (`materials`,
`models`, `sound`, `maps`, `scripts`, `resource`, `media`); the 2006 binaries never are.

## Building (Windows)

Requirements: Source SDK 2013 Multiplayer installed through Steam, Visual Studio 2022 (or its Build
Tools) with MSVC v143 and a Windows 10/11 SDK, and Python 3.13+.

```powershell
tools\build.ps1            # Release; add -Regen after changing .vpc files, -Configuration Debug for debug
```

This writes `client.dll` and `server.dll` to `game/mod_hidden/bin/x64` and the launcher to
`game/mod_hidden_win64.exe`.

## Running

- **Client:** run `game\mod_hidden_win64.exe` (Steam must be running).
- **Dedicated server (Windows):** use the 64-bit `srcds_win64.exe` from the Source SDK Base 2013
  Multiplayer install. The `srcds.exe` in the dedicated-server app (244310) is 32-bit and can't load
  64-bit game DLLs.

  ```powershell
  & "<SDK Base 2013 MP>\srcds_win64.exe" -console -game "<repo>\game\mod_hidden" +maxplayers 12 +map hdn_docks
  ```

- **Dedicated server (Linux):** to come. Build with `src/buildallprojects` against the Steam Runtime and
  run on SteamCMD app 244310.

## License

The code is under Valve's [SOURCE 1 SDK LICENSE](LICENSE). Hidden: Source content belongs to Calibre
Studios and is not distributed here.
