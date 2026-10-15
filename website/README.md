# Nexo website

Static, dependency-free marketing + documentation site. Plain HTML/CSS with no
build step; the only JavaScript is the optional in-browser emulator.

## Pages

| File           | Purpose |
| -------------- | ------- |
| `index.html`   | Landing page: hero, features, firmware screenshots |
| `download.html`| Firmware / Android / macOS downloads (latest release) |
| `docs.html`    | Quick start + links to the in-repo docs |
| `emulator.html`| In-browser WebAssembly emulator of the firmware UI |
| `terms.html`   | Terms of Service |
| `privacy.html` | Privacy Policy |
| `404.html`     | Not-found page |

Download buttons point at the GitHub **latest release** assets, e.g.
`https://github.com/apiwishboon-spec/Nexo/releases/latest/download/Nexo-android-release.apk`,
so they always serve the newest published build.

## Fonts

`Press Start 2P` and `VT323` are bundled under `assets/fonts/` under the SIL
Open Font License (see the `OFL-*.txt` files). This matches the pixel theme used
in the companion app.

## Deploying to Cloudflare Pages

1. Create a Pages project connected to the GitHub repo
   (`apiwishboon-spec/Nexo`).
2. Set **Build output directory** to `website`.
3. Leave the build command empty (there is no build step).
4. Deploy. Cloudflare serves `index.html` at `/` and `404.html` for misses; clean
   URLs (`/download`) work automatically.

Any other static host works too — just serve this folder.

## Emulator

`emulator.html` runs the **real firmware UI** compiled to WebAssembly. The
artifacts live in `assets/emu/` (`nexo_emu.js` + `nexo_emu.wasm`) and are built
by the [`emulator`](../.github/workflows/emulator.yml) GitHub Actions workflow
from `firmware/tools/preview/`. The page loads them, draws the 240×135 frame to a
canvas, and wires the on-screen keypad + keyboard to the emulator core.

To rebuild locally (needs the Emscripten SDK on `PATH`):

```bash
bash firmware/tools/preview/build_wasm.sh
```

If `assets/emu/` is missing, the page degrades gracefully with a notice.

## Local preview

```bash
cd website
python3 -m http.server 8080
# open http://localhost:8080
```
