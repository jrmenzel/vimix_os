#!/bin/bash
set -e

usage() {
  echo "Usage: $0 SCRIPT [EMU] [SEARCH_STRING] [CPU_COUNT] [MEMORY]"
  echo "  SCRIPT        : file name under root/tests (e.g. echo.sh)"
  echo "  EMU           : emulator setting (default: rv64)"
  echo "  SEARCH_STRING : string to search the log for (default: ALL TESTS PASSED)"
  echo "  CPU_COUNT     : number of CPUs to use for qemu-run (default: 4)"
  echo "  MEMORY        : amount of memory in megabyte to use for qemu-run (default: 64)"
  echo "  EMU_COMMAND   : default: qemu, can be spike"
  exit 2
}

if [ "$#" -lt 1 ] || [ "$#" -gt 6 ]; then
  usage
fi

SCRIPT="$1"
EMU="${2:-rv64}"
SEARCH="${3:-ALL TESTS PASSED}"
CPU_COUNT="${4:-4}"
MEMORY="${5:-64}"
EMU_APP="${6:-qemu}"

# qemu-run does not rebuild, but Make still needs the target in order to select
# architecture-specific settings such as the deployed kernel file format.
case "$EMU" in
  arm64|raspi4|kvm)
    TARGET="arm64"
    ;;
  sbi32|sbi-rdisk32)
    TARGET="rv32"
    ;;
  sbi64|sbi-rdisk64)
    TARGET="rv64"
    ;;
  m32|m-rdisk32)
    TARGET="rv32m"
    ;;
  m64|m-rdisk64)
    TARGET="rv64m"
    ;;
  *)
    echo "Error: cannot determine TARGET from emulator '$EMU'" >&2
    exit 2
    ;;
esac

# check if script exists
TARGET_SCRIPT="root/tests/${SCRIPT}"
if [ ! -f "$TARGET_SCRIPT" ]; then
  echo "Error: script '$TARGET_SCRIPT' not found" >&2
  exit 2
fi

LOGDIR="build/logs"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/${EMU_APP}-run_${SCRIPT}_${EMU}_$(date +%Y%m%d_%H%M%S).log"

echo "Setting autoexec to $TARGET_SCRIPT"
./tools/set_autoexec.sh "$TARGET_SCRIPT"

if [ "$EMU" = "raspi4" ]
then
    MAKE_PARAMS="EMU=$EMU"
else
    MAKE_PARAMS="EMU=$EMU CPUS=$CPU_COUNT MEMORY_SIZE=$MEMORY"
fi

echo "Running: make ${EMU_APP}-run TARGET=$TARGET $MAKE_PARAMS (logging to $LOGFILE)"
# run make, capture both stdout+stderr, tee to logfile while printing to console
if ! make ${EMU_APP}-run TARGET="$TARGET" $MAKE_PARAMS 2>&1 | tee "$LOGFILE"; then
  echo "make ${EMU_APP}-run exited with non-zero status; continuing to search log" >&2
fi

if grep -F -- "$SEARCH" "$LOGFILE" > /dev/null 2>&1; then
  echo "success"
  exit 0
else
  echo "failure"
  exit 1
fi
