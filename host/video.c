/* Video frames to RGBA, and into the current GL context as a texture.
   Bend has no codec library. Frame 0 is what the probe uses to decide that
   a file decodes. bw_video_tex uploads frame `index` for playback. */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdlib.h>
#include <string.h>

int bw_gl_rgba(int w, int h, const uint8_t* px, int n);

static int convert_frame(AVFrame* fr, int* w, int* h, uint8_t** rgba) {
  AVFrame* rgb = NULL;
  struct SwsContext* sws = NULL;
  uint8_t* buf = NULL;
  int i;
  if (!fr || fr->width < 1 || fr->height < 1) return 0;
  rgb = av_frame_alloc();
  if (!rgb) return 0;
  rgb->format = AV_PIX_FMT_RGBA;
  rgb->width = fr->width;
  rgb->height = fr->height;
  if (av_frame_get_buffer(rgb, 1) < 0) { av_frame_free(&rgb); return 0; }
  sws = sws_getContext(fr->width, fr->height, fr->format, fr->width, fr->height,
                       AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws) { av_frame_free(&rgb); return 0; }
  sws_scale(sws, (const uint8_t* const*)fr->data, fr->linesize, 0, fr->height, rgb->data, rgb->linesize);
  buf = (uint8_t*)malloc((size_t)fr->width * (size_t)fr->height * 4);
  if (!buf) { sws_freeContext(sws); av_frame_free(&rgb); return 0; }
  for (i = 0; i < fr->height; i++)
    memcpy(buf + (size_t)i * (size_t)fr->width * 4,
           rgb->data[0] + (size_t)i * (size_t)rgb->linesize[0],
           (size_t)fr->width * 4);
  *w = fr->width;
  *h = fr->height;
  *rgba = buf;
  sws_freeContext(sws);
  av_frame_free(&rgb);
  return 1;
}

static int take_frame(AVCodecContext* ctx, AVFrame* fr, int* seen, int index, int* w, int* h, uint8_t** rgba) {
  int ret;
  while ((ret = avcodec_receive_frame(ctx, fr)) == 0) {
    if (*seen == index) {
      int ok = convert_frame(fr, w, h, rgba);
      av_frame_unref(fr);
      return ok ? 1 : -1;
    }
    (*seen)++;
    av_frame_unref(fr);
  }
  return 0;
}

int bw_video_frame(const char* path, int index, int* w, int* h, uint8_t** rgba) {
  AVFormatContext* fmt = NULL;
  AVCodecContext* ctx = NULL;
  const AVCodec* codec;
  AVPacket* pkt = NULL;
  AVFrame* fr = NULL;
  int stream = -1, i, seen = 0, got = 0;
  if (w) *w = 0;
  if (h) *h = 0;
  if (rgba) *rgba = NULL;
  if (!path || !w || !h || !rgba || index < 0) return 0;
  if (avformat_open_input(&fmt, path, NULL, NULL) < 0) return 0;
  if (avformat_find_stream_info(fmt, NULL) < 0) goto fail;
  for (i = 0; i < (int)fmt->nb_streams; i++) {
    if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { stream = i; break; }
  }
  if (stream < 0) goto fail;
  codec = avcodec_find_decoder(fmt->streams[stream]->codecpar->codec_id);
  if (!codec) goto fail;
  ctx = avcodec_alloc_context3(codec);
  if (!ctx) goto fail;
  if (avcodec_parameters_to_context(ctx, fmt->streams[stream]->codecpar) < 0) goto fail;
  if (avcodec_open2(ctx, codec, NULL) < 0) goto fail;
  pkt = av_packet_alloc();
  fr = av_frame_alloc();
  if (!pkt || !fr) goto fail;
  while (!got && av_read_frame(fmt, pkt) >= 0) {
    if (pkt->stream_index == stream && avcodec_send_packet(ctx, pkt) == 0) {
      int n = take_frame(ctx, fr, &seen, index, w, h, rgba);
      if (n < 0) { av_packet_unref(pkt); goto fail; }
      if (n > 0) got = 1;
    }
    av_packet_unref(pkt);
  }
  if (!got) {
    avcodec_send_packet(ctx, NULL);
    if (take_frame(ctx, fr, &seen, index, w, h, rgba) > 0) got = 1;
  }
  av_frame_free(&fr);
  av_packet_free(&pkt);
  avcodec_free_context(&ctx);
  avformat_close_input(&fmt);
  return got;
fail:
  av_frame_free(&fr);
  av_packet_free(&pkt);
  if (ctx) avcodec_free_context(&ctx);
  if (fmt) avformat_close_input(&fmt);
  return 0;
}

int bw_video_rgba(const char* path, int* w, int* h, uint8_t** rgba) {
  return bw_video_frame(path, 0, w, h, rgba);
}

int bw_video_ok(const char* path) {
  int w = 0, h = 0;
  uint8_t* px = NULL;
  int ok = bw_video_rgba(path, &w, &h, &px);
  free(px);
  return ok && w > 0 && h > 0;
}

int bw_video_tex(const char* path, int index) {
  int w = 0, h = 0, tex;
  uint8_t* px = NULL;
  if (!bw_video_frame(path, index, &w, &h, &px)) return 0;
  tex = bw_gl_rgba(w, h, px, w * h * 4);
  free(px);
  return tex;
}
