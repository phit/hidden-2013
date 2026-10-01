# Branding

The Hidden: Rebuild logo, banner and icon.

| File | What |
|---|---|
| `logo.svg` / `.png` | HIDDEN REBUILD, transparent background |
| `logo-stacked.svg` / `.png` | HIDDEN over REBUILD, transparent background |
| `logo-splash.svg` / `.png` | The banner without its black: HIDDEN REBUILD on the blood splash, transparent (PNG at 2x) |
| `banner.svg` / `.png` | 763×174 header for the site and launcher, on black with a blood splash (PNG at 2x) |
| `icon.svg` / `.png` / `.ico` | The knife alone; the `.ico` is the game launcher's and the Hidden: Rebuild Launcher's icon |

It follows Hidden: Source's intro logo. The lettering is white, with a black rim, a pale outer
glow and a drop shadow. A KA-BAR with a partly serrated edge, the original's knife, stands in
for the I, tip down, in its own gap between the H and the D. The letters are Anton squeezed to
80% width, an openly licensed stand-in for Haettenschweiler. They're converted to outlines, so the SVGs need no font installed.

## Rebuilding

```sh
pip install fonttools resvg-py pillow
py branding/build.py --launcher ../launcher
```

This writes the files above and copies them into place: `web/src/img/logo.png`,
`web/src/favicon.ico`, `src/launcher_main/res/hidden2013.ico`, and, with `--launcher`, the
launcher's `gui/logo.png` and `gui/favicon.ico`. Change the design in `build.py` (layout,
effects) and `kabar.py` (the knife), not in the outputs. Each icon size is rendered separately, so
the small ones keep a visible rim.

## Credits and licences

- **Knife:** traced by us (`kabar.py`) from a photo of the KA-BAR USMC serrated knife.
- **Font:** [Anton](https://github.com/googlefonts/AntonFont) by the Anton Project Authors, under
  the [SIL Open Font License 1.1](src/Anton-OFL.txt) (`src/Anton-Regular.ttf`).
- **The logo, banner and icon** are licensed
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); credit "Hidden: Rebuild".
