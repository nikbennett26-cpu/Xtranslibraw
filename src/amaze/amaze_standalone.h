// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren  (standalone wrapper)
// AMaZE algorithm: Copyright (C) Emil J. Martinec / RawTherapee (GPL-3).
//
// Free-function entry point that adapts RawTherapee's amaze_demosaic_RT to the
// libraw-wasm-gpl3 CFA interface. Implemented in amaze_port.cc.
#pragma once

// Demosaic a Bayer mosaic using RawTherapee's AMaZE.
//   width, height : image dimensions
//   rawData       : height row pointers into a float mosaic, black-subtracted
//                   and scaled to RawTherapee's ~0..65535 range
//   red/green/blue : height row pointers to output planes (written ~0..65535)
//   fcfn(row,col,ctx) : returns 0=R, 1=G, 2=B for the given pixel
//   fcctx         : opaque context passed to fcfn
void amaze_demosaic(int width, int height,
                    float* const* rawData,
                    float** red, float** green, float** blue,
                    int (*fcfn)(int, int, void*), void* fcctx);
