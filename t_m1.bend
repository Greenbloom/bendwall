import Base
import ./src/pkg.bend as P
import ./src/tex.bend as T
import ./src/lz4.bend as L
import ./src/bytes.bend as B
import ./src/img.bend as I

def nyb(n: U32, hi: Bool) -> String:
  match hi:
    case True{}:
      SCon{Chr{(n + 87 : U32)}, SNil{}}
    case False{}:
      SCon{Chr{(n + 48 : U32)}, SNil{}}

def hex2(+b: U32) -> String:
  +hi = (b >> 4n : U32)
  +lo = U32.and(b, 15)
  nyb(hi, U32.is_ge(hi, 10)) ++ nyb(lo, U32.is_ge(lo, 10))

@unsafe
def hexn(xs: List<&2, U32>, n: U32, acc: String, done: Bool) -> String:
  match done:
    case True{}:
      acc
    case False{}:
      hexn.step(xs, n, acc)

@unsafe
def hexn.step(xs: List<&2, U32>, n: U32, acc: String) -> String:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      +j = (n - 1 : U32)
      hexn(t, j, acc ++ hex2(h), U32.is_zero(j))

def show_i32.go(u: U32, neg: Bool) -> String:
  match neg:
    case False{}:
      U32.show(u)
    case True{}:
      "-" ++ U32.show((U32.not(u) + 1 : U32))

def show_i32(+u: U32) -> String:
  show_i32.go(u, U32.is_ge(u, 2147483648))

def ent1(e: P.Entry) -> String:
  P.Entry{name, off, sz} = e
  name ++ "@" ++ U32.show(off) ++ "@" ++ U32.show(sz)

@unsafe
def ents.more(e: P.Entry, t: List<&2, P.Entry>) -> String:
  match t:
    case Nil{}:
      ent1(e)
    case _:
      ent1(e) ++ "," ++ ents(t)

@unsafe
def ents(es: List<&2, P.Entry>) -> String:
  match es:
    case Nil{}:
      SNil{}
    case Con{e, t}:
      ents.more(e, t)

def show_pkg.p(path: String, n: U32, p: P.Pkg) -> String:
  P.Pkg{ver, start, entries, err} = p
  "PKG path=" ++ path ++ " err=" ++ U32.show(err) ++ " ver=" ++ U32.show(ver) ++ " start=" ++ U32.show(start) ++ " n=" ++ U32.show(n) ++ " ents=" ++ ents(entries)

def show_pkg(path: String, +bs: List<&2, U32>) -> String:
  show_pkg.p(path, B.ulen(bs, 0), P.parse(bs))

def n_im(xs: List<&2, T.IM>, acc: U32) -> U32:
  match xs:
    case Nil{}:
      acc
    case Con{_, t}:
      n_im(t, (acc + 1 : U32))

def head_im(xs: List<&2, T.IM>) -> T.IM:
  match xs:
    case Con{im, _}:
      im
    case Nil{}:
      T.IM{0, T.mip0()}

@unsafe
def fr1(f: T.Frame) -> String:
  T.Frame{img, tb, x, y, w, wy, hx, h} = f
  U32.show(img) ++ "," ++ U32.show(tb) ++ "," ++ U32.show(x) ++ "," ++ U32.show(y) ++ "," ++ U32.show(w) ++ "," ++ U32.show(wy) ++ "," ++ U32.show(hx) ++ "," ++ U32.show(h)

@unsafe
def frs.more(f: T.Frame, t: List<&2, T.Frame>) -> String:
  match t:
    case Nil{}:
      fr1(f)
    case _:
      fr1(f) ++ "|" ++ frs(t)

@unsafe
def frs(xs: List<&2, T.Frame>) -> String:
  match xs:
    case Nil{}:
      SNil{}
    case Con{f, t}:
      frs.more(f, t)

def show_tex.px(path: String, err: U32, ver: U32, +format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, fif: U32, cond: U32, ni: U32, sw: U32, sh: U32, sv: U32, frames: List<&2, T.Frame>, nm: U32, w: U32, h: U32, d: U32, lz: U32, raw: U32, csz: U32, +px: List<&2, U32>) -> String:
  "TEX path=" ++ path ++ " err=" ++ U32.show(err) ++ " ver=" ++ U32.show(ver) ++ " fmt=" ++ U32.show(format) ++ " flags=" ++ U32.show(flags) ++ " tw=" ++ U32.show(tw) ++ " th=" ++ U32.show(th) ++ " iw=" ++ U32.show(iw) ++ " ih=" ++ U32.show(ih) ++ " dep=" ++ U32.show(depth) ++ " unk=" ++ U32.show(unk) ++ " bv=" ++ U32.show(bv) ++ " fif=" ++ show_i32(fif) ++ " cond=" ++ U32.show(cond) ++ " ni=" ++ U32.show(ni) ++ " nm=" ++ U32.show(nm) ++ " mw=" ++ U32.show(w) ++ " mh=" ++ U32.show(h) ++ " md=" ++ U32.show(d) ++ " lz=" ++ U32.show(lz) ++ " raw=" ++ U32.show(raw) ++ " csz=" ++ U32.show(csz) ++ " plen=" ++ U32.show(B.ulen(px, 0)) ++ " px=" ++ hexn(px, 48, SNil{}, U32.is_zero(48)) ++ " exp=" ++ hexn(T.expand(format, px), 8, SNil{}, False{}) ++ " sw=" ++ U32.show(sw) ++ " sh=" ++ U32.show(sh) ++ " sv=" ++ U32.show(sv) ++ " fr=" ++ frs(frames)

def show_tex.m(path: String, err: U32, ver: U32, format: U32, flags: U32, tw: U32, th: U32, iw: U32, ih: U32, depth: U32, unk: U32, bv: U32, fif: U32, cond: U32, ni: U32, sw: U32, sh: U32, sv: U32, frames: List<&2, T.Frame>, im: T.IM) -> String:
  T.IM{nm, mip} = im
  T.Mip{w, h, d, lz, raw, csz, px} = mip
  show_tex.px(path, err, ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond, ni, sw, sh, sv, frames, nm, w, h, d, lz, raw, csz, px)

def show_tex.t(path: String, tex: T.Tex) -> String:
  T.Tex{err, ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond, +imgs, sw, sh, sv, frames} = tex
  show_tex.m(path, err, ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond, n_im(imgs, 0), sw, sh, sv, frames, head_im(imgs))

def show_tex(path: String, bs: List<&2, U32>) -> String:
  show_tex.t(path, T.parse(bs))

def blob.m(im: T.IM) -> List<&2, U32>:
  T.IM{n, mip} = im
  T.Mip{w, h, d, lz, raw, csz, px} = mip
  px

def blob.t(tex: T.Tex) -> List<&2, U32>:
  T.Tex{err, ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond, imgs, sw, sh, sv, frames} = tex
  blob.m(head_im(imgs))

def blob(bs: List<&2, U32>) -> List<&2, U32>:
  blob.t(T.parse(bs))

def le4(xs: List<&2, U32>) -> U32 & List<&2, U32>:
  match xs:
    case Con{a, Con{b, Con{c, Con{d, rest}}}}:
      ((a + (b << 8n) + (c << 16n) + (d << 24n) : U32), rest)
    case _:
      (0, Nil{})

def show_img.h(path: String, w: U32, r: U32 & List<&2, U32>) -> String:
  (h, +px) = r
  "IMG path=" ++ path ++ " w=" ++ U32.show(w) ++ " h=" ++ U32.show(h) ++ " plen=" ++ U32.show(B.ulen(px, 0)) ++ " px=" ++ hexn(px, 64, SNil{}, False{})

def show_img.w(path: String, r: U32 & List<&2, U32>) -> String:
  (w, rest) = r
  show_img.h(path, w, le4(rest))

def show_img(path: String, xs: List<&2, U32>) -> String:
  show_img.w(path, le4(xs))

def show_lz(name: String, d: L.Dec) -> String:
  L.Dec{ok, +op, px} = d
  "LZ4 name=" ++ name ++ " ok=" ++ U32.show(ok) ++ " n=" ++ U32.show(op) ++ " px=" ++ hexn(px, 32, SNil{}, U32.is_zero(op))

def lz4_lines() -> String:
  show_lz("match", L.decode([96, 97, 98, 99, 100, 101, 102, 6, 0], 10)) ++ "\n" ++ show_lz("lit", L.decode([240, 1, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7], 16))

def rev_s(xs: String, acc: String) -> String:
  match xs:
    case SNil{}:
      acc
    case SCon{c, t}:
      rev_s(t, SCon{c, acc})

@unsafe
def split.go(s: String, acc: String) -> String & String:
  match s:
    case SNil{}:
      (rev_s(acc, SNil{}), SNil{})
    case SCon{Chr{10}, rest}:
      (rev_s(acc, SNil{}), rest)
    case SCon{c, rest}:
      split.go(rest, SCon{c, acc})

def split(s: String) -> String & String:
  split.go(s, SNil{})

def cmd.of(c: U32, rest: String) -> U32 & String:
  (c, rest)

def cmd(s: String) -> U32 & String:
  match s:
    case SCon{Chr{c}, SCon{Chr{32}, rest}}:
      cmd.of(c, rest)
    case SCon{Chr{c}, SNil{}}:
      cmd.of(c, SNil{})
    case _:
      cmd.of(0, s)

def empty_s(s: String) -> Bool:
  match s:
    case SNil{}:
      True{}
    case _:
      False{}

@unsafe
def drive(s: String) -> IO(Unit):
  drive.p(split(s))

@unsafe
def drive.p(p: String & String) -> IO(Unit):
  (+line, +rest) = p
  drive.line(line, rest, Bool.and(empty_s(line), empty_s(rest)))

@unsafe
def drive.line(+line: String, rest: String, stop: Bool) -> IO(Unit):
  match stop:
    case True{}:
      IO.pure(Unit, Unit{})
    case False{}:
      drive.skip(line, rest, empty_s(line))

@unsafe
def drive.skip(line: String, rest: String, blank: Bool) -> IO(Unit):
  match blank:
    case True{}:
      drive(rest)
    case False{}:
      drive.cmd(cmd(line), rest)

@unsafe
def drive.cmd(p: U32 & String, rest: String) -> IO(Unit):
  (c, path) = p
  do IO<Unit>:
    _u : Unit <- run(c, path)
    drive(rest)

@unsafe
def loadb.got(m: File & Result<&1, &1, U32 & String, List<&2, U32>>, path: String, kind: U32) -> IO(Unit):
  (f, r) = m
  do IO<Unit>:
    bs : List<&2, U32> <- IO.pass(List<&2, U32>, r)
    File.close(f)
    emit(path, bs, kind)

@unsafe
def emit(path: String, +bs: List<&2, U32>, +kind: U32) -> IO(Unit):
  emit.go(path, bs, kind, U32.is_eq(kind, 73))

@unsafe
def emit.go(path: String, +bs: List<&2, U32>, kind: U32, img: Bool) -> IO(Unit):
  match img:
    case True{}:
      emit.img(path, bs)
    case False{}:
      emit.one(path, bs, kind)

@unsafe
def emit.one(path: String, bs: List<&2, U32>, kind: U32) -> IO(Unit):
  emit.one2(path, bs, U32.is_eq(kind, 80))

@unsafe
def emit.one2(path: String, bs: List<&2, U32>, pkg: Bool) -> IO(Unit):
  match pkg:
    case True{}:
      IO.print(show_pkg(path, bs))
    case False{}:
      IO.print(show_tex(path, bs))

@unsafe
def emit.img(+path: String, +bs: List<&2, U32>) -> IO(Unit):
  do IO<Unit>:
    IO.print(show_tex(path, bs))
    px : List<&2, U32> <- I.Img.rgba(blob(bs))
    IO.print(show_img(path, px))

@unsafe
def run(+c: U32, path: String) -> IO(Unit):
  run.l(c, path, U32.is_eq(c, 76))

@unsafe
def run.l(c: U32, path: String, is_l: Bool) -> IO(Unit):
  match is_l:
    case True{}:
      IO.print(lz4_lines())
    case False{}:
      run.file(path, c)

@unsafe
def run.file(+path: String, kind: U32) -> IO(Unit):
  do IO<Unit>:
    f : File <- IO.try(File, File.open(path, "r"))
    m : File & Result<&1, &1, U32 & String, List<&2, U32>> <- File.read_bytes(f, 2000000)
    loadb.got(m, path, kind)

def loads.got(m: File & Result<&1, &1, U32 & String, String>) -> IO(Unit):
  (f, r) = m
  do IO<Unit>:
    s : String <- IO.pass(String, r)
    File.close(f)
    drive(s)

def arg1(xs: List<String>) -> String:
  match xs:
    case Con{_, Con{p, _}}:
      p
    case _:
      "fixtures/m1/manifest.txt"

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    f : File <- IO.try(File, File.open(arg1(args), "r"))
    m : File & Result<&1, &1, U32 & String, String> <- File.read(f, 200000)
    loads.got(m)
