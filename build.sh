#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the WASM artifact. Two modes:
#   MODE=core  -> demosaic-only module (no LibRaw). Proves the toolchain fast.
#   MODE=full  -> LibRaw decode + demosaic (default when vendor/LibRaw present).


set -euo pipefail
cd "$(dirname "$0")"
ROOT="$PWD"

# Activate emsdk if present.
if [ -f emsdk/emsdk_env.sh ]; then
  # shellcheck disable=SC1091
  source emsdk/emsdk_env.sh >/dev/null 2>&1 || true
fi
command -v emcc >/dev/null 2>&1 || { echo "!! emcc not on PATH. Run scripts/bootstrap-toolchain.sh and source emsdk_env.sh" >&2; exit 1; }

mkdir -p dist build
MODE="${MODE:-$([ -d vendor/LibRaw ] && echo full || echo core)}"
echo ">> build mode: $MODE"

COMMON_FLAGS=(
  -O3 -std=c++17
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,worker,node
  -sALLOW_MEMORY_GROWTH=1
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPU16,HEAP32,HEAPF32
  -sEXPORT_NAME=createLibRawGPL3
  -sSINGLE_FILE=1
  -sUSE_ZLIB=1 -sUSE_LIBJPEG=1
)
# SINGLE_FILE=1 embeds the .wasm binary as base64 directly inside the output
# .js, so dist/libraw-gpl3.js is fully self-contained -- no separate .wasm to
# link. This matches irlab.uk's own single-file architecture: the built .js
# gets pasted inline into a <script> tag in index.html, same as the rest of
# the tool, with no external network request for a wasm binary. Trade-off:
# base64 inflates the binary ~33% and inline JS parses slightly slower than
# loading a binary file directly -- negligible for a decoder this size.

# Functions exported to JS. lr_* only exist in full builds.
EXPORTS='["_malloc","_free","_dm_demosaic_raw"]'
SRCS=(src/api.cpp src/demosaic_bilinear.cpp src/demosaic_amaze.cpp src/demosaic_lmmse.cpp src/demosaic_rcd.cpp src/demosaic_igv.cpp src/demosaic_ahd.cpp src/demosaic_xtrans_fast.cpp)
INCLUDES=(-Isrc)
LIBS=()
# tree is vendored. They compile RT's SSE2 SIMD path, emulated to WASM SIMD via
# -msse2 -msimd128.
if [ -d vendor/RawTherapee/rtengine ] && [ -f src/amaze/amaze_port.cc ]; then
  echo ">> AMaZE + LMMSE + RCD + IGV + AHD enabled (RawTherapee SIMD path)"
  COMMON_FLAGS+=(-DHAVE_AMAZE -DHAVE_LMMSE -DHAVE_RCD -DHAVE_IGV -DHAVE_AHD -DHAVE_XTRANS_FAST -DNDEBUG -msse2 -msimd128)
  SRCS+=(src/amaze/amaze_port.cc src/amaze/lmmse_port.cc src/amaze/rcd_port.cc src/amaze/igv_port.cc src/amaze/ahd_port.cc src/amaze/xtrans_fast_port.cc)
  INCLUDES+=(-Isrc/amaze -Ivendor/RawTherapee/rtengine)
fi

if [ "$MODE" = "full" ]; then
  EXPORTS='["_malloc","_free","_dm_demosaic_raw","_lr_open","_lr_width","_lr_height","_lr_cfa","_lr_black","_lr_white","_lr_cam_mul","_lr_rgb_cam","_lr_mosaic","_lr_demosaic","_lr_free"]'
  COMMON_FLAGS+=(-DHAVE_LIBRAW)

  # Build LibRaw to a static library with the Emscripten toolchain (once).
  # LibRaw ships autotools (configure.ac/Makefile.am), not CMake -- there is
  # no CMakeLists.txt in the real source tree, so emcmake/cmake could never
  # have worked here.
  if [ -z "$(find build/libraw -name 'libraw*.a' 2>/dev/null | head -1)" ]; then
    echo ">> configuring LibRaw (autotools)"
    if [ ! -f vendor/LibRaw/configure ]; then
      ( cd vendor/LibRaw && autoreconf --install )
    fi
    mkdir -p build/libraw
    ( cd build/libraw && CFLAGS="-sUSE_ZLIB=1 -sUSE_LIBJPEG=1" CXXFLAGS="-sUSE_ZLIB=1 -sUSE_LIBJPEG=1" emconfigure ../../vendor/LibRaw/configure --disable-shared --disable-examples --disable-openmp )
    emmake make -C build/libraw -j4
  fi
  INCLUDES+=(-Ivendor/LibRaw)
  # locate the built archive
  LIBRAW_A=$(find build/libraw -name 'libraw*.a' | head -1)
  [ -n "$LIBRAW_A" ] || { echo "!! libraw.a not built" >&2; exit 1; }
  LIBS+=("$LIBRAW_A")
fi

echo ">> compiling module"
em++ "${COMMON_FLAGS[@]}" "${INCLUDES[@]}" \
  -sEXPORTED_FUNCTIONS="$EXPORTS" \
  "${SRCS[@]}" ${LIBS[@]+"${LIBS[@]}"} \
  -o dist/libraw-gpl3.js

echo ">> wrote dist/libraw-gpl3.js + dist/libraw-gpl3.wasm"
ls -la dist

