#!/bin/bash
set -e

usage() {
  echo "Usage: $0 SCRIPT [PLATFORM] [SEARCH_STRING]"
  echo "  SCRIPT        : file name under root/tests (e.g. echo.sh)"
  echo "  PLATFORM      : platform name (default: qemu64)"
  echo "  SEARCH_STRING : string to search the log for (default: ALL TESTS PASSED)"
  exit 2
}

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
  usage
fi

SCRIPT="$1"
PLATFORM="${2:-qemu64}"
SEARCH="${3:-ALL TESTS PASSED}"

# check if script exists
TARGET_SCRIPT="root/tests/${SCRIPT}"
if [ ! -f "$TARGET_SCRIPT" ]; then
  echo "Error: script '$TARGET_SCRIPT' not found" >&2
  exit 2
fi

LOGDIR="build/logs"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/qemu-run_${SCRIPT}_${PLATFORM}_$(date +%Y%m%d_%H%M%S).log"

echo "Setting autoexec to $TARGET_SCRIPT"
./tools/set_autoexec.sh "$TARGET_SCRIPT"

echo "Running: make qemu-run PLATFORM=$PLATFORM (logging to $LOGFILE)"
# run make, capture both stdout+stderr, tee to logfile while printing to console
if ! make qemu-run PLATFORM="$PLATFORM" 2>&1 | tee "$LOGFILE"; then
  echo "make qemu-run exited with non-zero status; continuing to search log" >&2
fi

if grep -F -- "$SEARCH" "$LOGFILE" > /dev/null 2>&1; then
  echo "success"
  exit 0
else
  echo "failure"
  exit 1
fi
