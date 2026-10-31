import Base

# Wallpaper Engine gates from the REA spec.
# pkg version fails at >= 25. Names longer than 0x800 are dropped.
# Texture width and height cap at 8192. Depth caps at 128.

def version_ok(v: U32) -> Bool:
  U32.is_lt(v, 25)

def name_kept(n: U32) -> Bool:
  U32.is_le(n, 2048)

def magic_len_ok(n: U32) -> Bool:
  U32.is_le(n, 8)

def res_ok(w: U32, h: U32) -> Bool:
  Bool.and(U32.is_le(w, 8192), U32.is_le(h, 8192))

def depth_ok(d: U32) -> Bool:
  U32.is_le(d, 128)

# count entries fit in `remain` bytes when each entry is at least 12 bytes.
def count_fits.go(count: U32, remain: U32, huge: Bool) -> Bool:
  match huge:
    case True{}:
      False{}
    case False{}:
      U32.is_le((count * 12 : U32), remain)

def count_fits(+count: U32, remain: U32) -> Bool:
  count_fits.go(count, remain, U32.is_gt(count, 357913941))
