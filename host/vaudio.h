#ifndef BW_VAUDIO_H
#define BW_VAUDIO_H

/* The video picture decoder does not share this demuxer. A seek on the
   picture stream cannot starve the soundtrack, and these samples are not
   written into the spectrum buffer. */
typedef float (*BwGainFn)(void);
void bw_va_open(const char* path, BwGainFn gain);
void bw_va_close(void);
int bw_va_pcm(float* dst, int cap);

#endif
