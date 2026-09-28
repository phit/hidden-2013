title: Dedicated servers and VAC
summary: v1.0.1.0 ships dedicated server packages for Windows and Linux, and gets Steam players onto VAC servers.

# Dedicated servers and VAC

2026-09-28

The second release, [v1.0.1.0](https://github.com/phit/hidden-2013/releases/tag/v1.0.1.0), is
about playing together: running a server, and getting onto one.

## Servers

- **Dedicated server packages for Windows and Linux.** Steam's dedicated server download only
  comes with a 32-bit launcher, which can't run the mod, so the packages bring their own 64-bit
  one. The [servers page]({{root}}servers/) walks through the setup. Both have been tested with
  players joining over the internet.
- **Linux:** Beta 4b has file and folder names with capitals, which a Linux server can't find. The
  packages include a script that renames them once.
- **Listen servers** (Create Server in the game) work over the internet through Steam's relay:
  friends join from the Friends tab or a Steam invite, without any port forwarding.

## Getting onto VAC servers

Started from the Steam library, the game ran in insecure mode, and VAC-secured servers turned it
away. Steam passes the option that should prevent that, but the engine drops it the way Steam starts
mods. The fix is one line in Hidden: Rebuild's launch options in Steam:

```
-sacrifice -steam
```

The first option is lost on the way, so `-sacrifice` goes and `-steam` survives. The
[install guide]({{root}}install/) has the details.

## Gameplay and fixes

- **Beta 5 Physics**, another SourceMod plugin now built in (`hsm_b5phys`, off by default): the
  Hidden's thrown props do 75 damage instead of killing outright, unless the marine was already
  that low. See the [plugins]({{root}}plugins/).
- Spawning no longer lists your weapons and ammo on the right of the screen, as in Beta 4b.
- The cvars' help texts say what they do, on the site and in the console.

Servers only accept players on the same version, so update both when you get this one.
