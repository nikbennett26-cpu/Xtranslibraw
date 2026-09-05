// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren (standalone wrapper) -- DRAFT, structure
// now matches port-amaze.md's documented convention exactly (confirmed
// against the real doc, not guessed): this declares the bridge function
// implemented in the paired port file (src/amaze/xtrans_fast_port.cc),
// same relationship as amaze_standalone.h has with amaze_port.cc.
//
// Fast X-Trans algorithm: adapted from RawTherapee's
// RawImageSource::xtransborder_interpolate + fast_xtrans_interpolate
// (GPL-3, Gabor Horvath / RawTherapee contributors). Deliberately the
// smaller, shallower-dependency tier -- no CIELAB conversion, no
// multi-pass tile reconstruction like full Markesteijn has.
//
// Algorithm logic verified against a real Fuji X-E2 RAF file (real 6x6
// pattern extracted via rawpy/LibRaw) via a Python reimplementation --
// produced a clean, artifact-free photo, using this exact 0..65535 scale
// (confirmed correct against the real demosaic_rcd.cpp/demosaic_ahd.cpp).
// This exact C++ has not been compiled; that's still the one open step.
#pragma once

// Bridge into the ported algorithm. rawData is already black-subtracted
// and scaled to RT's native ~0..65535 range (port-amaze.md's own
// documented signature target) -- the black/white conversion and [0,1]
// output normalisation both happen in the paired wrapper,
// src/demosaic_xtrans_fast.cpp, not here.
//   w, h    : image dimensions
//   xtrans  : the sensor's real 6x6 CFA pattern (already widened from
//             LibRaw's char[6][6] by the caller)
//   rawData : height row pointers into the ~0..65535-scaled mosaic
//   red/green/blue : height row pointers to output planes, same scale
void xtrans_fast_demosaic_port(int w, int h, const int xtrans[6][6],
                                float* const* rawData,
                                float** red, float** green, float** blue);
