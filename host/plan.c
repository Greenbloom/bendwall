/* Scene picture resolution. Top-level project.json keys, then
   object → model json → material json → texture. An image value may be
   that path or an object carrying it (file / path / model / value) or a
   material reference. .tex is TEXV/TEXI/TEXB: LZ4 block, raw
   rgba/rg/r, DXT, an embedded PNG/JPEG, and TEXS frames. The same entry
   point feeds the player and the probe. */
#include "plan.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <png.h>
#include <setjmp.h>

/* One 8192² RGBA atlas is 256MB. GIF sheets in the wild are that size,
   and the compressed member can be large too. */
#define CAP_FILE (256 * 1024 * 1024)
#define CAP_RAW (8192 * 8192 * 4)
#define MAX_LAYER 64
#define MAX_PART_SYS 256

static uint32_t ru32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t ri32(const uint8_t* p) {
    return (int32_t)ru32(p);
}

static int ends_with(const char* s, const char* ext) {
    size_t n, e;
    if (!s || !ext) return 0;
    n = strlen(s);
    e = strlen(ext);
    if (n < e) return 0;
    return strcasecmp(s + n - e, ext) == 0;
}

static void slash(char* s) {
    for (; s && *s; s++)
        if (*s == '\\') *s = '/';
}

static void lower_copy(const char* s, char* d, int cap) {
    int i = 0;
    if (!s) s = "";
    for (; s[i] && i + 1 < cap; i++) d[i] = (char)tolower((unsigned char)s[i]);
    d[i] = 0;
}

static float pf(const char* s, const char** end) {
    const char* p = s ? s : "";
    int sign = 1;
    double ip = 0, fp = 0, place = 1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+')
        p++;
    while (*p >= '0' && *p <= '9') {
        ip = ip * 10.0 + (*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            place *= 10.0;
            fp = fp * 10.0 + (*p - '0');
            p++;
        }
    }
    if (end) *end = p;
    return (float)(sign < 0 ? -(ip + fp / place) : (ip + fp / place));
}

static uint8_t* read_bin(const char* path, int* n_out) {
    FILE* f;
    long sz;
    uint8_t* b;
    if (n_out) *n_out = 0;
    if (!path || !path[0]) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 1 || sz > CAP_FILE) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    b = (uint8_t*)malloc((size_t)sz + 1);
    if (!b) {
        fclose(f);
        return NULL;
    }
    if (fread(b, 1, (size_t)sz, f) != (size_t)sz) {
        free(b);
        fclose(f);
        return NULL;
    }
    fclose(f);
    b[sz] = 0;
    if (n_out) *n_out = (int)sz;
    return b;
}

static const char* base_of(const char* name) {
    const char* s;
    if (!name) return "";
    s = strrchr(name, '/');
    if (!s) s = strrchr(name, '\\');
    return s ? s + 1 : name;
}

/* Exact member name scores 2. A basename hit under materials/ scores 3,
   any other basename hit scores 1. PKGV0001 through PKGV0009 share this
   table; version >= 25 is rejected. */
static int name_score(const char* name, const char* want, int by_base) {
    char alt[640];
    const char* b;
    int hit;
    if (!name || !want || !name[0] || !want[0]) return 0;
    if (!by_base) return strcasecmp(name, want) == 0 ? 2 : 0;
    b = base_of(name);
    hit = strcasecmp(b, want) == 0;
    if (!hit && !ends_with(want, ".tex")) {
        snprintf(alt, sizeof alt, "%s.tex", want);
        hit = strcasecmp(b, alt) == 0;
    }
    if (!hit && !ends_with(want, ".png")) {
        snprintf(alt, sizeof alt, "%s.png", want);
        hit = strcasecmp(b, alt) == 0;
    }
    if (!hit && !ends_with(want, ".jpg") && !ends_with(want, ".jpeg")) {
        snprintf(alt, sizeof alt, "%s.jpg", want);
        hit = strcasecmp(b, alt) == 0;
        if (!hit) {
            snprintf(alt, sizeof alt, "%s.jpeg", want);
            hit = strcasecmp(b, alt) == 0;
        }
    }
    if (!hit) return 0;
    if (!strncasecmp(name, "materials/", 10)) return 3;
    return 1;
}

static int pkg_find(const char* pkg, const char* want, int by_base, uint8_t** out, int* n_out) {
    FILE* f;
    uint8_t hdr[8];
    uint32_t magic_len, count, i;
    uint32_t found_off = 0, found_sz = 0;
    int best = 0;
    long start;
    uint8_t* buf;
    char magic[16];
    if (!pkg || !want || !out) return 0;
    *out = NULL;
    f = fopen(pkg, "rb");
    if (!f) return 0;
    if (fread(hdr, 1, 4, f) != 4) goto fail;
    magic_len = ru32(hdr);
    if (magic_len < 5 || magic_len > 8) goto fail;
    if (fread(magic, 1, magic_len, f) != magic_len) goto fail;
    magic[magic_len] = 0;
    if (memcmp(magic, "PKGV", 4) != 0) goto fail;
    if (atoi(magic + 4) >= 25) goto fail;
    if (fread(hdr, 1, 4, f) != 4) goto fail;
    count = ru32(hdr);
    if (count > 100000) goto fail;
    for (i = 0; i < count; i++) {
        uint32_t nl, off, sz;
        char* name;
        int score;
        if (fread(hdr, 1, 4, f) != 4) goto fail;
        nl = ru32(hdr);
        if (nl > 1000000) goto fail;
        name = (char*)malloc((size_t)nl + 1);
        if (!name) goto fail;
        if (nl && fread(name, 1, nl, f) != nl) {
            free(name);
            goto fail;
        }
        name[nl] = 0;
        if (fread(hdr, 1, 8, f) != 8) {
            free(name);
            goto fail;
        }
        off = ru32(hdr);
        sz = ru32(hdr + 4);
        score = (nl > 0 && nl <= 0x800) ? name_score(name, want, by_base) : 0;
        if (score > best && sz >= 1 && sz <= CAP_FILE) {
            best = score;
            found_off = off;
            found_sz = sz;
        }
        free(name);
    }
    start = ftell(f);
    if (!best) goto fail;
    if (fseek(f, start + (long)found_off, SEEK_SET) != 0) goto fail;
    buf = (uint8_t*)malloc((size_t)found_sz + 1);
    if (!buf) goto fail;
    if (fread(buf, 1, found_sz, f) != found_sz) {
        free(buf);
        goto fail;
    }
    buf[found_sz] = 0;
    fclose(f);
    *out = buf;
    if (n_out) *n_out = (int)found_sz;
    return 1;
fail:
    fclose(f);
    return 0;
}

static int pkg_member(const char* pkg, const char* want, uint8_t** out, int* n_out) {
    return pkg_find(pkg, want, 0, out, n_out);
}

static int pkg_dir_mode(const char* dir, const char* name, int by_base, uint8_t** out, int* n_out) {
    static const char* known[] = {"scene.pkg", "gifscene.pkg", NULL};
    char path[1100];
    DIR* d;
    struct dirent* ent;
    int k, hits = 0;
    for (k = 0; known[k]; k++) {
        snprintf(path, sizeof path, "%s/%s", dir, known[k]);
        if (pkg_find(path, name, by_base, out, n_out)) return 1;
    }
    d = opendir(dir);
    if (!d) return 0;
    while ((ent = readdir(d)) != NULL && hits < 8) {
        size_t n;
        if (ent->d_name[0] == '.') continue;
        n = strlen(ent->d_name);
        if (n < 5 || strcasecmp(ent->d_name + n - 4, ".pkg") != 0) continue;
        if (!strcasecmp(ent->d_name, "scene.pkg") || !strcasecmp(ent->d_name, "gifscene.pkg")) continue;
        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
        hits++;
        if (pkg_find(path, name, by_base, out, n_out)) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

static int pkg_dir(const char* dir, const char* name, uint8_t** out, int* n_out) {
    return pkg_dir_mode(dir, name, 0, out, n_out);
}

static int read_named(const char* dir, const char* assets, const char* rel, uint8_t** out, int* n_out) {
    char path[1100];
    if (!rel || !rel[0]) return 0;
    if (rel[0] == '/') {
        *out = read_bin(rel, n_out);
        return *out != NULL;
    }
    if (dir && dir[0]) {
        snprintf(path, sizeof path, "%s/%s", dir, rel);
        *out = read_bin(path, n_out);
        if (*out) return 1;
        if (pkg_dir(dir, rel, out, n_out)) return 1;
    }
    if (assets && assets[0]) {
        snprintf(path, sizeof path, "%s/%s", assets, rel);
        *out = read_bin(path, n_out);
        if (*out) return 1;
    }
    return 0;
}

static int read_rel(const char* dir, const char* assets, const char* rel, const char* ext, uint8_t** out, int* n_out) {
    char alt[1100];
    if (read_named(dir, assets, rel, out, n_out)) return 1;
    if (ext && ext[0] && !ends_with(rel, ext)) {
        snprintf(alt, sizeof alt, "%s%s", rel, ext);
        if (read_named(dir, assets, alt, out, n_out)) return 1;
    }
    return 0;
}

struct jerr_s {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
};

static void jerr_exit(j_common_ptr c) {
    struct jerr_s* e = (struct jerr_s*)c->err;
    longjmp(e->jb, 1);
}

static int decode_jpeg_mem(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h) {
    struct jpeg_decompress_struct cinfo;
    struct jerr_s jerr;
    uint8_t* out = NULL;
    int x, comps;
    if (!b || n < 4) return 0;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jerr_exit;
    if (setjmp(jerr.jb)) {
        jpeg_destroy_decompress(&cinfo);
        free(out);
        return 0;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, b, (unsigned long)n);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    *w = (int)cinfo.output_width;
    *h = (int)cinfo.output_height;
    comps = (int)cinfo.output_components;
    if (*w < 1 || *h < 1 || *w > 8192 || *h > 8192 || comps < 3) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }
    out = (uint8_t*)malloc((size_t)(*w) * (size_t)(*h) * 4);
    if (!out) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }
    while ((int)cinfo.output_scanline < *h) {
        uint8_t row[8192 * 3];
        JSAMPROW rows[1];
        int yy = (int)cinfo.output_scanline;
        rows[0] = row;
        jpeg_read_scanlines(&cinfo, rows, 1);
        for (x = 0; x < *w; x++) {
            uint8_t* d = out + ((size_t)yy * (size_t)(*w) + (size_t)x) * 4;
            d[0] = row[x * comps];
            d[1] = row[x * comps + 1];
            d[2] = row[x * comps + 2];
            d[3] = 255;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    *rgba = out;
    return 1;
}

static int decode_png_mem(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h) {
    png_image image;
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, b, (png_size_t)n)) return 0;
    image.format = PNG_FORMAT_RGBA;
    *w = (int)image.width;
    *h = (int)image.height;
    if (*w < 1 || *h < 1 || *w > 8192 || *h > 8192) {
        png_image_free(&image);
        return 0;
    }
    *rgba = (uint8_t*)malloc((size_t)(*w) * (size_t)(*h) * 4);
    if (!*rgba || !png_image_finish_read(&image, NULL, *rgba, 0, NULL)) {
        free(*rgba);
        *rgba = NULL;
        png_image_free(&image);
        return 0;
    }
    png_image_free(&image);
    return 1;
}

static int lz4_block(const uint8_t* src, int src_n, uint8_t* dst, int dst_n) {
    int si = 0, di = 0;
    while (si < src_n) {
        int token, lit, match, b;
        if (si >= src_n) return 0;
        token = src[si++];
        lit = token >> 4;
        if (lit == 15) {
            do {
                if (si >= src_n) return 0;
                b = src[si++];
                lit += b;
            } while (b == 255);
        }
        if (lit < 0 || di + lit > dst_n || si + lit > src_n) return 0;
        if (lit) memcpy(dst + di, src + si, (size_t)lit);
        di += lit;
        si += lit;
        if (si >= src_n) break;
        if (si + 2 > src_n) return 0;
        match = src[si] | (src[si + 1] << 8);
        si += 2;
        if (match == 0 || match > di) return 0;
        b = token & 15;
        if (b == 15) {
            int x;
            do {
                if (si >= src_n) return 0;
                x = src[si++];
                b += x;
            } while (x == 255);
        }
        b += 4;
        if (di + b > dst_n) return 0;
        for (int i = 0; i < b; i++) dst[di + i] = dst[di - match + i];
        di += b;
    }
    return di == dst_n;
}

static void c565(unsigned c, uint8_t o[3]) {
    o[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
    o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
    o[2] = (uint8_t)((c & 31) * 255 / 31);
}

static int dxt_decode(const uint8_t* d, int n, int w, int h, int kind, uint8_t** rgba) {
    int bs = kind == 1 ? 8 : 16;
    int o = 0, by, bx;
    uint8_t* out;
    if (w < 1 || h < 1 || w > 8192 || h > 8192) return 0;
    out = (uint8_t*)calloc((size_t)w * (size_t)h, 4);
    if (!out) return 0;
    for (by = 0; by < h; by += 4) {
        for (bx = 0; bx < w; bx += 4) {
            const uint8_t* blk;
            uint8_t alpha[16];
            uint8_t pal[4][3];
            unsigned c0, c1, idx;
            int i;
            if (o + bs > n) {
                free(out);
                return 0;
            }
            blk = d + o;
            o += bs;
            for (i = 0; i < 16; i++) alpha[i] = 255;
            if (kind == 5) {
                int a0 = blk[0], a1 = blk[1];
                unsigned long long bits = 0;
                int ap[8];
                for (i = 0; i < 6; i++) bits |= (unsigned long long)blk[2 + i] << (8 * i);
                ap[0] = a0;
                ap[1] = a1;
                if (a0 > a1) {
                    for (i = 0; i < 6; i++) ap[2 + i] = ((6 - i) * a0 + (i + 1) * a1) / 7;
                } else {
                    for (i = 0; i < 4; i++) ap[2 + i] = ((4 - i) * a0 + (i + 1) * a1) / 5;
                    ap[6] = 0;
                    ap[7] = 255;
                }
                for (i = 0; i < 16; i++) alpha[i] = (uint8_t)ap[(bits >> (3 * i)) & 7];
            } else if (kind == 3) {
                unsigned long long bits = 0;
                for (i = 0; i < 8; i++) bits |= (unsigned long long)blk[i] << (8 * i);
                for (i = 0; i < 16; i++) alpha[i] = (uint8_t)(((bits >> (4 * i)) & 15) * 17);
            }
            {
                const uint8_t* cb = blk + (bs - 8);
                c0 = (unsigned)cb[0] | ((unsigned)cb[1] << 8);
                c1 = (unsigned)cb[2] | ((unsigned)cb[3] << 8);
                idx = (unsigned)cb[4] | ((unsigned)cb[5] << 8) | ((unsigned)cb[6] << 16) | ((unsigned)cb[7] << 24);
            }
            c565(c0, pal[0]);
            c565(c1, pal[1]);
            if (c0 > c1 || kind != 1) {
                for (i = 0; i < 3; i++) {
                    pal[2][i] = (uint8_t)((2 * pal[0][i] + pal[1][i]) / 3);
                    pal[3][i] = (uint8_t)((pal[0][i] + 2 * pal[1][i]) / 3);
                }
            } else {
                for (i = 0; i < 3; i++) pal[2][i] = (uint8_t)((pal[0][i] + pal[1][i]) / 2);
                pal[3][0] = pal[3][1] = pal[3][2] = 0;
            }
            for (i = 0; i < 16; i++) {
                int y = by + i / 4, x = bx + i % 4;
                uint8_t* px;
                int pi = (idx >> (2 * i)) & 3;
                if (y >= h || x >= w) continue;
                px = out + ((size_t)y * (size_t)w + (size_t)x) * 4;
                px[0] = pal[pi][0];
                px[1] = pal[pi][1];
                px[2] = pal[pi][2];
                px[3] = alpha[i];
                if (kind == 1 && c0 <= c1 && pi == 3) px[3] = 0;
            }
        }
    }
    *rgba = out;
    return 1;
}

static int take_tag(const uint8_t* b, int n, int* i, char* out) {
    if (*i + 9 > n) return 0;
    memcpy(out, b + *i, 8);
    out[8] = 0;
    if (b[*i + 8] != 0) return 0;
    *i += 9;
    return 1;
}

static int need(int i, int n, int k) { return i >= 0 && k >= 0 && i + k <= n; }

static float rf32(const uint8_t* p) {
    float f;
    uint32_t u = ru32(p);
    memcpy(&f, &u, 4);
    return f;
}

static void free_sheets(BwSheet* sheets, int n) {
    int i;
    if (!sheets) return;
    for (i = 0; i < n; i++) free(sheets[i].rgba);
    free(sheets);
}

int bw_frame_index(const BwFrame* frames, int nframes, int time_ms) {
    double total = 0, acc = 0, t;
    int i;
    if (!frames || nframes <= 1) return 0;
    for (i = 0; i < nframes; i++) {
        double d = frames[i].dur;
        if (d < 0.001) d = 0.001;
        total += d;
    }
    if (total <= 0) return 0;
    t = time_ms <= 0 ? 0.0 : (double)time_ms / 1000.0;
    t = fmod(t, total);
    if (t < 0) t += total;
    for (i = 0; i < nframes; i++) {
        double d = frames[i].dur;
        if (d < 0.001) d = 0.001;
        acc += d;
        if (t < acc) return i;
    }
    return nframes - 1;
}

static void sample_px(const BwSheet* sh, int x, int y, uint8_t* d) {
    if (x < 0 || y < 0 || x >= sh->w || y >= sh->h) {
        d[0] = d[1] = d[2] = 0;
        d[3] = 0;
        return;
    }
    memcpy(d, sh->rgba + ((size_t)y * (size_t)sh->w + (size_t)x) * 4, 4);
}

int bw_tex_crop(const BwSheet* sheets, int nsheets, const BwFrame* fr, uint8_t** rgba, int* w, int* h) {
    const BwSheet* sh;
    float x, y, bw, wy, hx, hh;
    int axis, iw, ih, row, col;
    uint8_t* out;
    if (!sheets || !fr || !rgba || fr->sheet < 0 || fr->sheet >= nsheets) return 0;
    sh = &sheets[fr->sheet];
    if (!sh->rgba || sh->w < 1 || sh->h < 1) return 0;
    x = fr->x;
    y = fr->y;
    bw = fr->w;
    wy = fr->wy;
    hx = fr->hx;
    hh = fr->h;
    axis = fabsf(wy) <= 0.51f && fabsf(hx) <= 0.51f;
    if (axis) {
        if (bw < 0) {
            x += bw;
            bw = -bw;
        }
        if (hh < 0) {
            y += hh;
            hh = -hh;
        }
        iw = (int)floorf(bw + 0.5f);
        ih = (int)floorf(hh + 0.5f);
    } else {
        iw = (int)(sqrtf(bw * bw + wy * wy) + 0.5f);
        ih = (int)(sqrtf(hx * hx + hh * hh) + 0.5f);
    }
    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;
    if (iw > 8192) iw = 8192;
    if (ih > 8192) ih = 8192;
    out = (uint8_t*)malloc((size_t)iw * (size_t)ih * 4);
    if (!out) return 0;
    if (axis) {
        int x0 = (int)floorf(x);
        int y0 = (int)floorf(y);
        for (row = 0; row < ih; row++) {
            for (col = 0; col < iw; col++)
                sample_px(sh, x0 + col, y0 + row, out + ((size_t)row * (size_t)iw + (size_t)col) * 4);
        }
    } else {
        for (row = 0; row < ih; row++) {
            for (col = 0; col < iw; col++) {
                float u = x + ((float)col + 0.5f) / (float)iw * bw + ((float)row + 0.5f) / (float)ih * hx;
                float v = y + ((float)col + 0.5f) / (float)iw * wy + ((float)row + 0.5f) / (float)ih * hh;
                sample_px(sh, (int)floorf(u), (int)floorf(v), out + ((size_t)row * (size_t)iw + (size_t)col) * 4);
            }
        }
    }
    *rgba = out;
    *w = iw;
    *h = ih;
    return 1;
}

static uint8_t* pixels_from_mip(int fmt, int fif, const uint8_t* data, uint32_t sz, int mw, int mh, int* tw, int* th) {
    uint8_t* full = NULL;
    if (fif != -1) {
        int ok = decode_png_mem(data, (int)sz, &full, tw, th);
        if (!ok) ok = decode_jpeg_mem(data, (int)sz, &full, tw, th);
        return ok ? full : NULL;
    }
    if (fmt == 0) {
        if (sz < (uint32_t)mw * (uint32_t)mh * 4) return NULL;
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) return NULL;
        memcpy(full, data, (size_t)mw * (size_t)mh * 4);
        *tw = mw;
        *th = mh;
    } else if (fmt == 1) {
        int x, y;
        if (sz < (uint32_t)mw * (uint32_t)mh * 3) return NULL;
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) return NULL;
        for (y = 0; y < mh; y++) {
            for (x = 0; x < mw; x++) {
                const uint8_t* s = data + ((size_t)y * (size_t)mw + (size_t)x) * 3;
                uint8_t* d = full + ((size_t)y * (size_t)mw + (size_t)x) * 4;
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = 255;
            }
        }
        *tw = mw;
        *th = mh;
    } else if (fmt == 8) {
        int x, y;
        if (sz < (uint32_t)mw * (uint32_t)mh * 2) return NULL;
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) return NULL;
        for (y = 0; y < mh; y++) {
            for (x = 0; x < mw; x++) {
                const uint8_t* s = data + ((size_t)y * (size_t)mw + (size_t)x) * 2;
                uint8_t* d = full + ((size_t)y * (size_t)mw + (size_t)x) * 4;
                d[0] = d[1] = d[2] = s[0];
                d[3] = s[1];
            }
        }
        *tw = mw;
        *th = mh;
    } else if (fmt == 9) {
        int x, y;
        if (sz < (uint32_t)mw * (uint32_t)mh) return NULL;
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) return NULL;
        for (y = 0; y < mh; y++) {
            for (x = 0; x < mw; x++) {
                uint8_t* d = full + ((size_t)y * (size_t)mw + (size_t)x) * 4;
                d[0] = d[1] = d[2] = data[(size_t)y * (size_t)mw + (size_t)x];
                d[3] = 255;
            }
        }
        *tw = mw;
        *th = mh;
    } else if (fmt == 7 || fmt == 6 || fmt == 4) {
        int kind = fmt == 7 ? 1 : (fmt == 6 ? 3 : 5);
        if (!dxt_decode(data, (int)sz, mw, mh, kind, &full)) return NULL;
        *tw = mw;
        *th = mh;
    } else {
        return NULL;
    }
    return full;
}

static int crop_to_image(uint8_t** full, int* tw, int* th, int iw, int ih) {
    uint8_t* crop;
    int y;
    if (!(iw > 0 && ih > 0 && (iw < *tw || ih < *th) && iw <= *tw && ih <= *th)) return 1;
    crop = (uint8_t*)malloc((size_t)iw * (size_t)ih * 4);
    if (!crop) return 0;
    for (y = 0; y < ih; y++)
        memcpy(crop + (size_t)y * (size_t)iw * 4, *full + (size_t)y * (size_t)(*tw) * 4, (size_t)iw * 4);
    free(*full);
    *full = crop;
    *tw = iw;
    *th = ih;
    return 1;
}

static int skip_mip_body(const uint8_t* b, int n, int* i, int bv, int vol) {
    uint32_t sz;
    if (!need(*i, n, 8)) return 0;
    *i += 8;
    if (vol) {
        if (!need(*i, n, 4)) return 0;
        *i += 4;
    }
    if (bv >= 2) {
        if (!need(*i, n, 8)) return 0;
        *i += 8;
    }
    if (!need(*i, n, 4)) return 0;
    sz = ru32(b + *i);
    *i += 4;
    if (sz > (uint32_t)CAP_FILE || !need(*i, n, (int)sz)) return 0;
    *i += (int)sz;
    return 1;
}

/* Decode mip 0 and leave *i on the following mip, or past this image. */
static int read_image(const uint8_t* b, int n, int* i, int bv, int vol, int fif, int fmt, int iw, int ih, BwSheet* sheet) {
    uint32_t mip_count, lz4 = 0, raw = 0, sz;
    int mw, mh, tw = 0, th = 0, extra;
    const uint8_t* data;
    uint8_t* rawb = NULL;
    uint8_t* full;
    memset(sheet, 0, sizeof *sheet);
    if (!need(*i, n, 4)) return 0;
    mip_count = ru32(b + *i);
    *i += 4;
    if (mip_count < 1 || mip_count > 16) return 0;
    if (!need(*i, n, 8)) return 0;
    mw = (int)ru32(b + *i);
    mh = (int)ru32(b + *i + 4);
    *i += 8;
    if (vol) {
        if (!need(*i, n, 4)) return 0;
        *i += 4;
    }
    if (bv >= 2) {
        if (!need(*i, n, 8)) return 0;
        lz4 = ru32(b + *i);
        raw = ru32(b + *i + 4);
        *i += 8;
    }
    if (!need(*i, n, 4)) return 0;
    sz = ru32(b + *i);
    *i += 4;
    if (!need(*i, n, (int)sz) || mw < 1 || mh < 1 || mw > 8192 || mh > 8192) return 0;
    data = b + *i;
    *i += (int)sz;
    if (lz4 & 1u) {
        if (raw < 1 || raw > (uint32_t)CAP_RAW) return 0;
        rawb = (uint8_t*)malloc(raw);
        if (!rawb || !lz4_block(data, (int)sz, rawb, (int)raw)) {
            free(rawb);
            return 0;
        }
        data = rawb;
        sz = raw;
    }
    full = pixels_from_mip(fmt, fif, data, sz, mw, mh, &tw, &th);
    free(rawb);
    if (!full) return 0;
    if (fif == -1 && !crop_to_image(&full, &tw, &th, iw, ih)) {
        free(full);
        return 0;
    }
    for (extra = 1; extra < (int)mip_count; extra++) {
        if (!skip_mip_body(b, n, i, bv, vol)) {
            free(full);
            return 0;
        }
    }
    sheet->rgba = full;
    sheet->w = tw;
    sheet->h = th;
    return 1;
}

/* A fully transparent TEXS frame is an empty disposal. Sampling a grid
   keeps a solid frame and drops one that has no ink, so playback does
   not start on a blank that the effect pass turns into black. */
static int frame_has_ink(const BwSheet* sheets, int nsheets, const BwFrame* fr) {
    const BwSheet* sh;
    int x, y, hits = 0, x0, x1, y0, y1, sx, sy;
    if (!sheets || !fr || fr->sheet < 0 || fr->sheet >= nsheets) return 1;
    sh = &sheets[fr->sheet];
    if (!sh->rgba || sh->w < 1 || sh->h < 1) return 1;
    x0 = (int)floorf(fr->x);
    y0 = (int)floorf(fr->y);
    x1 = (int)ceilf(fr->x + fr->w);
    y1 = (int)ceilf(fr->y + fr->h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > sh->w) x1 = sh->w;
    if (y1 > sh->h) y1 = sh->h;
    if (x1 <= x0 || y1 <= y0) return 1;
    sx = (x1 - x0) / 48;
    sy = (y1 - y0) / 48;
    if (sx < 1) sx = 1;
    if (sy < 1) sy = 1;
    for (y = y0; y < y1; y += sy) {
        for (x = x0; x < x1; x += sx) {
            hits++;
            if (sh->rgba[((size_t)y * (size_t)sh->w + (size_t)x) * 4 + 3] > 8) return 1;
        }
    }
    if (!hits) return 1;
    return 0;
}

static int parse_texs(const uint8_t* b, int n, int* i, const BwSheet* sheets, int nsheets, BwFrame** out, int* nout) {
    char tag[12];
    int sv, count, k, kept;
    int sheet_w = 0, sheet_h = 0;
    BwFrame* fr;
    *out = NULL;
    *nout = 0;
    if (*i + 9 > n || memcmp(b + *i, "TEXS", 4) != 0 || b[*i + 8] != 0) return 1;
    if (!take_tag(b, n, i, tag)) return 0;
    sv = atoi(tag + 4);
    if (!need(*i, n, 4)) return 0;
    count = (int)ru32(b + *i);
    *i += 4;
    if (count < 1 || count > 4096) return 0;
    if (sv >= 3) {
        if (!need(*i, n, 8)) return 0;
        sheet_w = (int)ru32(b + *i);
        sheet_h = (int)ru32(b + *i + 4);
        *i += 8;
    }
    fr = (BwFrame*)calloc((size_t)count, sizeof(BwFrame));
    if (!fr) return 0;
    kept = 0;
    for (k = 0; k < count; k++) {
        BwFrame one;
        float sx, sy;
        int img;
        memset(&one, 0, sizeof one);
        if (!need(*i, n, 8 + 24)) {
            free(fr);
            return 0;
        }
        img = (int)ru32(b + *i);
        one.dur = rf32(b + *i + 4);
        *i += 8;
        if (sv == 1) {
            one.x = (float)ru32(b + *i);
            one.y = (float)ru32(b + *i + 4);
            one.w = (float)ru32(b + *i + 8);
            one.wy = (float)ru32(b + *i + 12);
            one.hx = (float)ru32(b + *i + 16);
            one.h = (float)ru32(b + *i + 20);
        } else {
            one.x = rf32(b + *i);
            one.y = rf32(b + *i + 4);
            one.w = rf32(b + *i + 8);
            one.wy = rf32(b + *i + 12);
            one.hx = rf32(b + *i + 16);
            one.h = rf32(b + *i + 20);
        }
        *i += 24;
        if (img < 0 || img >= nsheets || !sheets[img].rgba) continue;
        one.sheet = img;
        sx = 1.f;
        sy = 1.f;
        if (sv >= 3 && sheet_w > 0 && sheet_h > 0) {
            sx = (float)sheets[img].w / (float)sheet_w;
            sy = (float)sheets[img].h / (float)sheet_h;
        }
        one.x *= sx;
        one.y *= sy;
        one.w *= sx;
        one.wy *= sy;
        one.hx *= sx;
        one.h *= sy;
        fr[kept++] = one;
    }
    if (kept > 1) {
        int wri = 0, fi;
        for (fi = 0; fi < kept; fi++) {
            if (!frame_has_ink(sheets, nsheets, &fr[fi])) continue;
            if (wri != fi) fr[wri] = fr[fi];
            wri++;
        }
        if (wri > 0) kept = wri;
    }
    if (kept < 1) {
        free(fr);
        return 1;
    }
    *out = fr;
    *nout = kept;
    return 1;
}

static int tex_load(const uint8_t* b, int n, int time_ms, int keep, uint8_t** rgba, int* w, int* h, BwSheet** sheets_out, int* nsheets_out, BwFrame** frames_out, int* nframes_out) {
    char texv[12], texi[12], texb[12];
    int i = 0, ver_i, bv, vol, fif = -1, fmt, flags, iw, ih, img;
    uint32_t image_count;
    BwSheet* sheets = NULL;
    BwFrame* frames = NULL;
    int nframes = 0;
    int fi;
    if (rgba) *rgba = NULL;
    if (sheets_out) *sheets_out = NULL;
    if (nsheets_out) *nsheets_out = 0;
    if (frames_out) *frames_out = NULL;
    if (nframes_out) *nframes_out = 0;
    if (!b || n < 32 || !rgba) return 0;
    if (!take_tag(b, n, &i, texv) || strncmp(texv, "TEXV", 4) != 0) return 0;
    if (!take_tag(b, n, &i, texi) || strncmp(texi, "TEXI", 4) != 0) return 0;
    ver_i = atoi(texi + 4);
    if (!need(i, n, 24)) return 0;
    fmt = (int)ru32(b + i);
    flags = (int)ru32(b + i + 4);
    i += 16;
    iw = (int)ru32(b + i);
    ih = (int)ru32(b + i + 4);
    i += 8;
    vol = (flags & 0x40) != 0;
    if (vol) {
        if (!need(i, n, 4)) return 0;
        i += 4;
    }
    if (ver_i > 0) {
        if (!need(i, n, 4)) return 0;
        i += 4;
    }
    if (!take_tag(b, n, &i, texb) || strncmp(texb, "TEXB", 4) != 0) return 0;
    bv = atoi(texb + 4);
    if (!need(i, n, 4)) return 0;
    image_count = ru32(b + i);
    i += 4;
    if (image_count < 1 || image_count > 32) return 0;
    if (bv >= 3) {
        if (!need(i, n, 4)) return 0;
        fif = (int)ri32(b + i);
        i += 4;
    }
    if (bv >= 4) {
        uint32_t cond;
        if (!need(i, n, 4)) return 0;
        cond = ru32(b + i);
        i += 4;
        if (cond != 0) return 0;
    }
    sheets = (BwSheet*)calloc(image_count, sizeof(BwSheet));
    if (!sheets) return 0;
    for (img = 0; img < (int)image_count; img++) {
        if (!read_image(b, n, &i, bv, vol, fif, fmt, iw, ih, &sheets[img])) {
            free_sheets(sheets, (int)image_count);
            return 0;
        }
    }
    if (!parse_texs(b, n, &i, sheets, (int)image_count, &frames, &nframes)) {
        free(frames);
        free_sheets(sheets, (int)image_count);
        return 0;
    }
    if (nframes >= 1) {
        fi = bw_frame_index(frames, nframes, time_ms);
        if (!bw_tex_crop(sheets, (int)image_count, &frames[fi], rgba, w, h)) {
            free(frames);
            free_sheets(sheets, (int)image_count);
            return 0;
        }
        if (keep && nframes >= 2 && sheets_out && frames_out) {
            *sheets_out = sheets;
            if (nsheets_out) *nsheets_out = (int)image_count;
            *frames_out = frames;
            if (nframes_out) *nframes_out = nframes;
            return 1;
        }
        free(frames);
        free_sheets(sheets, (int)image_count);
        return 1;
    }
    *rgba = sheets[0].rgba;
    *w = sheets[0].w;
    *h = sheets[0].h;
    sheets[0].rgba = NULL;
    free_sheets(sheets, (int)image_count);
    return *rgba != NULL;
}

int bw_tex_decode(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h) {
    return tex_load(b, n, 0, 0, rgba, w, h, NULL, NULL, NULL, NULL);
}

int bw_tex_decode_time(const uint8_t* b, int n, int time_ms, uint8_t** rgba, int* w, int* h) {
    return tex_load(b, n, time_ms, 0, rgba, w, h, NULL, NULL, NULL, NULL);
}

typedef struct {
    const char* p;
    int n;
    int kind;
} Jv;

static const char* skip_ws(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static const char* skip_str(const char* p, const char* end) {
    if (p >= end || *p != '"') return NULL;
    p++;
    while (p < end) {
        if (*p == '\\') {
            p += (p + 1 < end) ? 2 : 1;
            continue;
        }
        if (*p == '"') return p + 1;
        p++;
    }
    return NULL;
}

static const char* skip_val(const char* p, const char* end) {
    p = skip_ws(p, end);
    if (p >= end) return NULL;
    if (*p == '"') return skip_str(p, end);
    if (*p == '{' || *p == '[') {
        char open = *p;
        char close = open == '{' ? '}' : ']';
        int d = 1;
        p++;
        while (p < end && d) {
            if (*p == '"') {
                p = skip_str(p, end);
                if (!p) return NULL;
                continue;
            }
            if (*p == open) {
                d++;
                p++;
                continue;
            }
            if (*p == close) {
                d--;
                p++;
                continue;
            }
            p++;
        }
        return d == 0 ? p : NULL;
    }
    if (*p == 't' && p + 4 <= end && !memcmp(p, "true", 4)) return p + 4;
    if (*p == 'f' && p + 5 <= end && !memcmp(p, "false", 5)) return p + 5;
    if (*p == 'n' && p + 4 <= end && !memcmp(p, "null", 4)) return p + 4;
    if (*p == '-' || *p == '+' || *p == '.' || (*p >= '0' && *p <= '9')) {
        if (*p == '-' || *p == '+') p++;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-'))
            p++;
        return p;
    }
    return NULL;
}

static int j_val(const char* p, const char* end, Jv* o) {
    const char* a;
    const char* b;
    p = skip_ws(p, end);
    if (p >= end) return 0;
    a = p;
    b = skip_val(p, end);
    if (!b || b < a) return 0;
    o->p = a;
    o->n = (int)(b - a);
    if (*a == '"') o->kind = 1;
    else if (*a == '{') o->kind = 3;
    else if (*a == '[') o->kind = 4;
    else if (*a == 't') o->kind = 5;
    else if (*a == 'f') o->kind = 6;
    else if (*a == 'n') o->kind = 7;
    else o->kind = 2;
    return 1;
}

static int j_doc(const char* s, Jv* o) {
    const char* p = s;
    if (!s) return 0;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) p += 3;
    return j_val(p, p + strlen(p), o) && o->kind == 3;
}

static int j_str(const Jv* v, char* out, int cap) {
    const char* p;
    const char* end;
    int i = 0;
    if (!v || v->kind != 1 || v->n < 2 || cap < 2) return 0;
    p = v->p + 1;
    end = v->p + v->n - 1;
    while (p < end && i + 1 < cap) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            if (*p == 'n') out[i++] = '\n';
            else if (*p == 'r') out[i++] = '\r';
            else if (*p == 't') out[i++] = '\t';
            else if (*p == 'u') {
                int k;
                for (k = 0; k < 4 && p + 1 < end; k++) p++;
                out[i++] = '?';
            } else
                out[i++] = *p;
            p++;
        } else
            out[i++] = *p++;
    }
    out[i] = 0;
    return 1;
}

static int j_obj_get(const Jv* obj, const char* key, Jv* out) {
    const char* p;
    const char* end;
    if (!obj || obj->kind != 3 || obj->n < 2 || !key) return 0;
    p = obj->p + 1;
    end = obj->p + obj->n;
    while (p < end) {
        Jv k, v;
        char name[256];
        p = skip_ws(p, end);
        if (p < end && *p == '}') return 0;
        if (!j_val(p, end, &k) || k.kind != 1) return 0;
        if (!j_str(&k, name, sizeof name)) return 0;
        p = skip_ws(k.p + k.n, end);
        if (p >= end || *p != ':') return 0;
        p++;
        if (!j_val(p, end, &v)) return 0;
        if (strcmp(name, key) == 0) {
            *out = v;
            return 1;
        }
        p = v.p + v.n;
        p = skip_ws(p, end);
        if (p < end && *p == ',') p++;
    }
    return 0;
}

static void j_obj_each(const Jv* obj, void (*fn)(const char*, const Jv*, void*), void* ud) {
    const char* p;
    const char* end;
    if (!obj || obj->kind != 3 || obj->n < 2 || !fn) return;
    p = obj->p + 1;
    end = obj->p + obj->n;
    while (p < end) {
        Jv k, v;
        char name[256];
        p = skip_ws(p, end);
        if (p < end && *p == '}') return;
        if (!j_val(p, end, &k) || k.kind != 1) return;
        if (!j_str(&k, name, sizeof name)) return;
        p = skip_ws(k.p + k.n, end);
        if (p >= end || *p != ':') return;
        p++;
        if (!j_val(p, end, &v)) return;
        fn(name, &v, ud);
        p = v.p + v.n;
        p = skip_ws(p, end);
        if (p < end && *p == ',') p++;
    }
}

static int j_root(const char* json, const char* key, Jv* out) {
    Jv root;
    if (!j_doc(json, &root)) return 0;
    return j_obj_get(&root, key, out);
}

static Jv j_unwrap(const Jv* v) {
    Jv inner;
    if (v && v->kind == 3 && j_obj_get(v, "value", &inner)) return inner;
    return v ? *v : (Jv){0};
}

static int j_lit(const Jv* v, char* out, int cap) {
    Jv u = j_unwrap(v);
    if (u.kind == 1) return j_str(&u, out, cap);
    if ((u.kind == 2 || u.kind == 5 || u.kind == 6 || u.kind == 7) && u.n > 0 && cap > 1) {
        int n = u.n < cap - 1 ? u.n : cap - 1;
        memcpy(out, u.p, (size_t)n);
        out[n] = 0;
        return 1;
    }
    return 0;
}

static int j_field(const Jv* obj, const char* key, char* out, int cap) {
    Jv v;
    if (!j_obj_get(obj, key, &v)) return 0;
    return j_lit(&v, out, cap);
}

static int j_arr_at(const Jv* a, int index, Jv* out) {
    const char* p;
    const char* end;
    int n = 0;
    if (!a || a->kind != 4) return 0;
    p = a->p + 1;
    end = a->p + a->n;
    while (p < end) {
        Jv v;
        p = skip_ws(p, end);
        if (p < end && *p == ']') return 0;
        if (!j_val(p, end, &v)) return 0;
        if (n == index) {
            *out = v;
            return 1;
        }
        n++;
        p = v.p + v.n;
        p = skip_ws(p, end);
        if (p < end && *p == ',') p++;
    }
    return 0;
}

static int j_arr_count(const Jv* a) {
    int n = 0;
    Jv v;
    while (j_arr_at(a, n, &v)) n++;
    return n;
}

static float j_num(const Jv* obj, const char* key, float fallback, int* present) {
    Jv v;
    char b[64];
    if (present) *present = 0;
    if (!obj || !j_obj_get(obj, key, &v)) return fallback;
    v = j_unwrap(&v);
    if (v.kind == 5) {
        if (present) *present = 1;
        return 1.f;
    }
    if (v.kind == 6) {
        if (present) *present = 1;
        return 0.f;
    }
    if (!j_lit(&v, b, sizeof b)) return fallback;
    if (present) *present = 1;
    return pf(b, NULL);
}

static int j_vec(const Jv* obj, const char* key, float* x, float* y, float* z) {
    char b[128];
    const char* p;
    if (!j_field(obj, key, b, sizeof b)) return 0;
    p = b;
    *x = pf(p, &p);
    *y = pf(p, &p);
    *z = pf(p, &p);
    return 1;
}

static int raster_ext(const char* s) {
    return ends_with(s, ".png") || ends_with(s, ".jpg") || ends_with(s, ".jpeg") || ends_with(s, ".tex");
}

/* A material slot often names the source image (.jpg / .png). The compiled
   file drops that extension and is materials/<path>.tex. */
static int image_leaf(const char* s) {
    return ends_with(s, ".png") || ends_with(s, ".jpg") || ends_with(s, ".jpeg")
        || ends_with(s, ".tga") || ends_with(s, ".gif") || ends_with(s, ".webp")
        || ends_with(s, ".bmp");
}

static uint8_t* checker_px(int* w, int* h) {
    enum { N = 32 };
    uint8_t* px = (uint8_t*)malloc(N * N * 4);
    int x, y;
    if (!px) return NULL;
    for (y = 0; y < N; y++) {
        for (x = 0; x < N; x++) {
            int on = ((x / 8) ^ (y / 8)) & 1;
            uint8_t* p = px + (y * N + x) * 4;
            p[0] = (uint8_t)(on ? 20 : 220);
            p[1] = (uint8_t)(on ? 180 : 40);
            p[2] = 36;
            p[3] = 255;
        }
    }
    *w = N;
    *h = N;
    return px;
}

/* Particles never use the checker. A missing name, or a name that does
   not resolve, is a white radial falloff so the sprite stays soft. */
static uint8_t* soft_px(int* w, int* h) {
    enum { N = 32 };
    uint8_t* px = (uint8_t*)malloc(N * N * 4);
    int x, y;
    float cx, cy, rad;
    if (!px) return NULL;
    cx = (N - 1) * 0.5f;
    cy = cx;
    rad = cx;
    if (rad < 1.f) rad = 1.f;
    for (y = 0; y < N; y++) {
        for (x = 0; x < N; x++) {
            float dx = ((float)x - cx) / rad;
            float dy = ((float)y - cy) / rad;
            float d = sqrtf(dx * dx + dy * dy);
            float a = 1.f - d;
            uint8_t* p = px + ((size_t)y * N + (size_t)x) * 4;
            if (a < 0.f) a = 0.f;
            a = a * a;
            p[0] = 255;
            p[1] = 255;
            p[2] = 255;
            p[3] = (uint8_t)(a * 255.f + 0.5f);
        }
    }
    *w = N;
    *h = N;
    return px;
}

static void miss_log(const char* what, int decode_fail) {
    fprintf(stderr, "bendwall: texture %s %s\n",
            decode_fail ? "decode failed" : "not found",
            what && what[0] ? what : "(unnamed)");
}

static int push_layer(BwPlan* plan, BwLayer layer) {
    BwLayer* n;
    if (plan->nlayers >= MAX_LAYER) {
        free(layer.rgba);
        return 0;
    }
    n = (BwLayer*)realloc(plan->layers, (size_t)(plan->nlayers + 1) * sizeof(BwLayer));
    if (!n) {
        free(layer.rgba);
        return 0;
    }
    plan->layers = n;
    plan->layers[plan->nlayers++] = layer;
    return 1;
}

static void push_missing(BwPlan* plan, const char* what, int decode_fail, float cx, float cy, float lw, float lh, float deg, float alpha, int full) {
    BwLayer L;
    miss_log(what, decode_fail);
    plan->missing++;
    memset(&L, 0, sizeof L);
    L.rgba = checker_px(&L.w, &L.h);
    L.cx = cx;
    L.cy = cy;
    L.lw = lw;
    L.lh = lh;
    L.deg = deg;
    L.alpha = alpha > 0 ? alpha : 1.f;
    L.full = full || lw < 1 || lh < 1;
    L.missing = 1;
    push_layer(plan, L);
}

typedef struct PxGot {
    uint8_t* rgba;
    int w, h;
    BwSheet* sheets;
    int nsheets;
    BwFrame* frames;
    int nframes;
} PxGot;

static void clear_got(PxGot* g) {
    if (!g) return;
    free(g->rgba);
    free_sheets(g->sheets, g->nsheets);
    free(g->frames);
    memset(g, 0, sizeof *g);
}

static int decode_bytes(const uint8_t* b, int n, PxGot* g) {
    memset(g, 0, sizeof *g);
    if (!b || n < 4) return 0;
    if (n >= 8 && !memcmp(b, "TEXV", 4))
        return tex_load(b, n, 0, 1, &g->rgba, &g->w, &g->h, &g->sheets, &g->nsheets, &g->frames, &g->nframes);
    if (b[0] == 0x89 && b[1] == 'P') return decode_png_mem(b, n, &g->rgba, &g->w, &g->h);
    if (b[0] == 0xFF && b[1] == 0xD8) return decode_jpeg_mem(b, n, &g->rgba, &g->w, &g->h);
    return 0;
}

static void note_effect(BwPlan* plan, const char* name) {
    if (!strcmp(name, "scroll")) plan->scroll = 1;
    else if (!strcmp(name, "spin")) plan->spin = 1;
    else if (!strcmp(name, "opacity")) plan->opacity = 1;
    else if (!strcmp(name, "colorkey")) plan->colorkey = 1;
    else if (!strcmp(name, "tint")) plan->tint = 1;
    else if (!strcmp(name, "pulse")) plan->pulse = 1;
}

static void scan_effects(BwPlan* plan, const char* json) {
    const char* p = json;
    if (!json) return;
    while ((p = strstr(p, "effects/")) != NULL) {
        char name[64];
        int i = 0;
        p += 8;
        while (*p && *p != '/' && *p != '"' && *p != ' ' && i < 63) name[i++] = (char)tolower((unsigned char)*p++);
        name[i] = 0;
        note_effect(plan, name);
    }
}

static int visible_obj(const Jv* obj) {
    Jv v;
    char b[16];
    if (!j_obj_get(obj, "visible", &v)) return 1;
    v = j_unwrap(&v);
    if (v.kind == 6) return 0;
    if (j_lit(&v, b, sizeof b) && (strcmp(b, "0") == 0 || strcmp(b, "false") == 0)) return 0;
    return 1;
}

static void place_of(const Jv* obj, const BwPlan* plan, float mw, float mh, int model_full, float* cx, float* cy, float* lw, float* lh, float* deg, float* alpha, int* full) {
    float sx = 1, sy = 1, ox = 0, oy = 0, az = 0, zw = 0, zh = 0;
    int have_o = 0, have_s = 0, present = 0;
    float scx, scy, scz;
    *alpha = j_num(obj, "alpha", 1.f, &present);
    if (!present) *alpha = 1.f;
    if (*alpha < 0) *alpha = 0;
    if (*alpha > 1) *alpha = 1;
    if (j_vec(obj, "scale", &scx, &scy, &scz)) {
        sx = scx;
        sy = scy;
    }
    have_o = j_vec(obj, "origin", &ox, &oy, &scz);
    have_s = j_vec(obj, "size", &zw, &zh, &scz);
    if (j_vec(obj, "angles", &scx, &scy, &az)) *deg = az;
    else *deg = 0;
    *full = 0;
    if (model_full && plan->scene_w > 1.f && plan->scene_h > 1.f) {
        *lw = plan->scene_w;
        *lh = plan->scene_h;
        *cx = *lw * 0.5f;
        *cy = *lh * 0.5f;
        return;
    }
    if (have_s && zw > 0 && zh > 0) {
        *lw = zw * sx;
        *lh = zh * sy;
    } else if (mw > 0 && mh > 0) {
        *lw = mw * sx;
        *lh = mh * sy;
    } else {
        *full = 1;
        *lw = *lh = *cx = *cy = 0;
        return;
    }
    if (have_o) {
        *cx = ox;
        *cy = oy;
    } else if (plan->scene_w > 1.f) {
        *cx = plan->scene_w * 0.5f;
        *cy = plan->scene_h * 0.5f;
    } else {
        *full = 1;
    }
}

static char* load_json_rel(const char* dir, const char* assets, const char* rel) {
    uint8_t* b = NULL;
    int n = 0;
    char alt[1100];
    if (read_rel(dir, assets, rel, ".json", &b, &n)) return (char*)b;
    lower_copy(rel, alt, sizeof alt);
    if (strcmp(alt, rel) != 0 && read_rel(dir, assets, alt, ".json", &b, &n)) return (char*)b;
    return NULL;
}

/* 1 decoded, 0 absent, -1 bytes were found and are not a picture.
   why receives the path that failed to decode, or the name that was absent. */
static void copy_cap(char* dst, int cap, const char* src) {
    int i = 0;
    if (!dst || cap < 1) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static int stem_of(const char* rel, char* out, int cap) {
    const char* base;
    const char* dot;
    int n;
    if (!rel || !image_leaf(rel) || !out || cap < 2) return 0;
    copy_cap(out, cap, rel);
    base = base_of(out);
    dot = strrchr(base, '.');
    if (!dot || dot == base) return 0;
    n = (int)(dot - out);
    if (n < 1) return 0;
    out[n] = 0;
    return 1;
}

static int push_cand(char cand[][1100], int* nc, int maxc, const char* s) {
    int i;
    if (!s || !s[0] || !nc || *nc >= maxc) return 0;
    for (i = 0; i < *nc; i++)
        if (strcmp(cand[i], s) == 0) return 0;
    snprintf(cand[*nc], 1100, "%s", s);
    (*nc)++;
    return 1;
}

static int try_rel(const char* dir, const char* assets, const char* rel, PxGot* g) {
    uint8_t* b = NULL;
    int n = 0, ok;
    const char* ext = raster_ext(rel) ? "" : ".tex";
    if (!read_rel(dir, assets, rel, ext, &b, &n)) return 0;
    ok = decode_bytes(b, n, g);
    free(b);
    if (!ok) {
        clear_got(g);
        return -1;
    }
    return 1;
}

static int load_pixels(const char* dir, const char* assets, const char* rel, PxGot* g, char* why, int why_cap) {
    char cand[8][1100];
    char lower[1100];
    char stem[1000];
    char labeled[1100];
    const char* base;
    const char* hunt;
    int nc = 0, i, st, bad = 0;
    uint8_t* raw = NULL;
    int n = 0;
    if (g) memset(g, 0, sizeof *g);
    if (why && why_cap > 0) why[0] = 0;
    if (!rel || !rel[0] || !g) return 0;
    push_cand(cand, &nc, 8, rel);
    lower_copy(rel, lower, sizeof lower);
    if (strcmp(lower, rel) != 0) push_cand(cand, &nc, 8, lower);
    base = base_of(rel);
    /* A material slot named "ztyz" is materials/ztyz.tex in the package,
       then the same path under BENDWALL_ASSETS. A slot named
       particle/halo_4.jpg is materials/particle/halo_4.tex. */
    if (base == rel || base[0] == 0)
        snprintf(labeled, sizeof labeled, "materials/%s", rel);
    else if (strncasecmp(rel, "materials/", 10) != 0)
        snprintf(labeled, sizeof labeled, "materials/%s", base);
    else
        labeled[0] = 0;
    if (labeled[0]) push_cand(cand, &nc, 8, labeled);
    if (stem_of(rel, stem, sizeof stem)) {
        snprintf(labeled, sizeof labeled, "materials/%s", stem);
        push_cand(cand, &nc, 8, labeled);
        if (strchr(stem, '/')) push_cand(cand, &nc, 8, stem);
    }
    for (i = 0; i < nc; i++) {
        st = try_rel(dir, assets, cand[i], g);
        if (st > 0) {
            if (why && why_cap > 0) why[0] = 0;
            return 1;
        }
        if (st < 0) {
            bad = 1;
            if (why && why_cap > 0) copy_cap(why, why_cap, cand[i]);
        }
    }
    hunt = base[0] ? base : rel;
    if (stem_of(hunt, stem, sizeof stem)) hunt = base_of(stem);
    else if (image_leaf(hunt)) {
        if (stem_of(rel, stem, sizeof stem)) hunt = base_of(stem);
    }
    if (!bad && dir && dir[0] && pkg_dir_mode(dir, hunt, 1, &raw, &n)) {
        if (decode_bytes(raw, n, g)) {
            free(raw);
            if (why && why_cap > 0) why[0] = 0;
            return 1;
        }
        clear_got(g);
        free(raw);
        bad = 1;
        if (why && why_cap > 0) {
            snprintf(labeled, sizeof labeled, "materials/%s.tex", hunt);
            copy_cap(why, why_cap, labeled);
        }
    }
    if (why && why_cap > 0 && !why[0]) {
        if (stem_of(rel, stem, sizeof stem))
            snprintf(why, (size_t)why_cap, "materials/%s.tex", stem);
        else if (!strchr(rel, '/') && !raster_ext(rel))
            snprintf(why, (size_t)why_cap, "materials/%s%s", rel, ends_with(rel, ".tex") ? "" : ".tex");
        else
            copy_cap(why, why_cap, rel);
    }
    return bad ? -1 : 0;
}

static void add_pixels(BwPlan* plan, const Jv* obj, const char* what, int decode_fail, uint8_t* px, int w, int h, float mw, float mh, int model_full) {
    BwLayer L;
    float cx, cy, lw, lh, deg, alpha;
    int full;
    if (!px) {
        place_of(obj, plan, mw, mh, model_full, &cx, &cy, &lw, &lh, &deg, &alpha, &full);
        push_missing(plan, what, decode_fail, cx, cy, lw, lh, deg, alpha, full);
        return;
    }
    place_of(obj, plan, mw > 0 ? mw : (float)w, mh > 0 ? mh : (float)h, model_full, &cx, &cy, &lw, &lh, &deg, &alpha, &full);
    memset(&L, 0, sizeof L);
    L.rgba = px;
    L.w = w;
    L.h = h;
    L.cx = cx;
    L.cy = cy;
    L.lw = lw > 0 ? lw : (float)w;
    L.lh = lh > 0 ? lh : (float)h;
    L.deg = deg;
    L.alpha = alpha;
    L.full = full;
    plan->image_ok++;
    push_layer(plan, L);
}

static int texture_name(const Jv* one, char* out, int cap) {
    static const char* keys[] = {"name", "file", "path", "value", "image", "src", "texture", NULL};
    Jv inner;
    int i;
    if (!one || !out || cap < 2) return 0;
    out[0] = 0;
    if (one->kind == 1) {
        if (!j_str(one, out, cap)) return 0;
    } else if (one->kind == 3) {
        for (i = 0; keys[i]; i++) {
            if (!j_obj_get(one, keys[i], &inner)) continue;
            inner = j_unwrap(&inner);
            if (inner.kind == 1 && j_str(&inner, out, cap) && out[0]) break;
        }
        if (!out[0]) return 0;
    } else
        return 0;
    slash(out);
    if (!out[0] || strncmp(out, "_rt_", 4) == 0 || strncmp(out, "_alias_", 7) == 0) return 0;
    return 1;
}

static int tex_from_value(const Jv* v, char* out, int cap) {
    Jv one;
    int i, n;
    if (!v) return 0;
    if (v->kind == 1 || v->kind == 3) return texture_name(v, out, cap);
    if (v->kind != 4) return 0;
    n = j_arr_count(v);
    for (i = 0; i < n; i++) {
        if (!j_arr_at(v, i, &one)) break;
        if (texture_name(&one, out, cap)) return 1;
    }
    return 0;
}

static int first_texture(const Jv* mat, char* out, int cap) {
    Jv passes, pass, texs;
    if (!mat || mat->kind != 3) return 0;
    if (j_obj_get(mat, "passes", &passes) && passes.kind == 4 && j_arr_at(&passes, 0, &pass) && pass.kind == 3) {
        if (j_obj_get(&pass, "textures", &texs) && tex_from_value(&texs, out, cap)) return 1;
        if (j_obj_get(&pass, "usertextures", &texs) && tex_from_value(&texs, out, cap)) return 1;
    }
    if (j_obj_get(mat, "usertextures", &texs) && tex_from_value(&texs, out, cap)) return 1;
    if (j_obj_get(mat, "textures", &texs) && tex_from_value(&texs, out, cap)) return 1;
    return 0;
}

/* Scene-object texture override (PKGV0018 instance.textures / usertextures). */
static int instance_texture(const Jv* obj, char* out, int cap) {
    Jv inst, texs;
    if (!obj || obj->kind != 3) return 0;
    if (j_obj_get(obj, "instance", &inst) && inst.kind == 3) {
        if (j_obj_get(&inst, "textures", &texs) && tex_from_value(&texs, out, cap)) return 1;
        if (j_obj_get(&inst, "usertextures", &texs) && tex_from_value(&texs, out, cap)) return 1;
    }
    if (j_obj_get(obj, "usertextures", &texs) && tex_from_value(&texs, out, cap)) return 1;
    return 0;
}

static char* load_json_prefixed(const char* dir, const char* assets, const char* rel, const char* prefix) {
    char alt[1100];
    if (!rel || !rel[0] || strchr(rel, '/') || strchr(rel, '\\')) return NULL;
    snprintf(alt, sizeof alt, "%s%s", prefix, rel);
    return load_json_rel(dir, assets, alt);
}

static void paint_got(BwPlan* plan, const Jv* obj, const char* what, int decode_fail, PxGot* g, float mw, float mh, int model_full, int force_full) {
    uint8_t* px = g ? g->rgba : NULL;
    int w = g ? g->w : 0;
    int h = g ? g->h : 0;
    if (g) g->rgba = NULL;
    add_pixels(plan, obj, what, decode_fail, px, w, h, mw, mh, model_full);
    if (g && plan->nlayers > 0 && !plan->layers[plan->nlayers - 1].missing && g->nframes >= 2 && g->sheets) {
        BwLayer* L = &plan->layers[plan->nlayers - 1];
        L->sheets = g->sheets;
        L->nsheets = g->nsheets;
        L->frames = g->frames;
        L->nframes = g->nframes;
        g->sheets = NULL;
        g->frames = NULL;
        g->nsheets = 0;
        g->nframes = 0;
    }
    if (g) clear_got(g);
    if (force_full && plan->nlayers > 0) plan->layers[plan->nlayers - 1].full = 1;
}

/* A path buried in a bound object: value, file, path, model, src, or image. */
static int pull_path(const Jv* v, char* out, int cap, int depth) {
    static const char* keys[] = {"value", "file", "path", "model", "src", "image", NULL};
    Jv inner;
    int i;
    if (!v || depth > 6 || cap < 2) return 0;
    if (v->kind == 1) return j_str(v, out, cap) && out[0];
    if (v->kind != 3) return 0;
    for (i = 0; keys[i]; i++) {
        if (!j_obj_get(v, keys[i], &inner)) continue;
        if (pull_path(&inner, out, cap, depth + 1)) return 1;
    }
    return 0;
}

static int pull_material(const Jv* v, char* out, int cap, int depth) {
    Jv inner, mat;
    if (!v || depth > 6 || cap < 2) return 0;
    if (v->kind != 3) return 0;
    if (j_obj_get(v, "material", &mat)) {
        mat = j_unwrap(&mat);
        if (mat.kind == 1 && j_str(&mat, out, cap) && out[0]) return 1;
    }
    if (j_obj_get(v, "value", &inner) && pull_material(&inner, out, cap, depth + 1)) return 1;
    return 0;
}

static void model_tried(const char* path, char* out, int cap) {
    if (!path || !path[0]) {
        snprintf(out, (size_t)cap, "(no path)");
        return;
    }
    if (strchr(path, '/') || ends_with(path, ".json"))
        snprintf(out, (size_t)cap, "%s", path);
    else
        snprintf(out, (size_t)cap, "models/%s.json", path);
}

static void material_tried(const char* path, char* out, int cap) {
    if (!path || !path[0]) {
        snprintf(out, (size_t)cap, "(no path)");
        return;
    }
    if (strchr(path, '/') || ends_with(path, ".json"))
        snprintf(out, (size_t)cap, "%s", path);
    else
        snprintf(out, (size_t)cap, "materials/%s.json", path);
}

static int paint_material_doc(BwPlan* plan, const Jv* obj, const char* dir, const char* assets, const Jv* material, float mw, float mh, int model_full, const char* err_name) {
    char tex[512], why[1100];
    PxGot g;
    int st;
    memset(&g, 0, sizeof g);
    if (!first_texture(material, tex, sizeof tex)) {
        paint_got(plan, obj, err_name && err_name[0] ? err_name : "(no texture)", 0, &g, mw, mh, model_full, 0);
        return 1;
    }
    why[0] = 0;
    st = load_pixels(dir, assets, tex, &g, why, (int)sizeof why);
    paint_got(plan, obj, why[0] ? why : tex, st < 0, &g, mw, mh, model_full, 0);
    return 1;
}

/* 1 when this document was a model or a material and the layer was recorded. */
static int apply_doc(BwPlan* plan, const Jv* obj, const char* dir, const char* assets, const Jv* doc, const char* err_name) {
    char matp[512], tried[1100];
    char* mat;
    Jv material;
    int present = 0, model_full;
    float mw, mh;
    PxGot g;
    if (j_field(doc, "material", matp, sizeof matp) && matp[0]) {
        model_full = j_num(doc, "fullscreen", 0, &present) > 0.5f;
        mw = j_num(doc, "width", 0, &present);
        mh = j_num(doc, "height", 0, &present);
        slash(matp);
        mat = load_json_rel(dir, assets, matp);
        if (!mat) mat = load_json_prefixed(dir, assets, matp, "materials/");
        if (!mat || !j_doc(mat, &material)) {
            free(mat);
            material_tried(matp, tried, sizeof tried);
            memset(&g, 0, sizeof g);
            paint_got(plan, obj, tried, 0, &g, mw, mh, model_full, 0);
            return 1;
        }
        paint_material_doc(plan, obj, dir, assets, &material, mw, mh, model_full, matp);
        free(mat);
        return 1;
    }
    if (first_texture(doc, matp, sizeof matp)) {
        paint_material_doc(plan, obj, dir, assets, doc, 0, 0, 0, err_name);
        return 1;
    }
    return 0;
}

static char* load_model_or_material(const char* dir, const char* assets, const char* path) {
    char* blob = load_json_rel(dir, assets, path);
    if (blob) return blob;
    blob = load_json_prefixed(dir, assets, path, "models/");
    if (blob) return blob;
    return load_json_prefixed(dir, assets, path, "materials/");
}

#define MAX_PROPS 48
typedef struct PropTab {
    char name[MAX_PROPS][64];
    char value[MAX_PROPS][512];
    int n;
} PropTab;

static void prop_put(PropTab* t, const char* name, const Jv* val) {
    char b[512];
    Jv u;
    int i;
    if (!t || !name || !name[0] || !val) return;
    u = j_unwrap(val);
    b[0] = 0;
    if (u.kind == 1 || u.kind == 2 || u.kind == 5 || u.kind == 6) {
        if (!j_lit(&u, b, sizeof b)) return;
    } else if (u.kind == 3) {
        if (!pull_path(&u, b, sizeof b, 0)) return;
    } else
        return;
    if (!b[0]) return;
    for (i = 0; i < t->n; i++) {
        if (!strcasecmp(t->name[i], name)) {
            copy_cap(t->value[i], (int)sizeof t->value[i], b);
            return;
        }
    }
    if (t->n >= MAX_PROPS) return;
    copy_cap(t->name[t->n], (int)sizeof t->name[t->n], name);
    copy_cap(t->value[t->n], (int)sizeof t->value[t->n], b);
    t->n++;
}

static void prop_each(const char* name, const Jv* val, void* ud) {
    prop_put((PropTab*)ud, name, val);
}

static void collect_props(PropTab* t, const Jv* root) {
    Jv gen, props, item, val;
    int i, c;
    if (!t || !root || !j_obj_get(root, "general", &gen) || gen.kind != 3) return;
    if (!j_obj_get(&gen, "properties", &props)) return;
    if (props.kind == 4) {
        c = j_arr_count(&props);
        for (i = 0; i < c; i++) {
            char name[64];
            if (!j_arr_at(&props, i, &item) || item.kind != 3) continue;
            if (!j_field(&item, "name", name, sizeof name)) continue;
            if (!j_obj_get(&item, "value", &val)) continue;
            prop_put(t, name, &val);
        }
        return;
    }
    if (props.kind == 3) j_obj_each(&props, prop_each, t);
}

static int prop_find(const PropTab* t, const char* name, char* out, int cap) {
    int i;
    if (!t || !name || !name[0] || !out || cap < 2) return 0;
    for (i = 0; i < t->n; i++) {
        if (!strcasecmp(t->name[i], name)) {
            copy_cap(out, cap, t->value[i]);
            return out[0] != 0;
        }
    }
    return 0;
}

/* prefer: a non-empty project property wins, otherwise the inline value. */
static int resolve_user(const PropTab* props, const Jv* v, char* out, int cap) {
    char name[64], prop[512], inl[512];
    Jv u, inner;
    if (!v || v->kind != 3 || !out || cap < 2) return 0;
    if (!j_obj_get(v, "user", &u)) return 0;
    name[0] = 0;
    u = j_unwrap(&u);
    if (u.kind == 1) j_str(&u, name, sizeof name);
    else if (u.kind == 3) j_field(&u, "name", name, sizeof name);
    if (!name[0]) return 0;
    prop[0] = inl[0] = 0;
    prop_find(props, name, prop, sizeof prop);
    if (j_obj_get(v, "value", &inner)) {
        inner = j_unwrap(&inner);
        if (inner.kind == 1) j_str(&inner, inl, sizeof inl);
        else if (inner.kind == 3) pull_path(&inner, inl, sizeof inl, 0);
    }
    if (prop[0]) {
        copy_cap(out, cap, prop);
        return 1;
    }
    if (inl[0]) {
        copy_cap(out, cap, inl);
        return 1;
    }
    return 0;
}

/* null, false, "", or an object with no picture source. Particle objects
   often carry "image": null; that is not a layer and must not be a checker. */
static int image_skipped(const Jv* im) {
    char b[512];
    Jv v;
    if (!im) return 1;
    if (im->kind == 7 || im->kind == 6) return 1;
    if (im->kind == 1) return !j_str(im, b, sizeof b) || !b[0];
    if (im->kind != 3) return 1;
    if (pull_path(im, b, sizeof b, 0)) return 0;
    if (j_obj_get(im, "user", &v)) return 0;
    if (j_obj_get(im, "passes", &v)) return 0;
    if (j_obj_get(im, "textures", &v)) return 0;
    if (j_obj_get(im, "material", &v)) {
        v = j_unwrap(&v);
        if (v.kind == 3) return 0;
        if (v.kind == 1 && j_str(&v, b, sizeof b) && b[0]) return 0;
    }
    return 1;
}

static void no_path_label(const Jv* obj, char* out, int cap) {
    char name[64];
    name[0] = 0;
    if (obj) j_field(obj, "name", name, sizeof name);
    if (name[0])
        snprintf(out, (size_t)cap, "(no path) name=%s", name);
    else
        snprintf(out, (size_t)cap, "(no path)");
}

static int paint_path(BwPlan* plan, const Jv* obj, const char* dir, const char* assets, const char* path_in) {
    char path[512], tried[1100], why[1100];
    char* blob;
    Jv doc;
    PxGot g;
    int st;
    copy_cap(path, sizeof path, path_in);
    slash(path);
    if (!path[0]) return 0;
    if (raster_ext(path) || image_leaf(path)) {
        memset(&g, 0, sizeof g);
        why[0] = 0;
        st = load_pixels(dir, assets, path, &g, why, (int)sizeof why);
        /* direct stills cover the framebuffer, which is what the tint fixture draws */
        paint_got(plan, obj, why[0] ? why : path, st < 0, &g, 0, 0, 0, 1);
        return 1;
    }
    blob = load_model_or_material(dir, assets, path);
    if (blob && j_doc(blob, &doc) && apply_doc(plan, obj, dir, assets, &doc, path)) {
        free(blob);
        return 1;
    }
    free(blob);
    model_tried(path, tried, sizeof tried);
    memset(&g, 0, sizeof g);
    paint_got(plan, obj, tried, 0, &g, 0, 0, 0, 1);
    return 1;
}

static void take_image(BwPlan* plan, const char* dir, const char* assets, const Jv* obj, const PropTab* props) {
    Jv im, doc, mat, inner;
    char path[512], matp[512], tried[1100], why[1100], tex[512], over[512], label[160];
    char* blob = NULL;
    PxGot g;
    int st;
    if (!j_obj_get(obj, "image", &im)) return;
    if (image_skipped(&im)) return;
    plan->image_tried++;
    over[0] = 0;
    if (instance_texture(obj, over, sizeof over) && over[0]) {
        slash(over);
        memset(&g, 0, sizeof g);
        why[0] = 0;
        st = load_pixels(dir, assets, over, &g, why, (int)sizeof why);
        if (st > 0 && g.rgba) {
            paint_got(plan, obj, why[0] ? why : over, 0, &g, 0, 0, 0, 0);
            return;
        }
        clear_got(&g);
    }
    if (resolve_user(props, &im, path, sizeof path) && path[0]) {
        paint_path(plan, obj, dir, assets, path);
        return;
    }
    if (pull_path(&im, path, sizeof path, 0)) {
        paint_path(plan, obj, dir, assets, path);
        return;
    }
    if (im.kind == 3 && apply_doc(plan, obj, dir, assets, &im, "")) return;
    if (pull_material(&im, matp, sizeof matp, 0)) {
        slash(matp);
        blob = load_model_or_material(dir, assets, matp);
        if (blob && j_doc(blob, &doc) && apply_doc(plan, obj, dir, assets, &doc, matp)) {
            free(blob);
            return;
        }
        free(blob);
        material_tried(matp, tried, sizeof tried);
        memset(&g, 0, sizeof g);
        paint_got(plan, obj, tried, 0, &g, 0, 0, 0, 1);
        return;
    }
    if (im.kind == 3 && j_obj_get(&im, "material", &mat)) {
        mat = j_unwrap(&mat);
        if (mat.kind == 3 && apply_doc(plan, obj, dir, assets, &mat, "")) return;
    }
    if (im.kind == 3 && j_obj_get(&im, "value", &inner)) {
        inner = j_unwrap(&inner);
        if (inner.kind == 3 && apply_doc(plan, obj, dir, assets, &inner, "")) return;
    }
    if (im.kind == 3 && first_texture(&im, tex, sizeof tex)) {
        memset(&g, 0, sizeof g);
        why[0] = 0;
        st = load_pixels(dir, assets, tex, &g, why, (int)sizeof why);
        paint_got(plan, obj, why[0] ? why : tex, st < 0, &g, 0, 0, 0, 0);
        return;
    }
    no_path_label(obj, label, sizeof label);
    memset(&g, 0, sizeof g);
    paint_got(plan, obj, label, 0, &g, 0, 0, 0, 1);
}

static int block_named(const Jv* root, const char* section, const char* name, Jv* out) {
    Jv arr, item;
    int i, n;
    char got[64];
    if (!root || !j_obj_get(root, section, &arr) || arr.kind != 4) return 0;
    n = j_arr_count(&arr);
    for (i = 0; i < n; i++) {
        char* p;
        if (!j_arr_at(&arr, i, &item) || item.kind != 3) continue;
        if (!j_field(&item, "name", got, sizeof got)) continue;
        for (p = got; *p; ++p) *p = (char)tolower((unsigned char)*p);
        if (strcmp(got, name) == 0) {
            *out = item;
            return 1;
        }
    }
    return 0;
}

static int first_emitter(const Jv* doc, Jv* out) {
    Jv arr, item;
    if (block_named(doc, "emitter", "sphererandom", out)) return 1;
    if (block_named(doc, "emitter", "boxrandom", out)) return 1;
    if (block_named(doc, "emitter", "layerimage", out)) return 1;
    if (!j_obj_get(doc, "emitter", &arr) || arr.kind != 4) return 0;
    if (!j_arr_at(&arr, 0, &item) || item.kind != 3) return 0;
    *out = item;
    return 1;
}

static float over_mul(const Jv* obj, const char* key) {
    Jv over;
    int present = 0;
    float v;
    if (!obj || !j_obj_get(obj, "instanceoverride", &over) || over.kind != 3) return 1.f;
    v = j_num(&over, key, 1.f, &present);
    if (!present) return 1.f;
    if (v < 0.f) return 0.f;
    return v;
}

static int over_off(const Jv* obj) {
    Jv over, v;
    char b[16];
    if (!obj || !j_obj_get(obj, "instanceoverride", &over) || over.kind != 3) return 0;
    if (!j_obj_get(&over, "enabled", &v)) return 0;
    v = j_unwrap(&v);
    if (v.kind == 6) return 1;
    if (j_lit(&v, b, sizeof b) && (strcmp(b, "0") == 0 || strcmp(b, "false") == 0)) return 1;
    return 0;
}

static int over_vec(const Jv* obj, const char* key, float* x, float* y, float* z) {
    Jv over;
    if (!obj || !j_obj_get(obj, "instanceoverride", &over) || over.kind != 3) return 0;
    return j_vec(&over, key, x, y, z);
}

static void color_fix(float* r, float* g, float* b) {
    float m = *r;
    if (*g > m) m = *g;
    if (*b > m) m = *b;
    if (m > 1.f) {
        *r /= 255.f;
        *g /= 255.f;
        *b /= 255.f;
    }
    if (*r < 0.f) *r = 0.f;
    if (*g < 0.f) *g = 0.f;
    if (*b < 0.f) *b = 0.f;
    if (*r > 1.f) *r = 1.f;
    if (*g > 1.f) *g = 1.f;
    if (*b > 1.f) *b = 1.f;
}

static void rot2(float deg, float x, float y, float* ox, float* oy) {
    float rad = deg * 0.01745329251f;
    float c = cosf(rad);
    float s = sinf(rad);
    *ox = x * c - y * s;
    *oy = x * s + y * c;
}

static int pass_additive(const Jv* material) {
    Jv passes, pass;
    char b[32];
    char* p;
    if (!material || !j_obj_get(material, "passes", &passes) || passes.kind != 4) return 0;
    if (!j_arr_at(&passes, 0, &pass) || pass.kind != 3) return 0;
    if (!j_field(&pass, "blending", b, sizeof b)) return 0;
    for (p = b; *p; ++p) *p = (char)tolower((unsigned char)*p);
    return strcmp(b, "additive") == 0 || strcmp(b, "add") == 0;
}

static float num_key(const Jv* obj, const char* a, const char* b, float fallback, int* present) {
    int pa = 0, pb = 0;
    float v;
    v = j_num(obj, a, fallback, &pa);
    if (pa) {
        if (present) *present = 1;
        return v;
    }
    if (!b) {
        if (present) *present = 0;
        return fallback;
    }
    v = j_num(obj, b, fallback, &pb);
    if (present) *present = pb;
    return v;
}

static int vec_key(const Jv* obj, const char* a, const char* b, float* x, float* y, float* z) {
    if (j_vec(obj, a, x, y, z)) return 1;
    if (b && j_vec(obj, b, x, y, z)) return 1;
    return 0;
}

/* 1 decoded, 0 absent, -1 found but not a picture. Also tries .png and .jpg
   when the material omits the extension and the .tex slot is empty. */
static int load_picture(const char* dir, const char* assets, const char* rel, PxGot* g, char* why, int why_cap) {
    char alt[1100];
    int st;
    st = load_pixels(dir, assets, rel, g, why, why_cap);
    if (st > 0 || !rel || raster_ext(rel)) return st;
    snprintf(alt, sizeof alt, "%s.png", rel);
    st = load_pixels(dir, assets, alt, g, why, why_cap);
    if (st > 0) return st;
    snprintf(alt, sizeof alt, "%s.jpg", rel);
    return load_pixels(dir, assets, alt, g, why, why_cap);
}

static void release_part(BwPartSys* sys) {
    if (!sys) return;
    free(sys->rgba);
    free_sheets(sys->sheets, sys->nsheets);
    free(sys->frames);
    sys->rgba = NULL;
    sys->sheets = NULL;
    sys->frames = NULL;
    sys->nsheets = 0;
    sys->nframes = 0;
}

static int push_part(BwPlan* plan, BwPartSys sys) {
    BwPartSys* n;
    if (plan->nparts >= MAX_PART_SYS) {
        release_part(&sys);
        fprintf(stderr, "bendwall: particle cap\n");
        return -1;
    }
    n = (BwPartSys*)realloc(plan->parts, (size_t)(plan->nparts + 1) * sizeof(BwPartSys));
    if (!n) {
        release_part(&sys);
        return -1;
    }
    plan->parts = n;
    plan->parts[plan->nparts++] = sys;
    return plan->nparts - 1;
}

static void lower_buf(char* s) {
    for (; s && *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static int j_name(const Jv* obj, char* out, int cap) {
    if (!out || cap < 2) return 0;
    out[0] = 0;
    if (!j_field(obj, "name", out, cap) && !j_field(obj, "type", out, cap)) return 0;
    lower_buf(out);
    return out[0] != 0;
}

/* A box extent is a vec3. A bare number fills x, y, and z. A one-token
   string does too, so it is not (n, 0, 0). */
static int j_box(const Jv* obj, const char* key, float* x, float* y, float* z) {
    Jv v;
    char b[128];
    const char* p;
    const char* e1;
    const char* e2;
    const char* e3;
    float yv, zv, n;
    if (!obj || !j_obj_get(obj, key, &v)) return 0;
    v = j_unwrap(&v);
    if (v.kind == 7) return 0;
    if (!j_lit(&v, b, sizeof b)) return 0;
    if (!strcmp(b, "null")) return 0;
    if (v.kind == 2 || v.kind == 5 || v.kind == 6) {
        n = pf(b, NULL);
        *x = *y = *z = n;
        return 1;
    }
    p = b;
    *x = pf(p, &e1);
    if (e1 == p) return 0;
    p = e1;
    yv = pf(p, &e2);
    if (e2 == p) {
        *y = *z = *x;
        return 1;
    }
    *y = yv;
    p = e2;
    zv = pf(p, &e3);
    if (e3 == p) {
        *z = 0.f;
        return 1;
    }
    *z = zv;
    return 1;
}

static int comp_z(const Jv* obj, const char* a, const char* b, float* z) {
    float x, y;
    int present = 0;
    if (vec_key(obj, a, b, &x, &y, z)) return 1;
    *z = num_key(obj, a, b, 0.f, &present);
    return present;
}

static int block_last(const Jv* root, const char* section, const char* name, Jv* out) {
    Jv arr, item;
    int i, n, found = 0;
    char got[64];
    if (!root || !j_obj_get(root, section, &arr) || arr.kind != 4) return 0;
    n = j_arr_count(&arr);
    for (i = 0; i < n; i++) {
        if (!j_arr_at(&arr, i, &item) || item.kind != 3) continue;
        if (!j_name(&item, got, sizeof got)) continue;
        if (strcmp(got, name) == 0) {
            *out = item;
            found = 1;
        }
    }
    return found;
}

static void part_gap(const char* section, const char* name) {
    static char seen[48][72];
    static int nseen;
    char key[72];
    int i;
    if (!section || !name || !name[0]) return;
    snprintf(key, sizeof key, "%s %s", section, name);
    for (i = 0; i < nseen; i++)
        if (!strcmp(seen[i], key)) return;
    if (nseen < 48) snprintf(seen[nseen++], 72, "%s", key);
    fprintf(stderr, "bendwall: unsupported particle %s %s\n", section, name);
}

static int known_part(const char* section, const char* name) {
    if (!section || !name) return 0;
    if (!strcmp(section, "emitter"))
        return !strcmp(name, "sphererandom") || !strcmp(name, "boxrandom");
    if (!strcmp(section, "initializer"))
        return !strcmp(name, "lifetimerandom") || !strcmp(name, "sizerandom") || !strcmp(name, "alpharandom")
            || !strcmp(name, "colorrandom") || !strcmp(name, "rotationrandom") || !strcmp(name, "velocityrandom")
            || !strcmp(name, "angularvelocityrandom") || !strcmp(name, "turbulentvelocityrandom");
    if (!strcmp(section, "operator"))
        return !strcmp(name, "movement") || !strcmp(name, "alphafade") || !strcmp(name, "sizechange")
            || !strcmp(name, "colorchange") || !strcmp(name, "angularmovement") || !strcmp(name, "controlpointattract")
            || !strcmp(name, "turbulence") || !strcmp(name, "oscillatealpha");
    if (!strcmp(section, "renderer"))
        return !strcmp(name, "sprite") || !strcmp(name, "ropetrail") || !strcmp(name, "spritetrail");
    if (!strcmp(section, "child")) return !strcmp(name, "eventfollow");
    return 0;
}

static void audit_part(const Jv* doc) {
    static const char* secs[] = {"emitter", "initializer", "operator", "renderer", NULL};
    int s, i, n;
    for (s = 0; secs[s]; s++) {
        Jv arr, item;
        char name[64];
        if (!j_obj_get(doc, secs[s], &arr) || arr.kind != 4) continue;
        n = j_arr_count(&arr);
        for (i = 0; i < n; i++) {
            if (!j_arr_at(&arr, i, &item) || item.kind != 3) continue;
            if (!j_name(&item, name, sizeof name)) continue;
            if (!known_part(secs[s], name)) part_gap(secs[s], name);
        }
    }
    {
        Jv arr, item;
        char name[64];
        if (j_obj_get(doc, "children", &arr) && arr.kind == 4) {
            n = j_arr_count(&arr);
            for (i = 0; i < n; i++) {
                if (!j_arr_at(&arr, i, &item) || item.kind != 3) continue;
                name[0] = 0;
                if (!j_field(&item, "type", name, sizeof name)) j_name(&item, name, sizeof name);
                lower_buf(name);
                /* emberglow is a follow only when it names a particle file.
                   With no file there is no motion to copy, so it stays unsupported. */
                if (!strcmp(name, "emberglow")) {
                    char path[512];
                    path[0] = 0;
                    if (j_field(&item, "particle", path, sizeof path) && path[0] && strcmp(path, "None")
                        && (strchr(path, '/') || strstr(path, ".json")))
                        continue;
                    path[0] = 0;
                    if (j_field(&item, "name", path, sizeof path) && path[0]
                        && (strchr(path, '/') || strstr(path, ".json")))
                        continue;
                }
                if (name[0] && !known_part("child", name)) part_gap("child", name);
            }
        }
    }
}

static int emitter_pick(const Jv* doc, Jv* out, char* name, int cap) {
    Jv arr, item;
    if (block_named(doc, "emitter", "sphererandom", out)) {
        snprintf(name, (size_t)cap, "sphererandom");
        return 1;
    }
    if (block_named(doc, "emitter", "boxrandom", out)) {
        snprintf(name, (size_t)cap, "boxrandom");
        return 1;
    }
    if (!j_obj_get(doc, "emitter", &arr) || arr.kind != 4) return 0;
    if (!j_arr_at(&arr, 0, &item) || item.kind != 3) return 0;
    *out = item;
    if (!j_name(&item, name, cap)) snprintf(name, (size_t)cap, "emitter");
    return 1;
}

static int take_particle(BwPlan* plan, const char* dir, const char* assets, const Jv* obj, const PropTab* props, const char* force_path, int parent, int as_child, int depth) {
    Jv field, doc, em, block, mat;
    BwPartSys sys;
    char path[512], matp[512], tex[512], why[1100], alt[1100], ename[64];
    char* blob = NULL;
    char* matb = NULL;
    PxGot g;
    float ox, oy, oz, ex, ey, ez, deg, ax, ay, az;
    float dx, dy, dz, len, sign, bright, mul;
    float cpx[8], cpy[8];
    int cpmouse[8];
    int i, present, st, id, nc, idx, supported;
    if (depth > 3) return -1;
    if (!as_child && over_off(obj)) return -1;
    path[0] = 0;
    if (force_path && force_path[0]) {
        snprintf(path, sizeof path, "%s", force_path);
    } else {
        if (!j_obj_get(obj, "particle", &field)) return -1;
        if (!pull_path(&field, path, sizeof path, 0) || !path[0]) {
            if (!resolve_user(props, &field, path, sizeof path) || !path[0]) {
                fprintf(stderr, "bendwall: particle not found (no path)\n");
                return -1;
            }
        }
    }
    slash(path);
    blob = load_json_rel(dir, assets, path);
    if (!blob && !strchr(path, '/')) blob = load_json_prefixed(dir, assets, path, "particles/");
    if (!blob && !strchr(path, '/')) {
        snprintf(alt, sizeof alt, "particles/presets/%s", path);
        blob = load_json_rel(dir, assets, alt);
    }
    if (!blob || !j_doc(blob, &doc)) {
        fprintf(stderr, "bendwall: particle not found %s\n", path);
        free(blob);
        return -1;
    }
    memset(&sys, 0, sizeof sys);
    sys.size_b = -1.f;
    sys.a0 = sys.a1 = 1.f;
    sys.r0 = sys.g0 = sys.b0 = 1.f;
    sys.r1 = sys.g1 = sys.b1 = 1.f;
    sys.er = sys.eg = sys.eb = 1.f;
    sys.life0 = sys.life1 = 2.f;
    sys.size0 = sys.size1 = 16.f;
    sys.maxcount = 32;
    sys.size_exp = 1.f;
    sys.seqmul = 1.f;
    sys.sx = sys.sy = 1.f;
    sys.parent = -1;
    sys.attract_thr = 512.f;
    audit_part(&doc);
    if (!emitter_pick(&doc, &em, ename, (int)sizeof ename)) {
        fprintf(stderr, "bendwall: particle not found %s\n", path);
        free(blob);
        return -1;
    }
    supported = !strcmp(ename, "sphererandom") || !strcmp(ename, "boxrandom");
    sys.box = !strcmp(ename, "boxrandom");
    if (!supported) {
        part_gap("emitter", ename);
        sys.rate = 0.f;
        sys.instant = 0;
    } else {
        sys.rate = j_num(&em, "rate", 5.f, &present);
        if (!present) sys.rate = 5.f;
        /* instantaneous is how many particles appear at birth, not a flag. */
        sys.instant = (int)(j_num(&em, "instantaneous", 0.f, &present) + 0.5f);
        if (sys.instant < 0) sys.instant = 0;
    }
    sys.speed0 = num_key(&em, "speedmin", "minspeed", 0.f, &present);
    sys.speed1 = num_key(&em, "speedmax", "maxspeed", sys.speed0, &present);
    if (!present) sys.speed1 = sys.speed0;
    sign = j_num(&em, "sign", 1.f, &present);
    if (sys.box) {
        /* Absent distancemax is (256, 256, 0), not the sphere's scalar 0. */
        if (!j_box(&em, "distancemin", &sys.bminx, &sys.bminy, &sys.bminz)
            && !j_box(&em, "mindistance", &sys.bminx, &sys.bminy, &sys.bminz))
            sys.bminx = sys.bminy = sys.bminz = 0.f;
        if (!j_box(&em, "distancemax", &sys.bmaxx, &sys.bmaxy, &sys.bmaxz)
            && !j_box(&em, "maxdistance", &sys.bmaxx, &sys.bmaxy, &sys.bmaxz)) {
            /* Absent max is 256 on x and y. z stays 0, the same as directions. */
            sys.bmaxx = sys.bmaxy = 256.f;
            sys.bmaxz = 0.f;
        }
        if (!j_vec(&em, "directions", &dx, &dy, &dz)) {
            dx = 1.f;
            dy = 1.f;
            dz = 0.f;
        }
        if (sign < 0.f) {
            dx = -dx;
            dy = -dy;
            dz = -dz;
        }
        sys.dvx = dx;
        sys.dvy = dy;
        sys.dvz = dz;
    } else {
        sys.dist0 = num_key(&em, "distancemin", "mindistance", 0.f, &present);
        sys.dist1 = num_key(&em, "distancemax", "maxdistance", sys.dist0, &present);
        if (!present) sys.dist1 = sys.dist0;
        dx = dy = dz = 0.f;
        if (!j_vec(&em, "directions", &dx, &dy, &dz)) dx = dy = 0.f;
        len = sqrtf(dx * dx + dy * dy);
        if (len > 0.0001f) {
            dx /= len;
            dy /= len;
        } else {
            dx = 0.f;
            dy = 0.f;
        }
        if (sign < 0.f) {
            dx = -dx;
            dy = -dy;
        }
    }
    ex = ey = ez = 0.f;
    j_vec(&em, "origin", &ex, &ey, &ez);
    if (block_named(&doc, "initializer", "lifetimerandom", &block)) {
        sys.life0 = num_key(&block, "min", "minimum", sys.life0, &present);
        sys.life1 = num_key(&block, "max", "maximum", sys.life0, &present);
        if (!present) sys.life1 = sys.life0;
    }
    if (block_named(&doc, "initializer", "sizerandom", &block)) {
        sys.size0 = num_key(&block, "min", "minimum", sys.size0, &present);
        sys.size1 = num_key(&block, "max", "maximum", sys.size0, &present);
        if (!present) sys.size1 = sys.size0;
        /* The quad width is half the authored range. Overrides still multiply after this. */
        sys.size0 *= 0.5f;
        sys.size1 *= 0.5f;
        sys.size_exp = j_num(&block, "exponent", 1.f, &present);
        if (!present || sys.size_exp == 0.f) sys.size_exp = 1.f;
    }
    if (block_named(&doc, "initializer", "alpharandom", &block)) {
        sys.a0 = num_key(&block, "min", "minimum", 1.f, &present);
        sys.a1 = num_key(&block, "max", "maximum", sys.a0, &present);
        if (!present) sys.a1 = sys.a0;
    }
    if (block_named(&doc, "initializer", "colorrandom", &block)) {
        if (!vec_key(&block, "min", "minimum", &sys.r0, &sys.g0, &sys.b0)) {
            sys.r0 = sys.g0 = sys.b0 = 1.f;
        }
        if (!vec_key(&block, "max", "maximum", &sys.r1, &sys.g1, &sys.b1)) {
            sys.r1 = sys.r0;
            sys.g1 = sys.g0;
            sys.b1 = sys.b0;
        }
        color_fix(&sys.r0, &sys.g0, &sys.b0);
        color_fix(&sys.r1, &sys.g1, &sys.b1);
    }
    if (block_named(&doc, "initializer", "velocityrandom", &block)) {
        sys.has_vel = 1;
        if (!vec_key(&block, "min", "minimum", &sys.vminx, &sys.vminy, &sys.vminz)) {
            sys.vminx = -32.f;
            sys.vminy = -32.f;
            sys.vminz = 0.f;
        }
        if (!vec_key(&block, "max", "maximum", &sys.vmaxx, &sys.vmaxy, &sys.vmaxz)) {
            sys.vmaxx = 32.f;
            sys.vmaxy = 32.f;
            sys.vmaxz = 0.f;
        }
    }
    if (block_named(&doc, "initializer", "turbulentvelocityrandom", &block)) {
        sys.has_turb = 1;
        sys.turb_scale = j_num(&block, "scale", 1.f, &present);
        if (!present) sys.turb_scale = 1.f;
        sys.turb_time = j_num(&block, "timescale", 1.f, &present);
        if (!present) sys.turb_time = 1.f;
        sys.turb_off = j_num(&block, "offset", 0.f, &present);
        sys.turb_s0 = num_key(&block, "speedmin", "minspeed", 100.f, &present);
        if (!present) sys.turb_s0 = 100.f;
        sys.turb_s1 = num_key(&block, "speedmax", "maxspeed", 250.f, &present);
        if (!present) sys.turb_s1 = sys.turb_s0 > 250.f ? sys.turb_s0 : 250.f;
        sys.turb_p0 = num_key(&block, "phasemin", "minphase", 0.f, &present);
        sys.turb_p1 = num_key(&block, "phasemax", "maxphase", 0.1f, &present);
        if (!present) sys.turb_p1 = 0.1f;
        if (!j_vec(&block, "forward", &sys.turb_fx, &sys.turb_fy, &sys.turb_fz)) {
            sys.turb_fx = 0.f;
            sys.turb_fy = 1.f;
            sys.turb_fz = 0.f;
        }
        if (!j_vec(&block, "right", &sys.turb_rx, &sys.turb_ry, &sys.turb_rz)) {
            sys.turb_rx = 0.f;
            sys.turb_ry = 0.f;
            sys.turb_rz = 1.f;
        }
    }
    if (block_named(&doc, "initializer", "angularvelocityrandom", &block)) {
        sys.has_angv = 1;
        if (!comp_z(&block, "min", "minimum", &sys.av0)) sys.av0 = -5.f;
        if (!comp_z(&block, "max", "maximum", &sys.av1)) sys.av1 = 5.f;
    }
    /* Two rotationrandom blocks: the last one sets the angle. Radians. */
    if (block_last(&doc, "initializer", "rotationrandom", &block)) {
        if (!comp_z(&block, "min", "minimum", &sys.ang0)) sys.ang0 = 0.f;
        if (!comp_z(&block, "max", "maximum", &sys.ang1)) sys.ang1 = 6.2831853f;
    }
    if (block_named(&doc, "operator", "movement", &block)) {
        if (!j_vec(&block, "gravity", &sys.gravx, &sys.gravy, &ez)) sys.gravx = sys.gravy = 0.f;
        sys.drag = j_num(&block, "drag", 0.f, &present);
    }
    if (block_named(&doc, "operator", "alphafade", &block)) {
        sys.alphafade = 1;
        sys.fade_in = num_key(&block, "fadeintime", "fadein", 0.f, &present);
        sys.fade_out = num_key(&block, "fadeouttime", "fadeout", 0.f, &present);
    }
    if (block_named(&doc, "operator", "sizechange", &block)) {
        int ha = 0, hb = 0;
        float a = num_key(&block, "startvalue", "start", 1.f, &ha);
        float b = num_key(&block, "endvalue", "end", 1.f, &hb);
        if (ha && hb) {
            sys.size_a = a;
            sys.size_b = b;
        } else {
            sys.size_b = -1.f;
            sys.size_rate = j_num(&block, "rate", 0.f, &present);
        }
    }
    if (block_named(&doc, "operator", "colorchange", &block)) {
        float r, gch, b;
        if (vec_key(&block, "endvalue", "end", &r, &gch, &b)) {
            color_fix(&r, &gch, &b);
            sys.er = r;
            sys.eg = gch;
            sys.eb = b;
            sys.color_lerp = 1;
        } else {
            sys.color_lerp = 0;
        }
    }
    if (block_named(&doc, "operator", "angularmovement", &block)) {
        int pa = 0;
        float sp;
        sys.ang_op = 1;
        sys.ang_drag = j_num(&block, "drag", 0.f, &present);
        if (j_vec(&block, "force", &ax, &ay, &az)) sys.ang_force = az;
        /* speed/rate is degrees per second from older fixtures. force is rad/s^2. */
        sp = num_key(&block, "speed", "rate", 0.f, &pa);
        if (!pa && j_vec(&block, "angularvelocity", &ax, &ay, &az)) {
            sp = az;
            pa = 1;
        }
        if (pa) sys.ang_rate = sp * 0.01745329251f;
    }
    memset(cpx, 0, sizeof cpx);
    memset(cpy, 0, sizeof cpy);
    memset(cpmouse, 0, sizeof cpmouse);
    if (j_obj_get(&doc, "controlpoint", &block) && block.kind == 4) {
        nc = j_arr_count(&block);
        if (nc > 8) nc = 8;
        for (i = 0; i < nc; i++) {
            Jv cp;
            float cx, cy, cz;
            int flags;
            if (!j_arr_at(&block, i, &cp) || cp.kind != 3) continue;
            id = (int)j_num(&cp, "id", (float)i, &present);
            if (id < 0 || id > 7) continue;
            flags = (int)j_num(&cp, "flags", 0.f, &present);
            cpmouse[id] = (flags & 1) ? 1 : 0;
            if (!vec_key(&cp, "offset", "origin", &cx, &cy, &cz)) continue;
            cpx[id] = cx;
            cpy[id] = cy;
        }
    }
    for (i = 0; i < 8; i++) {
        char key[32];
        float cx, cy, cz;
        snprintf(key, sizeof key, "controlpoint%d", i);
        if (!over_vec(obj, key, &cx, &cy, &cz)) continue;
        cpx[i] = cx;
        cpy[i] = cy;
    }
    if (block_named(&doc, "operator", "controlpointattract", &block)) {
        id = (int)j_num(&block, "controlpoint", 0.f, &present);
        if (id < 0) id = 0;
        if (id > 7) id = 7;
        sys.attract = num_key(&block, "scale", "force", 0.f, &present);
        if (!present) sys.attract = num_key(&block, "strength", "rate", 0.f, &present);
        sys.cpx = cpx[id];
        sys.cpy = cpy[id];
        sys.cp_mouse = cpmouse[id];
        if (vec_key(&block, "origin", NULL, &ax, &ay, &az)) {
            sys.cpx = ax;
            sys.cpy = ay;
            sys.cp_mouse = 0;
        }
        sys.attract_thr = num_key(&block, "threshold", "distance", 512.f, &present);
        if (!present) sys.attract_thr = num_key(&block, "threadhold", "radius", 512.f, &present);
        if (!present) sys.attract_thr = 512.f;
        if (sys.attract_thr < 0.f) sys.attract_thr = 0.f;
        sys.has_attract = sys.attract != 0.f;
    }
    if (block_named(&doc, "operator", "turbulence", &block)) {
        sys.op_turb = 1;
        sys.op_scale = j_num(&block, "scale", 0.005f, &present);
        if (!present) sys.op_scale = 0.005f;
        sys.op_s0 = num_key(&block, "speedmin", "minspeed", 500.f, &present);
        if (!present) sys.op_s0 = 500.f;
        sys.op_s1 = num_key(&block, "speedmax", "maxspeed", 1000.f, &present);
        if (!present) sys.op_s1 = 1000.f;
        sys.op_time = j_num(&block, "timescale", 0.01f, &present);
        if (!present) sys.op_time = 0.01f;
        if (!j_vec(&block, "mask", &sys.op_mx, &sys.op_my, &sys.op_mz)) {
            sys.op_mx = 1.f;
            sys.op_my = 1.f;
            sys.op_mz = 0.f;
        }
        sys.op_p0 = num_key(&block, "phasemin", "minphase", 0.f, &present);
        sys.op_p1 = num_key(&block, "phasemax", "maxphase", 0.f, &present);
        if (!present) sys.op_p1 = 0.f;
    }
    if (block_named(&doc, "operator", "oscillatealpha", &block)) {
        sys.osc = 1;
        sys.of0 = num_key(&block, "frequencymin", "minfrequency", 0.f, &present);
        sys.of1 = num_key(&block, "frequencymax", "maxfrequency", 10.f, &present);
        if (!present) sys.of1 = 10.f;
        sys.os0 = num_key(&block, "scalemin", "minscale", 0.f, &present);
        sys.os1 = num_key(&block, "scalemax", "maxscale", 1.f, &present);
        if (!present) sys.os1 = 1.f;
        sys.oph0 = num_key(&block, "phasemin", "minphase", 0.f, &present);
        sys.oph1 = num_key(&block, "phasemax", "maxphase", 6.2831853f, &present);
        if (!present) sys.oph1 = 6.2831853f;
    }
    sys.start = j_num(&doc, "starttime", 0.f, &present);
    if (sys.start < 0.f) sys.start = 0.f;
    {
        char mode[32];
        Jv mv;
        mode[0] = 0;
        if (j_obj_get(&doc, "animationmode", &mv)) {
            mv = j_unwrap(&mv);
            if (mv.kind != 7 && j_lit(&mv, mode, sizeof mode) && strcmp(mode, "null") != 0) {
                lower_buf(mode);
                if (!strcmp(mode, "randomframe")) sys.anim = 1;
                else if (!strcmp(mode, "sequence")) sys.anim = 2;
                else if (mode[0]) part_gap("renderer", mode);
            }
        }
        sys.seqmul = j_num(&doc, "sequencemultiplier", 1.f, &present);
        if (!present) sys.seqmul = 1.f;
    }
    if (block_named(&doc, "renderer", "ropetrail", &block)) {
        sys.rope = 1;
        sys.rope_len = j_num(&block, "length", 0.05f, &present);
        if (!present) sys.rope_len = 0.05f;
        if (sys.rope_len < 0.f) sys.rope_len = 0.f;
    }
    if (block_named(&doc, "renderer", "spritetrail", &block)) {
        sys.trail = 1;
        sys.trail_len = j_num(&block, "length", 0.05f, &present);
        if (!present) sys.trail_len = 0.05f;
        sys.trail_max = j_num(&block, "maxlength", 10.f, &present);
        if (!present) sys.trail_max = 10.f;
        sys.trail_min = j_num(&block, "minlength", 0.f, &present);
        if (!present) sys.trail_min = 0.f;
        if (sys.trail_len < 0.f) sys.trail_len = 0.f;
        if (sys.trail_max < sys.trail_min) sys.trail_max = sys.trail_min;
    }
    sys.maxcount = (int)(j_num(&doc, "maxcount", 32.f, &present) + 0.5f);
    if (sys.maxcount < 1) sys.maxcount = 1;
    if (!as_child) {
        mul = over_mul(obj, "count");
        sys.maxcount = (int)((float)sys.maxcount * mul + 0.5f);
        sys.instant = (int)((float)sys.instant * mul + 0.5f);
        if (sys.instant < 0) sys.instant = 0;
        sys.rate *= over_mul(obj, "rate");
    }
    if (sys.maxcount > 4096) {
        fprintf(stderr, "bendwall: particle maxcount clamped\n");
        sys.maxcount = 4096;
    }
    if (sys.maxcount < 0) sys.maxcount = 0;
    if (!as_child) {
        mul = over_mul(obj, "lifetime");
        sys.life0 *= mul;
        sys.life1 *= mul;
        mul = over_mul(obj, "speed");
        sys.speed0 *= mul;
        sys.speed1 *= mul;
        sys.vminx *= mul;
        sys.vminy *= mul;
        sys.vminz *= mul;
        sys.vmaxx *= mul;
        sys.vmaxy *= mul;
        sys.vmaxz *= mul;
        sys.turb_s0 *= mul;
        sys.turb_s1 *= mul;
        sys.op_s0 *= mul;
        sys.op_s1 *= mul;
        mul = over_mul(obj, "size");
        sys.size0 *= mul;
        sys.size1 *= mul;
        mul = over_mul(obj, "alpha");
        sys.a0 *= mul;
        sys.a1 *= mul;
        bright = over_mul(obj, "brightness");
        sys.r0 *= bright;
        sys.g0 *= bright;
        sys.b0 *= bright;
        sys.r1 *= bright;
        sys.g1 *= bright;
        sys.b1 *= bright;
        sys.er *= bright;
        sys.eg *= bright;
        sys.eb *= bright;
    }
    if (sys.life0 < 0.05f) sys.life0 = 0.05f;
    if (sys.life1 < 0.05f) sys.life1 = 0.05f;
    ox = oy = oz = 0.f;
    deg = 0.f;
    if (!as_child) {
        j_vec(obj, "origin", &ox, &oy, &oz);
        if (j_vec(obj, "angles", &ax, &ay, &az)) deg = az;
    }
    {
        float rx, ry;
        rot2(deg, ex, ey, &rx, &ry);
        sys.ox = ox + rx;
        sys.oy = oy + ry;
        if (!sys.box) rot2(deg, dx, dy, &sys.dirx, &sys.diry);
        if (sys.has_attract && !sys.cp_mouse) {
            rot2(deg, sys.cpx, sys.cpy, &rx, &ry);
            sys.cpx = ox + rx;
            sys.cpy = oy + ry;
        }
    }
    sys.deg = deg;
    if (!as_child) {
        mul = j_num(obj, "alpha", 1.f, &present);
        if (!present) mul = 1.f;
        if (mul < 0.f) mul = 0.f;
        if (mul > 1.f) mul = 1.f;
        sys.a0 *= mul;
        sys.a1 *= mul;
        if (j_vec(obj, "scale", &ax, &ay, &az)) {
            if (ax > 0.f) {
                sys.size0 *= ax;
                sys.size1 *= ax;
                sys.sx = ax;
            }
            if (ay > 0.f) sys.sy = ay;
        }
    }
    if (as_child) {
        sys.follow = 1;
        sys.parent = parent;
    }
    matp[0] = 0;
    if (!j_field(&doc, "material", matp, sizeof matp) || !matp[0]) {
        if (!force_path && pull_material(&field, matp, sizeof matp, 0)) {
        } else
            matp[0] = 0;
    }
    slash(matp);
    sys.additive = 0;
    tex[0] = 0;
    {
        int mat_ok = 0;
        if (matp[0]) {
            matb = load_json_rel(dir, assets, matp);
            if (!matb) matb = load_json_prefixed(dir, assets, matp, "materials/");
            if (matb && j_doc(matb, &mat)) {
                mat_ok = 1;
                sys.additive = pass_additive(&mat);
                first_texture(&mat, tex, sizeof tex);
            } else {
                fprintf(stderr, "bendwall: particle material not found %s\n", matp);
            }
        }
        free(matb);
        matb = NULL;
        memset(&g, 0, sizeof g);
        why[0] = 0;
        st = 0;
        {
            char over[512];
            char fail[1100];
            int got = 0, named_fail = 0;
            over[0] = fail[0] = 0;
            if (!as_child && instance_texture(obj, over, sizeof over) && over[0]) {
                slash(over);
                st = load_picture(dir, assets, over, &g, why, (int)sizeof why);
                if (st > 0 && g.rgba) got = 1;
                else {
                    named_fail = 1;
                    copy_cap(fail, sizeof fail, why[0] ? why : over);
                    clear_got(&g);
                }
            }
            if (!got && tex[0]) {
                why[0] = 0;
                st = load_picture(dir, assets, tex, &g, why, (int)sizeof why);
                if (st > 0 && g.rgba) got = 1;
                else {
                    named_fail = 1;
                    if (!fail[0]) copy_cap(fail, sizeof fail, why[0] ? why : tex);
                    clear_got(&g);
                }
            }
            if (!got && !tex[0] && !over[0] && matp[0] && !mat_ok) {
                named_fail = 1;
                copy_cap(fail, sizeof fail, matp);
            }
            if (got) {
                sys.rgba = g.rgba;
                sys.tw = g.w;
                sys.th = g.h;
                g.rgba = NULL;
                if (g.nframes >= 2 && g.sheets && g.frames) {
                    sys.sheets = g.sheets;
                    sys.nsheets = g.nsheets;
                    sys.frames = g.frames;
                    sys.nframes = g.nframes;
                    g.sheets = NULL;
                    g.frames = NULL;
                    g.nsheets = 0;
                    g.nframes = 0;
                }
            } else if (named_fail) {
                miss_log(fail[0] ? fail : (tex[0] ? tex : matp), st < 0);
                plan->missing++;
                sys.rgba = soft_px(&sys.tw, &sys.th);
            } else {
                miss_log("(no path)", 0);
                sys.rgba = soft_px(&sys.tw, &sys.th);
            }
        }
        clear_got(&g);
    }
    if (!sys.rgba) {
        free(blob);
        release_part(&sys);
        return -1;
    }
    idx = push_part(plan, sys);
    if (idx >= 0 && depth < 3) {
        Jv kids, kid;
        int kn, ki;
        if (j_obj_get(&doc, "children", &kids) && kids.kind == 4) {
            kn = j_arr_count(&kids);
            for (ki = 0; ki < kn; ki++) {
                char ctype[64], cpath[512];
                if (!j_arr_at(&kids, ki, &kid) || kid.kind != 3) continue;
                ctype[0] = cpath[0] = 0;
                if (!j_field(&kid, "type", ctype, sizeof ctype)) continue;
                lower_buf(ctype);
                if (!strcmp(ctype, "eventfollow")) {
                    if (!j_field(&kid, "name", cpath, sizeof cpath) || !cpath[0]) continue;
                } else if (!strcmp(ctype, "emberglow")) {
                    /* Same follow as eventfollow when the child names a particle file. */
                    cpath[0] = 0;
                    if (!j_field(&kid, "particle", cpath, sizeof cpath) || !cpath[0] || !strcmp(cpath, "None")
                        || (!strchr(cpath, '/') && !strstr(cpath, ".json"))) {
                        cpath[0] = 0;
                        if (!j_field(&kid, "name", cpath, sizeof cpath) || !cpath[0]
                            || (!strchr(cpath, '/') && !strstr(cpath, ".json")))
                            continue;
                    }
                } else
                    continue;
                take_particle(plan, dir, assets, obj, props, cpath, idx, 1, depth + 1);
            }
        }
    }
    free(blob);
    return idx;
}

int bw_text_rgba(const char* s, int* w, int* h, uint8_t** rgba);

static int text_value(const Jv* obj, char* value, int cap) {
    Jv t, inner;
    if (!j_obj_get(obj, "text", &t) || !value || cap < 2) return 0;
    value[0] = 0;
    if (t.kind == 1) {
        if (!j_str(&t, value, cap) || !value[0]) return 0;
    } else if (t.kind == 3) {
        if (!j_obj_get(&t, "value", &inner)) return 0;
        inner = j_unwrap(&inner);
        if (!j_lit(&inner, value, cap) || !value[0]) return 0;
    } else
        return 0;
    return 1;
}

/* A text string that is a package path, not the letters themselves. */
static int text_file(const char* dir, const char* assets, const char* rel, char* out, int cap) {
    uint8_t* b = NULL;
    int n = 0, i, keep;
    if (!rel || !rel[0] || !out || cap < 2) return 0;
    if (strchr(rel, '\n') || strchr(rel, ' ')) return 0;
    if (!strchr(rel, '/') && !ends_with(rel, ".txt")) return 0;
    if (!read_rel(dir, assets, rel, "", &b, &n) || !b || n < 1) {
        free(b);
        return 0;
    }
    keep = n;
    if (keep > 4096) keep = 4096;
    for (i = 0; i < keep; i++) {
        unsigned char c = b[i];
        if (c == 0 || (c < 9) || (c > 13 && c < 32)) {
            free(b);
            return 0;
        }
    }
    if (keep >= cap) keep = cap - 1;
    memcpy(out, b, (size_t)keep);
    out[keep] = 0;
    free(b);
    return out[0] != 0;
}

static void take_text(BwPlan* plan, const char* dir, const char* assets, const Jv* obj, const PropTab* props) {
    char value[1024];
    char filebody[1024];
    Jv t;
    float x, y, z;
    size_t n;
    int placed;
    uint8_t* px = NULL;
    int tw = 0, th = 0;
    value[0] = 0;
    if (!j_obj_get(obj, "text", &t)) return;
    /* it.user("text"): a project property wins over the inline value. */
    if (!(t.kind == 3 && resolve_user(props, &t, value, (int)sizeof value) && value[0])) {
        if (!text_value(obj, value, (int)sizeof value)) return;
    }
    if (text_file(dir, assets, value, filebody, (int)sizeof filebody))
        snprintf(value, sizeof value, "%s", filebody);
    plan->has_text = 1;
    /* Origin or size is the object's rectangle. Corner text is only the
       strings that have neither, so a page is not a 7px HUD line. */
    placed = j_vec(obj, "origin", &x, &y, &z) || j_vec(obj, "size", &x, &y, &z);
    if (placed && bw_text_rgba(value, &tw, &th, &px) && px) {
        add_pixels(plan, obj, "text", 0, px, tw, th, (float)tw, (float)th, 0);
        return;
    }
    free(px);
    if (!plan->text[0])
        snprintf(plan->text, sizeof plan->text, "%s", value);
    else {
        n = strlen(plan->text);
        if (n + 2 < sizeof plan->text) {
            plan->text[n++] = ' ';
            plan->text[n] = 0;
            snprintf(plan->text + n, sizeof plan->text - n, "%s", value);
        }
    }
}

static void walk_scene(BwPlan* plan, const char* dir, const char* assets, const char* scene, const PropTab* props) {
    Jv root, gen, ortho;
    int i, n, present = 0;
    if (!j_doc(scene, &root)) return;
    scan_effects(plan, scene);
    if (j_obj_get(&root, "general", &gen) && j_obj_get(&gen, "orthogonalprojection", &ortho)) {
        plan->scene_w = j_num(&ortho, "width", 0, &present);
        plan->scene_h = j_num(&ortho, "height", 0, &present);
    }
    if (j_obj_get(&root, "objects", &gen) && gen.kind == 4) {
        n = j_arr_count(&gen);
        if (n > 256) n = 256;
        for (i = 0; i < n; i++) {
            Jv obj;
            if (!j_arr_at(&gen, i, &obj) || obj.kind != 3) continue;
            if (!visible_obj(&obj)) continue;
            take_text(plan, dir, assets, &obj, props);
            if (j_obj_get(&obj, "image", &ortho)) take_image(plan, dir, assets, &obj, props);
            if (j_obj_get(&obj, "particle", &ortho)) take_particle(plan, dir, assets, &obj, props, NULL, -1, 0, 0);
        }
    }
}

static int load_scene(const char* dir, const char* assets, const char* file_field, char** out) {
    char path[1100];
    const char* names[4];
    int n = 0, i, sz = 0;
    uint8_t* b;
    *out = NULL;
    if (file_field && strstr(file_field, ".json")) names[n++] = file_field;
    names[n++] = "scene.json";
    names[n++] = "gifscene.json";
    for (i = 0; i < n; i++) {
        if (read_rel(dir, assets, names[i], "", &b, &sz)) {
            *out = (char*)b;
            return 1;
        }
    }
    (void)path;
    return 0;
}

static float audio_gain(float v) {
    if (v > 1.f) v /= 100.f;
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    return v;
}

static int j_scalar(const Jv* v, float* out) {
    Jv u;
    char b[64];
    if (!v || !out) return 0;
    u = j_unwrap(v);
    if (u.kind == 5) {
        *out = 1.f;
        return 1;
    }
    if (u.kind == 6) {
        *out = 0.f;
        return 1;
    }
    if (!j_lit(&u, b, sizeof b)) return 0;
    *out = pf(b, NULL);
    return 1;
}

static void apply_audio_key(BwPlan* out, const char* name, const Jv* val) {
    char key[64];
    float n;
    int i;
    if (!out || !name || !name[0]) return;
    for (i = 0; name[i] && i < (int)sizeof key - 1; i++)
        key[i] = (char)tolower((unsigned char)name[i]);
    key[i] = 0;
    if (!j_scalar(val, &n)) return;
    if (!strcmp(key, "volume")) out->video_volume = audio_gain(n);
    else if (!strcmp(key, "mute") || !strcmp(key, "muted")) out->video_mute = n > 0.5f;
}

static void apply_audio_prop(const char* name, const Jv* val, void* ud) {
    apply_audio_key((BwPlan*)ud, name, val);
}

/* general.volume / mute first, then a property of the same name. A property
   wins when both are present. The array form is what the settings page
   reads; web items use an object map. */
static void take_project_audio(BwPlan* out, const Jv* root) {
    Jv gen, props, item, val;
    int present = 0;
    float n;
    int i, c;
    out->video_volume = 1.f;
    out->video_mute = 0;
    if (!root || !j_obj_get(root, "general", &gen) || gen.kind != 3) return;
    n = j_num(&gen, "volume", 1.f, &present);
    if (present) out->video_volume = audio_gain(n);
    n = j_num(&gen, "mute", 0.f, &present);
    if (present) out->video_mute = n > 0.5f;
    n = j_num(&gen, "muted", 0.f, &present);
    if (present && n > 0.5f) out->video_mute = 1;
    if (!j_obj_get(&gen, "properties", &props)) return;
    if (props.kind == 4) {
        c = j_arr_count(&props);
        for (i = 0; i < c; i++) {
            char name[64];
            if (!j_arr_at(&props, i, &item) || item.kind != 3) continue;
            if (!j_field(&item, "name", name, sizeof name)) continue;
            if (!j_obj_get(&item, "value", &val)) continue;
            apply_audio_key(out, name, &val);
        }
        return;
    }
    if (props.kind == 3) j_obj_each(&props, apply_audio_prop, out);
}

static void join_file(char* dst, int cap, const char* dir, const char* file) {
    if (!file || !file[0]) {
        dst[0] = 0;
        return;
    }
    slash((char*)file);
    if (file[0] == '/')
        snprintf(dst, (size_t)cap, "%s", file);
    else
        snprintf(dst, (size_t)cap, "%s/%s", dir ? dir : "", file);
}

int bw_plan_load(const char* dir, const char* assets, BwPlan* out) {
    uint8_t* proj = NULL;
    char* scene = NULL;
    int n = 0;
    char path[1100], type[32], file[512], title[160];
    Jv root, v;
    PropTab props;
    memset(out, 0, sizeof *out);
    memset(&props, 0, sizeof props);
    out->video_volume = 1.f;
    snprintf(out->kind, sizeof out->kind, "scene");
    if (!dir || !dir[0]) {
        snprintf(out->kind, sizeof out->kind, "demo");
        snprintf(out->text, sizeof out->text, "Bendwall");
        out->scroll = 1;
        return 1;
    }
    snprintf(path, sizeof path, "%s/project.json", dir);
    proj = read_bin(path, &n);
    type[0] = file[0] = title[0] = 0;
    if (proj && j_doc((char*)proj, &root)) {
        if (j_obj_get(&root, "type", &v) && j_str(&v, type, sizeof type)) {
            for (char* p = type; *p; p++) *p = (char)tolower((unsigned char)*p);
        }
        if (j_obj_get(&root, "file", &v)) j_str(&v, file, sizeof file);
        if (j_obj_get(&root, "title", &v)) j_str(&v, title, sizeof title);
        slash(file);
        take_project_audio(out, &root);
        collect_props(&props, &root);
    }
    free(proj);
    snprintf(out->type, sizeof out->type, "%s", type);
    snprintf(out->file, sizeof out->file, "%s", file);
    snprintf(out->title, sizeof out->title, "%s", title);
    /* The project title is metadata. It is not drawn on the wallpaper
       or in --watch. Scene text objects still are. */
    if (!strcmp(type, "web")) {
        snprintf(out->kind, sizeof out->kind, "web");
        join_file(out->page, (int)sizeof out->page, dir, file);
        return 1;
    }
    if (!strcmp(type, "video")) {
        snprintf(out->kind, sizeof out->kind, "video");
        join_file(out->video, (int)sizeof out->video, dir, file);
        return 1;
    }
    snprintf(out->kind, sizeof out->kind, "scene");
    if (!load_scene(dir, assets, file, &scene)) return 1;
    walk_scene(out, dir, assets, scene, &props);
    free(scene);
    return 1;
}

void bw_plan_free(BwPlan* plan) {
    int i;
    if (!plan) return;
    for (i = 0; i < plan->nlayers; i++) {
        free(plan->layers[i].rgba);
        free_sheets(plan->layers[i].sheets, plan->layers[i].nsheets);
        free(plan->layers[i].frames);
    }
    free(plan->layers);
    plan->layers = NULL;
    plan->nlayers = 0;
    if (plan->parts) {
        for (i = 0; i < plan->nparts; i++) {
            free(plan->parts[i].rgba);
            free_sheets(plan->parts[i].sheets, plan->parts[i].nsheets);
            free(plan->parts[i].frames);
        }
        free(plan->parts);
        plan->parts = NULL;
        plan->nparts = 0;
    }
}

int bw_scene_ready(const char* dir, const char* assets) {
    BwPlan plan;
    int ok;
    if (!bw_plan_load(dir, assets, &plan)) {
        bw_plan_free(&plan);
        return 0;
    }
    /* A checker anywhere in the picture is partial, even when another
       layer decoded. Text with no missing texture stays loaded. */
    if (plan.missing > 0) ok = 0;
    else if (plan.image_tried > 0) ok = plan.image_ok > 0;
    else ok = plan.has_text;
    bw_plan_free(&plan);
    return ok;
}
