#!/bin/bash
set -e

# hang in concreate, unlink (both processes)
# grind hand in unlink

#TARGETS=("rv64" "arm64")
#BTYPES=("release" "release")
#TEST_SCRIPTS=("grind_8.sh" "grind_8.sh")
#EMUS=("sbi64" "arm64")
#CPUS=(5 5)
#RAM=(64 64)

# test by CPU count
#TARGETS=("arm64" "arm64" "arm64" "arm64" "arm64" "arm64" "arm64" "arm64")
#BTYPES=("release" "release" "release" "release" "release" "release" "release" "release")
#TEST_SCRIPTS=("grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh")
#EMUS=("arm64" "arm64" "arm64" "arm64" "arm64" "arm64" "arm64" "arm64")
#CPUS=(1 2 3 4 5 6 7 8)
#RAM=(64 64 64 64 64 64 64 64)

TARGETS=("rv64" "rv64" "rv64" "rv64" "rv64" "rv64" "rv64" "rv64")
BTYPES=("release" "release" "release" "release" "release" "release" "release" "release")
TEST_SCRIPTS=("grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh" "grind_8.sh")
EMUS=("sbi64" "sbi64" "sbi64" "sbi64" "sbi64" "sbi64" "sbi64" "sbi64")
CPUS=(1 2 3 4 5 6 7 8)
RAM=(64 64 64 64 64 64 64 64)


LOGDIR="build/logs"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/grind_test_$(date +%Y%m%d_%H%M%S).log"

TOTAL_RUNS=20

run_test_setting() {
	local target="$1"
	local btype="$2"
	local test_script="$3"
	local emu="$4"
    local cpu_count="$5"
    local ram="$6"

	local make_params="-j$(nproc) EXTERNAL_KERNEL_FLAGS=-D_SHUTDOWN_ON_PANIC BUILD_TYPE=${btype}"
    echo "Building with: make ${make_params} TARGET=${target} (logging to $LOGFILE)"
	make ${make_params} TARGET=${target}

	local successful_runs=0
	local script=(./tools/tests/run_test.sh "${test_script}" "${emu}" "ALL TESTS PASSED" "${cpu_count}" "${ram}")

	for i in $(seq 1 "$TOTAL_RUNS"); do
		if "${script[@]}"; then
			successful_runs=$((successful_runs + 1))
		fi
	done

	local failed_runs=$((TOTAL_RUNS - successful_runs))
	local success_rate=$((successful_runs * 100 / TOTAL_RUNS))

	echo "Test script: ${test_script}, Emulator: ${emu}, Target: ${target}, Build type: ${btype} CPUS: ${cpu_count}, RAM: ${ram}" >> "$LOGFILE"
	echo "  Total runs:      $TOTAL_RUNS" >> "$LOGFILE"
	echo "  Successful runs: $successful_runs" >> "$LOGFILE"
	echo "  Failed runs:     $failed_runs" >> "$LOGFILE"
	echo "  Success rate:    ${success_rate}%" >> "$LOGFILE"
    echo "" >> "$LOGFILE"

    # intentionally print the whole log
    cat "$LOGFILE"
}

if [ "${#TARGETS[@]}" -ne "${#BTYPES[@]}" ] || [ "${#TARGETS[@]}" -ne "${#TEST_SCRIPTS[@]}" ] || [ "${#TARGETS[@]}" -ne "${#EMUS[@]}" ]; then
	echo "Error: configuration arrays TARGETS, BTYPES, TEST_SCRIPTS, and EMUS must have the same length" >&2
	exit 1
fi

for idx in "${!TARGETS[@]}"; do
	run_test_setting "${TARGETS[$idx]}" "${BTYPES[$idx]}" "${TEST_SCRIPTS[$idx]}" "${EMUS[$idx]}" "${CPUS[$idx]}" "${RAM[$idx]}"
done

