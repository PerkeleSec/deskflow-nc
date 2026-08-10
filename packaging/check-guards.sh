#!/usr/bin/env bash
#
# Guard census: fail if any file has lost DESKFLOW_NO_CLIPBOARD guards.
#
# The CI symbol check catches a dropped *platform* guard, because the OS
# clipboard APIs reappear in the binary's import table. It cannot catch a
# dropped *protocol* guard -- nothing in ServerProxy.cpp or ClientProxy1_6.cpp
# references an OS symbol, so clipboard data would start crossing the wire
# again with every existing check still green.
#
# This closes that gap the cheap way: record how many guards each file carries,
# and fail if a count drops or a file falls off the list entirely. Adding guards
# is always fine. Rebasing onto upstream is when this earns its keep, since the
# usual way to lose a guard is a conflict resolution that quietly takes
# upstream's side of a hunk.
#
#   packaging/check-guards.sh            # verify against the baseline
#   packaging/check-guards.sh --update   # re-record after an intentional change
#
set -euo pipefail

cd "$(dirname "$0")/.."
BASELINE="packaging/clipboard-guards.baseline"

census() {
  # shellcheck disable=SC2312
  grep -rc "DESKFLOW_NO_CLIPBOARD" src/ \
      --include=*.cpp --include=*.h --include=*.mm --include=*.m 2>/dev/null \
    | grep -v ':0$' \
    | sort
}

if [ "${1:-}" = "--update" ]; then
  census > "$BASELINE"
  echo "recorded $(wc -l < "$BASELINE") guarded files to $BASELINE"
  exit 0
fi

if [ ! -f "$BASELINE" ]; then
  echo "error: $BASELINE is missing; run '$0 --update' to create it" >&2
  exit 1
fi

CURRENT=$(census)
status=0

while IFS=: read -r file expected; do
  [ -n "$file" ] || continue
  actual=$(printf '%s\n' "$CURRENT" | awk -F: -v f="$file" '$1 == f {print $2}')
  if [ -z "$actual" ]; then
    echo "FAIL: $file has no DESKFLOW_NO_CLIPBOARD guards at all (expected $expected)"
    status=1
  elif [ "$actual" -lt "$expected" ]; then
    echo "FAIL: $file has $actual guards, expected at least $expected"
    status=1
  elif [ "$actual" -gt "$expected" ]; then
    echo "note: $file has $actual guards, baseline says $expected (guards were added; run --update to re-record)"
  fi
done < "$BASELINE"

if [ "$status" -ne 0 ]; then
  echo
  echo "A guard was lost. If this was deliberate, re-record with:"
  echo "  packaging/check-guards.sh --update"
  echo "If it was a rebase conflict resolution, restore the guard instead --"
  echo "the clipboard-symbol checks will NOT catch a missing protocol guard."
  exit 1
fi

echo "OK: all $(grep -c . "$BASELINE") guarded files still carry their guards"
