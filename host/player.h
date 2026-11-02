#ifndef BW_PLAYER_H
#define BW_PLAYER_H

#include <stdint.h>

/* Live scene image. Video, a still image, and the text bitmap are drawn
   into one color target, then the built-in effect passes. Plasma paints
   the readback. One player per process. */
int bw_play_open(const char* dir, int w, int h);
/* Plasma volume, 0..1. The default is 1. 0 mutes. Multiplied by the
   project volume from general / properties. */
void bw_play_set_video_volume(float volume);
float bw_play_video_volume(void);
/* Plasma mute. The default is on, so a soundtrack is opt-in. */
void bw_play_set_video_mute(int on);
/* 0 never ducks. That is the default. -1 ducks when another app is
   fullscreen or playing audio, unless BENDWALL_VIDEO_DUCK=0. Plasma
   AutoMuteOthers sets -1. 1 forces silence. */
void bw_play_set_video_duck(int mode);
/* Last soundtrack samples after volume, mute, and duck. Not the spectrum. */
int bw_play_video_pcm(float* dst, int cap);
void bw_play_audio(const float* interleaved, int nfloats);
int bw_play_step(int dt_ms);
int bw_play_rgba(uint8_t* dst, int cap);
int bw_play_size(int* w, int* h);
const char* bw_play_line(void);
const char* bw_play_kind(void);
const char* bw_play_page(void);
int bw_play_audio_out(float* dst128);
int bw_play_band(int which, int index, float* out);
int bw_play_png(const char* path);
void bw_play_close(void);
/* 0 skips glReadPixels. The default is on, so grabs and tests keep pixels.
   The live window turns it off and samples the color texture instead. */
void bw_play_set_readback(int on);
/* Mouse in the item, pixels, y down. valid is 0 when no pointer has moved. */
void bw_play_pointer(float x, float y, float item_w, float item_h, int valid);
/* Last frame split, microseconds. Read is 0 when readback is off. */
void bw_play_profile(int* sim_us, int* gl_us, int* read_us);
/* Sample the published color texture into the current GL framebuffer.
   Three textures rotate so the one in this draw is not cleared. */
int bw_play_blit(void);
/* 1 after a present-path frame exported its color texture. */
int bw_play_exported(void);

#endif
