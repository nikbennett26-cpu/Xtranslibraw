// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren  (integration glue)
// LMMSE algorithm: Copyright (C) Gabor Horvath / RawTherapee (GPL-3).
//
// Adapts RawTherapee's lmmse_interpolate_omp to our standalone CFA interface,
// mirroring demosaic_amaze.cpp: build float rawData (black-subtracted, scaled to
// RT's 0..65535 range), provide an FC() shim, call the ported routine, and copy
// the planes out normalised to [0,1]. When vendor/RawTherapee is absent HAVE_LMMSE
// is undefined and we fall back to bilinear so the artifact always builds.
#include "demosaic.h"

#ifdef HAVE_LMMSE
#include "lmmse_standalone.h"
#include "cfa.h"
#include <vector>
#include <algorithm>

namespace {
struct FcCtx { const int* cfa; };
int fc_shim(int row, int col, void* ctx) {
    return fc(row, col, static_cast<FcCtx*>(ctx)->cfa);
}
} // namespace

int demosaic_lmmse(const uint16_t* mosaic, int w, int h, const int cfa[4],
                   float black, float white,
                   float* R, float* G, float* B) {
    const float range = std::max(1.0f, white - black);
    const float toRT   = 65535.0f / range;   // RT expects ~0..65535 float raw
    const float fromRT = 1.0f / 65535.0f;

    std::vector<float>  rawFlat((size_t)w * h);
    std::vector<float*> rawRows(h), redRows(h), greenRows(h), blueRows(h);
    std::vector<float>  red((size_t)w * h), green((size_t)w * h), blue((size_t)w * h);
    for (int y = 0; y < h; ++y) {
        rawRows[y]   = &rawFlat[(size_t)y * w];
        redRows[y]   = &red[(size_t)y * w];
        greenRows[y] = &green[(size_t)y * w];
        blueRows[y]  = &blue[(size_t)y * w];
        for (int x = 0; x < w; ++x) {
            float v = ((float)mosaic[(size_t)y * w + x] - black) * toRT;
            rawRows[y][x] = v < 0.0f ? 0.0f : v;
        }
    }

    FcCtx ctx{cfa};
    lmmse_demosaic(w, h, rawRows.data(), redRows.data(), greenRows.data(),
                   blueRows.data(), &fc_shim, &ctx);

    for (size_t i = 0, n = (size_t)w * h; i < n; ++i) {
        R[i] = std::min(1.0f, std::max(0.0f, red[i]   * fromRT));
        G[i] = std::min(1.0f, std::max(0.0f, green[i] * fromRT));
        B[i] = std::min(1.0f, std::max(0.0f, blue[i]  * fromRT));
    }
    return 0;
}

#else  // HAVE_LMMSE not wired — fall back to bilinear

int demosaic_lmmse(const uint16_t* mosaic, int w, int h, const int cfa[4],
                   float black, float white,
                   float* R, float* G, float* B) {
    demosaic_bilinear(mosaic, w, h, cfa, black, white, R, G, B);
    return 1;  // signals "fell back"
}

#endif
