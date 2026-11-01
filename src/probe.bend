import Base
import ./json.bend as J
import ./scene.bend as Scn
import ./av.bend as Av
import ./vfs.bend as Vfs
import ./gaps.bend as Gaps

# One Workshop item after project.json is read.
# Status is loaded, partial, or unsupported. The third column is the feature
# that is missing or only partly run (REA §2, §16).
type Job is Data:
  Job{id: String, dir: String, root: String, typ: String, file: String, dep: String, assets: String}

type Bite is Data:
  Bite{c: U32, rest: String, ok: U32}

@unsafe
def readf(path: String) -> IO(String):
  do IO<String>:
    f : File <- IO.try(File, File.open(path, "r"))
    m : File & Result<&1, &1, U32 & String, String> <- File.read(f, 1000000)
    s : String <- unwrap(m)
    return s

@unsafe
def unwrap(m: File & Result<&1, &1, U32 & String, String>) -> IO(String):
  (f, r) = m
  do IO<String>:
    s : String <- IO.pass(String, r)
    u : Unit <- File.close(f)
    return s

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
def kids(j: J.J) -> List<&2, J.J>:
  J.J{k, key, s, xs} = j
  xs

@unsafe
def no_props() -> List<&2, Scn.Prop>:
  Nil{}

@unsafe
def three(h: J.J) -> String & String & String:
  three.a(Scn.dup_j(h))

@unsafe
def three.a(p: J.J & J.J) -> String & String & String:
  (a, b) = p
  three.b(J.lit(J.get(a, "id")), Scn.dup_j(b))

@unsafe
def three.b(id: String, p: J.J & J.J) -> String & String & String:
  (a, b) = p
  (id, J.lit(J.get(a, "dir")), J.lit(J.get(b, "root")))

@unsafe
def fields(p: Scn.Proj) -> String & String & String:
  Scn.Proj{typ, file, title, dep, audio, props, overs} = p
  (typ, file, dep)

@unsafe
def job_of(h: J.J, p: Scn.Proj, assets: String) -> Job:
  job_of.f(three(h), fields(p), assets)

@unsafe
def job_of.f(a: String & String & String, b: String & String & String, assets: String) -> Job:
  (id, dir, root) = a
  job_of.g(id, dir, root, b, assets)

@unsafe
def job_of.g(id: String, dir: String, root: String, b: String & String & String, assets: String) -> Job:
  (typ, file, dep) = b
  Job{id, dir, root, typ, file, dep, assets}

@unsafe
def dup_job(j: Job) -> Job & Job:
  Job{id, dir, root, typ, file, dep, assets} = j
  dup_job.id(Scn.dup_s(id), dir, root, typ, file, dep, assets)

@unsafe
def dup_job.id(p: String & String, dir: String, root: String, typ: String, file: String, dep: String, assets: String) -> Job & Job:
  (a, b) = p
  dup_job.dir(a, b, Scn.dup_s(dir), root, typ, file, dep, assets)

@unsafe
def dup_job.dir(id1: String, id2: String, p: String & String, root: String, typ: String, file: String, dep: String, assets: String) -> Job & Job:
  (a, b) = p
  dup_job.root(id1, id2, a, b, Scn.dup_s(root), typ, file, dep, assets)

@unsafe
def dup_job.root(id1: String, id2: String, dir1: String, dir2: String, p: String & String, typ: String, file: String, dep: String, assets: String) -> Job & Job:
  (a, b) = p
  dup_job.typ(id1, id2, dir1, dir2, a, b, Scn.dup_s(typ), file, dep, assets)

@unsafe
def dup_job.typ(id1: String, id2: String, dir1: String, dir2: String, root1: String, root2: String, p: String & String, file: String, dep: String, assets: String) -> Job & Job:
  (a, b) = p
  dup_job.file(id1, id2, dir1, dir2, root1, root2, a, b, Scn.dup_s(file), dep, assets)

@unsafe
def dup_job.file(id1: String, id2: String, dir1: String, dir2: String, root1: String, root2: String, typ1: String, typ2: String, p: String & String, dep: String, assets: String) -> Job & Job:
  (a, b) = p
  dup_job.dep(id1, id2, dir1, dir2, root1, root2, typ1, typ2, a, b, Scn.dup_s(dep), assets)

@unsafe
def dup_job.dep(id1: String, id2: String, dir1: String, dir2: String, root1: String, root2: String, typ1: String, typ2: String, file1: String, file2: String, p: String & String, assets: String) -> Job & Job:
  (a, b) = p
  dup_job.as(id1, id2, dir1, dir2, root1, root2, typ1, typ2, file1, file2, a, b, Scn.dup_s(assets))

@unsafe
def dup_job.as(id1: String, id2: String, dir1: String, dir2: String, root1: String, root2: String, typ1: String, typ2: String, file1: String, file2: String, dep1: String, dep2: String, p: String & String) -> Job & Job:
  (a, b) = p
  (Job{id1, dir1, root1, typ1, file1, dep1, a}, Job{id2, dir2, root2, typ2, file2, dep2, b})

@unsafe
def typ_of(j: Job) -> String & Job:
  Job{id, dir, root, typ, file, dep, assets} = j
  typ_of.d(id, dir, root, file, dep, assets, Scn.dup_s(typ))

@unsafe
def typ_of.d(id: String, dir: String, root: String, file: String, dep: String, assets: String, p: String & String) -> String & Job:
  (a, b) = p
  (a, Job{id, dir, root, b, file, dep, assets})

@unsafe
def line_loaded(j: Job) -> String:
  Job{id, dir, root, typ, file, dep, assets} = j
  id ++ " loaded\n"

@unsafe
def line_pf(j: Job, status: String, feature: String) -> String:
  Job{id, dir, root, typ, file, dep, assets} = j
  id ++ " " ++ status ++ " " ++ feature ++ "\n"

@unsafe
def bite(s: String) -> Bite:
  match s:
    case SNil{}:
      Bite{0, SNil{}, 0}
    case SCon{Chr{c}, rest}:
      Bite{c, rest, 1}

@unsafe
def k_or(+c: U32, +no: U32) -> U32:
  k_or.m(no, U32.is_eq(c, 107))

@unsafe
def k_or.m(+no: U32, yes: Bool) -> U32:
  match yes:
    case True{}:
      1
    case False{}:
      no

@unsafe
def expect(+c: U32, +want: U32, +yes: U32) -> U32:
  expect.m(c, yes, U32.is_eq(c, want))

@unsafe
def expect.m(+c: U32, +yes: U32, eq: Bool) -> U32:
  match eq:
    case True{}:
      yes
    case False{}:
      k_or(c, 0)

@unsafe
def step(+st: U32, +c: U32) -> U32:
  step.a(st, c, U32.is_zero(st))

@unsafe
def step.a(+st: U32, +c: U32, z: Bool) -> U32:
  match z:
    case True{}:
      k_or(c, 0)
    case False{}:
      step.b(st, c, U32.is_eq(st, 1))

@unsafe
def step.b(+st: U32, +c: U32, hit: Bool) -> U32:
  match hit:
    case True{}:
      expect(c, 105, 2)
    case False{}:
      step.c(st, c, U32.is_eq(st, 2))

@unsafe
def step.c(+st: U32, +c: U32, hit: Bool) -> U32:
  match hit:
    case True{}:
      expect(c, 110, 3)
    case False{}:
      step.d(st, c, U32.is_eq(st, 3))

@unsafe
def step.d(+st: U32, +c: U32, hit: Bool) -> U32:
  match hit:
    case True{}:
      expect(c, 100, 4)
    case False{}:
      step.e(st, c, U32.is_eq(st, 4))

@unsafe
def step.e(+st: U32, +c: U32, hit: Bool) -> U32:
  match hit:
    case True{}:
      expect(c, 61, 5)
    case False{}:
      k_or(c, 0)

@unsafe
def scan(s: String, +st: U32) -> String & U32:
  scan.p(bite(s), st)

@unsafe
def scan.p(p: Bite, +st: U32) -> String & U32:
  Bite{+c, rest, +ok} = p
  scan.e(c, rest, st, U32.is_zero(ok))

@unsafe
def scan.e(+c: U32, rest: String, +st: U32, end: Bool) -> String & U32:
  match end:
    case True{}:
      (SNil{}, 0)
    case False{}:
      scan.n(rest, step(st, c))

@unsafe
def scan.n(rest: String, +st: U32) -> String & U32:
  scan.d(rest, st, U32.is_eq(st, 5))

@unsafe
def scan.d(rest: String, +st: U32, done: Bool) -> String & U32:
  match done:
    case True{}:
      (rest, 1)
    case False{}:
      scan(rest, st)

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
def word(s: String) -> String & String:
  word.go(s, SNil{})

@unsafe
def word.go(s: String, acc: String) -> String & String:
  word.p(bite(s), acc)

@unsafe
def word.p(p: Bite, acc: String) -> String & String:
  Bite{+c, rest, +ok} = p
  word.e(c, rest, acc, U32.is_zero(ok))

@unsafe
def word.e(+c: U32, rest: String, acc: String, end: Bool) -> String & String:
  match end:
    case True{}:
      (rev(acc), SNil{})
    case False{}:
      word.sp(c, rest, acc, U32.is_eq(c, 32))

@unsafe
def word.sp(+c: U32, rest: String, acc: String, sp: Bool) -> String & String:
  match sp:
    case True{}:
      (rev(acc), rest)
    case False{}:
      word.nl(c, rest, acc, U32.is_eq(c, 10))

@unsafe
def word.nl(+c: U32, rest: String, acc: String, nl: Bool) -> String & String:
  match nl:
    case True{}:
      (rev(acc), rest)
    case False{}:
      word.go(rest, SCon{Chr{c}, acc})

@unsafe
def skip_kind(w: String) -> Bool & String:
  skip_kind.e(Scn.empty(w))

@unsafe
def skip_kind.e(p: Bool & String) -> Bool & String:
  (e, w) = p
  skip_kind.m(w, e)

@unsafe
def skip_kind.m(w: String, e: Bool) -> Bool & String:
  match e:
    case True{}:
      (True{}, w)
    case False{}:
      skip_kind.img(Scn.dup_s(w))

@unsafe
def skip_kind.img(p: String & String) -> Bool & String:
  (a, b) = p
  (String.eq(a, "image"), b)

@unsafe
def feature(s: String) -> String:
  feature.f(scan(s, 0))

@unsafe
def feature.f(p: String & U32) -> String:
  (rest, ok) = p
  feature.m(rest, U32.is_zero(ok))

@unsafe
def feature.m(rest: String, none: Bool) -> String:
  match none:
    case True{}:
      ""
    case False{}:
      feature.w(word(rest))

@unsafe
def feature.w(p: String & String) -> String:
  (w, rest) = p
  feature.sk(rest, skip_kind(w))

@unsafe
def feature.sk(rest: String, p: Bool & String) -> String:
  (sk, w) = p
  feature.ch(rest, w, sk)

@unsafe
def feature.ch(rest: String, w: String, sk: Bool) -> String:
  match sk:
    case True{}:
      feature(rest)
    case False{}:
      w

@unsafe
def scene_file(file: String) -> String:
  scene_file.e(Scn.empty(file))

@unsafe
def scene_file.e(p: Bool & String) -> String:
  (e, file) = p
  scene_file.m(file, e)

@unsafe
def scene_file.m(file: String, e: Bool) -> String:
  match e:
    case True{}:
      "scene.json"
    case False{}:
      file

@unsafe
def vpath(j: Job) -> String:
  Job{id, dir, root, typ, file, dep, assets} = j
  dir ++ "/" ++ file

@unsafe
def dpath(j: Job) -> String:
  Job{id, dir, root, typ, file, dep, assets} = j
  dpath.e(root, Scn.empty(dep))

@unsafe
def dpath.e(root: String, p: Bool & String) -> String:
  (e, dep) = p
  dpath.m(root, dep, e)

@unsafe
def dpath.m(root: String, dep: String, e: Bool) -> String:
  match e:
    case True{}:
      ""
    case False{}:
      root ++ "/" ++ dep ++ "/project.json"

@unsafe
def scene_feat(j: Job, feat: String) -> IO(String):
  scene_feat.e(j, Scn.empty(feat))

@unsafe
def scene_feat.e(j: Job, p: Bool & String) -> IO(String):
  (e, feat) = p
  scene_feat.m(j, feat, e)

@unsafe
def scene_feat.m(j: Job, feat: String, e: Bool) -> IO(String):
  match e:
    case True{}:
      picture_line(j)
    case False{}:
      scene_feat.part(j, feat)

@unsafe
def scene_feat.part(j: Job, feat: String) -> IO(String):
  do IO<String>:
    return line_pf(j, "partial", feat)

@unsafe
def picture_line(j: Job) -> IO(String):
  picture_line.d(dup_job(j))

@unsafe
def picture_line.d(p: Job & Job) -> IO(String):
  (a, b) = p
  picture_line.p(b, paths_of(a))

@unsafe
def picture_line.p(j: Job, p: String & String & String) -> IO(String):
  (dir, assets, file) = p
  picture_line.q(j, dir, assets, Vfs.drop_s(file))

@unsafe
def picture_line.q(j: Job, dir: String, assets: String, +n: U32) -> IO(String):
  do IO<String>:
    r : U32 <- Av.scene_ready(dir, assets)
    return picture_line.s(j, r)

@unsafe
def picture_line.s(j: Job, +n: U32) -> String:
  picture_line.z(j, U32.is_zero(n))

@unsafe
def picture_line.z(j: Job, z: Bool) -> String:
  match z:
    case True{}:
      line_pf(j, "partial", "texture")
    case False{}:
      line_loaded(j)

@unsafe
def paths_of(j: Job) -> String & String & String:
  Job{id, dir, root, typ, file, dep, assets} = j
  (dir, assets, file)

@unsafe
def scene_use(j: Job, src: String) -> IO(String):
  scene_use.e(j, Scn.empty(src))

@unsafe
def scene_use.e(j: Job, p: Bool & String) -> IO(String):
  (e, src) = p
  scene_use.m(j, src, e)

@unsafe
def scene_use.m(j: Job, src: String, e: Bool) -> IO(String):
  match e:
    case True{}:
      scene_use.bad(j)
    case False{}:
      scene_feat(j, Gaps.report(src))

@unsafe
def scene_use.bad(j: Job) -> IO(String):
  do IO<String>:
    return line_pf(j, "unsupported", "scene")

@unsafe
def scene_status(j: Job) -> IO(String):
  scene_status.d(dup_job(j))

@unsafe
def scene_status.d(p: Job & Job) -> IO(String):
  (a, b) = p
  scene_status.a(b, paths_of(a))

@unsafe
def scene_status.a(j: Job, p: String & String & String) -> IO(String):
  (dir, assets, file) = p
  do IO<String>:
    src : String <- Vfs.scene_text(dir, assets, file)
    s : String <- scene_use(j, src)
    return s

@unsafe
def play.s(j: Job, +n: U32) -> String:
  play.m(j, U32.is_zero(n))

@unsafe
def play.m(j: Job, z: Bool) -> String:
  match z:
    case True{}:
      line_pf(j, "unsupported", "video")
    case False{}:
      line_loaded(j)

@unsafe
def play(j: Job) -> IO(String):
  play.d(dup_job(j))

@unsafe
def play.d(p: Job & Job) -> IO(String):
  (a, b) = p
  do IO<String>:
    n : U32 <- Av.av_video(vpath(a))
    return play.s(b, n)

@unsafe
def preset.n(j: Job, +n: U32) -> String:
  preset.m(j, U32.is_zero(n))

@unsafe
def preset.m(j: Job, miss: Bool) -> String:
  match miss:
    case True{}:
      line_pf(j, "unsupported", "dependency")
    case False{}:
      line_loaded(j)

@unsafe
def preset_status(j: Job) -> IO(String):
  preset_status.d(dup_job(j))

@unsafe
def preset_status.d(p: Job & Job) -> IO(String):
  (a, b) = p
  do IO<String>:
    n : U32 <- exists(dpath(a))
    s : String <- preset_status.n(b, n)
    return s

@unsafe
def preset_status.n(j: Job, +n: U32) -> IO(String):
  preset_status.m(j, U32.is_zero(n))

@unsafe
def preset_status.m(j: Job, miss: Bool) -> IO(String):
  match miss:
    case True{}:
      do IO<String>:
        return line_pf(j, "unsupported", "dependency")
    case False{}:
      preset_load(j)

@unsafe
def preset_load(j: Job) -> IO(String):
  Job{id, dir, root, typ, file, dep, assets} = j
  preset_load.p(id, assets, Scn.dup_s(root), Scn.dup_s(dep))

@unsafe
def preset_load.p(id: String, assets: String, pr: String & String, pd: String & String) -> IO(String):
  (r1, r2) = pr
  preset_load.d(id, assets, r2, pd, r1)

@unsafe
def preset_load.d(id: String, assets: String, r2: String, pd: String & String, r1: String) -> IO(String):
  (d1, d2) = pd
  do IO<String>:
    src : String <- readf(r1 ++ "/" ++ d1 ++ "/project.json")
    s : String <- preset_base(id, assets, r2, d2, src)
    return s

@unsafe
def preset_base(id: String, assets: String, root: String, dep: String, src: String) -> IO(String):
  preset_base.r(id, assets, dep, src, Scn.dup_s(root))

@unsafe
def preset_base.r(id: String, assets: String, dep: String, src: String, p: String & String) -> IO(String):
  (r1, r2) = p
  preset_base.j(id, assets, dep, r1, r2, Scn.project(J.parse(src)))

@unsafe
def preset_base.j(id: String, assets: String, dep: String, r1: String, r2: String, p: Scn.Proj) -> IO(String):
  Scn.Proj{typ, file, title, bdep, audio, props, overs} = p
  decide(Job{id, r1 ++ "/" ++ dep, r2, typ, file, "", assets})

@unsafe
def preset_raw(j: Job, raw: String) -> Bool:
  Job{id, dir, root, typ, file, dep, assets} = j
  preset_raw.t(dep, raw, String.eq(typ, "preset"))

@unsafe
def preset_raw.t(dep: String, raw: String, hit: Bool) -> Bool:
  match hit:
    case True{}:
      preset_raw.yes(dep, raw)
    case False{}:
      preset_raw.e(dep, Scn.empty(raw))

@unsafe
def preset_raw.yes(dep: String, raw: String) -> Bool:
  preset_raw.yes2(Vfs.drop_s(dep), Vfs.drop_s(raw))

@unsafe
def preset_raw.yes2(+a: U32, +b: U32) -> Bool:
  True{}

@unsafe
def preset_raw.e(dep: String, p: Bool & String) -> Bool:
  (e, raw) = p
  preset_raw.em(dep, raw, e)

@unsafe
def preset_raw.em(dep: String, raw: String, e: Bool) -> Bool:
  match e:
    case False{}:
      preset_raw.no(dep, raw)
    case True{}:
      preset_raw.d(raw, Scn.empty(dep))

@unsafe
def preset_raw.no(dep: String, raw: String) -> Bool:
  preset_raw.no2(Vfs.drop_s(dep), Vfs.drop_s(raw))

@unsafe
def preset_raw.no2(+a: U32, +b: U32) -> Bool:
  False{}

@unsafe
def preset_raw.d(raw: String, p: Bool & String) -> Bool:
  (e, dep) = p
  preset_raw.dm(raw, dep, e)

@unsafe
def preset_raw.dm(raw: String, dep: String, e: Bool) -> Bool:
  match e:
    case True{}:
      preset_raw.no(dep, raw)
    case False{}:
      preset_raw.yes(dep, raw)

@unsafe
def route(j: Job, raw: String) -> IO(String):
  route.p(dup_job(j), raw)

@unsafe
def route.p(p: Job & Job, raw: String) -> IO(String):
  (a, b) = p
  route.q(b, preset_raw(a, raw))

@unsafe
def route.q(j: Job, hit: Bool) -> IO(String):
  match hit:
    case True{}:
      preset_status(j)
    case False{}:
      decide(j)

@unsafe
def decide(j: Job) -> IO(String):
  decide.app(typ_of(j))

@unsafe
def decide.app(p: String & Job) -> IO(String):
  (typ, j) = p
  decide.app.m(j, String.eq(typ, "application"))

@unsafe
def decide.app.m(j: Job, hit: Bool) -> IO(String):
  match hit:
    case True{}:
      do IO<String>:
        return line_pf(j, "unsupported", "application")
    case False{}:
      decide.web(typ_of(j))

@unsafe
def decide.web(p: String & Job) -> IO(String):
  (typ, j) = p
  decide.web.m(j, String.eq(typ, "web"))

@unsafe
def decide.web.m(j: Job, hit: Bool) -> IO(String):
  match hit:
    case True{}:
      do IO<String>:
        return line_loaded(j)
    case False{}:
      decide.video(typ_of(j))

@unsafe
def decide.video(p: String & Job) -> IO(String):
  (typ, j) = p
  decide.video.m(j, String.eq(typ, "video"))

@unsafe
def decide.video.m(j: Job, hit: Bool) -> IO(String):
  match hit:
    case True{}:
      play(j)
    case False{}:
      decide.preset(typ_of(j))

@unsafe
def decide.preset(p: String & Job) -> IO(String):
  (typ, j) = p
  decide.preset.m(j, String.eq(typ, "preset"))

@unsafe
def decide.preset.m(j: Job, hit: Bool) -> IO(String):
  match hit:
    case True{}:
      preset_status(j)
    case False{}:
      scene_status(j)

@unsafe
def one(h: J.J, assets: String) -> IO(String):
  one.d(Scn.dup_j(h), assets)

@unsafe
def one.d(p: J.J & J.J, assets: String) -> IO(String):
  (a, b) = p
  do IO<String>:
    src : String <- readf(J.lit(J.get(a, "dir")) ++ "/project.json")
    s : String <- one.job(b, assets, src)
    return s

@unsafe
def one.job(h: J.J, assets: String, src: String) -> IO(String):
  one.raw(h, assets, Scn.dup_j(J.parse(src)))

@unsafe
def one.raw(h: J.J, assets: String, p: J.J & J.J) -> IO(String):
  (ja, jb) = p
  one.proj(h, assets, J.lit(J.get(ja, "type")), Scn.project(jb))

@unsafe
def one.proj(h: J.J, assets: String, raw: String, proj: Scn.Proj) -> IO(String):
  route(job_of(h, proj, assets), raw)

@unsafe
def classify(xs: List<&2, J.J>, assets: String) -> IO(String):
  match xs:
    case Nil{}:
      classify.none(assets)
    case Con{h, t}:
      classify.step(h, t, assets)

@unsafe
def classify.none(assets: String) -> IO(String):
  classify.none2(Vfs.drop_s(assets))

@unsafe
def classify.none2(+n: U32) -> IO(String):
  do IO<String>:
    return ""

@unsafe
def classify.step(h: J.J, t: List<&2, J.J>, assets: String) -> IO(String):
  classify.go(h, t, Scn.dup_s(assets))

@unsafe
def classify.go(h: J.J, t: List<&2, J.J>, p: String & String) -> IO(String):
  (a1, a2) = p
  do IO<String>:
    line : String <- one(h, a1)
    rest : String <- classify(t, a2)
    return line ++ rest

@unsafe
def run() -> IO(String):
  do IO<String>:
    src : String <- readf("build/probe-manifest.json")
    text : String <- run.j(J.parse(src))
    return text

@unsafe
def run.j(j: J.J) -> IO(String):
  run.d(Scn.dup_j(j))

@unsafe
def run.d(p: J.J & J.J) -> IO(String):
  (a, b) = p
  classify(kids(J.get(b, "items")), J.lit(J.get(a, "assets")))
