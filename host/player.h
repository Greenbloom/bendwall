#ifndef BW_PLAYER_H
#define BW_PLAYER_H

#include <stdint.h>

/* Live scene image. Video, a still image, and the text bitmap are drawn
   into one color target, then the built-in effect passes. Plasma paints
   the readback. One player per process. */
int bw_play_open(const char* dir, int w, int h);
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

#endif
