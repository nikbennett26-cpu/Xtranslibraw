// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren  (integration glue)
// Fast X-Trans algorithm: adapted from RawTherapee's
// RawImageSource::xtransborder_interpolate + fast_xtrans_interpolate
// (GPL-3, Gabor Horvath / RawTherapee contributors).
//
// Adapts the ported routine to our standalone interface, same shape as
// demosaic_rcd.cpp:
//   1. build float rawData[h][w] (black-subtracted, scaled to RT's 0..65535),
//   2. copy LibRaw's char[6][6] xtrans pattern into an int[6][6] (type
//      mismatch caught while reviewing libraw_types.h -- LibRaw exposes it
//      as char, RawTherapee's own code expects int),
//   3. call the ported routine, and
//   4. copy red/green/blue out, normalised to [0,1].
//
// Unlike the Bayer ports (demosaic_rcd.cpp etc.), there's no FC() callback
// here -- X-Trans's CFA is a fixed 6x6 lookup table, not a computed
// function, so the ported routine takes the pattern directly. Matches how
// the original RT source's own fcol() macro works: direct array indexing,
// not a callback.
#include "demosaic.h"

#ifdef HAVE_XTRANS_FAST
#include <vector>
#include <algorithm>
#include <cstring>

// Declared in xtrans_fast_standalone.h (src/amaze/), implemented in the
// paired port file, src/amaze/xtrans_fast_port.cc.
void xtrans_fast_demosaic_port(int w, int h, const int xtrans[6][6],
                                float* const* rawData,
                                float** red, float** green, float** blue);

int demosaic_xtrans_fast(const uint16_t* mosaic, int w, int h,
                          const char xtrans_char[6][6],
                          float black, float white,
                          float* R, float* G, float* B) {
    const int n = w * h;
    const float range  = std::max(1.0f, white - black);
    const float toRT    = 65535.0f / range;
    const float fromRT  = 1.0f / 65535.0f;

    // LibRaw's imgdata.idata.xtrans is char[6][6]; RawTherapee's own code
    // (and our port of it) expects int[6][6] -- widening copy, not a
    // reinterpret or memcpy.
    int xtrans[6][6];
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c)
            xtrans[r][c] = xtrans_char[r][c];

    std::vector<float> rawFlat((size_t)n);
    std::vector<float> redFlat((size_t)n), greenFlat((size_t)n), blueFlat((size_t)n);
    std::vector<float*> rawRows(h), rRows(h), gRows(h), bRows(h);
    for (int y = 0; y < h; ++y) {
        rawRows[y] = &rawFlat[(size_t)y * w];
        rRows[y]   = &redFlat[(size_t)y * w];
        gRows[y]   = &greenFlat[(size_t)y * w];
        bRows[y]   = &blueFlat[(size_t)y * w];
        for (int x = 0; x < w; ++x) {
            float v = ((float)mosaic[(size_t)y * w + x] - black) * toRT;
            rawRows[y][x] = v < 0.0f ? 0.0f : v;   // same lower clamp as demosaic_rcd.cpp
        }
    }

    xtrans_fast_demosaic_port(w, h, xtrans, rawRows.data(), rRows.data(), gRows.data(), bRows.data());

    for (size_t i = 0; i < (size_t)n; ++i) {
        R[i] = std::min(1.0f, std::max(0.0f, redFlat[i]   * fromRT));
        G[i] = std::min(1.0f, std::max(0.0f, greenFlat[i] * fromRT));
        B[i] = std::min(1.0f, std::max(0.0f, blueFlat[i]  * fromRT));
    }
    return 0;
}

#else  // HAVE_XTRANS_FAST not wired — no fallback possible

// Unlike the Bayer ports, there's no sane bilinear fallback here: bilinear
// assumes a 2x2 Bayer repeat, which is meaningless for a 6x6 X-Trans
// pattern. Returning 1 (fell back / unavailable) with untouched output
// rather than silently producing a wrong image from the wrong assumption.
int demosaic_xtrans_fast(const uint16_t* mosaic, int w, int h,
                          const char xtrans[6][6],
                          float black, float white,
                          float* R, float* G, float* B) {
    (void)mosaic; (void)w; (void)h; (void)xtrans; (void)black; (void)white;
    (void)R; (void)G; (void)B;
    return 1;
}

#endif
