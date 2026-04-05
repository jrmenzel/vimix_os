#!/bin/bash
set -e

TEST=$1
MKFS=./build_host/root/usr/bin/mkfs

if ! [ -f Makefile ] 
then
    echo "call from project root directory"
    exit 1
fi

$MKFS --fs build/filesystem.img --in $TEST /autoexec.sh --uid 0 --gid 0 --dmode 0755 --fmode 0755
