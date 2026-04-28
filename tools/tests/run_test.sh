#!/bin/bash
set -e

usage() {
  echo "Usage: $0 SCRIPT [PLATFORM] [SEARCH_STRING] [CPU_COUNT] [MEMORY]"
  echo "  SCRIPT        : file name under root/tests (e.g. echo.sh)"
  echo "  PLATFORM      : platform name (default: qemu64)"
  echo "  SEARCH_STRING : string to search the log for (default: ALL TESTS PASSED)"
  echo "  CPU_COUNT     : number of CPUs to use for qemu-run (default: 4)"
  echo "  MEMORY        : amount of memory in megabyte to use for qemu-run (default: 64)"
  exit 2
}

if [ "$#" -lt 1 ] || [ "$#" -gt 5 ]; then
  usage
fi

SCRIPT="$1"
PLATFORM="${2:-qemu64}"
SEARCH="${3:-ALL TESTS PASSED}"
CPU_COUNT="${4:-4}"
MEMORY="${5:-64}"

# check if script exists
TARGET_SCRIPT="root/tests/${SCRIPT}"
if [ ! -f "$TARGET_SCRIPT" ]; then
  echo "Error: script '$TARGET_SCRIPT' not found" >&2
  exit 2
fi

case "$PLATFORM" in
  qemu32|qemu64)
    EMU="qemu"
    ;;
  spike32|spike64)
    EMU="spike"
    ;;
  *)
    EMU="qemu"
    ;;
esac

LOGDIR="build/logs"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/${EMU}-run_${SCRIPT}_${PLATFORM}_$(date +%Y%m%d_%H%M%S).log"

echo "Setting autoexec to $TARGET_SCRIPT"
./tools/set_autoexec.sh "$TARGET_SCRIPT"

MAKE_PARAMS="PLATFORM=$PLATFORM CPUS=$CPU_COUNT MEMORY_SIZE=$MEMORY"
echo "Running: make ${EMU}-run $MAKE_PARAMS (logging to $LOGFILE)"
# run make, capture both stdout+stderr, tee to logfile while printing to console
if ! make ${EMU}-run $MAKE_PARAMS 2>&1 | tee "$LOGFILE"; then
  echo "make ${EMU}-run exited with non-zero status; continuing to search log" >&2
fi

if grep -F -- "$SEARCH" "$LOGFILE" > /dev/null 2>&1; then
  echo "success"
  exit 0
else
  echo "failure"
  exit 1
fi
