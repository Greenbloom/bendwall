import Base
import ./LAWS.bend as Laws

def Laws.can(xs, k):
  match xs k:
    case _ 0n:
      {==}
    case Nil{} 1n+kp:
      {==}
    case Con{_, t} 1n+kp:
      Laws.can(t, kp)

def Laws.take_empty_err():
  {==}

def Laws.take_empty_idx():
  {==}

def Laws.take_one_err():
  {==}

def Laws.take_one_idx():
  {==}

def Laws.take_one_byte():
  {==}

def Laws.take_short_err():
  {==}

def Laws.take_short_idx():
  {==}

def Laws.version_24():
  {==}

def Laws.version_25():
  {==}

def Laws.name_2048():
  {==}

def Laws.name_2049():
  {==}

def Laws.magic_8():
  {==}

def Laws.magic_9():
  {==}

def Laws.res_8192():
  {==}

def Laws.res_8193():
  {==}

def Laws.depth_128():
  {==}

def Laws.depth_129():
  {==}

def Laws.count_ok():
  {==}

def Laws.count_short():
  {==}

def Laws.count_huge():
  {==}
