title: A launcher, and every plugin
summary: v1.0.2.0 comes with a launcher that sets everything up, brings the rest of the popular SourceMod plugins in, and keeps servers' own message of the day.

# A launcher, and every plugin

2026-09-29

The third release, [v1.0.2.0](https://github.com/phit/hidden-2013/releases/tag/v1.0.2.0), makes
getting in a lot easier, and finishes bringing the old server plugins in.

## The launcher

Installing used to take three downloads, two folders and a launch option. Now there's the
[Hidden: Rebuild Launcher](https://github.com/phit/hidden-2013-launcher/releases/latest), for
Windows and Linux. It:

- installs Hidden: Rebuild and keeps it up to date,
- uses your Beta 4b if it's in Steam's `sourcemods` folder, and downloads the official release if
  it isn't,
- has Steam install Source SDK Base 2013 Multiplayer if you don't have it,
- and starts the game ready for VAC-secured servers, with no launch options to set.

It can also follow the test builds instead of releases. The [install guide]({{root}}install/)
covers it, and the manual install still works as before. The launcher is a fork of mastercomfig's
Team Comtress launcher.

## Plugins

The rest of the popular Hidden:SourceMod plugins are now built in, each off until a server turns it
on: Friendly Fire Radio, Radio to Voice, Radio Taunts, Radio+, Radio Trip Alarm, Ammo Requester,
Dynamic Round Timer, Anti-Blur, Fake Tickrate, HandiCap and the Spectator Hidden Trail. Only Hidden
Ranks is left out, a whole stats system of its own. The [plugins page]({{root}}plugins/) lists what
each does and its cvars.

Plugin menus, like HandiCap's, show up and work now: Beta 4b's HUD had no fonts or colours for them,
and picking an item never reached the plugin.

## Gameplay and fixes

- **Marine clips:** marines no longer stutter against the invisible walls some maps put up for
  them only. Their game now knows the walls are there, instead of walking through and being pulled
  back by the server (Beta 4b did that too).
- **Bots** mostly stop walking off ledges and roofs, and jumping into the void on maps like
  Eminence (the odd one still slips off a plank there).
- **The team and weapon menus** sit in the middle of wide screens instead of the left.

## Servers

- **Your message of the day survives updates.** Put it in `hidden2013/cfg/motd.txt`; updates never
  touch it. The one that comes with the game is new and links this site and the community Discord.
- **Pterodactyl:** an egg for Pterodactyl panels installs the server, Beta 4b and the game, and
  keeps it updated on every start; the Linux package's `hidden_update.sh` does the updating
  anywhere else. See the [servers page]({{root}}servers/).

The site has a new look too, in hidden-source.com's colours.

Servers only accept players on the same version, so update both when you get this one; the launcher
does it for you.
