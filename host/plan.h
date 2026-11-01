#ifndef BW_PLAN_H
#define BW_PLAN_H

#include <stdint.h>

/* One decoded TEXB image. A TEXS sheet keeps these so a later frame can
   crop without reading the file again. */
typedef struct BwSheet {
    uint8_t* rgba;
    int w, h;
} BwSheet;

/* One TEXS frame. dur is seconds. The six floats are the affine basis
   (origin, width axis, height axis) in sheet pixels. */
typedef struct BwFrame {
    int sheet;
    float dur;
    float x, y, w, wy, hx, h;
} BwFrame;

/* One drawable image layer in scene pixels. y grows downward.
   full means stretch to the framebuffer (a loose PNG/JPEG, or a model
   with no scene size). rgba is frame 0 (or the still). sheets and frames
   are owned by the plan until bw_plan_free, unless the player takes them. */
typedef struct BwLayer {
    uint8_t* rgba;
    int w, h;
    float cx, cy, lw, lh, deg, alpha;
    int full;
    int missing;
    BwSheet* sheets;
    int nsheets;
    BwFrame* frames;
    int nframes;
} BwLayer;

typedef struct BwPlan {
    char type[32];
    char file[512];
    char title[160];
    char text[160];
    char video[1100];
    char page[1100];
    char kind[16];
    int scroll, spin, opacity, colorkey, tint, pulse;
    int image_tried, image_ok, missing;
    int has_text;
    float scene_w, scene_h;
    BwLayer* layers;
    int nlayers;
} BwPlan;

/* Top-level project keys only. Scene images walk object → model → material
   → texture, then a .tex / PNG / JPEG in the project or BENDWALL_ASSETS.
   A missing texture is logged and stored as a checkerboard layer. */
int bw_plan_load(const char* dir, const char* assets, BwPlan* out);
void bw_plan_free(BwPlan* plan);
/* 1 when every requested picture decoded, or the scene is text only.
   0 when any texture is missing or failed: the frame would include the
   checker, so the probe says partial texture. */
int bw_scene_ready(const char* dir, const char* assets);
int bw_tex_decode(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h);
/* Same decoder, at a playback time. TEXS picks the frame; a still ignores time. */
int bw_tex_decode_time(const uint8_t* b, int n, int time_ms, uint8_t** rgba, int* w, int* h);
int bw_frame_index(const BwFrame* frames, int nframes, int time_ms);
int bw_tex_crop(const BwSheet* sheets, int nsheets, const BwFrame* frame, uint8_t** rgba, int* w, int* h);

#endif
