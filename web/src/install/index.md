title: Install

# Installing Hidden: Rebuild

Hidden: Rebuild is a Source SDK 2013 mod. It brings the game code; the maps, models, sounds and
textures come from your own copy of Hidden: Source Beta 4b, which it loads from next to it. Both go
in Steam's `sourcemods` folder.

## 1. Source SDK Base 2013 Multiplayer

Install **Source SDK Base 2013 Multiplayer** from Steam: in the Library, switch the filter to
include Tools, or open [steam://install/243750](steam://install/243750). Windows and Linux are
both supported, 64-bit only.

## 2. Hidden: Source Beta 4b

Download the official Beta 4b release,
[hsb4b-full.zip](https://www.hidden-source.com/downloads/hsb4b-full.zip), and extract it into
`steamapps/sourcemods`, so that its `gameinfo.txt` ends up in `steamapps/sourcemods/hidden/`. It
needs no patches: Hidden: Rebuild only uses its content, never its 2006 game code.

If you still have Beta 4b installed from back in the day, that copy works as is.

## 3. Hidden: Rebuild

Download the package for your system from the [downloads]({{root}}#downloads) (the `-symbols`
archives are for crash reports only), and extract it into `steamapps/sourcemods` too. You should end
up with:

```
steamapps/sourcemods/
  hidden/        Hidden: Source Beta 4b
  hidden2013/    Hidden: Rebuild
```

Restart Steam, and **Hidden: Rebuild** shows up in your Library. Right-click it, open
**Properties**, and put this in **Launch Options**:

```
-sacrifice -steam
```

The engine loses the first launch option the way Steam starts mods, so `-sacrifice` goes and
`-steam` survives. Without `-steam` the game runs in insecure mode, which keeps you off VAC-secured
servers.

**On Linux**, run this once as well. Beta 4b has file and folder names with capitals, which the
game looks up in lower case, and Linux file names are case-sensitive; the script lower-cases the
names in the `hidden` folder (Windows doesn't mind either way):

```
sh ~/.steam/steam/steamapps/sourcemods/hidden2013/lowercase_beta4b.sh
```

To update, extract a newer package over the old one. Servers only take players on the same version,
so update when a new release is out.

## 4. More maps (optional)

The community's fixed versions of two stock maps, `hdn_decay_fixed` and `hdn_origin_fixed`, are in
[ghs-patch-assets-v2.zip](https://github.com/phit/hidden-source-ianua/releases/download/assets-v1/ghs-patch-assets-v2.zip).
Copy the two `.bsp` files from its `maps` folder into `sourcemods/hidden2013/maps` (create it if
it's not there). Leave out the rest of that archive; it's for another launcher.

The [community map pack](https://github.com/phit/hidden-source-ianua/releases/download/assets-v1/ghs-mappack-v1.zip)
(471 MB) has 107 maps servers ran over the years. Extract its `.bsp` files into
`sourcemods/hidden2013/maps` as well.

Servers that run these maps need them too; players without a map download it from the server if the
server offers downloads.

## Problems

- **Hidden: Rebuild doesn't show up in Steam:** check that the folder is
  `sourcemods/hidden2013` (not `sourcemods/hidden2013/hidden2013`), then restart Steam.
- **Missing textures, errors about models or maps:** Beta 4b isn't where Hidden: Rebuild looks for
  it. It must be in `sourcemods/hidden`, right next to `hidden2013`.
- **"You are in insecure mode" when joining a server:** the `-sacrifice -steam` launch options
  from step 3 are missing.
- **"Server is running a newer/older version":** you and the server have different releases;
  update to the newest.
- Anything else: [open an issue on GitHub](https://github.com/phit/hidden-2013/issues).
