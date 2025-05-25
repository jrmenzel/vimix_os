#!/bin/bash

BINARY=build/boot/kernel-vimix
BINARY_SEARCH_PATH=build/root/usr/bin
BINARY_SEARCH_PATH_HOST=build_host/root/usr/bin
OBJDUMP=riscv64-elf-objdump
OBJDUMP_HOST=objdump
OBJDUMP_PARAM="--disassembler-color=color --visualize-jumps=color -l"


function print_help {
    echo "usage: disassemble.sh -a <address> <binary> <host>"
    echo "       disassemble.sh -f <function> <binary> <host>"
    echo "<binary> is optional, default is the kernel"
    echo "<binary> can be a path or a file in /usr/bin"
    echo "<host> is optional, if set user space apps for the host get disassembled"
    exit 1
}

function disasm_addr {
    STOP=$(( $1 + $2 ))
    # start must hits a valid instruction (with C extension, we mix 2 and 4 byte instructions)
    # so we can't just start at "given address - some range"
    $OBJDUMP -d --start-address=$1 --stop-address=$STOP $OBJDUMP_PARAM $BINARY
}

function disasm_func {
    $OBJDUMP --disassemble=$1 $OBJDUMP_PARAM $BINARY
}

function main {
    if [ "$1" = "" ]
    then 
        print_help
    fi

    if [ "$4" == "host" ]
    then
        # disassemble host binary
        OBJDUMP=$OBJDUMP_HOST
        BINARY_SEARCH_PATH=$BINARY_SEARCH_PATH_HOST
    fi

    # set optional binary, defaults to the kernel
    if [ "$3" != "" ]
    then
        if [ -f $3 ] 
        then
            BINARY=$3
        elif [ -f $BINARY_SEARCH_PATH"/"$3 ] 
        then
            BINARY=$BINARY_SEARCH_PATH"/"$3
        fi
    fi


    if [ "$1" = "-a" ]
    then
        # disassemble at address
        ADDRESS=$2
        RANGE=0x20
        disasm_addr $ADDRESS $RANGE
    elif [ "$1" = "-f" ]
    then
        # disassemble a function
        FUNC=$2
        disasm_func $FUNC
    else
        print_help
    fi
}

main $1 $2 $3 $4
