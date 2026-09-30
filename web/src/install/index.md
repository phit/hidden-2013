title: Install
summary: Install Hidden: Rebuild with the launcher, which sets up Beta 4b and Source SDK Base 2013 for you, or by hand.

# Installing Hidden: Rebuild

Hidden: Rebuild is a Source SDK 2013 mod. It brings the game code; the maps, models, sounds and
textures come from your own copy of Hidden: Source Beta 4b, which it loads from next to it. It runs
on Source SDK Base 2013 Multiplayer, a free tool on Steam. Windows and Linux are both supported,
64-bit only.

## With the launcher

The easy way. The launcher needs Steam installed and does the rest:

- installs Hidden: Rebuild and keeps it up to date,
- uses your Beta 4b if it's in Steam's `sourcemods` folder, or downloads the
  [official release](https://www.hidden-source.com/downloads/hsb4b-full.zip) if it isn't,
- opens Steam's install dialog for Source SDK Base 2013 Multiplayer if you don't have it,
- starts Steam if it isn't running, and starts the game, with no
  launch options to set.

To install it:

1. Download it:
   [Windows](https://github.com/phit/hidden-2013-launcher/releases/latest/download/HiddenLauncher.exe)
   or [Linux](https://github.com/phit/hidden-2013-launcher/releases/latest/download/HiddenLauncher-linux).
2. Put it wherever you like and run it. Windows may warn about an unrecognized app, because the
   launcher isn't signed: click **More info**, then **Run anyway**. On Linux, make it executable
   first (`chmod +x HiddenLauncher-linux`).
3. Wait for it to finish setting up, then press **Play**.

It installs into `%LOCALAPPDATA%\HiddenRebuild` on Windows and `$XDG_DATA_HOME/HiddenRebuild` on
Linux (usually `~/.local/share/HiddenRebuild`); **Settings** can move that folder, and has
**Snapshot builds** for the newest test build instead of the last release. The launcher updates itself too.

On Windows it needs the WebView2 Runtime, which Windows 11 and up-to-date Windows 10 already have
([download](https://go.microsoft.com/fwlink/p/?LinkId=2124703)). On Linux its window opens in your
web browser.

## More maps (optional)

Maps go in the `maps` folder inside `hidden2013`: with the launcher,
**Settings → Browse install folder** opens the folder that has `hidden2013`; for a manual install,
it's `sourcemods/hidden2013/maps`.

The community's fixed versions of two stock maps, `hdn_decay_fixed` and `hdn_origin_fixed`, are in
[ghs-patch-assets-v2.zip](https://github.com/phit/hidden-source-ianua/releases/download/assets-v1/ghs-patch-assets-v2.zip).
Copy the two `.bsp` files from its `maps` folder. Leave out the rest of that archive; it's for
another launcher.

The [community map pack](https://github.com/phit/hidden-source-ianua/releases/download/assets-v1/ghs-mappack-v1.zip) has 107 maps servers ran over the years. Extract its `.bsp` files there as well.

Servers that run these maps need them too; players without a map download it from the server if the
server offers downloads.

## Manual install

Everything goes in Steam's `sourcemods` folder, and Hidden: Rebuild shows up in your Steam Library.

### 1. Source SDK Base 2013 Multiplayer

Install **Source SDK Base 2013 Multiplayer** from Steam: in the Library, switch the filter to
include Tools, or open [steam://install/243750](steam://install/243750).

### 2. Hidden: Source Beta 4b

Download the official Beta 4b release,
[hsb4b-full.zip](https://www.hidden-source.com/downloads/hsb4b-full.zip), and extract it into
`steamapps/sourcemods`, so that its `gameinfo.txt` ends up in `steamapps/sourcemods/hidden/`. It
needs no patches: Hidden: Rebuild only uses its content, never its 2006 game code.

If you still have Beta 4b installed from back in the day, that copy works as is.

### 3. Hidden: Rebuild

Download the package for your system from the [downloads]({{root}}#downloads) (the plain
`-windows` or `-linux` one; `-symbols` archives are for crash reports only, `-standalone` ones are
the launcher's), and extract it into `steamapps/sourcemods` too. You should end up with:

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

## Problems

- **The launcher says Source SDK Base 2013 Multiplayer is missing although Steam is installing
  it:** wait for Steam to finish; the launcher notices by itself.
- **The launcher can't install something:** its log is `launcher_log.txt` in its folder
  (`%LOCALAPPDATA%\HiddenRebuild` or `$XDG_DATA_HOME/HiddenRebuild`).
- **Hidden: Rebuild doesn't show up in Steam** (manual install): check that the folder is
  `sourcemods/hidden2013` (not `sourcemods/hidden2013/hidden2013`), then restart Steam.
- **Missing textures, errors about models or maps** (manual install): Beta 4b isn't where Hidden:
  Rebuild looks for it. It must be in `sourcemods/hidden`, right next to `hidden2013`.
- **"You are in insecure mode" when joining a server** (manual install): the `-sacrifice -steam`
  launch options from step 3 are missing.
- **"Server is running a newer/older version":** you and the server have different releases;
  update to the newest (in the launcher, turn **Snapshot builds** off for the release).
- Anything else: [open an issue on GitHub](https://github.com/phit/hidden-2013/issues).
