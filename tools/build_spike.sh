#!/bin/bash

function build_spike {
    if [ -d riscv-isa-sim ]
    then
        cd riscv-isa-sim
        git pull
    else
        git clone --recursive https://github.com/riscv-software-src/riscv-isa-sim.git
        cd riscv-isa-sim
    fi

    if [ -d build ] 
    then
    rm -rf build/*
    fi

    mkdir -p build
    cd build
    ../configure
    make -j $(nproc)

    cd ..
    cd ..
}

function main {
    if ! [ -f Makefile ] 
    then
        echo "call from project root directory"
        exit 1
    fi

    mkdir -p spike
    cd spike

    build_spike

    cd ..
}

main
