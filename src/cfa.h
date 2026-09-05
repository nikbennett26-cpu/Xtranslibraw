// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren
//
// CFA (colour filter array) helper. LibRaw exposes the Bayer pattern as a packed
// value; here we work with an explicit 2x2 pattern array cfa[4] laid out
// row-major from the sensor's top-left:
//     cfa[0] cfa[1]     with colour codes  0 = R, 1 = G, 2 = B
//     cfa[2] cfa[3]
// This decouples the demosaic code from LibRaw / RawTherapee's own FC().
#pragma once
#include <cstdint>

// Colour of the pixel at (row, col) for a 2x2 CFA. Bayer only (no X-Trans).
static inline int fc(int row, int col, const int cfa[4]) {
    return cfa[((row & 1) << 1) | (col & 1)];
}
