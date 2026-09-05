// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren
//
// Demosaic core: pure functions over a Bayer mosaic, independent of LibRaw so
// they can be built and unit-tested on their own. Inputs are the raw u16 mosaic
// plus black/white levels; outputs are planar linear RGB normalised to [0,1].
#pragma once
#include <cstdint>

// qual codes shared with the C API (see api.h).
enum DemosaicQual {
    QUAL_BILINEAR = 0,
    QUAL_AMAZE    = 1,
    QUAL_LMMSE    = 2,
    QUAL_DCB      = 3,  // LibRaw native (dcraw_process user_qual 4); full build only
    QUAL_DHT      = 4,  // LibRaw native (dcraw_process user_qual 11); full build only
    QUAL_RCD      = 5,  // RawTherapee port
    QUAL_IGV      = 6,  // RawTherapee port
    QUAL_AHD      = 7,  // RawTherapee port
    QUAL_XTRANS_FAST = 8,  // X-Trans only (fast tier); RawTherapee port
};

// Bilinear (our GPL-3 baseline). Always available.
void demosaic_bilinear(const uint16_t* mosaic, int w, int h, const int cfa[4],
                       float black, float white,
                       float* R, float* G, float* B);

// AMaZE, ported from RawTherapee (GPL-3). Falls back to bilinear until the
// vendored RT source is wired in (HAVE_AMAZE). Returns 0 on success, 1 if it
// fell back.
int demosaic_amaze(const uint16_t* mosaic, int w, int h, const int cfa[4],
                   float black, float white,
                   float* R, float* G, float* B);

// LMMSE, ported from RawTherapee (GPL-3). Falls back to bilinear until the
// vendored RT source is wired in (HAVE_LMMSE). Returns 0 on success, 1 if it
// fell back.
int demosaic_lmmse(const uint16_t* mosaic, int w, int h, const int cfa[4],
                   float black, float white,
                   float* R, float* G, float* B);

// RCD, ported from RawTherapee (GPL-3). Falls back to bilinear until the
// vendored RT source is wired in (HAVE_RCD). Returns 0 on success, 1 if it
// fell back.
int demosaic_rcd(const uint16_t* mosaic, int w, int h, const int cfa[4],
                 float black, float white,
                 float* R, float* G, float* B);

// IGV, ported from RawTherapee (GPL-3). Falls back to bilinear until the
// vendored RT source is wired in (HAVE_IGV). Returns 0 on success, 1 if it
// fell back.
int demosaic_igv(const uint16_t* mosaic, int w, int h, const int cfa[4],
                 float black, float white,
                 float* R, float* G, float* B);

// AHD, ported from RawTherapee (GPL-3). Falls back to bilinear until the
// vendored RT source is wired in (HAVE_AHD). rgb_cam is an optional row-major
// 3x4 camera->linear matrix (12 floats, nullable). Returns 0 on success, 1 if
// it fell back.
int demosaic_ahd(const uint16_t* mosaic, int w, int h, const int cfa[4],
                 float black, float white, const float* rgb_cam,
                 float* R, float* G, float* B);

// Fast X-Trans demosaic, ported from RawTherapee (GPL-3). X-Trans only --
// meaningless for Bayer sensors, unlike every other function here. Takes
// LibRaw's raw char[6][6] CFA pattern directly (imgdata.idata.xtrans) --
// widening to int happens internally, not the caller's job. No bilinear
// fallback exists for this one (a 2x2 repeat assumption is meaningless for
// a 6x6 pattern); returns 1 rather than guessing if HAVE_XTRANS_FAST isn't
// wired in, leaving R/G/B untouched.
int demosaic_xtrans_fast(const uint16_t* mosaic, int w, int h,
                         const char xtrans[6][6],
                         float black, float white,
                         float* R, float* G, float* B);
