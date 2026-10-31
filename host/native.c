/* Image decode for Bend effects. libpng + libjpeg, dlopened as libbendwall.so.
   Bend has no image decoder; the effect only moves bytes. */
#include <png.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct jerr_s {
  struct jpeg_error_mgr pub;
  jmp_buf jb;
};

static void jerr_exit(j_common_ptr c) {
  struct jerr_s* e = (struct jerr_s*)c->err;
  longjmp(e->jb, 1);
}

static uint8_t* wrap(int w, int h, const uint8_t* rgba, int* out_n) {
  size_t n;
  uint8_t* b;
  if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return NULL;
  n = (size_t)w * (size_t)h * 4u;
  b = (uint8_t*)malloc(8u + n);
  if (!b) return NULL;
  b[0] = (uint8_t)(w & 255);
  b[1] = (uint8_t)((w >> 8) & 255);
  b[2] = (uint8_t)((w >> 16) & 255);
  b[3] = (uint8_t)((w >> 24) & 255);
  b[4] = (uint8_t)(h & 255);
  b[5] = (uint8_t)((h >> 8) & 255);
  b[6] = (uint8_t)((h >> 16) & 255);
  b[7] = (uint8_t)((h >> 24) & 255);
  memcpy(b + 8, rgba, n);
  *out_n = (int)(8u + n);
  return b;
}

static uint8_t* dec_png(const uint8_t* data, int n, int* out_n) {
  png_image image;
  uint8_t *px, *o;
  int w, h;
  memset(&image, 0, sizeof image);
  image.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&image, data, (png_size_t)n)) return NULL;
  image.format = PNG_FORMAT_RGBA;
  w = (int)image.width;
  h = (int)image.height;
  if (w <= 0 || h <= 0 || w > 8192 || h > 8192) {
    png_image_free(&image);
    return NULL;
  }
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4u);
  if (!px || !png_image_finish_read(&image, NULL, px, 0, NULL)) {
    free(px);
    png_image_free(&image);
    return NULL;
  }
  o = wrap(w, h, px, out_n);
  free(px);
  png_image_free(&image);
  return o;
}

static uint8_t* dec_jpg(const uint8_t* data, int n, int* out_n) {
  struct jpeg_decompress_struct cinfo;
  struct jerr_s jerr;
  uint8_t *rgb = NULL, *rgba = NULL, *o = NULL;
  int w, h, i, comps;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jerr_exit;
  if (setjmp(jerr.jb)) {
    jpeg_destroy_decompress(&cinfo);
    free(rgb);
    free(rgba);
    return NULL;
  }
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, (unsigned long)n);
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }
  cinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&cinfo);
  w = (int)cinfo.output_width;
  h = (int)cinfo.output_height;
  comps = cinfo.output_components;
  if (comps != 3 || w <= 0 || h <= 0 || w > 8192 || h > 8192) {
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }
  rgb = (uint8_t*)malloc((size_t)w * (size_t)h * 3u);
  rgba = (uint8_t*)malloc((size_t)w * (size_t)h * 4u);
  if (!rgb || !rgba) {
    jpeg_destroy_decompress(&cinfo);
    free(rgb);
    free(rgba);
    return NULL;
  }
  while (cinfo.output_scanline < cinfo.output_height) {
    JSAMPROW row = rgb + (size_t)cinfo.output_scanline * (size_t)w * 3u;
    jpeg_read_scanlines(&cinfo, &row, 1);
  }
  for (i = 0; i < w * h; i++) {
    rgba[i * 4 + 0] = rgb[i * 3 + 0];
    rgba[i * 4 + 1] = rgb[i * 3 + 1];
    rgba[i * 4 + 2] = rgb[i * 3 + 2];
    rgba[i * 4 + 3] = 255;
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  o = wrap(w, h, rgba, out_n);
  free(rgb);
  free(rgba);
  return o;
}

uint8_t* bw_image(const uint8_t* data, int n, int* out_n) {
  *out_n = 0;
  if (!data || n < 4) return NULL;
  if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4e && data[3] == 0x47)
    return dec_png(data, n, out_n);
  if (data[0] == 0xff && data[1] == 0xd8) return dec_jpg(data, n, out_n);
  return NULL;
}

void bw_free(void* p) { free(p); }
