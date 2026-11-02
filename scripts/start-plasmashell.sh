#!/bin/sh
# Restart plasmashell so it maps the library install.sh just renamed into place.
# pkill -f also matches a script whose command line contains plasmashell, so
# this uses an exact comm match.
set -e
pids=$(pgrep -x plasmashell || true)
if [ -n "$pids" ]; then
  # shellcheck disable=SC2086
  kill $(pgrep -x plasmashell) || true
  i=0
  while pgrep -x plasmashell >/dev/null 2>&1; do
    i=$((i + 1))
    if [ "$i" -ge 50 ]; then
      # shellcheck disable=SC2046
      kill -9 $(pgrep -x plasmashell) || true
      break
    fi
    sleep 0.1
  done
  i=0
  while pgrep -x plasmashell >/dev/null 2>&1; do
    i=$((i + 1))
    if [ "$i" -ge 50 ]; then
      echo "plasmashell did not exit" >&2
      exit 1
    fi
    sleep 0.1
  done
fi
if command -v kstart >/dev/null 2>&1; then
  exec kstart plasmashell
fi
if command -v kstart5 >/dev/null 2>&1; then
  exec kstart5 plasmashell
fi
if ! command -v plasmashell >/dev/null 2>&1; then
  echo "plasmashell is not installed" >&2
  exit 1
fi
plasmashell >/dev/null 2>&1 &
