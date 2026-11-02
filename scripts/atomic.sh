# Replace a file by rename. The previous inode stays mapped, so a running
# plasmashell does not SIGBUS when libbendwall.so is installed over itself.
# POSIX sh functions share variables. Every name here is unique, and
# atomic_tree runs atomic_install in a subshell so a tree of more than one
# file cannot overwrite src or dest and then mkdir the previous file.
atomic_install() {
  inst_src=$1
  inst_dest=$2
  inst_mode=$3
  inst_dir=$(dirname "$inst_dest")
  inst_base=$(basename "$inst_dest")
  inst_tmp=""
  mkdir -p "$inst_dir" || return 1
  inst_tmp=$(mktemp "$inst_dir/.$inst_base.XXXXXX") || return 1
  if ! cp -f "$inst_src" "$inst_tmp"; then
    rm -f "$inst_tmp"
    return 1
  fi
  if [ -n "$inst_mode" ]; then
    chmod "$inst_mode" "$inst_tmp" || { rm -f "$inst_tmp"; return 1; }
  else
    chmod --reference="$inst_src" "$inst_tmp" || { rm -f "$inst_tmp"; return 1; }
  fi
  mv -f "$inst_tmp" "$inst_dest"
}

atomic_write() {
  write_dest=$1
  write_dir=$(dirname "$write_dest")
  write_base=$(basename "$write_dest")
  write_tmp=""
  mkdir -p "$write_dir" || return 1
  write_tmp=$(mktemp "$write_dir/.$write_base.XXXXXX") || return 1
  if ! cat > "$write_tmp"; then
    rm -f "$write_tmp"
    return 1
  fi
  chmod 644 "$write_tmp" || { rm -f "$write_tmp"; return 1; }
  mv -f "$write_tmp" "$write_dest"
}

atomic_tree() {
  tree_src=$1
  tree_dest=$2
  tree_list=""
  tree_file=""
  tree_rel=""
  tree_list=$(mktemp) || return 1
  if ! find "$tree_src" -type f > "$tree_list"; then
    rm -f "$tree_list"
    return 1
  fi
  while IFS= read -r tree_file; do
    [ -n "$tree_file" ] || continue
    tree_rel=${tree_file#"$tree_src"/}
    mkdir -p "$tree_dest/$(dirname "$tree_rel")" || { rm -f "$tree_list"; return 1; }
    ( atomic_install "$tree_file" "$tree_dest/$tree_rel" ) || { rm -f "$tree_list"; return 1; }
  done < "$tree_list"
  rm -f "$tree_list"
}
