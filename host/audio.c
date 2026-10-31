/* Pulse/PipeWire loopback. Bend has no audio binding. A missing server, or a
   read that does not return within a second, answers 0 so the wallpaper
   keeps drawing. Samples stay in this process; Bend only sees the count. */
#include <pulse/error.h>
#include <pulse/simple.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static void bw_alrm(int s) { (void)s; }

int bw_audio_capture(float* dst, int n) {
  pa_sample_spec ss;
  pa_buffer_attr attr;
  pa_simple* s;
  struct sigaction sa, old;
  int err = 0;
  int got;
  if (!dst || n < 2) return 0;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = bw_alrm;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, &old);
  alarm(1);
  memset(&ss, 0, sizeof ss);
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = 44100;
  ss.channels = 2;
  memset(&attr, 0, sizeof attr);
  attr.maxlength = (uint32_t)n * (uint32_t)sizeof(float);
  attr.fragsize = attr.maxlength;
  s = pa_simple_new(NULL, "bendwall", PA_STREAM_RECORD, NULL, "loop", &ss, NULL, &attr, &err);
  if (!s) {
    alarm(0);
    sigaction(SIGALRM, &old, NULL);
    return 0;
  }
  got = pa_simple_read(s, dst, (size_t)n * sizeof(float), &err);
  alarm(0);
  sigaction(SIGALRM, &old, NULL);
  pa_simple_free(s);
  if (got < 0) return 0;
  return n;
}
