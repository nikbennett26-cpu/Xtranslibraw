// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Phil Warren
#include "api.h"
#include "demosaic.h"
#include <vector>
#include <cstring>

static int dispatch_demosaic(const uint16_t* m, int w, int h, const int cfa[4],
                             float black, float white, int qual,
                             const float* rgb_cam,
                             float* R, float* G, float* B) {
    switch (qual) {
        case QUAL_AMAZE:
            return demosaic_amaze(m, w, h, cfa, black, white, R, G, B);
        case QUAL_LMMSE:
            return demosaic_lmmse(m, w, h, cfa, black, white, R, G, B);
        case QUAL_RCD:
            return demosaic_rcd(m, w, h, cfa, black, white, R, G, B);
        case QUAL_IGV:
            return demosaic_igv(m, w, h, cfa, black, white, R, G, B);
        case QUAL_AHD:
            return demosaic_ahd(m, w, h, cfa, black, white, rgb_cam, R, G, B);
        case QUAL_BILINEAR:
        default:
            demosaic_bilinear(m, w, h, cfa, black, white, R, G, B);
            return 0;
    }
}

extern "C" int dm_demosaic_raw(const uint16_t* mosaic, int w, int h,
                               int cfa0, int cfa1, int cfa2, int cfa3,
                               float black, float white, int qual,
                               float* R, float* G, float* B) {
    const int cfa[4] = {cfa0, cfa1, cfa2, cfa3};
    // No camera matrix available on the pure path; AHD falls back to identity.
    return dispatch_demosaic(mosaic, w, h, cfa, black, white, qual, nullptr, R, G, B);
}

#ifdef HAVE_LIBRAW
#include <libraw/libraw.h>

struct LRContext {
    LibRaw proc;
    int w = 0, h = 0;
    int cfa[4] = {0, 1, 1, 2};
    int black = 0, white = 65535;
    std::vector<uint16_t> mosaic;   // cropped visible area
    float cam_mul[4] = {1, 1, 1, 1};
    float rgb_cam[12] = {0};
};

// Map LibRaw COLOR() code (0=R,1=G,2=B,3=G2) to our 0=R 1=G 2=B.
static inline int normColour(int c) { return c == 3 ? 1 : c; }

extern "C" LRContext* lr_open(const uint8_t* bytes, size_t len) {
    LRContext* ctx = new LRContext();
    if (ctx->proc.open_buffer((void*)bytes, len) != LIBRAW_SUCCESS ||
        ctx->proc.unpack() != LIBRAW_SUCCESS) {
        delete ctx;
        return nullptr;
    }
    // Only classic 2x2 Bayer is supported here. X-Trans (filters==9) and
    // full-colour / Foveon (filters==0) need different handling — the host app
    // falls back to its preview path for these.
    const unsigned filters = ctx->proc.imgdata.idata.filters;
    if (filters == 0 || filters == 9) {
        delete ctx;
        return nullptr;
    }
    auto& S = ctx->proc.imgdata.sizes;
    auto& C = ctx->proc.imgdata.color;

    const int W = S.width, H = S.height;         // visible dimensions
    const int L = S.left_margin, T = S.top_margin;
    const int RW = S.raw_width;
    ctx->w = W; ctx->h = H;

    // CFA from visible top-left (COLOR uses absolute raw coordinates).
    ctx->cfa[0] = normColour(ctx->proc.COLOR(T + 0, L + 0));
    ctx->cfa[1] = normColour(ctx->proc.COLOR(T + 0, L + 1));
    ctx->cfa[2] = normColour(ctx->proc.COLOR(T + 1, L + 0));
    ctx->cfa[3] = normColour(ctx->proc.COLOR(T + 1, L + 1));

    // Black level. LibRaw exposes a base black plus a cblack[] structure:
    //   cblack[0..3] : per-CFA-channel offsets, OR
    //   cblack[4],cblack[5] : rows,cols of a repeating black pattern whose
    //   cblack[6 + r*cols + c] values apply per position (Nikon uses this).
    // Our demosaic takes a single scalar, so average whichever form is present
    // (pattern values are typically uniform, making the average exact).
    double blackAcc = 0.0; int blackN = 0;
    if (C.cblack[4] && C.cblack[5]) {
        const unsigned cnt = C.cblack[4] * C.cblack[5];
        for (unsigned i = 0; i < cnt; ++i) { blackAcc += C.cblack[6 + i]; ++blackN; }
    } else {
        for (int i = 0; i < 4; ++i) { blackAcc += C.cblack[i]; ++blackN; }
    }
    ctx->black = C.black + (blackN ? (int)(blackAcc / blackN + 0.5) : 0);
    ctx->white = C.maximum > 0 ? C.maximum : 65535;

    for (int i = 0; i < 4; ++i) ctx->cam_mul[i] = C.cam_mul[i];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            ctx->rgb_cam[r * 4 + c] = C.rgb_cam[r][c];

    // Crop the visible mosaic out of the full raw plane.
    const uint16_t* raw = ctx->proc.imgdata.rawdata.raw_image;
    if (!raw) { delete ctx; return nullptr; }
    ctx->mosaic.resize((size_t)W * H);
    for (int y = 0; y < H; ++y)
        std::memcpy(&ctx->mosaic[(size_t)y * W],
                    &raw[(size_t)(T + y) * RW + L],
                    (size_t)W * sizeof(uint16_t));
    return ctx;
}

extern "C" int  lr_width(LRContext* c)  { return c ? c->w : 0; }
extern "C" int  lr_height(LRContext* c) { return c ? c->h : 0; }
extern "C" int  lr_black(LRContext* c)  { return c ? c->black : 0; }
extern "C" int  lr_white(LRContext* c)  { return c ? c->white : 0; }
extern "C" void lr_cfa(LRContext* c, int out4[4]) {
    for (int i = 0; i < 4; ++i) out4[i] = c ? c->cfa[i] : 0;
}
extern "C" void lr_cam_mul(LRContext* c, float out4[4]) {
    for (int i = 0; i < 4; ++i) out4[i] = c ? c->cam_mul[i] : 1.0f;
}
extern "C" void lr_rgb_cam(LRContext* c, float out12[12]) {
    for (int i = 0; i < 12; ++i) out12[i] = c ? c->rgb_cam[i] : 0.0f;
}
extern "C" const uint16_t* lr_mosaic(LRContext* c) {
    return (c && !c->mosaic.empty()) ? c->mosaic.data() : nullptr;
}

// DCB and DHT are LibRaw's own interpolators (no RawTherapee equivalent), run
// through dcraw_process. We configure it to emit camera-native linear RGB
// (raw colour, unity WB, linear gamma, sensor orientation) so the output is
// interchangeable with our custom demosaic planes: [0,1], pre-white-balance.
static int libraw_native_demosaic(LRContext* c, int user_qual,
                                  float* R, float* G, float* B) {
    auto& P = c->proc.imgdata.params;
    P.user_qual      = user_qual;   // 4 = DCB, 11 = DHT
    P.output_color   = 0;           // raw camera colour, no colour matrix
    P.output_bps     = 16;
    P.no_auto_bright = 1;
    P.use_camera_wb  = 0;
    P.use_auto_wb    = 0;
    P.user_mul[0] = P.user_mul[1] = P.user_mul[2] = P.user_mul[3] = 1.0f; // no WB
    P.gamm[0] = P.gamm[1] = 1.0;    // linear
    P.user_flip      = 0;           // keep sensor orientation (match custom path)
    P.half_size      = 0;
    P.highlight      = 0;
    if (user_qual == 4) { P.dcb_iterations = 0; P.dcb_enhance_fl = 1; }

    if (c->proc.dcraw_process() != LIBRAW_SUCCESS) return -2;
    int err = 0;
    libraw_processed_image_t* img = c->proc.dcraw_make_mem_image(&err);
    if (!img) return err ? err : -3;
    if (img->type != LIBRAW_IMAGE_BITMAP || img->bits != 16 || img->colors != 3) {
        c->proc.dcraw_clear_mem(img);
        return -4;
    }
    const uint16_t* px = reinterpret_cast<const uint16_t*>(img->data);
    const int iw = img->width, ih = img->height;
    const float inv = 1.0f / 65535.0f;
    for (int y = 0; y < c->h; ++y)
        for (int x = 0; x < c->w; ++x) {
            const int di = y * c->w + x;
            if (y < ih && x < iw) {
                const uint16_t* p = px + ((size_t)y * iw + x) * 3;
                R[di] = p[0] * inv; G[di] = p[1] * inv; B[di] = p[2] * inv;
            } else {
                R[di] = G[di] = B[di] = 0.0f;
            }
        }
    c->proc.dcraw_clear_mem(img);
    return 0;
}

extern "C" int lr_demosaic(LRContext* c, int qual, float* R, float* G, float* B) {
    if (!c || c->mosaic.empty()) return -1;
    if (qual == QUAL_DCB || qual == QUAL_DHT)
        return libraw_native_demosaic(c, qual == QUAL_DCB ? 4 : 11, R, G, B);
    return dispatch_demosaic(c->mosaic.data(), c->w, c->h, c->cfa,
                             (float)c->black, (float)c->white, qual,
                             c->rgb_cam, R, G, B);
}
extern "C" void lr_free(LRContext* c) { delete c; }
#endif // HAVE_LIBRAW
