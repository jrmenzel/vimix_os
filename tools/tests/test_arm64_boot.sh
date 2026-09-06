#!/bin/bash
# Repeated boots exercise cache publication during SMP startup and the first
# switch to a user page table. Run after `make TARGET=arm64`.
set -euo pipefail

EMU="${1:-kvm}"
REPEATS="${2:-10}"
case "$EMU" in
  arm64|kvm) ;;
  *) echo "Usage: $0 [kvm|arm64] [repeat-count]" >&2; exit 2 ;;
esac
if ! [[ "$REPEATS" =~ ^[1-9][0-9]*$ ]]; then
  echo "repeat-count must be a positive integer" >&2
  exit 2
fi

cd "$(dirname "$0")/../.."
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
LOG_DIR="build/logs/arm64-boot-${EMU}-$(date +%Y%m%d_%H%M%S)"
mkdir -p "$LOG_DIR"

# Each QEMU boot receives its own in-memory ramdisk. Keep the deployed
# filesystem and its autoexec script intact.
cp build/boot/filesystem.img "$TEST_DIR/filesystem.img"
cat > "$TEST_DIR/autoexec.sh" <<'EOF'
#!/usr/bin/sh
# The first process can run before the boot CPU finishes starting all cores.
sleep 1
echo ARM64_BOOT_OK
shutdown -h
EOF
./build_host/root/usr/bin/mkfs --fs "$TEST_DIR/filesystem.img" \
  --in "$TEST_DIR/autoexec.sh" /autoexec.sh \
  --uid 0 --gid 0 --dmode 0755 --fmode 0755

for CPUS in 1 2 4; do
  for ((RUN = 1; RUN <= REPEATS; RUN++)); do
    LOG="$LOG_DIR/cpus-${CPUS}-run-${RUN}.log"
    if ! timeout 30s make qemu-run TARGET=arm64 EMU="$EMU" CPUS="$CPUS" \
      FILESYSTEM_IMG_DEPLOY="$TEST_DIR/filesystem.img" > "$LOG" 2>&1; then
      echo "FAIL: $LOG" >&2
      exit 1
    fi
    if ! grep -q 'ARM64_BOOT_OK' "$LOG" ||
       ! grep -q 'All other CPUs halted.' "$LOG" ||
       grep -Eq 'Kernel PANIC|Fatal:|Failed to boot CPU|failed to start within timeout' "$LOG"; then
      echo "FAIL: $LOG" >&2
      exit 1
    fi
    echo "PASS: $EMU, $CPUS CPUs, boot $RUN/$REPEATS"
  done
done
