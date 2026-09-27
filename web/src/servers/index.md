title: Servers

# Running a server

A dedicated server runs on **Source SDK Base 2013 Dedicated Server** from
[SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD) (app 244310), with Hidden: Rebuild's
**server package** and Hidden: Source Beta 4b next to it. SteamCMD's server has the 64-bit engine but
only 32-bit launchers, which can't load the mod; the server package brings the 64-bit launcher.

1. Install the dedicated server:

   ```
   steamcmd +force_install_dir /path/to/server +login anonymous +app_update 244310 validate +quit
   ```

2. Download the server package for your system from the
   [releases](https://github.com/phit/hidden-2013/releases) (`hidden2013-<version>-windows-server.zip`
   or `-linux-server.tar.gz`) and extract it into the server's folder.
3. Put Beta 4b's `hidden` folder there too ([hsb4b-full.zip](https://www.hidden-source.com/downloads/hsb4b-full.zip)
   has it). The server's folder then has:

   ```
   bin/  hl2/  hl2mp/  platform/    from SteamCMD
   hidden/                          Hidden: Source Beta 4b
   hidden2013/                      Hidden: Rebuild
   srcds_win64.exe or srcds_linux64 Hidden: Rebuild's 64-bit launcher
   ```

## Windows

```
srcds_win64.exe -console -game hidden2013 +maxplayers 12 +map hdn_docks
```

## Linux

Two one-time steps. Beta 4b has file and folder names with capitals (`materials/Wall`, ...), and
the engine looks everything up in lower case, which Linux's case-sensitive file system doesn't
find. The server package brings a script that lower-cases the names in the `hidden` folder:

```
sh hidden2013/lowercase_beta4b.sh
```

And the Steamworks library looks for the 64-bit `steamclient.so` in `~/.steam/sdk64`. SteamCMD
has it; link it there:

```
mkdir -p ~/.steam/sdk64
ln -s /path/to/steamcmd/linux64/steamclient.so ~/.steam/sdk64/steamclient.so
```

Then start the server from anywhere:

```
/path/to/server/srcds_linux64 -console -game hidden2013 +maxplayers 12 +map hdn_docks
```

## Being found

- **LAN:** players on your network see the server under Find Servers, LAN.
- **Internet:** the server registers with Steam's master server by itself, but players can only
  reach it once UDP port 27015 is forwarded to it on your router (TCP 27015 too, for rcon).
- **More than one network adapter** (VPNs, virtual machines, WSL): the server may pick the wrong one
  and not be reachable. Add `+ip 0.0.0.0` to listen on all of them, or `+ip <address>` for one.
- **Listen servers** (Create Server in the game) answer on your LAN, and over the internet they run
  through Steam's relay network instead of your IP. Other players joining one isn't tested yet.

## Configuring

- **`cfg/server.cfg`** runs at every map start: hostname, passwords, and the cvars below. Old
  Hidden: Source server configs work, including the [plugins]({{root}}plugins/)' cvars.
- **`cfg/mapcycle.txt`** is the map rotation. The one that ships is Beta 4b's, with the fixed
  `hdn_decay` and `hdn_origin`. Maps the server doesn't have are skipped, and a missing `_fixed` map
  falls back to the original.
- **Bots** play both sides: `bot_add` adds one, `hdn_bot_quota` keeps the server topped up with them.
- **Versions:** players can only join a server on the same release, so update the server when you
  update the game.

## Cvars

This list is generated from the source code, so it's always the current build's. Server cvars go in
`cfg/server.cfg`; client cvars are for players' own configs. The [plugins]({{root}}plugins/) have
their own cvars.

<!-- CVARS -->

## Commands

<!-- COMMANDS -->
