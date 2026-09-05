# Proposed api.cpp changes for X-Trans support (fast tier)

Based on reading the real api.cpp you sent. NOT compiled — same caveat as
everything else so far. This is written as a clear diff rather than a
full file replacement so you can see exactly what's proposed and why,
against the actual lines in your real file.

## 1. LRContext struct (around line 42-50) — add two fields

```cpp
struct LRContext {
    LibRaw proc;
    int w = 0, h = 0;
    int cfa[4] = {0, 1, 1, 2};
    int xtrans[6][6] = {{0}};   // NEW — only populated when isXTrans is true
    bool isXTrans = false;      // NEW
    int black = 0, white = 65535;
    std::vector<uint16_t> mosaic;
    float cam_mul[4] = {1, 1, 1, 1};
    float rgb_cam[12] = {0};
};
```

## 2. lr_open() rejection check (lines 62-69) — split X-Trans from Foveon

Foveon (filters==0) is a genuinely different problem — full-colour sensor,
no demosaic needed or possible in the usual sense — and stays rejected.
X-Trans (filters==9) now takes its own path instead of an early return:

```cpp
// Foveon (filters==0) has no CFA to demosaic at all — different problem,
// still out of scope here. X-Trans (filters==9) now has real handling
// below instead of being rejected alongside it.
const unsigned filters = ctx->proc.imgdata.idata.filters;
if (filters == 0) {
    delete ctx;
    return nullptr;
}
ctx->isXTrans = (filters == 9);
```

## 3. CFA extraction (lines 78-82) — branch on isXTrans

The existing `COLOR()`-sampling approach is Bayer-specific (it only makes
sense for a 2x2 repeat). X-Trans doesn't need that — LibRaw already hands
over the full 6x6 pattern directly as `imgdata.idata.xtrans`, just as
`char`, not `int` (confirmed from the real libraw_types.h):

```cpp
if (ctx->isXTrans) {
    // imgdata.idata.xtrans is char[6][6] — widening copy into our int[6][6],
    // not a straight memcpy (this is the type mismatch caught earlier
    // while reviewing libraw_types.h).
    for (int r = 0; r < 6; ++r)
        for (int col = 0; col < 6; ++col)
            ctx->xtrans[r][col] = ctx->proc.imgdata.idata.xtrans[r][col];
} else {
    // existing Bayer path, unchanged
    ctx->cfa[0] = normColour(ctx->proc.COLOR(T + 0, L + 0));
    ctx->cfa[1] = normColour(ctx->proc.COLOR(T + 0, L + 1));
    ctx->cfa[2] = normColour(ctx->proc.COLOR(T + 1, L + 0));
    ctx->cfa[3] = normColour(ctx->proc.COLOR(T + 1, L + 1));
}
```

Everything else in lr_open() — black/white level, cam_mul, rgb_cam, and
critically the mosaic-cropping loop at the bottom (lines 105-112) — reads
S.width/height/left_margin/top_margin/raw_width, none of which are
CFA-shape-specific. X-Trans sensors crop the same rectangular way Bayer
ones do; only the pattern shape differs. So none of that code needs to
change or branch — it already works for both once the rejection above is
narrowed to just Foveon.

## 4. lr_demosaic() (lines 178-185) — route X-Trans before the normal dispatch

dispatch_demosaic()'s signature takes `const int cfa[4]` — a fixed 2x2
shape baked into every existing demosaic_XXX() function. X-Trans's 6x6
pattern doesn't fit that shape, so it can't reuse dispatch_demosaic() as
written. Simplest, least invasive option: handle it as its own branch,
the same way QUAL_DCB/QUAL_DHT already get special-cased above it:

```cpp
extern "C" int lr_demosaic(LRContext* c, int qual, float* R, float* G, float* B) {
    if (!c || c->mosaic.empty()) return -1;
    if (c->isXTrans) {
        // Only the fast tier exists right now — see xtrans_fast_port.cc.
        // qual is ignored here since there's only one X-Trans option so far;
        // extending this switch is how a future Markesteijn tier would slot in.
        return demosaic_xtrans_fast(c->mosaic.data(), c->w, c->h, c->xtrans,
                                     (float)c->black, (float)c->white, R, G, B);
    }
    if (qual == QUAL_DCB || qual == QUAL_DHT)
        return libraw_native_demosaic(c, qual == QUAL_DCB ? 4 : 11, R, G, B);
    return dispatch_demosaic(c->mosaic.data(), c->w, c->h, c->cfa,
                             (float)c->black, (float)c->white, qual,
                             c->rgb_cam, R, G, B);
}
```

## The earlier gap — now resolved, not just papered over

The previous draft of this patch had `xtrans_fast_demosaic()` taking an
already-converted float mosaic, with an explicit TODO for where that
conversion would come from. Got `demosaic.h` since then, which documents
the real contract directly: every demosaic_XXX() function takes the raw
`uint16_t* mosaic` plus `black`/`white` as plain scalars, and outputs
planar RGB normalised to `[0,1]` — not the ~0..65535 range my draft had
assumed. `xtrans_fast_port.cc` has been corrected to match: it's now
`demosaic_xtrans_fast()`, same parameter shape as its siblings, doing the
black/white-to-[0,1] conversion internally rather than expecting it
pre-done. Also checked (not assumed) that the underlying algorithm's own
weighted-average math has no absolute constants or integer clamping
anywhere in it, so feeding it [0,1]-scaled input instead of the
~0..65535 range it was originally verified against produces the same
result correctly rescaled, not a different one.

This one is genuinely done, pending your compile.
