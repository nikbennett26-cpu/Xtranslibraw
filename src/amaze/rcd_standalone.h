// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren  (standalone wrapper)
// RCD algorithm: Copyright (c) Luis Sanz Rodriguez / RawTherapee (GPL-3).
//
// Free-function entry point that adapts RawTherapee's rcd_demosaic to the
// libraw-wasm-gpl3 CFA interface. Implemented in rcd_port.cc.
#pragma once

// Demosaic a Bayer mosaic using RawTherapee's RCD.
//   width, height : image dimensions
//   rawData       : height row pointers into a float mosaic, black-subtracted
//                   and scaled to RawTherapee's ~0..65535 range
//   red/green/blue : height row pointers to output planes (written ~0..65535)
//   fcfn(row,col,ctx) : returns 0=R, 1=G, 2=B for the given pixel
//   fcctx         : opaque context passed to fcfn
void rcd_demosaic(int width, int height,
                  float* const* rawData,
                  float** red, float** green, float** blue,
                  int (*fcfn)(int, int, void*), void* fcctx);
