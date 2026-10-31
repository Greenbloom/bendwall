import Base
import ./bytes.bend as B
import ./spec.bend as Spec
import ./str.bend as S

type Entry is Data:
  Entry{name: String, off: U32, size: U32}

type Pkg is Data:
  Pkg{ver: U32, start: U32, entries: List<&2, Entry>, err: U32}

type PS is Data:
  PS{c: B.Cur, acc: List<&2, Entry>, dead: U32}

@unsafe
def rev(xs: List<&2, Entry>, acc: List<&2, Entry>) -> List<&2, Entry>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev(t, Con{h, acc})

@unsafe
def step.cons(c: B.Cur, acc: List<&2, Entry>, name: String, off: U32, sz: U32, bad: Bool) -> PS:
  match bad:
    case True{}:
      PS{c, acc, 1}
    case False{}:
      PS{c, Con{Entry{name, off, sz}, acc}, 0}

@unsafe
def step.sz(r: B.Cur & U32, acc: List<&2, Entry>, name: String, off: U32) -> PS:
  (c, sz) = r
  step.sz2(B.fork_err(c), acc, name, off, sz)

@unsafe
def step.sz2(r: B.Cur & U32, acc: List<&2, Entry>, name: String, off: U32, sz: U32) -> PS:
  (c, err) = r
  step.cons(c, acc, name, off, sz, U32.is_ne(err, 0))

@unsafe
def step.off(r: B.Cur & U32, acc: List<&2, Entry>, name: String) -> PS:
  (c, off) = r
  step.sz(B.u32(c), acc, name, off)

@unsafe
def step.name(r: B.Cur & List<&2, U32>, acc: List<&2, Entry>, kept: Bool) -> PS:
  (c, raw) = r
  step.name2(B.fork_err(c), acc, S.name_of(raw, kept))

@unsafe
def step.name2(r: B.Cur & U32, acc: List<&2, Entry>, name: String) -> PS:
  (c, err) = r
  step.name3(c, acc, name, U32.is_ne(err, 0))

@unsafe
def step.name3(c: B.Cur, acc: List<&2, Entry>, name: String, bad: Bool) -> PS:
  match bad:
    case True{}:
      PS{c, acc, 1}
    case False{}:
      step.off(B.u32(c), acc, name)

@unsafe
def step.take(c: B.Cur, +n: U32, acc: List<&2, Entry>) -> PS:
  step.name(B.take(c, n), acc, Spec.name_kept(n))

@unsafe
def step.nl(r: B.Cur & U32, acc: List<&2, Entry>) -> PS:
  (c, n) = r
  step.nl2(B.fork_err(c), n, acc)

@unsafe
def step.nl2(r: B.Cur & U32, n: U32, acc: List<&2, Entry>) -> PS:
  (c, err) = r
  step.nl3(c, n, acc, U32.is_ne(err, 0))

@unsafe
def step.nl3(c: B.Cur, n: U32, acc: List<&2, Entry>, bad: Bool) -> PS:
  match bad:
    case True{}:
      PS{c, acc, 1}
    case False{}:
      step.take(c, n, acc)

@unsafe
def step(ps: PS) -> PS:
  PS{c, acc, +dead} = ps
  step.b(c, acc, dead, U32.is_ne(dead, 0))

@unsafe
def step.b(c: B.Cur, acc: List<&2, Entry>, dead: U32, stop: Bool) -> PS:
  match stop:
    case True{}:
      PS{c, acc, dead}
    case False{}:
      step.nl(B.u32(c), acc)

@unsafe
def eloop(n: Nat, ps: PS) -> PS:
  match n:
    case 0n:
      ps
    case 1n+p:
      eloop(p, step(ps))

@unsafe
def span.sz(too: Bool) -> U32:
  match too:
    case True{}:
      1
    case False{}:
      0

@unsafe
def span.room(sz: U32, room: U32) -> U32:
  span.sz(U32.is_gt(sz, room))

@unsafe
def span.mark2(off: U32, sz: U32, remain: U32, too: Bool) -> U32:
  match too:
    case True{}:
      1
    case False{}:
      span.room(sz, U32.sub(remain, off))

@unsafe
def span.mark(+off: U32, +sz: U32, +remain: U32) -> U32:
  span.mark2(off, sz, remain, U32.is_gt(off, remain))

@unsafe
def span.or2(err: U32, mark: U32, bad: Bool) -> U32:
  match bad:
    case True{}:
      err
    case False{}:
      mark

@unsafe
def span.or(+err: U32, mark: U32) -> U32:
  span.or2(err, mark, U32.is_ne(err, 0))

type SS is Data:
  SS{es: List<&2, Entry>, remain: U32, err: U32}

@unsafe
def sstep.b(t: List<&2, Entry>, +remain: U32, +err: U32, +off: U32, +sz: U32, bad: Bool) -> SS:
  match bad:
    case True{}:
      SS{t, remain, err}
    case False{}:
      SS{t, remain, span.or(err, span.mark(off, sz, remain))}

@unsafe
def sstep.e(t: List<&2, Entry>, remain: U32, +err: U32, e: Entry) -> SS:
  Entry{name, off, sz} = e
  sstep.b(t, remain, err, off, sz, U32.is_ne(err, 0))

@unsafe
def sstep(ss: SS) -> SS:
  SS{es, remain, err} = ss
  match es:
    case Nil{}:
      SS{Nil{}, remain, err}
    case Con{e, t}:
      sstep.e(t, remain, err, e)

@unsafe
def sloop(n: Nat, ss: SS) -> SS:
  match n:
    case 0n:
      ss
    case 1n+p:
      sloop(p, sstep(ss))

@unsafe
def span_err.of(ss: SS) -> U32:
  SS{es, remain, err} = ss
  err

@unsafe
def span_list(+es: List<&2, Entry>, remain: U32) -> U32:
  span_err.of(sloop(List.length(&2, Entry, es), SS{es, remain, 0}))

@unsafe
def span_err2(es: List<&2, Entry>, start: U32, file_len: U32, too: Bool) -> U32:
  match too:
    case True{}:
      1
    case False{}:
      span_list(es, U32.sub(file_len, start))

@unsafe
def span_err(+es: List<&2, Entry>, +start: U32, +file_len: U32) -> U32:
  span_err2(es, start, file_len, U32.is_gt(start, file_len))

@unsafe
def ver_gate2(ok: Bool) -> U32:
  match ok:
    case True{}:
      0
    case False{}:
      3

@unsafe
def ver_gate(ver: U32) -> U32:
  ver_gate2(Spec.version_ok(ver))

@unsafe
def combine2(a: U32, b: U32, bad: Bool) -> U32:
  match bad:
    case True{}:
      a
    case False{}:
      b

@unsafe
def combine(+a: U32, b: U32) -> U32:
  combine2(a, b, U32.is_ne(a, 0))

@unsafe
def finish4(ver: U32, es: List<&2, Entry>, start: U32, err: U32, mark: U32) -> Pkg:
  Pkg{ver, start, es, combine(err, mark)}

@unsafe
def finish3(ver: U32, +es: List<&2, Entry>, err: U32, +start: U32, file_len: U32) -> Pkg:
  finish4(ver, es, start, err, span_err(es, start, file_len))

@unsafe
def finish2(c: B.Cur, ver: U32, file_len: U32, es: List<&2, Entry>, err: U32) -> Pkg:
  finish3(ver, es, err, B.idx_of(c), file_len)

@unsafe
def finish(c: B.Cur, +ver: U32, file_len: U32, +es: List<&2, Entry>, dead: U32) -> Pkg:
  finish2(c, ver, file_len, es, combine(dead, ver_gate(ver)))

@unsafe
def run(c: B.Cur, ver: U32, file_len: U32, count: U32) -> Pkg:
  run.at(ver, file_len, eloop(U32.to_nat(count), PS{c, Nil{}, 0}))

@unsafe
def run.at(ver: U32, file_len: U32, ps: PS) -> Pkg:
  PS{c, acc, dead} = ps
  finish(c, ver, file_len, rev(acc, Nil{}), dead)

@unsafe
def before3(c: B.Cur, ver: U32, file_len: U32, count: U32, fits: Bool) -> Pkg:
  match fits:
    case False{}:
      Pkg{ver, B.idx_of(c), Nil{}, 1}
    case True{}:
      run(c, ver, file_len, count)

@unsafe
def before2(c: B.Cur, ver: U32, file_len: U32, count: U32, fits: Bool, bad: Bool) -> Pkg:
  match bad:
    case True{}:
      before.bad(B.fork_err(c), ver)
    case False{}:
      before3(c, ver, file_len, count, fits)

@unsafe
def before.bad(r: B.Cur & U32, ver: U32) -> Pkg:
  (c, err) = r
  Pkg{ver, B.idx_of(c), Nil{}, err}

@unsafe
def before(c: B.Cur, ver: U32, file_len: U32, +count: U32) -> Pkg:
  B.Cur{+rest, i, +err} = c
  before2(B.Cur{rest, i, err}, ver, file_len, count, Spec.count_fits(count, B.ulen(rest, 0)), U32.is_ne(err, 0))

@unsafe
def after.count(r: B.Cur & U32, ver: U32, file_len: U32) -> Pkg:
  (c, count) = r
  after.count2(B.fork_err(c), ver, file_len, count)

@unsafe
def after.count2(r: B.Cur & U32, ver: U32, file_len: U32, count: U32) -> Pkg:
  (c, err) = r
  after.count3(c, ver, file_len, count, U32.is_ne(err, 0))

@unsafe
def after.count3(c: B.Cur, ver: U32, file_len: U32, count: U32, bad: Bool) -> Pkg:
  match bad:
    case True{}:
      before.bad(B.fork_err(c), ver)
    case False{}:
      before(c, ver, file_len, count)

@unsafe
def err_keep(c: B.Cur) -> U32:
  B.err_of(c)

@unsafe
def after.ver2(c: B.Cur, file_len: U32, ver: U32, ok: Bool) -> Pkg:
  match ok:
    case True{}:
      after.count(B.u32(c), ver, file_len)
    case False{}:
      Pkg{ver, B.idx_of(c), Nil{}, 3}

@unsafe
def after.ver(c: B.Cur, file_len: U32, +ver: U32) -> Pkg:
  after.ver2(c, file_len, ver, Spec.version_ok(ver))

@unsafe
def after.magic(c: B.Cur, file_len: U32, raw: List<&2, U32>) -> Pkg:
  after.magic2(B.fork_err(c), file_len, raw)

@unsafe
def after.magic2(r: B.Cur & U32, file_len: U32, +raw: List<&2, U32>) -> Pkg:
  (c, err) = r
  after.magic3(c, file_len, raw, B.ulen(raw, 0), U32.is_ne(err, 0))

@unsafe
def after.magic3(c: B.Cur, file_len: U32, +raw: List<&2, U32>, +n: U32, bad: Bool) -> Pkg:
  match bad:
    case True{}:
      before.bad(B.fork_err(c), 0)
    case False{}:
      after.ver(c, file_len, S.ver_of(S.from_bytes(raw), n))

@unsafe
def after.len(c: B.Cur, +n: U32, file_len: U32, ok: Bool, bad: Bool) -> Pkg:
  match bad:
    case True{}:
      before.bad(B.fork_err(c), 0)
    case False{}:
      after.len2(c, n, file_len, ok)

@unsafe
def after.len2(c: B.Cur, n: U32, file_len: U32, ok: Bool) -> Pkg:
  match ok:
    case False{}:
      Pkg{0, B.idx_of(B.fail(c, 2)), Nil{}, 2}
    case True{}:
      after.magic_take(B.take(c, n), file_len)

@unsafe
def after.magic_take(r: B.Cur & List<&2, U32>, file_len: U32) -> Pkg:
  (c, raw) = r
  after.magic(c, file_len, raw)

@unsafe
def parse.at(r: B.Cur & U32, file_len: U32) -> Pkg:
  (c, n) = r
  parse.n(B.fork_err(c), n, file_len)

@unsafe
def parse.n(r: B.Cur & U32, +n: U32, file_len: U32) -> Pkg:
  (c, err) = r
  after.len(c, n, file_len, Spec.magic_len_ok(n), U32.is_ne(err, 0))

@unsafe
def parse(+bs: List<&2, U32>) -> Pkg:
  parse.at(B.u32(B.cur(bs)), B.ulen(bs, 0))

type Hit is Data:
  Hit{ok: U32, off: U32, size: U32}

@unsafe
def find.b(name: String, t: List<&2, Entry>, off: U32, sz: U32, eq: Bool) -> Hit:
  match eq:
    case True{}:
      Hit{1, off, sz}
    case False{}:
      find(name, t)

@unsafe
def find(name: String, es: List<&2, Entry>) -> Hit:
  match es:
    case Nil{}:
      Hit{0, 0, 0}
    case Con{e, t}:
      find.e(name, t, e)

@unsafe
def find.e(+name: String, t: List<&2, Entry>, e: Entry) -> Hit:
  Entry{+n, off, sz} = e
  find.b(name, t, off, sz, String.eq(S.lower_str(n), name))
