#!/bin/sh
# A mapped file must keep its old bytes when install replaces it.
set -e
cd "$(dirname "$0")/.."
. scripts/atomic.sh
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/src/qml" "$root/dest"
printf 'OLDOLDOLD' > "$root/dest/libbendwall.so"
printf 'NEWNEWNEW' > "$root/src/libbendwall.so"
printf 'qml-new' > "$root/src/qml/main.qml"
python3 - "$root" << 'PY' &
import mmap, os, sys, time
root = sys.argv[1]
fd = os.open(os.path.join(root, "dest", "libbendwall.so"), os.O_RDONLY)
mm = mmap.mmap(fd, 0, prot=mmap.PROT_READ)
open(os.path.join(root, "ready"), "w").write("1")
while not os.path.exists(os.path.join(root, "go")):
    time.sleep(0.02)
data = mm[:]
open(os.path.join(root, "mapped"), "wb").write(data)
mm.close()
os.close(fd)
PY
i=0
while [ ! -f "$root/ready" ]; do
  i=$((i + 1))
  if [ "$i" -gt 100 ]; then
    echo "mapper did not start" >&2
    exit 1
  fi
  sleep 0.02
done
atomic_install "$root/src/libbendwall.so" "$root/dest/libbendwall.so"
atomic_tree "$root/src/qml" "$root/dest/pkg"
: > "$root/go"
wait
mapped=$(cat "$root/mapped")
live=$(cat "$root/dest/libbendwall.so")
qml=$(cat "$root/dest/pkg/main.qml")
if [ "$mapped" != "OLDOLDOLD" ]; then
  echo "mapped file changed: $mapped" >&2
  exit 1
fi
if [ "$live" != "NEWNEWNEW" ]; then
  echo "installed file is $live" >&2
  exit 1
fi
if [ "$qml" != "qml-new" ]; then
  echo "qml tree is $qml" >&2
  exit 1
fi
srcmode=$(stat -c %a "$root/src/libbendwall.so")
livemode=$(stat -c %a "$root/dest/libbendwall.so")
qmlmode=$(stat -c %a "$root/dest/pkg/main.qml")
if [ "$livemode" != "$srcmode" ] || [ "$qmlmode" != "$srcmode" ]; then
  echo "mode src=$srcmode so=$livemode qml=$qmlmode" >&2
  exit 1
fi
echo ATOMIC PASS
