title: Servers

# Running a server

A dedicated server needs the same two folders as a player: Hidden: Rebuild's `hidden2013` and
Hidden: Source Beta 4b's `hidden` next to it (see [Install]({{root}}install/)). Put both in the
server's folder, then start the server with `-game hidden2013`.

## Windows

Use `srcds_win64.exe` from the **Source SDK Base 2013 Multiplayer** install. The `srcds.exe` of the
dedicated server app from SteamCMD (244310) is 32-bit and can't load the mod.

```
srcds_win64.exe -console -game "C:\server\hidden2013" +maxplayers 12 +map hdn_docks
```

## Linux

The dedicated server from SteamCMD, app 244310, with `-game hidden2013`; the package has native
64-bit Linux builds. This isn't tested yet, so please
[report how it goes](https://github.com/phit/hidden-2013/issues).

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
