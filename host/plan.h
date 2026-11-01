#ifndef BW_PLAN_H
#define BW_PLAN_H

#include <stdint.h>

/* One drawable image layer in scene pixels. y grows downward.
   full means stretch to the framebuffer (a loose PNG/JPEG, or a model
   with no scene size). rgba is owned by the plan until bw_plan_free. */
typedef struct BwLayer {
    uint8_t* rgba;
    int w, h;
    float cx, cy, lw, lh, deg, alpha;
    int full;
    int missing;
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
/* 1 when the scene would show a decoded picture or a text layer.
   0 when the picture would be the missing-texture checker. */
int bw_scene_ready(const char* dir, const char* assets);
int bw_tex_decode(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h);

#endif
