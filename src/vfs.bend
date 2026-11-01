import Base
import ./pkg.bend as Pkg
import ./str.bend as S

# Scene and asset lookup. A loose file wins, then <basename>.pkg (scene.pkg,
# gifscene.pkg), then BENDWALL_ASSETS. Pkg names are matched case-insensitively.
# Offsets are relative to the byte after the entry table. A read is capped at
# 2000000 bytes, so a later .tex must not blank out an earlier scene.json:
# a member that sits in the cap is sliced, and one past it is read at its offset.

type Got is Data:
  Got{ok: U32, bs: List<&2, U32>}

@unsafe
def pair(s: String) -> String & String:
  match s:
    case SNil{}:
      (SNil{}, SNil{})
    case SCon{Chr{c}, t}:
      pair.c(c, pair(t))

@unsafe
def pair.c(+c: U32, p: String & String) -> String & String:
  (a, b) = p
  (SCon{Chr{c}, a}, SCon{Chr{c}, b})

@unsafe
def empty(s: String) -> Bool & String:
  match s:
    case SNil{}:
      (True{}, SNil{})
    case SCon{c, t}:
      (False{}, SCon{c, t})

@unsafe
def rev(s: String) -> String:
  rev.go(s, SNil{})

@unsafe
def rev.go(s: String, acc: String) -> String:
  match s:
    case SNil{}:
      acc
    case SCon{c, t}:
      rev.go(t, SCon{c, acc})

@unsafe
def starts(s: String, p: String) -> Bool:
  match p:
    case SNil{}:
      True{}
    case SCon{Chr{c}, t}:
      starts.c(s, t, c)

@unsafe
def starts.c(s: String, t: String, +c: U32) -> Bool:
  match s:
    case SNil{}:
      False{}
    case SCon{Chr{d}, r}:
      starts.eq(r, t, U32.is_eq(c, d))

@unsafe
def starts.eq(s: String, p: String, eq: Bool) -> Bool:
  match eq:
    case True{}:
      starts(s, p)
    case False{}:
      False{}

@unsafe
def ends_json(s: String) -> Bool:
  starts(rev(s), "nosj.")

@unsafe
def drop_chars(s: String, +n: U32) -> String:
  drop_chars.m(s, n, U32.is_zero(n))

@unsafe
def drop_chars.m(s: String, +n: U32, z: Bool) -> String:
  match z:
    case True{}:
      s
    case False{}:
      drop_chars.s(s, n)

@unsafe
def drop_chars.s(s: String, +n: U32) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{_, t}:
      drop_chars(t, (n - 1 : U32))

@unsafe
def drop_last(s: String, +n: U32) -> String:
  rev(drop_chars(rev(s), n))

@unsafe
def pkg_name(file: String) -> String:
  pkg_name.d(pair(file))

@unsafe
def pkg_name.d(p: String & String) -> String:
  (a, b) = p
  pkg_name.m(b, ends_json(a))

@unsafe
def pkg_name.m(file: String, yes: Bool) -> String:
  match yes:
    case True{}:
      drop_last(file, 5) ++ ".pkg"
    case False{}:
      file ++ ".pkg"

@unsafe
def norm_file(file: String) -> String:
  norm_file.e(empty(file))

@unsafe
def norm_file.e(p: Bool & String) -> String:
  (e, file) = p
  norm_file.m(file, e)

@unsafe
def norm_file.m(file: String, e: Bool) -> String:
  match e:
    case True{}:
      "scene.json"
    case False{}:
      file

@unsafe
def dupbs(xs: List<&2, U32>) -> List<&2, U32> & List<&2, U32>:
  match xs:
    case Nil{}:
      (Nil{}, Nil{})
    case Con{h, t}:
      dupbs.c(h, dupbs(t))

@unsafe
def dupbs.c(+h: U32, p: List<&2, U32> & List<&2, U32>) -> List<&2, U32> & List<&2, U32>:
  (a, b) = p
  (Con{h, a}, Con{h, b})

@unsafe
def revbs(xs: List<&2, U32>) -> List<&2, U32>:
  revbs.go(xs, Nil{})

@unsafe
def revbs.go(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      revbs.go(t, Con{h, acc})

@unsafe
def drop_n(xs: List<&2, U32>, +n: U32) -> List<&2, U32>:
  drop_n.m(xs, n, U32.is_zero(n))

@unsafe
def drop_n.m(xs: List<&2, U32>, +n: U32, z: Bool) -> List<&2, U32>:
  match z:
    case True{}:
      xs
    case False{}:
      drop_n.s(xs, n)

@unsafe
def drop_n.s(xs: List<&2, U32>, +n: U32) -> List<&2, U32>:
  match xs:
    case Nil{}:
      Nil{}
    case Con{_, t}:
      drop_n(t, (n - 1 : U32))

@unsafe
def take_acc(xs: List<&2, U32>, +n: U32, acc: List<&2, U32>) -> List<&2, U32>:
  take_acc.m(xs, n, acc, U32.is_zero(n))

@unsafe
def take_acc.m(xs: List<&2, U32>, +n: U32, acc: List<&2, U32>, z: Bool) -> List<&2, U32>:
  match z:
    case True{}:
      acc
    case False{}:
      take_acc.s(xs, n, acc)

@unsafe
def take_acc.s(xs: List<&2, U32>, +n: U32, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      take_acc(t, (n - 1 : U32), Con{h, acc})

@unsafe
def take_n(xs: List<&2, U32>, +n: U32) -> List<&2, U32>:
  revbs(take_acc(xs, n, Nil{}))

@unsafe
def take_at(xs: List<&2, U32>, +at: U32, +n: U32) -> List<&2, U32>:
  take_n(drop_n(xs, at), n)

@unsafe
def drop_s(s: String) -> U32:
  match s:
    case SNil{}:
      0
    case SCon{_, t}:
      drop_s(t)

@unsafe
def drop_all(xs: List<&2, U32>) -> U32:
  match xs:
    case Nil{}:
      0
    case Con{_, t}:
      drop_all(t)

@unsafe
def miss_got(xs: List<&2, U32>) -> Got:
  miss_got.n(drop_all(xs))

@unsafe
def miss_got.n(+n: U32) -> Got:
  Got{0, Nil{}}

@unsafe
def member_bad(bs: List<&2, U32>, +at: U32, +size: U32, bad: Bool) -> Got:
  match bad:
    case True{}:
      miss_got(bs)
    case False{}:
      Got{1, take_at(bs, at, size)}

# mode 0: no such member. mode 1: the bytes are in `bs`. mode 2: `at`/`size`
# are absolute, but those bytes were past the bytes we held.
type Spot is Data:
  Spot{mode: U32, at: U32, size: U32, bs: List<&2, U32>}

@unsafe
def measure(xs: List<&2, U32>, acc: List<&2, U32>, +n: U32) -> List<&2, U32> & U32:
  match xs:
    case Nil{}:
      (revbs(acc), n)
    case Con{h, t}:
      measure(t, Con{h, acc}, (n + 1 : U32))

@unsafe
def member_full(xs: List<&2, U32>, +at: U32, +size: U32) -> Spot:
  member_full.r(measure(xs, Nil{}, 0), at, size)

@unsafe
def member_full.r(p: List<&2, U32> & U32, +at: U32, +size: U32) -> Spot:
  (xs, n) = p
  member_full.e(xs, at, size, U32.is_eq(n, size))

@unsafe
def member_full.e(xs: List<&2, U32>, +at: U32, +size: U32, eq: Bool) -> Spot:
  match eq:
    case True{}:
      Spot{1, at, size, xs}
    case False{}:
      member_full.miss(xs, at, size)

@unsafe
def member_full.miss(xs: List<&2, U32>, +at: U32, +size: U32) -> Spot:
  member_full.missn(at, size, drop_all(xs))

@unsafe
def member_full.missn(+at: U32, +size: U32, +n: U32) -> Spot:
  Spot{2, at, size, Nil{}}

@unsafe
def member_none(bs: List<&2, U32>) -> Spot:
  member_none.n(drop_all(bs))

@unsafe
def member_none.n(+n: U32) -> Spot:
  Spot{0, 0, 0, Nil{}}

@unsafe
def member_pick(bs: List<&2, U32>, +at: U32, +size: U32, +ok: U32) -> Spot:
  member_pick.m(bs, at, size, U32.is_zero(ok))

@unsafe
def member_pick.m(bs: List<&2, U32>, +at: U32, +size: U32, miss: Bool) -> Spot:
  match miss:
    case True{}:
      member_none(bs)
    case False{}:
      member_full(take_at(bs, at, size), at, size)

@unsafe
def member_ok(bs: List<&2, U32>, +start: U32, +off: U32, +size: U32, +err: U32, +ok: U32) -> Got:
  member_ok.s(member_pick(bs, (start + off : U32), size, ok))

@unsafe
def member_ok.s(s: Spot) -> Got:
  Spot{mode, at, size, bs} = s
  member_ok.m(bs, U32.is_eq(mode, 1))

@unsafe
def member_ok.m(bs: List<&2, U32>, have: Bool) -> Got:
  match have:
    case True{}:
      Got{1, bs}
    case False{}:
      miss_got(bs)

@unsafe
def member_h(bs: List<&2, U32>, +start: U32, +err: U32, h: Pkg.Hit) -> Got:
  Pkg.Hit{ok, off, size} = h
  member_ok(bs, start, off, size, err, ok)

@unsafe
def member_p(bs: List<&2, U32>, name: String, p: Pkg.Pkg) -> Got:
  Pkg.Pkg{ver, start, entries, err} = p
  member_h(bs, start, err, Pkg.find(name, entries))

@unsafe
def member_d(name: String, p: List<&2, U32> & List<&2, U32>) -> Got:
  (a, b) = p
  member_p(b, S.lower_str(name), Pkg.parse(a))

@unsafe
def member_got(bs: List<&2, U32>, name: String) -> Got:
  member_ok.s(spot(bs, name))

@unsafe
def spot(bs: List<&2, U32>, name: String) -> Spot:
  spot.d(name, dupbs(bs))

@unsafe
def spot.d(name: String, p: List<&2, U32> & List<&2, U32>) -> Spot:
  (a, b) = p
  spot.p(b, S.lower_str(name), Pkg.parse(a))

@unsafe
def spot.p(bs: List<&2, U32>, name: String, p: Pkg.Pkg) -> Spot:
  Pkg.Pkg{ver, start, entries, err} = p
  spot.h(bs, start, err, Pkg.find(name, entries))

@unsafe
def spot.h(bs: List<&2, U32>, +start: U32, +err: U32, h: Pkg.Hit) -> Spot:
  Pkg.Hit{ok, off, size} = h
  member_pick(bs, (start + off : U32), size, ok)

@unsafe
def fetch(path: String, name: String) -> IO(Got):
  fetch.p(pair(path), name)

@unsafe
def fetch.p(ps: String & String, name: String) -> IO(Got):
  (p1, p2) = ps
  do IO<Got>:
    bs : List<&2, U32> <- read_bin(p1)
    g : Got <- fetch.s(p2, spot(bs, name))
    return g

@unsafe
def fetch.s(path: String, s: Spot) -> IO(Got):
  Spot{+mode, at, size, bs} = s
  fetch.z(path, at, size, bs, mode, U32.is_zero(mode))

@unsafe
def fetch.z(path: String, +at: U32, +size: U32, bs: List<&2, U32>, +mode: U32, miss: Bool) -> IO(Got):
  match miss:
    case True{}:
      do IO<Got>:
        return miss_got(bs)
    case False{}:
      fetch.h(path, at, size, bs, U32.is_eq(mode, 1))

@unsafe
def fetch.h(path: String, +at: U32, +size: U32, bs: List<&2, U32>, have: Bool) -> IO(Got):
  match have:
    case True{}:
      do IO<Got>:
        return Got{1, bs}
    case False{}:
      fetch.disk(path, at, size, bs)

@unsafe
def fetch.disk(path: String, +at: U32, +size: U32, bs: List<&2, U32>) -> IO(Got):
  fetch.cap(path, at, size, drop_all(bs), U32.is_gt(size, 2000000))

@unsafe
def fetch.cap(path: String, +at: U32, +size: U32, +gone: U32, huge: Bool) -> IO(Got):
  match huge:
    case True{}:
      do IO<Got>:
        return Got{0, Nil{}}
    case False{}:
      read_at_got(path, at, size)

@unsafe
def read_at_got(path: String, +at: U32, +size: U32) -> IO(Got):
  do IO<Got>:
    r : Result<&1, &1, U32 & String, File> <- File.open(path, "r")
    g : Got <- read_at_open(r, at, size)
    return g

@unsafe
def read_at_open(r: Result<&1, &1, U32 & String, File>, +at: U32, +size: U32) -> IO(Got):
  match r:
    case Fail{_}:
      do IO<Got>:
        return Got{0, Nil{}}
    case Done{f}:
      read_at_file(f, at, size)

@unsafe
def read_at_file(f: File, +at: U32, +size: U32) -> IO(Got):
  do IO<Got>:
    m : File & Result<&1, &1, U32 & String, List<&2, U32>> <- File.read_at(f, at, size)
    g : Got <- read_at_unwrap(m, size)
    return g

@unsafe
def read_at_unwrap(m: File & Result<&1, &1, U32 & String, List<&2, U32>>, +size: U32) -> IO(Got):
  (f, r) = m
  do IO<Got>:
    xs : List<&2, U32> <- IO.pass(List<&2, U32>, r)
    u : Unit <- File.close(f)
    return read_at_full(xs, size)

@unsafe
def read_at_full(xs: List<&2, U32>, +size: U32) -> Got:
  read_at_full.r(measure(xs, Nil{}, 0), size)

@unsafe
def read_at_full.r(p: List<&2, U32> & U32, +size: U32) -> Got:
  (xs, n) = p
  read_at_full.e(xs, U32.is_eq(n, size))

@unsafe
def read_at_full.e(xs: List<&2, U32>, eq: Bool) -> Got:
  match eq:
    case True{}:
      Got{1, xs}
    case False{}:
      miss_got(xs)

@unsafe
def got_text.m(bs: List<&2, U32>, miss: Bool) -> String:
  match miss:
    case True{}:
      miss_text(bs)
    case False{}:
      S.from_bytes(bs)

@unsafe
def miss_text(bs: List<&2, U32>) -> String:
  miss_text.n(drop_all(bs))

@unsafe
def miss_text.n(+n: U32) -> String:
  ""

@unsafe
def got_text(g: Got) -> String:
  Got{ok, bs} = g
  got_text.m(bs, U32.is_zero(ok))

@unsafe
def got_bs(g: Got) -> List<&2, U32>:
  Got{ok, bs} = g
  bs

@unsafe
def member_text(bs: List<&2, U32>, name: String) -> String:
  got_text(member_got(bs, name))

@unsafe
def read_text(path: String) -> IO(String):
  do IO<String>:
    f : File <- IO.try(File, File.open(path, "r"))
    m : File & Result<&1, &1, U32 & String, String> <- File.read(f, 2000000)
    s : String <- unwrap_s(m)
    return s

@unsafe
def unwrap_s(m: File & Result<&1, &1, U32 & String, String>) -> IO(String):
  (f, r) = m
  do IO<String>:
    s : String <- IO.pass(String, r)
    u : Unit <- File.close(f)
    return s

@unsafe
def read_bin(path: String) -> IO(List<&2, U32>):
  do IO<List<&2, U32>>:
    f : File <- IO.try(File, File.open(path, "r"))
    m : File & Result<&1, &1, U32 & String, List<&2, U32>> <- File.read_bytes(f, 2000000)
    xs : List<&2, U32> <- unwrap_b(m)
    return xs

@unsafe
def unwrap_b(m: File & Result<&1, &1, U32 & String, List<&2, U32>>) -> IO(List<&2, U32>):
  (f, r) = m
  do IO<List<&2, U32>>:
    xs : List<&2, U32> <- IO.pass(List<&2, U32>, r)
    u : Unit <- File.close(f)
    return xs

@unsafe
def exists(path: String) -> IO(U32):
  do IO<U32>:
    r : Result<&1, &1, U32 & String, File> <- File.open(path, "r")
    n : U32 <- exists.r(r)
    return n

@unsafe
def exists.r(r: Result<&1, &1, U32 & String, File>) -> IO(U32):
  match r:
    case Fail{_}:
      do IO<U32>:
        return 0
    case Done{f}:
      do IO<U32>:
        u : Unit <- File.close(f)
        return 1

@unsafe
def assets_text(assets: String, rel: String) -> IO(String):
  assets_text.e(rel, empty(assets))

@unsafe
def assets_text.e(rel: String, p: Bool & String) -> IO(String):
  (e, assets) = p
  assets_text.em(rel, assets, e)

@unsafe
def assets_text.em(rel: String, assets: String, e: Bool) -> IO(String):
  match e:
    case True{}:
      do IO<String>:
        return ""
    case False{}:
      assets_text.f(assets, rel)

@unsafe
def assets_text.f(assets: String, rel: String) -> IO(String):
  assets_text.d(pair(assets), pair(rel))

@unsafe
def assets_text.d(pa: String & String, pr: String & String) -> IO(String):
  (a1, a2) = pa
  assets_text.r(a2, pr, a1)

@unsafe
def assets_text.r(a2: String, pr: String & String, a1: String) -> IO(String):
  (r1, r2) = pr
  do IO<String>:
    n : U32 <- exists(a1 ++ "/" ++ r1)
    s : String <- assets_text.n(a2, r2, n)
    return s

@unsafe
def assets_text.n(assets: String, rel: String, +n: U32) -> IO(String):
  assets_text.m(assets, rel, U32.is_zero(n))

@unsafe
def assets_text.m(assets: String, rel: String, miss: Bool) -> IO(String):
  match miss:
    case True{}:
      do IO<String>:
        return ""
    case False{}:
      read_text(assets ++ "/" ++ rel)

@unsafe
def assets_bin(assets: String, rel: String) -> IO(Got):
  assets_bin.e(rel, empty(assets))

@unsafe
def assets_bin.e(rel: String, p: Bool & String) -> IO(Got):
  (e, assets) = p
  assets_bin.em(rel, assets, e)

@unsafe
def assets_bin.em(rel: String, assets: String, e: Bool) -> IO(Got):
  match e:
    case True{}:
      do IO<Got>:
        return Got{0, Nil{}}
    case False{}:
      assets_bin.f(assets, rel)

@unsafe
def assets_bin.f(assets: String, rel: String) -> IO(Got):
  assets_bin.d(pair(assets), pair(rel))

@unsafe
def assets_bin.d(pa: String & String, pr: String & String) -> IO(Got):
  (a1, a2) = pa
  assets_bin.r(a2, pr, a1)

@unsafe
def assets_bin.r(a2: String, pr: String & String, a1: String) -> IO(Got):
  (r1, r2) = pr
  do IO<Got>:
    n : U32 <- exists(a1 ++ "/" ++ r1)
    g : Got <- assets_bin.n(a2, r2, n)
    return g

@unsafe
def assets_bin.n(assets: String, rel: String, +n: U32) -> IO(Got):
  assets_bin.m(assets, rel, U32.is_zero(n))

@unsafe
def assets_bin.m(assets: String, rel: String, miss: Bool) -> IO(Got):
  match miss:
    case True{}:
      do IO<Got>:
        return Got{0, Nil{}}
    case False{}:
      do IO<Got>:
        bs : List<&2, U32> <- read_bin(assets ++ "/" ++ rel)
        return Got{1, bs}

@unsafe
def scene_pkg(dir: String, assets: String, rel: String) -> IO(String):
  scene_pkg.r(dir, assets, pair(rel))

@unsafe
def scene_pkg.r(dir: String, assets: String, p: String & String) -> IO(String):
  (r1, r2) = p
  scene_pkg.d(assets, r2, pair(dir), pkg_name(r1))

@unsafe
def scene_pkg.d(assets: String, rel: String, ds: String & String, pkg: String) -> IO(String):
  (d1, d2) = ds
  do IO<String>:
    n : U32 <- exists(d1 ++ "/" ++ pkg)
    s : String <- scene_pkg.n(d2, assets, rel, n)
    return s

@unsafe
def scene_pkg.n(dir: String, assets: String, rel: String, +n: U32) -> IO(String):
  scene_pkg.m(dir, assets, rel, U32.is_zero(n))

@unsafe
def scene_pkg.m(dir: String, assets: String, rel: String, miss: Bool) -> IO(String):
  match miss:
    case True{}:
      assets_text(assets, rel)
    case False{}:
      scene_pkg.read(dir, assets, rel)

@unsafe
def scene_pkg.read(dir: String, assets: String, rel: String) -> IO(String):
  scene_pkg.read2(assets, pair(dir), pair(rel))

@unsafe
def scene_pkg.read2(assets: String, ds: String & String, rs: String & String) -> IO(String):
  (d1, d2) = ds
  scene_pkg.read3(drop_s(d2), assets, rs, d1)

@unsafe
def scene_pkg.read3(+gone: U32, assets: String, rs: String & String, d1: String) -> IO(String):
  (r1, r2) = rs
  scene_pkg.read4(assets, d1, pkg_name(r1), pair(r2))

@unsafe
def scene_pkg.read4(assets: String, d1: String, pkg: String, ns: String & String) -> IO(String):
  (n1, n2) = ns
  do IO<String>:
    g : Got <- fetch(d1 ++ "/" ++ pkg, n1)
    s : String <- scene_pkg.mem(assets, n2, g)
    return s

@unsafe
def scene_pkg.mem(assets: String, rel: String, g: Got) -> IO(String):
  Got{ok, bs} = g
  scene_pkg.mm(assets, rel, bs, U32.is_zero(ok))

@unsafe
def scene_pkg.mm(assets: String, rel: String, bs: List<&2, U32>, miss: Bool) -> IO(String):
  match miss:
    case False{}:
      do IO<String>:
        return S.from_bytes(bs)
    case True{}:
      scene_pkg.miss(assets, rel, bs)

@unsafe
def scene_pkg.miss(assets: String, rel: String, bs: List<&2, U32>) -> IO(String):
  scene_pkg.miss2(assets, rel, drop_all(bs))

@unsafe
def scene_pkg.miss2(assets: String, rel: String, +n: U32) -> IO(String):
  assets_text(assets, rel)

@unsafe
def scene_text(dir: String, assets: String, file: String) -> IO(String):
  scene_text.r(dir, assets, norm_file(file))

@unsafe
def scene_text.r(dir: String, assets: String, rel: String) -> IO(String):
  scene_text.d(assets, pair(dir), pair(rel))

@unsafe
def scene_text.d(assets: String, ds: String & String, rs: String & String) -> IO(String):
  (d1, d2) = ds
  scene_text.e(assets, d2, rs, d1)

@unsafe
def scene_text.e(assets: String, d2: String, rs: String & String, d1: String) -> IO(String):
  (r1, r2) = rs
  do IO<String>:
    n : U32 <- exists(d1 ++ "/" ++ r1)
    s : String <- scene_text.n(d2, assets, r2, n)
    return s

@unsafe
def scene_text.n(dir: String, assets: String, rel: String, +n: U32) -> IO(String):
  scene_text.m(dir, assets, rel, U32.is_zero(n))

@unsafe
def scene_text.m(dir: String, assets: String, rel: String, miss: Bool) -> IO(String):
  match miss:
    case False{}:
      read_text(dir ++ "/" ++ rel)
    case True{}:
      scene_pkg(dir, assets, rel)

@unsafe
def load_got(dir: String, assets: String, rel: String, pkg: String) -> IO(Got):
  load_got.d(assets, rel, pkg, pair(dir))

@unsafe
def load_got.d(assets: String, rel: String, pkg: String, ds: String & String) -> IO(Got):
  (d1, d2) = ds
  load_got.r(assets, pkg, d2, pair(rel), d1)

@unsafe
def load_got.r(assets: String, pkg: String, d2: String, rs: String & String, d1: String) -> IO(Got):
  (r1, r2) = rs
  do IO<Got>:
    n : U32 <- exists(d1 ++ "/" ++ r1)
    g : Got <- load_got.n(d2, assets, r2, pkg, n)
    return g

@unsafe
def load_got.n(dir: String, assets: String, rel: String, pkg: String, +n: U32) -> IO(Got):
  load_got.m(dir, assets, rel, pkg, U32.is_zero(n))

@unsafe
def load_got.m(dir: String, assets: String, rel: String, pkg: String, miss: Bool) -> IO(Got):
  match miss:
    case False{}:
      do IO<Got>:
        bs : List<&2, U32> <- read_bin(dir ++ "/" ++ rel)
        return Got{1, bs}
    case True{}:
      load_pkg(dir, assets, rel, pkg)

@unsafe
def load_pkg(dir: String, assets: String, rel: String, pkg: String) -> IO(Got):
  load_pkg.d(assets, rel, pair(dir), pair(pkg))

@unsafe
def load_pkg.d(assets: String, rel: String, ds: String & String, ps: String & String) -> IO(Got):
  (d1, d2) = ds
  load_pkg.p(assets, rel, d2, ps, d1)

@unsafe
def load_pkg.p(assets: String, rel: String, d2: String, ps: String & String, d1: String) -> IO(Got):
  (p1, p2) = ps
  do IO<Got>:
    n : U32 <- exists(d1 ++ "/" ++ p1)
    g : Got <- load_pkg.n(d2, assets, rel, p2, n)
    return g

@unsafe
def load_pkg.n(dir: String, assets: String, rel: String, pkg: String, +n: U32) -> IO(Got):
  load_pkg.m(dir, assets, rel, pkg, U32.is_zero(n))

@unsafe
def load_pkg.m(dir: String, assets: String, rel: String, pkg: String, miss: Bool) -> IO(Got):
  match miss:
    case True{}:
      assets_bin(assets, rel)
    case False{}:
      load_pkg.read(dir, assets, rel, pkg)

@unsafe
def load_pkg.read(dir: String, assets: String, rel: String, pkg: String) -> IO(Got):
  load_pkg.read2(assets, rel, pair(dir), pkg)

@unsafe
def load_pkg.read2(assets: String, rel: String, ds: String & String, pkg: String) -> IO(Got):
  (d1, d2) = ds
  load_pkg.read3(drop_s(d2), assets, pkg, d1, pair(rel))

@unsafe
def load_pkg.read3(+gone: U32, assets: String, pkg: String, d1: String, ns: String & String) -> IO(Got):
  (n1, n2) = ns
  do IO<Got>:
    g0 : Got <- fetch(d1 ++ "/" ++ pkg, n1)
    g : Got <- load_pkg.mem(assets, n2, g0)
    return g

@unsafe
def load_pkg.mem(assets: String, rel: String, g: Got) -> IO(Got):
  Got{ok, bs} = g
  load_pkg.mm(assets, rel, bs, ok)

@unsafe
def load_pkg.mm(assets: String, rel: String, bs: List<&2, U32>, +ok: U32) -> IO(Got):
  load_pkg.mb(assets, rel, bs, U32.is_zero(ok))

@unsafe
def load_pkg.mb(assets: String, rel: String, bs: List<&2, U32>, miss: Bool) -> IO(Got):
  match miss:
    case False{}:
      do IO<Got>:
        return Got{1, bs}
    case True{}:
      load_pkg.miss(assets, rel, bs)

@unsafe
def load_pkg.miss(assets: String, rel: String, bs: List<&2, U32>) -> IO(Got):
  load_pkg.miss2(assets, rel, drop_all(bs))

@unsafe
def load_pkg.miss2(assets: String, rel: String, +n: U32) -> IO(Got):
  assets_bin(assets, rel)

@unsafe
def load_text(dir: String, assets: String, rel: String, pkg: String) -> IO(String):
  do IO<String>:
    g : Got <- load_got(dir, assets, rel, pkg)
    return got_text(g)

@unsafe
def load_bin(dir: String, assets: String, rel: String, pkg: String) -> IO(List<&2, U32>):
  do IO<List<&2, U32>>:
    g : Got <- load_got(dir, assets, rel, pkg)
    return got_bs(g)
