# Replace a file by rename. The previous inode stays mapped, so a running
# plasmashell does not SIGBUS when libbendwall.so is installed over itself.
atomic_install() {
  src=$1
  dest=$2
  mode=$3
  dir=$(dirname "$dest")
  base=$(basename "$dest")
  tmp=""
  mkdir -p "$dir" || return 1
  tmp=$(mktemp "$dir/.$base.XXXXXX") || return 1
  if ! cp -f "$src" "$tmp"; then
    rm -f "$tmp"
    return 1
  fi
  if [ -n "$mode" ]; then
    chmod "$mode" "$tmp" || { rm -f "$tmp"; return 1; }
  else
    chmod --reference="$src" "$tmp" || { rm -f "$tmp"; return 1; }
  fi
  mv -f "$tmp" "$dest"
}

atomic_write() {
  dest=$1
  dir=$(dirname "$dest")
  base=$(basename "$dest")
  tmp=""
  mkdir -p "$dir" || return 1
  tmp=$(mktemp "$dir/.$base.XXXXXX") || return 1
  if ! cat > "$tmp"; then
    rm -f "$tmp"
    return 1
  fi
  chmod 644 "$tmp" || { rm -f "$tmp"; return 1; }
  mv -f "$tmp" "$dest"
}

atomic_tree() {
  src=$1
  dest=$2
  list=""
  list=$(mktemp) || return 1
  if ! find "$src" -type f > "$list"; then
    rm -f "$list"
    return 1
  fi
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    rel=${f#"$src"/}
    mkdir -p "$dest/$(dirname "$rel")" || { rm -f "$list"; return 1; }
    atomic_install "$f" "$dest/$rel" || { rm -f "$list"; return 1; }
  done < "$list"
  rm -f "$list"
}
