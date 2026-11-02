import Base
import ./bytes.bend as B
import ./spec.bend as Spec
import ./str.bend as S
import ./lz4.bend as L

# .tex reader. Layout is TEXV, TEXI, TEXB, and an optional TEXS.
# LZ4 bit0 is a raw block. FreeImage payloads stay intact for the image effect.
# rg88 / r8 expansion is separate (`expand`). DXT stays compressed.

type Mip is Data:
  Mip{w: U32, h: U32, d: U32, lz4: U32, raw: U32, csz: U32, px: List<&2, U32>}

type IM is Data:
  IM{n: U32, mip: Mip}

type Frame is Data:
  Frame{img: U32, tb: U32, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32}

type Tex is Data:
  Tex{err: U32, ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, fif: U32, cond: U32, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, frames: List<&2, Frame>}

type Tg is Data:
  Tg{c: B.Cur, k: U32, v: U32}

type H is Data:
  H{ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, fif: U32, cond: U32}

type G is Data:
  G{c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, i: U32}

type Acc is Data:
  Acc{c: B.Cur, imgs: List<&2, IM>}

def k1(eq: Bool) -> U32:
  match eq:
    case True{}:
      4
    case False{}:
      0

def k2(d: U32, eq: Bool) -> U32:
  match eq:
    case True{}:
      3
    case False{}:
      k1(U32.is_eq(d, 83))

def k3(+d: U32, eq: Bool) -> U32:
  match eq:
    case True{}:
      2
    case False{}:
      k2(d, U32.is_eq(d, 66))

def k4(+d: U32, eq: Bool) -> U32:
  match eq:
    case True{}:
      1
    case False{}:
      k3(d, U32.is_eq(d, 73))

def tag_info.tex(rest: String, +d: U32, tex: Bool) -> U32 & U32:
  match tex:
    case False{}:
      (0, 0)
    case True{}:
      (k4(d, U32.is_eq(d, 86)), S.atoi(rest, 0))

def tag_info(s: String) -> U32 & U32:
  match s:
    case SCon{Chr{a}, SCon{Chr{b}, SCon{Chr{c}, SCon{Chr{d}, rest}}}}:
      tag_info.tex(rest, d, Bool.and(U32.is_eq(a, 84), Bool.and(U32.is_eq(b, 69), U32.is_eq(c, 88))))
    case _:
      (0, 0)

def take_tag.info(c: B.Cur, r: U32 & U32) -> Tg:
  (k, v) = r
  Tg{c, k, v}

def take_tag.got(r: B.Cur & List<&2, U32>) -> Tg:
  (c, xs) = r
  take_tag.info(c, tag_info(S.from_bytes(xs)))

def take_tag(c: B.Cur) -> Tg:
  take_tag.got(B.take(c, 9))

def err_or.pick(err: U32, e: U32, sticky: Bool) -> U32:
  match sticky:
    case True{}:
      err
    case False{}:
      e

def err_or.go(r: B.Cur & U32, e: U32) -> U32:
  (c, +err) = r
  err_or.pick(err, e, U32.is_ne(err, 0))

def err_or(c: B.Cur, e: U32) -> U32:
  err_or.go(B.fork_err(c), e)

def mip0() -> Mip:
  Mip{0, 0, 1, 0, 0, 0, Nil{}}

def bad(c: B.Cur, ver: U32) -> Tex:
  Tex{err_or(c, 2), ver, 0, 0, 0, 0, 0, 0, 1, 0, 0, 4294967295, 0, Nil{}, 0, 0, 0, Nil{}}

def mkH(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, fif: U32, cond: U32) -> H:
  H{ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond}

def pack(h: H, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, frames: List<&2, Frame>, err: U32) -> Tex:
  H{ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond} = h
  Tex{err, ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond, imgs, sw, sh, sv, frames}

def vol_of.go(b: Bool) -> U32:
  match b:
    case True{}:
      1
    case False{}:
      0

def vol_of(flags: U32) -> U32:
  vol_of.go(U32.is_ne(U32.and(flags, 64), 0))

def flag4(h: H) -> Bool:
  H{ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond} = h
  U32.is_ne(U32.and(flags, 4), 0)

@unsafe
def rev_u(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev_u(t, Con{h, acc})

@unsafe
def rev_im(xs: List<&2, IM>, acc: List<&2, IM>) -> List<&2, IM>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev_im(t, Con{h, acc})

@unsafe
def rev_fr(xs: List<&2, Frame>, acc: List<&2, Frame>) -> List<&2, Frame>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev_fr(t, Con{h, acc})

@unsafe
def g6(g: G, done: Bool) -> G:
  match done:
    case True{}:
      g
    case False{}:
      g6.one(g)

@unsafe
def g6.step(c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, i: U32) -> G:
  +j = (i + 1 : U32)
  g6(G{c, x, y, w, wy, hx, h, j}, U32.is_ge(j, 6))

@unsafe
def g6.p4(c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, i: U32, v: U32, eq: Bool) -> G:
  match eq:
    case True{}:
      g6.step(c, x, y, w, wy, v, h, i)
    case False{}:
      g6.step(c, x, y, w, wy, hx, v, i)

@unsafe
def g6.p3(c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, +i: U32, v: U32, eq: Bool) -> G:
  match eq:
    case True{}:
      g6.step(c, x, y, w, v, hx, h, i)
    case False{}:
      g6.p4(c, x, y, w, wy, hx, h, i, v, U32.is_eq(i, 4))

@unsafe
def g6.p2(c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, +i: U32, v: U32, eq: Bool) -> G:
  match eq:
    case True{}:
      g6.step(c, x, y, v, wy, hx, h, i)
    case False{}:
      g6.p3(c, x, y, w, wy, hx, h, i, v, U32.is_eq(i, 3))

@unsafe
def g6.p1(c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, +i: U32, v: U32, eq: Bool) -> G:
  match eq:
    case True{}:
      g6.step(c, x, v, w, wy, hx, h, i)
    case False{}:
      g6.p2(c, x, y, w, wy, hx, h, i, v, U32.is_eq(i, 2))

@unsafe
def g6.put(c: B.Cur, x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, +i: U32, v: U32, eq: Bool) -> G:
  match eq:
    case True{}:
      g6.step(c, v, y, w, wy, hx, h, i)
    case False{}:
      g6.p1(c, x, y, w, wy, hx, h, i, v, U32.is_eq(i, 1))

@unsafe
def g6.got(x: U32, y: U32, w: U32, wy: U32, hx: U32, h: U32, +i: U32, r: B.Cur & U32) -> G:
  (c, v) = r
  g6.put(c, x, y, w, wy, hx, h, i, v, U32.is_eq(i, 0))

@unsafe
def g6.one(g: G) -> G:
  G{c, x, y, w, wy, hx, h, i} = g
  g6.got(x, y, w, wy, hx, h, i, B.u32(c))

@unsafe
def read_frame.g(img: U32, tb: U32, g: G) -> B.Cur & Frame:
  G{c, x, y, w, wy, hx, h, i} = g
  (c, Frame{img, tb, x, y, w, wy, hx, h})

@unsafe
def read_frame.tb(img: U32, r: B.Cur & U32) -> B.Cur & Frame:
  (c, tb) = r
  read_frame.g(img, tb, g6(G{c, 0, 0, 0, 0, 0, 0, 0}, False{}))

@unsafe
def read_frame.img(r: B.Cur & U32) -> B.Cur & Frame:
  (c, img) = r
  read_frame.tb(img, B.u32(c))

@unsafe
def read_frame(c: B.Cur) -> B.Cur & Frame:
  read_frame.img(B.u32(c))

@unsafe
def read_mip.dec3(c: B.Cur, w: U32, h: U32, d: U32, lz: U32, raw: U32, csz: U32, px: List<&2, U32>, good: Bool) -> B.Cur & Mip:
  match good:
    case True{}:
      (c, Mip{w, h, d, lz, raw, csz, px})
    case False{}:
      (B.fail(c, 7), Mip{w, h, d, lz, raw, csz, Nil{}})

@unsafe
def read_mip.dec2(c: B.Cur, w: U32, h: U32, d: U32, lz: U32, raw: U32, csz: U32, dec: L.Dec) -> B.Cur & Mip:
  L.Dec{+ok, op, px} = dec
  read_mip.dec3(c, w, h, d, lz, raw, csz, px, U32.is_eq(ok, 1))

@unsafe
def read_mip.got(c: B.Cur, w: U32, h: U32, d: U32, lz: U32, +raw: U32, csz: U32, dec: L.Dec) -> B.Cur & Mip:
  read_mip.dec2(c, w, h, d, lz, raw, csz, dec)

@unsafe
def read_mip.dec(c: B.Cur, w: U32, h: U32, d: U32, lz: U32, +raw: U32, csz: U32, px: List<&2, U32>, on: Bool) -> B.Cur & Mip:
  match on:
    case False{}:
      (c, Mip{w, h, d, lz, raw, csz, px})
    case True{}:
      read_mip.got(c, w, h, d, lz, raw, csz, L.decode(px, raw))

@unsafe
def read_mip.bit(c: B.Cur, w: U32, h: U32, d: U32, +lz: U32, raw: U32, csz: U32, px: List<&2, U32>) -> B.Cur & Mip:
  read_mip.dec(c, w, h, d, lz, raw, csz, px, U32.is_eq(U32.and(lz, 1), 1))

@unsafe
def read_mip.res(c: B.Cur, w: U32, h: U32, d: U32, lz: U32, raw: U32, csz: U32, px: List<&2, U32>, ok: Bool) -> B.Cur & Mip:
  match ok:
    case False{}:
      (B.fail(c, 3), Mip{w, h, d, lz, raw, csz, Nil{}})
    case True{}:
      read_mip.bit(c, w, h, d, lz, raw, csz, px)

@unsafe
def read_mip.px(+w: U32, +h: U32, +d: U32, lz: U32, raw: U32, csz: U32, r: B.Cur & List<&2, U32>) -> B.Cur & Mip:
  (c, px) = r
  read_mip.res(c, w, h, d, lz, raw, csz, px, Bool.and(Spec.res_ok(w, h), Spec.depth_ok(d)))

@unsafe
def read_mip.sz(w: U32, h: U32, d: U32, lz: U32, raw: U32, r: B.Cur & U32) -> B.Cur & Mip:
  (c, +csz) = r
  read_mip.px(w, h, d, lz, raw, csz, B.take(c, csz))

@unsafe
def read_mip.raw(w: U32, h: U32, d: U32, lz: U32, r: B.Cur & U32) -> B.Cur & Mip:
  (c, raw) = r
  read_mip.sz(w, h, d, lz, raw, B.u32(c))

@unsafe
def read_mip.lzgot(w: U32, h: U32, d: U32, r: B.Cur & U32) -> B.Cur & Mip:
  (c, lz) = r
  read_mip.raw(w, h, d, lz, B.u32(c))

@unsafe
def read_mip.lz2(w: U32, h: U32, d: U32, c: B.Cur, ge: Bool) -> B.Cur & Mip:
  match ge:
    case False{}:
      read_mip.sz(w, h, d, 0, 0, B.u32(c))
    case True{}:
      read_mip.lzgot(w, h, d, B.u32(c))

@unsafe
def read_mip.lz(+bv: U32, w: U32, h: U32, d: U32, c: B.Cur) -> B.Cur & Mip:
  read_mip.lz2(w, h, d, c, U32.is_ge(bv, 2))

@unsafe
def read_mip.dgot(bv: U32, w: U32, h: U32, r: B.Cur & U32) -> B.Cur & Mip:
  (c, d) = r
  read_mip.lz(bv, w, h, d, c)

@unsafe
def read_mip.d(bv: U32, w: U32, h: U32, c: B.Cur, isvol: Bool) -> B.Cur & Mip:
  match isvol:
    case False{}:
      read_mip.lz(bv, w, h, 1, c)
    case True{}:
      read_mip.dgot(bv, w, h, B.u32(c))

@unsafe
def read_mip.h(bv: U32, +vol: U32, w: U32, r: B.Cur & U32) -> B.Cur & Mip:
  (c, h) = r
  read_mip.d(bv, w, h, c, U32.is_eq(vol, 1))

@unsafe
def read_mip.w(bv: U32, vol: U32, r: B.Cur & U32) -> B.Cur & Mip:
  (c, w) = r
  read_mip.h(bv, vol, w, B.u32(c))

@unsafe
def read_mip(c: B.Cur, bv: U32, vol: U32) -> B.Cur & Mip:
  read_mip.w(bv, vol, B.u32(c))

@unsafe
def read_mips(c: B.Cur, bv: U32, vol: U32, n: U32, left: U32, keep: U32, first: Mip, done: Bool) -> B.Cur & IM:
  match done:
    case True{}:
      (c, IM{n, first})
    case False{}:
      read_mips.one(c, bv, vol, n, left, keep, first)

@unsafe
def read_mips.keep(c: B.Cur, bv: U32, vol: U32, n: U32, +left: U32, first: Mip, m: Mip, is1: Bool) -> B.Cur & IM:
  +j = (left - 1 : U32)
  read_mips.keep2(c, bv, vol, n, j, first, m, is1, U32.is_zero(j))

@unsafe
def read_mips.keep2(c: B.Cur, bv: U32, vol: U32, n: U32, j: U32, first: Mip, m: Mip, is1: Bool, done: Bool) -> B.Cur & IM:
  match is1:
    case True{}:
      read_mips(c, bv, vol, n, j, 0, m, done)
    case False{}:
      read_mips(c, bv, vol, n, j, 0, first, done)

@unsafe
def read_mips.got(bv: U32, vol: U32, n: U32, left: U32, keep: U32, first: Mip, r: B.Cur & Mip) -> B.Cur & IM:
  (c, m) = r
  read_mips.keep(c, bv, vol, n, left, first, m, U32.is_eq(keep, 1))

@unsafe
def read_mips.one(c: B.Cur, +bv: U32, +vol: U32, n: U32, left: U32, keep: U32, first: Mip) -> B.Cur & IM:
  read_mips.got(bv, vol, n, left, keep, first, read_mip(c, bv, vol))

@unsafe
def read_img.cap(c: B.Cur, bv: U32, vol: U32, +n: U32, huge: Bool) -> B.Cur & IM:
  match huge:
    case True{}:
      (B.fail(c, 1), IM{0, mip0()})
    case False{}:
      read_mips(c, bv, vol, n, n, 1, mip0(), U32.is_zero(n))

@unsafe
def read_img.n(bv: U32, vol: U32, r: B.Cur & U32) -> B.Cur & IM:
  (c, +n) = r
  read_img.cap(c, bv, vol, n, U32.is_gt(n, 16))

@unsafe
def read_img(c: B.Cur, bv: U32, vol: U32) -> B.Cur & IM:
  read_img.n(bv, vol, B.u32(c))

@unsafe
def parse.fr.go(h: H, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, c: B.Cur, acc: List<&2, Frame>, left: U32, done: Bool) -> Tex:
  match done:
    case True{}:
      parse.fr.end(h, imgs, sw, sh, sv, acc, B.fork_err(c))
    case False{}:
      parse.fr.step(h, imgs, sw, sh, sv, c, acc, left)

@unsafe
def parse.fr.end(h: H, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, acc: List<&2, Frame>, r: B.Cur & U32) -> Tex:
  (c, err) = r
  pack(h, imgs, sw, sh, sv, rev_fr(acc, Nil{}), err)

@unsafe
def parse.fr.next(h: H, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, acc: List<&2, Frame>, +left: U32, r: B.Cur & Frame) -> Tex:
  (c, fr) = r
  +j = (left - 1 : U32)
  parse.fr.go(h, imgs, sw, sh, sv, c, Con{fr, acc}, j, U32.is_zero(j))

@unsafe
def parse.fr.step(h: H, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, c: B.Cur, acc: List<&2, Frame>, left: U32) -> Tex:
  parse.fr.next(h, imgs, sw, sh, sv, acc, left, read_frame(c))

@unsafe
def parse.frames(h: H, imgs: List<&2, IM>, sw: U32, sh: U32, sv: U32, c: B.Cur, +n: U32) -> Tex:
  parse.fr.go(h, imgs, sw, sh, sv, c, Nil{}, n, U32.is_zero(n))

@unsafe
def parse.sh(h: H, imgs: List<&2, IM>, sv: U32, n: U32, sw: U32, r: B.Cur & U32) -> Tex:
  (c, sh) = r
  parse.frames(h, imgs, sw, sh, sv, c, n)

@unsafe
def parse.sw(h: H, imgs: List<&2, IM>, sv: U32, n: U32, r: B.Cur & U32) -> Tex:
  (c, sw) = r
  parse.sh(h, imgs, sv, n, sw, B.u32(c))

@unsafe
def parse.sheet(h: H, imgs: List<&2, IM>, +iw: U32, +ih: U32, sv: U32, c: B.Cur, n: U32, ge: Bool) -> Tex:
  match ge:
    case False{}:
      parse.frames(h, imgs, iw, ih, sv, c, n)
    case True{}:
      parse.sw(h, imgs, sv, n, B.u32(c))

@unsafe
def parse.nf.cap(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, +sv: U32, c: B.Cur, n: U32, huge: Bool) -> Tex:
  match huge:
    case True{}:
      pack(h, imgs, 0, 0, sv, Nil{}, err_or(B.fail(c, 1), 1))
    case False{}:
      parse.sheet(h, imgs, iw, ih, sv, c, n, U32.is_ge(sv, 3))

@unsafe
def parse.nf(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, +sv: U32, r: B.Cur & U32) -> Tex:
  (c, +n) = r
  parse.nf.cap(h, imgs, iw, ih, sv, c, n, U32.is_gt(n, 4096))

@unsafe
def parse.texs.ok(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, c: B.Cur, sv: U32, ok: Bool) -> Tex:
  match ok:
    case False{}:
      pack(h, imgs, 0, 0, 0, Nil{}, err_or(B.fail(c, 2), 2))
    case True{}:
      parse.nf(h, imgs, iw, ih, sv, B.u32(c))

@unsafe
def parse.texs.tg(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, tg: Tg) -> Tex:
  Tg{c, +k, +sv} = tg
  parse.texs.ok(h, imgs, iw, ih, c, sv, Bool.and(U32.is_eq(k, 4), Bool.and(U32.is_ge(sv, 1), U32.is_le(sv, 3))))

@unsafe
def parse.texs(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, c: B.Cur) -> Tex:
  parse.texs.tg(h, imgs, iw, ih, take_tag(c))

@unsafe
def parse.nosprite(h: H, imgs: List<&2, IM>, missing: Bool) -> Tex:
  match missing:
    case True{}:
      pack(h, imgs, 0, 0, 0, Nil{}, 1)
    case False{}:
      pack(h, imgs, 0, 0, 0, Nil{}, 0)

@unsafe
def parse.rest5(+h: H, +imgs: List<&2, IM>, iw: U32, ih: U32, c: B.Cur, zero: Bool) -> Tex:
  match zero:
    case True{}:
      parse.nosprite(h, imgs, flag4(h))
    case False{}:
      parse.texs(h, imgs, iw, ih, c)

@unsafe
def parse.rest4(+h: H, imgs: List<&2, IM>, iw: U32, ih: U32, c: B.Cur, n: U32, e: U32, bad: Bool) -> Tex:
  match bad:
    case True{}:
      pack(h, imgs, 0, 0, 0, Nil{}, e)
    case False{}:
      parse.rest5(h, imgs, iw, ih, c, U32.is_zero(n))

@unsafe
def parse.rest.err(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, n: U32, r: B.Cur & U32) -> Tex:
  (c, +e) = r
  parse.rest4(h, imgs, iw, ih, c, n, e, U32.is_ne(e, 0))

def cur_rest.pair(c: B.Cur, n: U32) -> B.Cur & U32:
  (c, n)

def cur_rest(c: B.Cur) -> B.Cur & U32:
  B.Cur{+rest, i, +err} = c
  cur_rest.pair(B.Cur{rest, i, err}, B.ulen(rest, 0))

@unsafe
def parse.rest3(h: H, imgs: List<&2, IM>, iw: U32, ih: U32, r: B.Cur & U32) -> Tex:
  (c, n) = r
  parse.rest.err(h, imgs, iw, ih, n, B.fork_err(c))

@unsafe
def parse.rest2(h: H, imgs: List<&2, IM>, +iw: U32, +ih: U32, c: B.Cur) -> Tex:
  parse.rest3(h, imgs, iw, ih, cur_rest(c))

@unsafe
def dims(h: H) -> U32 & U32:
  H{ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond} = h
  (iw, ih)

@unsafe
def parse.rest.dims(h: H, imgs: List<&2, IM>, c: B.Cur, r: U32 & U32) -> Tex:
  (iw, ih) = r
  parse.rest2(h, imgs, iw, ih, c)

@unsafe
def parse.rest(+h: H, imgs: List<&2, IM>, c: B.Cur) -> Tex:
  parse.rest.dims(h, imgs, c, dims(h))

@unsafe
def parse.imgs.done(h: H, acc: Acc) -> Tex:
  Acc{c, imgs} = acc
  parse.rest(h, rev_im(imgs, Nil{}), c)

@unsafe
def parse.imgs.go(h: H, bv: U32, vol: U32, acc: Acc, left: U32, done: Bool) -> Tex:
  match done:
    case True{}:
      parse.imgs.done(h, acc)
    case False{}:
      parse.imgs.step(h, bv, vol, acc, left)

@unsafe
def parse.imgs.next(h: H, bv: U32, vol: U32, imgs: List<&2, IM>, +left: U32, r: B.Cur & IM) -> Tex:
  (c, im) = r
  +j = (left - 1 : U32)
  parse.imgs.go(h, bv, vol, Acc{c, Con{im, imgs}}, j, U32.is_zero(j))

@unsafe
def parse.imgs.step(h: H, +bv: U32, +vol: U32, acc: Acc, left: U32) -> Tex:
  Acc{c, imgs} = acc
  parse.imgs.next(h, bv, vol, imgs, left, read_img(c, bv, vol))

@unsafe
def parse.imgs(+h: H, +bv: U32, +n: U32, c: B.Cur) -> Tex:
  parse.imgs.go(h, bv, vol_of(flags_of(h)), Acc{c, Nil{}}, n, U32.is_zero(n))

def flags_of(h: H) -> U32:
  H{ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond} = h
  flags

@unsafe
def parse.cond.bad(h: H, bv: U32, n: U32, c: B.Cur, cond: U32, badc: Bool) -> Tex:
  match badc:
    case True{}:
      pack(h, Nil{}, 0, 0, 0, Nil{}, err_or(B.fail(c, 5), 5))
    case False{}:
      parse.imgs(h, bv, n, c)

@unsafe
def parse.cond.got(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, +bv: U32, fif: U32, n: U32, r: B.Cur & U32) -> Tex:
  (c, +cond) = r
  parse.cond.bad(mkH(ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond), bv, n, c, cond, U32.is_ne(cond, 0))

@unsafe
def parse.cond2(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, +bv: U32, fif: U32, n: U32, c: B.Cur, ge: Bool) -> Tex:
  match ge:
    case False{}:
      parse.imgs(mkH(ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, 0), bv, n, c)
    case True{}:
      parse.cond.got(ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, n, B.u32(c))

@unsafe
def parse.cond(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, +bv: U32, fif: U32, n: U32, c: B.Cur) -> Tex:
  parse.cond2(ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, n, c, U32.is_ge(bv, 4))

@unsafe
def parse.fif.got(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, n: U32, r: B.Cur & U32) -> Tex:
  (c, fif) = r
  parse.cond(ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, n, c)

@unsafe
def parse.fif2(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, +bv: U32, n: U32, c: B.Cur, ge: Bool) -> Tex:
  match ge:
    case False{}:
      parse.cond(ver, format, flags, tw, th, iw, ih, depth, unk, bv, 4294967295, n, c)
    case True{}:
      parse.fif.got(ver, format, flags, tw, th, iw, ih, depth, unk, bv, n, B.u32(c))

@unsafe
def parse.fif(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, +bv: U32, n: U32, c: B.Cur) -> Tex:
  parse.fif2(ver, format, flags, tw, th, iw, ih, depth, unk, bv, n, c, U32.is_ge(bv, 3))

@unsafe
def parse.nimg.cap(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, c: B.Cur, n: U32, huge: Bool) -> Tex:
  match huge:
    case True{}:
      bad(B.fail(c, 1), ver)
    case False{}:
      parse.fif(ver, format, flags, tw, th, iw, ih, depth, unk, bv, n, c)

@unsafe
def parse.nimg(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, r: B.Cur & U32) -> Tex:
  (c, +n) = r
  parse.nimg.cap(ver, format, flags, tw, th, iw, ih, depth, unk, bv, c, n, U32.is_gt(n, 32))

@unsafe
def parse.texb2(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, c: B.Cur, bv: U32, ok: Bool) -> Tex:
  match ok:
    case False{}:
      bad(B.fail(c, 2), ver)
    case True{}:
      parse.nimg(ver, format, flags, tw, th, iw, ih, depth, unk, bv, B.u32(c))

@unsafe
def parse.texb(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, tg: Tg) -> Tex:
  Tg{c, +k, +bv} = tg
  parse.texb2(ver, format, flags, tw, th, iw, ih, depth, unk, c, bv, Bool.and(U32.is_eq(k, 3), Bool.and(U32.is_ge(bv, 1), U32.is_le(bv, 4))))

@unsafe
def parse.unk.got(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, r: B.Cur & U32) -> Tex:
  (c, unk) = r
  parse.texb(ver, format, flags, tw, th, iw, ih, depth, unk, take_tag(c))

@unsafe
def parse.unk(ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, c: B.Cur, +iv: U32, want: Bool) -> Tex:
  match want:
    case False{}:
      parse.texb(ver, format, flags, tw, th, iw, ih, depth, 0, take_tag(c))
    case True{}:
      parse.unk.got(ver, format, flags, tw, th, iw, ih, depth, B.u32(c))

@unsafe
def parse.depth.ok(ver: U32, +iv: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, c: B.Cur, +d: U32, ok: Bool) -> Tex:
  match ok:
    case False{}:
      bad(B.fail(c, 4), ver)
    case True{}:
      parse.unk(ver, format, flags, tw, th, iw, ih, d, c, iv, U32.is_gt(iv, 0))

@unsafe
def parse.depth.got(ver: U32, iv: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, r: B.Cur & U32) -> Tex:
  (c, +d) = r
  parse.depth.ok(ver, iv, format, flags, tw, th, iw, ih, c, d, Spec.depth_ok(d))

@unsafe
def parse.depth(ver: U32, +iv: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, c: B.Cur, vol: Bool) -> Tex:
  match vol:
    case False{}:
      parse.unk(ver, format, flags, tw, th, iw, ih, 1, c, iv, U32.is_gt(iv, 0))
    case True{}:
      parse.depth.got(ver, iv, format, flags, tw, th, iw, ih, B.u32(c))

@unsafe
def parse.res2(ver: U32, +iv: U32, format: U32, +flags: U32, tw: U32, th: U32, iw: U32, ih: U32, c: B.Cur, ok: Bool) -> Tex:
  match ok:
    case False{}:
      bad(B.fail(c, 3), ver)
    case True{}:
      parse.depth(ver, iv, format, flags, tw, th, iw, ih, c, U32.is_ne(U32.and(flags, 64), 0))

@unsafe
def parse.res(ver: U32, iv: U32, format: U32, flags: U32, +tw: U32, +th: U32, +iw: U32, +ih: U32, c: B.Cur) -> Tex:
  parse.res2(ver, iv, format, flags, tw, th, iw, ih, c, Bool.and(Spec.res_ok(tw, th), Spec.res_ok(iw, ih)))

@unsafe
def parse.ih(ver: U32, iv: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, r: B.Cur & U32) -> Tex:
  (c, ih) = r
  parse.res(ver, iv, format, flags, tw, th, iw, ih, c)

@unsafe
def parse.iw(ver: U32, iv: U32, format: U32, flags: U32, tw: U32, th: U32, r: B.Cur & U32) -> Tex:
  (c, iw) = r
  parse.ih(ver, iv, format, flags, tw, th, iw, B.u32(c))

@unsafe
def parse.th(ver: U32, iv: U32, format: U32, flags: U32, tw: U32, r: B.Cur & U32) -> Tex:
  (c, th) = r
  parse.iw(ver, iv, format, flags, tw, th, B.u32(c))

@unsafe
def parse.tw(ver: U32, iv: U32, format: U32, flags: U32, r: B.Cur & U32) -> Tex:
  (c, tw) = r
  parse.th(ver, iv, format, flags, tw, B.u32(c))

@unsafe
def parse.flags(ver: U32, iv: U32, format: U32, r: B.Cur & U32) -> Tex:
  (c, flags) = r
  parse.tw(ver, iv, format, flags, B.u32(c))

@unsafe
def parse.fmt(ver: U32, iv: U32, r: B.Cur & U32) -> Tex:
  (c, format) = r
  parse.flags(ver, iv, format, B.u32(c))

@unsafe
def parse.texi2(ver: U32, c: B.Cur, +iv: U32, ok: Bool) -> Tex:
  match ok:
    case False{}:
      bad(B.fail(c, 2), ver)
    case True{}:
      parse.fmt(ver, iv, B.u32(c))

@unsafe
def parse.texi(ver: U32, tg: Tg) -> Tex:
  Tg{c, k, iv} = tg
  parse.texi2(ver, c, iv, U32.is_eq(k, 2))

def texv_ok(+v: U32) -> Bool:
  Bool.or(U32.is_eq(v, 4), U32.is_eq(v, 5))

@unsafe
def parse.texv.bad(c: B.Cur, v: U32, isv: Bool) -> Tex:
  match isv:
    case True{}:
      bad(B.fail(c, 6), v)
    case False{}:
      bad(B.fail(c, 2), 0)

@unsafe
def parse.texv2(c: B.Cur, +k: U32, +v: U32, ok: Bool) -> Tex:
  match ok:
    case True{}:
      parse.texi(v, take_tag(c))
    case False{}:
      parse.texv.bad(c, v, U32.is_eq(k, 1))

@unsafe
def parse.texv(tg: Tg) -> Tex:
  Tg{c, +k, +v} = tg
  parse.texv2(c, k, v, Bool.and(U32.is_eq(k, 1), texv_ok(v)))

@unsafe
def parse(bs: List<&2, U32>) -> Tex:
  parse.texv(take_tag(B.cur(bs)))

@unsafe
def exp8(px: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match px:
    case Con{+v, Con{a, t}}:
      exp8(t, Con{a, Con{v, Con{v, Con{v, acc}}}})
    case Nil{}:
      rev_u(acc, Nil{})
    case _:
      Nil{}

@unsafe
def exp9(px: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match px:
    case Nil{}:
      rev_u(acc, Nil{})
    case Con{+v, t}:
      exp9(t, Con{255, Con{v, Con{v, Con{v, acc}}}})

@unsafe
def expand.r8(px: List<&2, U32>, is9: Bool) -> List<&2, U32>:
  match is9:
    case True{}:
      exp9(px, Nil{})
    case False{}:
      px

@unsafe
def expand.rg(fmt: U32, px: List<&2, U32>, is8: Bool) -> List<&2, U32>:
  match is8:
    case True{}:
      exp8(px, Nil{})
    case False{}:
      expand.r8(px, U32.is_eq(fmt, 9))

@unsafe
def expand.go(+fmt: U32, px: List<&2, U32>, is0: Bool) -> List<&2, U32>:
  match is0:
    case True{}:
      px
    case False{}:
      expand.rg(fmt, px, U32.is_eq(fmt, 8))

@unsafe
def expand(+fmt: U32, px: List<&2, U32>) -> List<&2, U32>:
  expand.go(fmt, px, U32.is_eq(fmt, 0))
