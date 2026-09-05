// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren
//
// C API for the WASM artifact. Two layers:
//   * dm_demosaic_raw(): pure demosaic over a caller-supplied mosaic. No LibRaw
//     needed — used by the smoke test and by callers that already have a mosaic.
//   * lr_*(): LibRaw-backed decode (open/unpack/metadata) + demosaic. Compiled
//     only when HAVE_LIBRAW is defined.
#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ---- pure demosaic (always available) --------------------------------------
// cfa: 2x2 pattern from sensor top-left, 0=R 1=G 2=B. qual: see DemosaicQual.
// R/G/B: caller-allocated float[w*h] each. Returns 0 on success, 1 if the
// requested algorithm fell back (e.g. AMaZE before it is wired in).
int dm_demosaic_raw(const uint16_t* mosaic, int w, int h,
                    int cfa0, int cfa1, int cfa2, int cfa3,
                    float black, float white, int qual,
                    float* R, float* G, float* B);

#ifdef HAVE_LIBRAW
// ---- LibRaw-backed decode --------------------------------------------------
typedef struct LRContext LRContext;

// Parse + unpack a RAW held in a WASM heap buffer. Returns NULL on failure.
LRContext* lr_open(const uint8_t* bytes, size_t len);

int  lr_width(LRContext*);     // visible mosaic width
int  lr_height(LRContext*);    // visible mosaic height
void lr_cfa(LRContext*, int out4[4]);        // 2x2 pattern, 0=R 1=G 2=B
int  lr_black(LRContext*);
int  lr_white(LRContext*);
void lr_cam_mul(LRContext*, float out4[4]);  // camera white balance
void lr_rgb_cam(LRContext*, float out12[12]);// 3x4 camera->linear matrix

// Pointer to the visible-area u16 mosaic (row-major, w*h). Owned by the context.
const uint16_t* lr_mosaic(LRContext*);

// Demosaic into caller-allocated float[w*h] planes. Returns 0 / 1 (fell back).
int  lr_demosaic(LRContext*, int qual, float* R, float* G, float* B);

void lr_free(LRContext*);
#endif // HAVE_LIBRAW

#ifdef __cplusplus
}
#endif
