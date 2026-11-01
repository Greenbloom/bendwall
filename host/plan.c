/* Scene picture resolution. Top-level project.json keys, then
   object → model json → material json → texture. .tex is TEXV/TEXI/TEXB
   (REA §4): LZ4 block, raw rgba/rg/r, DXT, or an embedded PNG/JPEG.
   The same entry point feeds the player and the probe. */
#include "plan.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <png.h>
#include <setjmp.h>

#define CAP_FILE (64 * 1024 * 1024)
#define CAP_RAW (64 * 1024 * 1024)
#define MAX_LAYER 32

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

int bw_tex_decode(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h) {
    char texv[12], texi[12], texb[12];
    int i = 0, ver_i, bv, vol, fif = -1, fmt, flags, iw, ih, mw, mh;
    uint32_t mip_count, image_count, lz4 = 0, raw = 0, sz;
    const uint8_t* data;
    uint8_t* rawb = NULL;
    uint8_t* full = NULL;
    int tw, th;
    if (!b || n < 32 || !rgba) return 0;
    *rgba = NULL;
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
    if (image_count < 1 || image_count > 64) return 0;
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
    if (!need(i, n, 4)) return 0;
    mip_count = ru32(b + i);
    i += 4;
    if (mip_count < 1 || mip_count > 16) return 0;
    if (!need(i, n, 8)) return 0;
    mw = (int)ru32(b + i);
    mh = (int)ru32(b + i + 4);
    i += 8;
    if (vol) {
        if (!need(i, n, 4)) return 0;
        i += 4;
    }
    if (bv >= 2) {
        if (!need(i, n, 8)) return 0;
        lz4 = ru32(b + i);
        raw = ru32(b + i + 4);
        i += 8;
    }
    if (!need(i, n, 4)) return 0;
    sz = ru32(b + i);
    i += 4;
    if (!need(i, n, (int)sz) || mw < 1 || mh < 1 || mw > 8192 || mh > 8192) return 0;
    data = b + i;
    if (lz4 & 1u) {
        if (raw < 1 || raw > CAP_RAW) return 0;
        rawb = (uint8_t*)malloc(raw);
        if (!rawb || !lz4_block(data, (int)sz, rawb, (int)raw)) {
            free(rawb);
            return 0;
        }
        data = rawb;
        sz = raw;
    }
    if (fif != -1) {
        int ok = decode_png_mem(data, (int)sz, &full, &tw, &th);
        if (!ok) ok = decode_jpeg_mem(data, (int)sz, &full, &tw, &th);
        free(rawb);
        if (!ok) return 0;
        *rgba = full;
        *w = tw;
        *h = th;
        return 1;
    }
    if (fmt == 0) {
        if (sz < (uint32_t)mw * (uint32_t)mh * 4) {
            free(rawb);
            return 0;
        }
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) {
            free(rawb);
            return 0;
        }
        memcpy(full, data, (size_t)mw * (size_t)mh * 4);
        tw = mw;
        th = mh;
    } else if (fmt == 1) {
        int x, y;
        if (sz < (uint32_t)mw * (uint32_t)mh * 3) {
            free(rawb);
            return 0;
        }
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) {
            free(rawb);
            return 0;
        }
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
        tw = mw;
        th = mh;
    } else if (fmt == 8) {
        int x, y;
        if (sz < (uint32_t)mw * (uint32_t)mh * 2) {
            free(rawb);
            return 0;
        }
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) {
            free(rawb);
            return 0;
        }
        for (y = 0; y < mh; y++) {
            for (x = 0; x < mw; x++) {
                const uint8_t* s = data + ((size_t)y * (size_t)mw + (size_t)x) * 2;
                uint8_t* d = full + ((size_t)y * (size_t)mw + (size_t)x) * 4;
                d[0] = d[1] = d[2] = s[0];
                d[3] = s[1];
            }
        }
        tw = mw;
        th = mh;
    } else if (fmt == 9) {
        int x, y;
        if (sz < (uint32_t)mw * (uint32_t)mh) {
            free(rawb);
            return 0;
        }
        full = (uint8_t*)malloc((size_t)mw * (size_t)mh * 4);
        if (!full) {
            free(rawb);
            return 0;
        }
        for (y = 0; y < mh; y++) {
            for (x = 0; x < mw; x++) {
                uint8_t* d = full + ((size_t)y * (size_t)mw + (size_t)x) * 4;
                d[0] = d[1] = d[2] = data[(size_t)y * (size_t)mw + (size_t)x];
                d[3] = 255;
            }
        }
        tw = mw;
        th = mh;
    } else if (fmt == 7 || fmt == 6 || fmt == 4) {
        int kind = fmt == 7 ? 1 : (fmt == 6 ? 3 : 5);
        if (!dxt_decode(data, (int)sz, mw, mh, kind, &full)) {
            free(rawb);
            return 0;
        }
        tw = mw;
        th = mh;
    } else {
        free(rawb);
        return 0;
    }
    free(rawb);
    if (iw > 0 && ih > 0 && (iw < tw || ih < th) && iw <= tw && ih <= th) {
        uint8_t* crop = (uint8_t*)malloc((size_t)iw * (size_t)ih * 4);
        int y;
        if (!crop) {
            free(full);
            return 0;
        }
        for (y = 0; y < ih; y++)
            memcpy(crop + (size_t)y * (size_t)iw * 4, full + (size_t)y * (size_t)tw * 4, (size_t)iw * 4);
        free(full);
        full = crop;
        tw = iw;
        th = ih;
    }
    *rgba = full;
    *w = tw;
    *h = th;
    return full != NULL;
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

static int decode_bytes(const uint8_t* b, int n, uint8_t** rgba, int* w, int* h) {
    if (!b || n < 4) return 0;
    if (n >= 8 && !memcmp(b, "TEXV", 4)) return bw_tex_decode(b, n, rgba, w, h);
    if (b[0] == 0x89 && b[1] == 'P') return decode_png_mem(b, n, rgba, w, h);
    if (b[0] == 0xFF && b[1] == 0xD8) return decode_jpeg_mem(b, n, rgba, w, h);
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

static int try_rel(const char* dir, const char* assets, const char* rel, uint8_t** rgba, int* w, int* h) {
    uint8_t* b = NULL;
    int n = 0, ok;
    const char* ext = raster_ext(rel) ? "" : ".tex";
    if (!read_rel(dir, assets, rel, ext, &b, &n)) return 0;
    ok = decode_bytes(b, n, rgba, w, h);
    free(b);
    return ok ? 1 : -1;
}

static int load_pixels(const char* dir, const char* assets, const char* rel, uint8_t** rgba, int* w, int* h, char* why, int why_cap) {
    char cand[4][1100];
    char lower[1100];
    const char* base;
    int nc = 0, i, st, bad = 0;
    uint8_t* raw = NULL;
    int n = 0;
    if (why && why_cap > 0) why[0] = 0;
    if (!rel || !rel[0]) return 0;
    snprintf(cand[nc++], 1100, "%s", rel);
    lower_copy(rel, lower, sizeof lower);
    if (nc < 4 && strcmp(lower, rel) != 0) snprintf(cand[nc++], 1100, "%s", lower);
    base = base_of(rel);
    /* A material slot named "ztyz" is materials/ztyz.tex in the package,
       then the same path under BENDWALL_ASSETS. */
    if (nc < 4 && (base == rel || base[0] == 0))
        snprintf(cand[nc++], 1100, "materials/%s", rel);
    else if (nc < 4 && strncasecmp(rel, "materials/", 10) != 0)
        snprintf(cand[nc++], 1100, "materials/%s", base);
    for (i = 0; i < nc; i++) {
        st = try_rel(dir, assets, cand[i], rgba, w, h);
        if (st > 0) return 1;
        if (st < 0) {
            bad = 1;
            if (why && why_cap > 0) copy_cap(why, why_cap, cand[i]);
        }
    }
    if (!bad && dir && dir[0] && pkg_dir_mode(dir, base[0] ? base : rel, 1, &raw, &n)) {
        if (decode_bytes(raw, n, rgba, w, h)) {
            free(raw);
            return 1;
        }
        free(raw);
        bad = 1;
        if (why && why_cap > 0) {
            char labeled[1100];
            snprintf(labeled, sizeof labeled, "materials/%s", base[0] ? base : rel);
            copy_cap(why, why_cap, labeled);
        }
    }
    if (why && why_cap > 0 && !why[0]) copy_cap(why, why_cap, rel);
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

static int first_texture(const Jv* mat, char* out, int cap) {
    Jv passes, pass, texs, one;
    int i, n;
    if (!j_obj_get(mat, "passes", &passes) || passes.kind != 4) return 0;
    if (!j_arr_at(&passes, 0, &pass) || pass.kind != 3) return 0;
    if (!j_obj_get(&pass, "textures", &texs) || texs.kind != 4) return 0;
    n = j_arr_count(&texs);
    for (i = 0; i < n; i++) {
        if (!j_arr_at(&texs, i, &one)) break;
        if (one.kind != 1) continue;
        if (!j_str(&one, out, cap)) continue;
        slash(out);
        if (!out[0] || strncmp(out, "_rt_", 4) == 0) continue;
        return 1;
    }
    return 0;
}

static char* load_json_prefixed(const char* dir, const char* assets, const char* rel, const char* prefix) {
    char alt[1100];
    if (!rel || !rel[0] || strchr(rel, '/') || strchr(rel, '\\')) return NULL;
    snprintf(alt, sizeof alt, "%s%s", prefix, rel);
    return load_json_rel(dir, assets, alt);
}

static void take_image(BwPlan* plan, const char* dir, const char* assets, const Jv* obj) {
    Jv im;
    char path[512], tex[512], matp[512], why[1100];
    char* model = NULL;
    char* mat = NULL;
    Jv root, material;
    int present = 0, model_full = 0, st = 0;
    float mw = 0, mh = 0;
    uint8_t* px = NULL;
    int w = 0, h = 0;
    plan->image_tried++;
    if (!j_obj_get(obj, "image", &im)) return;
    im = j_unwrap(&im);
    if (im.kind != 1 || !j_str(&im, path, sizeof path)) {
        push_missing(plan, "image", 0, 0, 0, 0, 0, 0, 1, 1);
        return;
    }
    slash(path);
    if (raster_ext(path)) {
        st = load_pixels(dir, assets, path, &px, &w, &h, why, (int)sizeof why);
        if (st <= 0) px = NULL;
        add_pixels(plan, obj, why[0] ? why : path, st < 0, px, w, h, 0, 0, 0);
        if (px && plan->nlayers > 0) {
            /* direct stills cover the framebuffer, which is what the tint fixture draws */
            plan->layers[plan->nlayers - 1].full = 1;
        }
        return;
    }
    model = load_json_rel(dir, assets, path);
    if (!model) model = load_json_prefixed(dir, assets, path, "models/");
    if (!model || !j_doc(model, &root)) {
        free(model);
        push_missing(plan, path, 0, 0, 0, 0, 0, 0, 1, 1);
        return;
    }
    model_full = j_num(&root, "fullscreen", 0, &present) > 0.5f;
    mw = j_num(&root, "width", 0, &present);
    mh = j_num(&root, "height", 0, &present);
    if (!j_field(&root, "material", matp, sizeof matp)) {
        free(model);
        push_missing(plan, path, 0, 0, 0, 0, 0, 0, 1, 1);
        return;
    }
    slash(matp);
    free(model);
    mat = load_json_rel(dir, assets, matp);
    if (!mat) mat = load_json_prefixed(dir, assets, matp, "materials/");
    if (!mat || !j_doc(mat, &material) || !first_texture(&material, tex, sizeof tex)) {
        free(mat);
        push_missing(plan, matp, 0, 0, 0, 0, 0, 0, 1, 1);
        return;
    }
    free(mat);
    why[0] = 0;
    st = load_pixels(dir, assets, tex, &px, &w, &h, why, (int)sizeof why);
    if (st <= 0) px = NULL;
    add_pixels(plan, obj, why[0] ? why : tex, st < 0, px, w, h, mw, mh, model_full);
}

static void take_text(BwPlan* plan, const Jv* obj) {
    Jv t, inner;
    char value[160];
    if (plan->has_text) return;
    if (!j_obj_get(obj, "text", &t)) return;
    if (t.kind == 1) {
        if (!j_str(&t, value, sizeof value) || !value[0]) return;
    } else if (t.kind == 3) {
        if (!j_obj_get(&t, "value", &inner)) return;
        inner = j_unwrap(&inner);
        if (!j_lit(&inner, value, sizeof value) || !value[0]) return;
    } else
        return;
    snprintf(plan->text, sizeof plan->text, "%s", value);
    plan->has_text = 1;
}

static void walk_scene(BwPlan* plan, const char* dir, const char* assets, const char* scene) {
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
        if (n > 64) n = 64;
        for (i = 0; i < n; i++) {
            Jv obj;
            if (!j_arr_at(&gen, i, &obj) || obj.kind != 3) continue;
            if (!visible_obj(&obj)) continue;
            take_text(plan, &obj);
            if (j_obj_get(&obj, "image", &ortho)) take_image(plan, dir, assets, &obj);
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
    memset(out, 0, sizeof *out);
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
    }
    free(proj);
    snprintf(out->type, sizeof out->type, "%s", type);
    snprintf(out->file, sizeof out->file, "%s", file);
    snprintf(out->title, sizeof out->title, "%s", title);
    if (title[0] && !out->text[0]) snprintf(out->text, sizeof out->text, "%s", title);
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
    walk_scene(out, dir, assets, scene);
    free(scene);
    if (!out->has_text && title[0]) snprintf(out->text, sizeof out->text, "%s", title);
    return 1;
}

void bw_plan_free(BwPlan* plan) {
    int i;
    if (!plan) return;
    for (i = 0; i < plan->nlayers; i++) free(plan->layers[i].rgba);
    free(plan->layers);
    plan->layers = NULL;
    plan->nlayers = 0;
}

int bw_scene_ready(const char* dir, const char* assets) {
    BwPlan plan;
    int ok;
    if (!bw_plan_load(dir, assets, &plan)) {
        bw_plan_free(&plan);
        return 0;
    }
    if (plan.image_tried > 0) ok = plan.image_ok > 0;
    else ok = plan.has_text;
    bw_plan_free(&plan);
    return ok;
}
