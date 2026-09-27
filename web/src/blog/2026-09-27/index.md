title: Hidden: Rebuild
summary: Hidden: Source Beta 4b, rebuilt on the current Source SDK 2013.

# Hidden: Rebuild

2026-09-27

Hidden: Source's code was lost years ago, and Beta 4b's 2006 binaries don't run on today's Source
engine. Hidden: Rebuild rebuilds the game on the modern SDK and keeps using the original content.

It plays Beta 4b as it was. The numbers, the round flow, the Hidden's pounce, cling, aura and
pigstick, the IRIS loadouts, sonic alarms and radar, the menus and the HUD are all worked out from
the original game, with the maps, models and sounds loaded from your own Beta 4b install. What's new
is underneath: a maintained 64-bit engine, native Linux builds, and dedicated servers for both.

Also in:

- **Bots** for both sides, so a small server still gets a game going (`bot_add`, `hdn_bot_quota`).
- **The popular SourceMod plugins**, built in with their old cvars:
  [PigShove, Mirror Damage, Team Aura and more]({{root}}plugins/). Old server configs keep working.
- **Beta 4b's own anti-cheat material check**, ported as it was.
- **The community's fixed `hdn_decay` and `hdn_origin`** in the map cycle, falling back to the
  originals where a server doesn't have them.

Test builds are on the [downloads]({{root}}#downloads); the [install guide]({{root}}install/)
walks through it. Where something doesn't play the way you remember Beta 4b,
[open an issue](https://github.com/phit/hidden-2013/issues): matching the original comes first.
