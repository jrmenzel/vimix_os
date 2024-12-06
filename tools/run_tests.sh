#!/bin/bash
set -e

BUILD_ROOT=build_tests
EMU_FLAGS=SPIKE_BUILD=../riscv-simulator/riscv-isa-sim/build/

# platform debug|release
function build_config {
    # build VIMIX
    BUILD_DIR=${BUILD_ROOT}/build_$1_$2
    make clean BUILD_DIR=${BUILD_DIR}
    make -j $(nproc) PLATFORM=$1 BUILD_TYPE=$2 BUILD_DIR=${BUILD_DIR} || exit 1
}

function build_config_release_debug {
    build_config $1 debug
    build_config $1 release
}

# platform debug|release emulator testname expected_result CPUs
function test_config {
    BUILD_DIR=${BUILD_ROOT}/build_$1_$2
    RESULT_FILE=${BUILD_ROOT}/testrun_$1_$2_$4_on_$3_$6_CPUs.txt
    EXPECTED_RESULT="$5"
    # copy test
    cp ${BUILD_DIR}/root/tests/$4.sh ${BUILD_DIR}/root/tests/autoexec.sh
    # re-create fs image
    make -j $(nproc) PLATFORM=$1 BUILD_TYPE=$2 BUILD_DIR=${BUILD_DIR} ${BUILD_DIR}/filesystem.img || exit 1
    # run test
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

function main {
    if ! [ -f Makefile ] 
    then
        echo "call from project root directory"
        exit 1
    fi

    if [ -d ${BUILD_ROOT} ] 
    then
    rm -rf ${BUILD_ROOT}/*
    fi
    mkdir -p ${BUILD_ROOT}

    build_config_release_debug qemu32
    build_config_release_debug qemu64
    build_config_release_debug spike32
    build_config_release_debug spike64
    build_config_release_debug visionfive2

    echo "Compiling done"

    # trivial test to verify boot and console out
    test_config qemu32 debug   qemu echo "ALL TESTS PASSED" 1
    test_config qemu32 release qemu echo "ALL TESTS PASSED" 3
    test_config qemu64 debug   qemu echo "ALL TESTS PASSED" 5
    test_config qemu64 release qemu echo "ALL TESTS PASSED" 8

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

    # print stats
    echo "Kernels:"
    ls -lh ${BUILD_ROOT}/*/kernel-vimix

    echo "Passed:"
    ls ${BUILD_ROOT} | grep PASS

    echo "Failed:"
    ls ${BUILD_ROOT} | grep FAIL
}

main
