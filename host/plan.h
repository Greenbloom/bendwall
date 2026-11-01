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

/* One sprite particle system, already scaled by instanceoverride.
   Positions are scene pixels, x right, y down, the same as image layers.
   size_b < 0 means size grows by size_rate per second instead of a lerp.
   rgba is the material texture, or a white radial sprite when that
   texture has no name or does not resolve. Particles are never the checker. */
typedef struct BwPartSys {
    float ox, oy;
    float dirx, diry;
    float dist0, dist1;
    float rate;
    float life0, life1;
    float speed0, speed1;
    float size0, size1;
    float size_a, size_b, size_rate;
    float a0, a1;
    float fade_in, fade_out;
    int alphafade;
    float r0, g0, b0, r1, g1, b1;
    float er, eg, eb;
    int color_lerp;
    float gravx, gravy, drag;
    float ang0, ang1, ang_rate;
    /* starttime is a pre-simulate, in seconds, before the wallpaper clock. */
    float start;
    float attract, cpx, cpy, attract_thr;
    int has_attract;
    int cp_mouse;
    int additive;
    int maxcount;
    /* instantaneous is a count, not a flag. */
    int instant;
    int box;
    float bminx, bminy, bminz, bmaxx, bmaxy, bmaxz;
    float dvx, dvy, dvz;
    int has_vel;
    float vminx, vminy, vminz, vmaxx, vmaxy, vmaxz;
    int has_turb;
    float turb_scale, turb_off, turb_s0, turb_s1, turb_p0, turb_p1, turb_time;
    float turb_fx, turb_fy, turb_fz;
    float turb_rx, turb_ry, turb_rz;
    int has_angv;
    float av0, av1;
    float ang_force, ang_drag;
    int ang_op;
    float sx, sy, deg;
    int rope;
    float rope_len;
    int anim;
    float seqmul;
    float size_exp;
    int follow;
    int parent;
    uint8_t* rgba;
    int tw, th;
    BwSheet* sheets;
    int nsheets;
    BwFrame* frames;
    int nframes;
} BwPartSys;

typedef struct BwPlan {
    char type[32];
    char file[512];
    char title[160];
    char text[512];
    char video[1100];
    char page[1100];
    char kind[16];
    int scroll, spin, opacity, colorkey, tint, pulse;
    int image_tried, image_ok, missing;
    int has_text;
    float scene_w, scene_h;
    BwLayer* layers;
    int nlayers;
    BwPartSys* parts;
    int nparts;
    /* Soundtrack. 1 is full. A general.volume or a property named volume
       above 1 is a percent. mute / muted silence the track. */
    float video_volume;
    int video_mute;
} BwPlan;

/* Top-level project keys only. Scene images walk object → model → material
   → texture, then a .tex / PNG / JPEG in the project or BENDWALL_ASSETS.
   A .jpg/.png slot is also materials/<path-without-ext>.tex. A missing
   image texture is logged and stored as a checkerboard layer. An image
   value of null is not a layer. The project title is not drawn. */
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
