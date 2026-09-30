title: Carrying without the jank
summary: v1.0.3.0 fixes pouncing and lag while the Hidden carries something, the missing-texture paint splats, and gives servers an example config and two new options.

# Carrying without the jank

2026-09-30

[v1.0.3.0](https://github.com/phit/hidden-2013/releases/tag/v1.0.3.0) fixes what players ran into
on the first public servers, and has Hidden: Rebuild's own logo.

## Gameplay and fixes

- **Carrying:** a barrel or a body in the Hidden's hands no longer blocks his pounce or makes it
  lag. What he carries stopped pushing him around, and his game stopped guessing wrong about his
  weapon while his hands are full.
- **Paint and blood on props:** FN303 paint splats and blood on crates, barrels and pipes show up
  as splats again instead of big pink and black checkerboards, and the console no longer fills up
  with "bad reference count" warnings.
- **Bunny hopping** was checked against Beta 4b: jumping, air control and the speed limits are the
  same. How fast you can build speed does depend on the server's tickrate.

## Servers

- **An example config:** `cfg/server_example.cfg` lists every server setting of the release and
  its plugins, with its default and what it does. Copy it to `cfg/server.cfg` and change what you
  need; updates never touch your copy. See the [servers page]({{root}}servers/).
- **`hdn_hidehiddendecals 1`** keeps blood and bullet marks off the Hidden. Beta 4b let them show
  where he'd been hit, so they still do by default.
- **`hdn_targetnames 0`** turns off the name and health under the crosshair ("Friend: ..." for
  marines, "Enemy: ..." for the Hidden up close). On by default, as in Beta 4b.

The game, this site and the launcher have a new logo and icon of our own.

Servers only accept players on the same version, so update both when you get this one; the launcher
does it for you.
