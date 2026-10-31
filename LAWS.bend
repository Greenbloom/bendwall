import Base
import ./src/bytes.bend as Bytes
import ./src/spec.bend as Spec

# A read of k bytes fits in the unread tail exactly when k is at most the tail's length.
law can:
  for xs: List<&2, U32>
  for k: Nat
  {Bytes.can(xs, k) == Nat.is_le(k, Bytes.len(xs)) : Bool}

# An empty tail refuses a 4-byte read and does not advance.
law take_empty_err:
  {Bytes.take_err(Bytes.take(Bytes.cur(Nil{}), 4)) == 1 : U32}

law take_empty_idx:
  {Bytes.take_idx(Bytes.take(Bytes.cur(Nil{}), 4)) == 0 : U32}

# A one-byte tail yields that byte and moves the index by one.
law take_one_err:
  {Bytes.take_err(Bytes.take(Bytes.cur(Con{65, Nil{}}), 1)) == 0 : U32}

law take_one_idx:
  {Bytes.take_idx(Bytes.take(Bytes.cur(Con{65, Nil{}}), 1)) == 1 : U32}

law take_one_byte:
  {Bytes.take_head(Bytes.take(Bytes.cur(Con{65, Nil{}}), 1)) == 65 : U32}

# A 2-byte request against a 1-byte tail is a fault, and the index stays.
law take_short_err:
  {Bytes.take_err(Bytes.take(Bytes.cur(Con{9, Nil{}}), 2)) == 1 : U32}

law take_short_idx:
  {Bytes.take_idx(Bytes.take(Bytes.cur(Con{9, Nil{}}), 2)) == 0 : U32}

law version_24:
  {Spec.version_ok(24) == True{} : Bool}

law version_25:
  {Spec.version_ok(25) == False{} : Bool}

law name_2048:
  {Spec.name_kept(2048) == True{} : Bool}

law name_2049:
  {Spec.name_kept(2049) == False{} : Bool}

law magic_8:
  {Spec.magic_len_ok(8) == True{} : Bool}

law magic_9:
  {Spec.magic_len_ok(9) == False{} : Bool}

law res_8192:
  {Spec.res_ok(8192, 8192) == True{} : Bool}

law res_8193:
  {Spec.res_ok(8193, 1) == False{} : Bool}

law depth_128:
  {Spec.depth_ok(128) == True{} : Bool}

law depth_129:
  {Spec.depth_ok(129) == False{} : Bool}

# A pkg directory of `count` entries needs 12 bytes each, and a huge count is rejected
# before the loop so a corrupt header cannot walk off the file.
law count_ok:
  {Spec.count_fits(2, 24) == True{} : Bool}

law count_short:
  {Spec.count_fits(2, 23) == False{} : Bool}

law count_huge:
  {Spec.count_fits(357913942, 100) == False{} : Bool}
