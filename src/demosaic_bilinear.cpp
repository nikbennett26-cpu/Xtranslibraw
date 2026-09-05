// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren
//
// From-scratch bilinear demosaic. This is the GPL-3 baseline: correct, simple,
// and dependency-free, used to prove the toolchain/API/glue end-to-end and as
// the fast fallback for low-end GPUs. AMaZE/LMMSE (RawTherapee) replace it for
// quality output.
#include "demosaic.h"
#include "cfa.h"
#include <algorithm>

namespace {
inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
// Average of in-bounds neighbours matching predicate on colour.
} // namespace

void demosaic_bilinear(const uint16_t* mosaic, int w, int h, const int cfa[4],
                       float black, float white,
                       float* R, float* G, float* B) {
    const float scale = 1.0f / std::max(1.0f, white - black);
    auto at = [&](int y, int x) -> float {
        float v = (float)mosaic[(size_t)y * w + x] - black;
        return v * scale;
    };
    auto avg = [&](int y, int x, int wantColour) -> float {
        float sum = 0.0f; int n = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int yy = y + dy, xx = x + dx;
                if (yy < 0 || xx < 0 || yy >= h || xx >= w) continue;
                if (fc(yy, xx, cfa) != wantColour) continue;
                sum += at(yy, xx); ++n;
            }
        }
        return n ? sum / n : 0.0f;
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = (size_t)y * w + x;
            const int c = fc(y, x, cfa);
            const float v = clampf(at(y, x), 0.0f, 1.0f);
            float r, g, b;
            if (c == 0) {            // R site
                r = v;
                g = avg(y, x, 1);
                b = avg(y, x, 2);
            } else if (c == 2) {     // B site
                b = v;
                g = avg(y, x, 1);
                r = avg(y, x, 0);
            } else {                 // G site
                g = v;
                r = avg(y, x, 0);
                b = avg(y, x, 2);
            }
            R[i] = clampf(r, 0.0f, 1.0f);
            G[i] = clampf(g, 0.0f, 1.0f);
            B[i] = clampf(b, 0.0f, 1.0f);
        }
    }
}
