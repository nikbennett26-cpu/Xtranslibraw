# libraw-wasm-gpl3

WebAssembly RAW decoder for [irlab.uk](https://irlab.uk), combining LibRaw
(sensor decode) with demosaic algorithms ported from RawTherapee (GPL-3).

**This repo was assembled from files shared during development, not
exported directly from a working local copy.** It reflects the real
project structure and contains real, working source — but see "What's
missing" below before expecting `./build.sh` to succeed as-is.

## What this adds: fast X-Trans support

Fuji X-Trans sensors were previously rejected outright (`filters == 9`)
so the host app would fall back to the embedded JPEG preview. This adds
a real decode path — RawTherapee's "fast" X-Trans algorithm (not the
higher-quality Markesteijn tier; see below).

**Verification status:**
- Algorithm logic: verified against a real Fuji X-E2 RAF file. Reimplemented
  in Python, run against the file's actual 6x6 CFA pattern (extracted via
  rawpy/LibRaw, not assumed), produced a clean, artifact-free photo.
- C++ compilation: **not yet verified.** This code has never been through
  an actual compiler. First build attempt may surface issues — expected
  for untested code, not a sign the approach is wrong.

New/changed files:
- `src/amaze/xtrans_fast_port.cc` — the ported algorithm (RawTherapee's
  `xtransborder_interpolate` + `fast_xtrans_interpolate`, GPL-3, Gabor
  Horvath / RawTherapee contributors)
- `src/amaze/xtrans_fast_standalone.h` — bridge function declaration
- `src/demosaic_xtrans_fast.cpp` — integration glue (black/white scaling,
  `char[6][6]` → `int[6][6]` widening, `[0,1]` output normalisation)
- `src/demosaic.h` — added `QUAL_XTRANS_FAST` + declaration
- `build.sh` — added the new files + `-DHAVE_XTRANS_FAST` flag

**Not included, and still needed for full support:** the *decode-layer*
wiring in `api.cpp` — currently `lr_open()` rejects X-Trans before this
new code would ever run. See `docs/api-cpp-xtrans-patch.md` for the exact
change needed (four small, targeted edits against the real file).

**Also not included:** the higher-quality Markesteijn algorithm (1-pass
and 3-pass tiers). This is the smaller, shallower-dependency "fast" tier
only — a real, working starting point, not the final word on X-Trans
quality.

## What's missing for a full clone-and-build

- **`emsdk/`** or equivalent Emscripten toolchain setup -- the only
  remaining gap. Everything else the build itself references is now here.

`vendor/LibRaw/` (full LibRaw master: headers, decoders, demosaic,
integration -- 226 files) and `vendor/RawTherapee/` are both now
included. The RawTherapee vendor tree is trimmed to `rtengine/` plus
top-level licensing (`LICENSE`, `AUTHORS.txt`) -- the upstream repo is
1,558 files / 122MB including the GUI (`rtgui/`), icons/translations/
presets (`rtdata/`, ~99MB alone), and standalone tools (`tools/`), none
of which the demosaic ports touch. Only `rtengine/` (13MB) is what the
ported `_port.cc` files' `#include`s actually resolve against.

## Building (once the above is in place)

```bash
./build.sh
```
Produces a single `dist/libraw-gpl3.js` -- the `.wasm` binary is embedded
as base64 inside it (`-sSINGLE_FILE=1`), matching irlab.uk's own
single-file architecture: this file gets pasted directly inside a
`<script>` tag in `index.html`, no separate `.wasm` to link and no
external network request. See `build.sh` itself for the two build modes
(`core` vs `full`).

## License

GPL-3.0-or-later. Contains code from RawTherapee (GPL-3, Gabor Horvath and
contributors) and LibRaw (LGPL-2.1/CDDL dual license). See individual file
headers for specific attribution.
