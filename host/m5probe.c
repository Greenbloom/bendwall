#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bw_audio_capture(float* dst, int n);
char* bw_script_update(const char* src, const char* value);
int bw_video_rgba(const char* path, int* w, int* h, uint8_t** rgba);

int main(int argc, char** argv) {
  float tmp[8];
  char* s;
  int w = 0, h = 0, cap, i, bad = 0;
  uint8_t* px = NULL;
  const char* vid = argc > 1 ? argv[1] : "fixtures/m5/red.mkv";
  cap = bw_audio_capture(tmp, 8);
  printf("AUDIO capture=%d\n", cap);
  s = bw_script_update("function update(value){ return value + \"!\"; }", "hi");
  printf("SCRIPT fn=%s\n", s ? s : "");
  free(s);
  s = bw_script_update("return value;", "1");
  printf("SCRIPT body=%s\n", s ? s : "");
  free(s);
  if (!bw_video_rgba(vid, &w, &h, &px)) {
    printf("VIDEO fail\n");
    return 1;
  }
  printf("VIDEO %dx%d", w, h);
  for (i = 0; i < 4 && px; i++) printf(" %u", px[i]);
  printf("\n");
  if (w < 1 || !px || px[0] < 200 || px[1] > 30 || px[2] > 30) bad = 1;
  free(px);
  return bad;
}
