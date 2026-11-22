#!/bin/bash
MARKER=/var/tmp/cores/.processed
touch "$MARKER"
for c in /var/tmp/cores/core.*; do
  [ -f "$c" ] || continue
  [ "$c" -nt "$MARKER" ] || continue
  {
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') CORE: $c ==="
    gdb -batch -ex 'set pagination off' -ex bt /opt/multics/multics.x64 "$c" 2>/dev/null
    echo
  } >> /var/tmp/crash_reports.log
  mv "$c" "$c.done"
done
touch "$MARKER"
