#!/bin/bash
set -e

BUILD_ROOT=build_tests
EXTERNAL_KERNEL_FLAGS=-D_SHUTDOWN_ON_PANIC
EMU_FLAGS=SPIKE_BUILD=./spike/riscv-isa-sim/build/

RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# platform debug|release
function build_config {
    # build VIMIX
    echo -e "${YELLOW}Build config: $1 $2${NC}"
    BUILD_DIR=${BUILD_ROOT}/build_$1_$2
    make clean BUILD_DIR=${BUILD_DIR}
    make -j $(nproc) PLATFORM=$1 BUILD_TYPE=$2 BUILD_DIR=${BUILD_DIR} || exit 1
}

# platform debug|release|compiler
# on Ubuntu multiple 64-bit gcc versions can be installed in parallel
function build_config_compiler {
    # build VIMIX
    echo -e "${YELLOW}Build config: $1 $2 on gcc$3 ${NC}"
    BUILD_DIR=${BUILD_ROOT}/build_$1_$2_gcc$3
    make clean BUILD_DIR=${BUILD_DIR}
    make -j $(nproc) PLATFORM=$1 BUILD_TYPE=$2 BUILD_DIR=${BUILD_DIR} TOOLPREFIX=riscv64-linux-gnu- GCCPOSTFIX=-$3 || exit 1
}

function build_config_release_debug {
    build_config $1 debug
    build_config $1 release
}

function build_compiler_versions {
    build_config_compiler $1 debug 11
    build_config_compiler $1 release 11
    build_config_compiler $1 debug 12
    build_config_compiler $1 release 12
    build_config_compiler $1 debug 13
    build_config_compiler $1 release 13
    build_config_compiler $1 debug 14
    build_config_compiler $1 release 14
}

function test_build_compiler_versions {
    build_compiler_versions qemu64
    build_compiler_versions spike64
    build_compiler_versions qemu32
    build_compiler_versions spike32
    build_compiler_versions visionfive2
}

# platform debug|release emulator testname expected_result CPUs
function test_config {
    BUILD_DIR=${BUILD_ROOT}/build_$1_$2
    RESULT_FILE=${BUILD_ROOT}/testrun_$1_$2_$4_on_$3_$6_CPUs.txt
    EXPECTED_RESULT="$5"
    echo -e "${YELLOW}Test config: $1 $2 ${NC}"
    # copy test
    cp ${BUILD_DIR}/root/tests/$4.sh ${BUILD_DIR}/root/tests/autoexec.sh
    # re-create fs image
    make -j $(nproc) PLATFORM=$1 BUILD_TYPE=$2 EXTERNAL_KERNEL_FLAGS="$(EXTERNAL_KERNEL_FLAGS)" BUILD_DIR=${BUILD_DIR} ${BUILD_DIR}/filesystem.img || exit 1
    # run test
    echo -e "${YELLOW}Test config: make PLATFORM=$1 BUILD_TYPE=$2 BUILD_DIR=${BUILD_DIR} CPUS=$6 ${EMU_FLAGS} $3 ${NC}"
    make PLATFORM=$1 BUILD_TYPE=$2 BUILD_DIR=${BUILD_DIR} CPUS=$6 ${EMU_FLAGS} $3 > ${RESULT_FILE} || exit 1
    # get file system content, clear target dir fist:
    #if [ -d ${BUILD_DIR}/root_out ]
    #then
    #rm -rf ${BUILD_DIR}/root_out/*
    #fi
    #./build_host/root/usr/bin/mkfs ${BUILD_DIR}/filesystem.img --out ${BUILD_DIR}/root_out

    set +e
    # check test result
    grep "${EXPECTED_RESULT}" ${RESULT_FILE} > /dev/null
    if [ $? == 0 ]
    then
        touch ${BUILD_ROOT}/PASS_$1_$2_$4_on_$3.txt
    else
        touch ${BUILD_ROOT}/FAIL_$1_$2_$4_on_$3.txt
    fi
    set -e
}

function prepare_tests {
    if [ -d ${BUILD_ROOT} ] 
    then
    rm -rf ${BUILD_ROOT}/*
    fi
    mkdir -p ${BUILD_ROOT}
}

function test_build_qemu {
    build_config_release_debug qemu32
    build_config_release_debug qemu64
}

function test_build {
    test_build_qemu

    build_config_release_debug spike32
    build_config_release_debug spike64
    build_config_release_debug visionfive2

    echo "Compiling done"
}

function test_boot {
    # trivial test to verify boot and console out
    test_config qemu32 debug   qemu echo "ALL TESTS PASSED" 1
    test_config qemu32 release qemu echo "ALL TESTS PASSED" 3
    test_config qemu64 debug   qemu echo "ALL TESTS PASSED" 5
    test_config qemu64 release qemu echo "ALL TESTS PASSED" 8

    test_config spike32 debug   spike echo "ALL TESTS PASSED" 1
    test_config spike32 release spike echo "ALL TESTS PASSED" 2
    test_config spike64 debug   spike echo "ALL TESTS PASSED" 3
    test_config spike64 release spike echo "ALL TESTS PASSED" 4
}

function test_grind {
    test_config qemu32 release qemu grind_2 "grind passed" 4
    test_config qemu64 debug   qemu grind_2 "grind passed" 4

    test_config qemu64 release qemu grind_8 "grind passed" 8
    test_config qemu32 debug   qemu grind_8 "grind passed" 8
}

function test_functional {
    # passed if it didn't crash before
    test_config qemu64 release qemu stack "Foo of 1199 is 1199" 2
    test_config qemu32 release qemu stack "Foo of 1199 is 1199" 3

    test_config qemu64 release qemu grind_2 "grind passed" 4

    test_config qemu32 release qemu forktest    "ALL TESTS PASSED" 1
    test_config qemu32 debug   qemu forktest    "ALL TESTS PASSED" 2
    test_config qemu64 release qemu forktest    "ALL TESTS PASSED" 5
    test_config qemu64 debug   qemu forktest    "ALL TESTS PASSED" 8

    test_config qemu64 release qemu usertests   "ALL TESTS PASSED" 1
    test_config qemu64 debug   qemu usertests_q "ALL TESTS PASSED" 5
    test_config qemu32 release qemu usertests_q "ALL TESTS PASSED" 3
    test_config qemu32 debug   qemu usertests   "ALL TESTS PASSED" 8
}

function print_stats {
    # print stats
    echo "Kernels:"
    ls -lh ${BUILD_ROOT}/*/kernel-vimix

    echo "Passed:"
    ls ${BUILD_ROOT} | grep PASS

    echo "Failed:"
    ls ${BUILD_ROOT} | grep FAIL
}

function main {
    if ! [ -f Makefile ] 
    then
        echo "call from project root directory"
        exit 1
    fi

    prepare_tests

    if [ "$1" = "build" ]
    then 
        test_build
        print_stats
        exit 0
    fi

    if [ "$1" = "compilers" ]
    then 
        test_build_compiler_versions
        print_stats
        exit 0
    fi

    if [ "$1" = "boot" ]
    then 
        test_build
        test_boot
        print_stats
        exit 0
    fi

    if [ "$1" = "grind" ]
    then 
        test_build_qemu
        test_grind
        print_stats
        exit 0
    fi

    if [ "$1" = "all" ]
    then 
        test_build
        test_boot
        test_functional
        print_stats
        exit 0
    fi

    echo "Usage: run_tests.sh {build | boot | all}"
    echo " build: builds for all plattforms"
    echo " boot:  builds and boots on qemu"
    echo " all:   builds and performs usertests, forktest, grind, etc."
}

main $1
