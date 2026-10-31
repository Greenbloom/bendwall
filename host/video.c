/* First video frame to RGBA. Bend has no codec library. */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdlib.h>
#include <string.h>

int bw_video_rgba(const char* path, int* w, int* h, uint8_t** rgba);

int bw_video_ok(const char* path) {
  int w = 0, h = 0;
  uint8_t* px = NULL;
  int ok = bw_video_rgba(path, &w, &h, &px);
  free(px);
  return ok && w > 0 && h > 0;
}

int bw_video_rgba(const char* path, int* w, int* h, uint8_t** rgba) {
  AVFormatContext* fmt = NULL;
  AVCodecContext* ctx = NULL;
  const AVCodec* codec;
  AVPacket* pkt = NULL;
  AVFrame* fr = NULL;
  AVFrame* rgb = NULL;
  struct SwsContext* sws = NULL;
  int stream = -1, ret, i, got = 0;
  uint8_t* buf = NULL;
  if (w) *w = 0;
  if (h) *h = 0;
  if (rgba) *rgba = NULL;
  if (!path || !w || !h || !rgba) return 0;
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
    if (pkt->stream_index != stream) { av_packet_unref(pkt); continue; }
    if (avcodec_send_packet(ctx, pkt) < 0) { av_packet_unref(pkt); break; }
    av_packet_unref(pkt);
    ret = avcodec_receive_frame(ctx, fr);
    if (ret == 0) got = 1;
  }
  if (!got) {
    avcodec_send_packet(ctx, NULL);
    if (avcodec_receive_frame(ctx, fr) == 0) got = 1;
  }
  if (!got || fr->width < 1 || fr->height < 1) goto fail;
  rgb = av_frame_alloc();
  if (!rgb) goto fail;
  rgb->format = AV_PIX_FMT_RGBA;
  rgb->width = fr->width;
  rgb->height = fr->height;
  if (av_frame_get_buffer(rgb, 1) < 0) goto fail;
  sws = sws_getContext(fr->width, fr->height, fr->format, fr->width, fr->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws) goto fail;
  sws_scale(sws, (const uint8_t* const*)fr->data, fr->linesize, 0, fr->height, rgb->data, rgb->linesize);
  buf = (uint8_t*)malloc((size_t)fr->width * (size_t)fr->height * 4);
  if (!buf) goto fail;
  for (i = 0; i < fr->height; i++)
    memcpy(buf + (size_t)i * (size_t)fr->width * 4, rgb->data[0] + (size_t)i * (size_t)rgb->linesize[0], (size_t)fr->width * 4);
  *w = fr->width;
  *h = fr->height;
  *rgba = buf;
  buf = NULL;
  sws_freeContext(sws);
  av_frame_free(&rgb);
  av_frame_free(&fr);
  av_packet_free(&pkt);
  avcodec_free_context(&ctx);
  avformat_close_input(&fmt);
  return 1;
fail:
  free(buf);
  if (sws) sws_freeContext(sws);
  av_frame_free(&rgb);
  av_frame_free(&fr);
  av_packet_free(&pkt);
  if (ctx) avcodec_free_context(&ctx);
  if (fmt) avformat_close_input(&fmt);
  return 0;
}
