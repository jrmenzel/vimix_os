#!/bin/bash
set -e

if ! [ -f Makefile ] 
then
    echo "call from project root directory"
    exit 1
fi

BUILD_ROOT=build_tests
if [ -d ${BUILD_ROOT} ] 
then
rm -r ${BUILD_ROOT}/*
fi
mkdir -p ${BUILD_ROOT}

COMMON_FLAGS=EXTRA_DEBUG_FLAGS=-DDEBUG_AUTOSTART_USERTESTS


# test_config [32|64] [debug|release] [yes|no] [32] [1]
#                |          |             |     |    \-- CPUs
#                |          |             |     \-- MB RAM
#                |          |             \-- SBI support
#                |          \-- build type
#                \- bit width
function test_config {
    # build and run OS
    BUILD_DIR=${BUILD_ROOT}/build_$1_$2_$3_SBI_$4mb_$5cpu
    make clean BUILD_DIR=${BUILD_DIR}
    make qemu -j 1 BITWIDTH=$1 BUILD_TYPE=$2 SBI_SUPPORT=$3 MEMORY_SIZE=$4 CPUS=$5 ${COMMON_FLAGS} BUILD_DIR=${BUILD_DIR}
    ./build_host/root/usr/bin/mkfs ${BUILD_DIR}/filesystem.img --out ${BUILD_DIR}/root_out

    TEST_RESULT_DIR=${BUILD_DIR}/root_out/test_results

    set +e
    # check forktest result
    grep "fork test OK" ${TEST_RESULT_DIR}/forktest.txt > /dev/null
    if [ $? == 0 ]
    then
        touch ${BUILD_ROOT}/PASS_FORK_$1_$2_$3_SBI_$4mb_$5cpu.txt
    else
        touch ${BUILD_ROOT}/FAIL_FORK_$1_$2_$3_SBI_$4mb_$5cpu.txt
    fi

    # check usertests result
    grep "ALL TESTS PASSED" ${TEST_RESULT_DIR}/usertests.txt > /dev/null
    if [ $? == 0 ]
    then
        touch ${BUILD_ROOT}/PASS_USERTESTS_$1_$2_$3_SBI_$4mb_$5cpu.txt
    else
        touch ${BUILD_ROOT}/FAIL_USERTESTS_$1_$2_$3_SBI_$4mb_$5cpu.txt
    fi

    # check if "echo Hello World > h_world.txt" worked
    FILE_CONTENT=$(cat ${TEST_RESULT_DIR}/h_world.txt)
    if [ "$FILE_CONTENT" == "Hello World" ]
    then
        touch ${BUILD_ROOT}/PASS_ECHO_TO_FILE_$1_$2_$3_SBI_$4mb_$5cpu.txt
    else
        touch ${BUILD_ROOT}/FAIL_ECHO_TO_FILE_$1_$2_$3_SBI_$4mb_$5cpu.txt
    fi

    # check if "cat /README.md | grep RISC | wc > wc.txt" worked
    FILE_CONTENT=$(cat ${TEST_RESULT_DIR}/wc.txt)
    echo $FILE_CONTENT
    if [[ ${FILE_CONTENT:0:8} == "3 66 496" ]];
    then
        touch ${BUILD_ROOT}/PASS_PIPELINE_TO_FILE_$1_$2_$3_SBI_$4mb_$5cpu.txt
    else
        touch ${BUILD_ROOT}/FAIL_PIPELINE_TO_FILE_$1_$2_$3_SBI_$4mb_$5cpu.txt
    fi

    set -e
}

test_config 32 debug no 32 1 
test_config 64 debug no 32 1 

test_config 32 release no 32 1 
test_config 64 release no 32 1 

test_config 32 debug yes 32 1 
test_config 64 debug yes 32 1 

test_config 32 release yes 32 1 
test_config 64 release yes 32 1 

test_config 32 debug no 64 4 
test_config 64 debug no 64 4 

test_config 32 release no 64 4 
test_config 64 release no 64 4 

test_config 32 debug yes 64 4 
test_config 64 debug yes 64 4 

test_config 32 release yes 64 4 
test_config 64 release yes 64 4 
