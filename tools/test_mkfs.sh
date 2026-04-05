#!/bin/bash

set -e

MKFS="./build_host/root/usr/bin/mkfs"
FSCK="./build_host/root/usr/bin/fsck.vimixfs"
FS_IMAGE="./build/test_fs.img"

echo -e "\nCreating tiny test filesystem image...\n"
$MKFS --fs $FS_IMAGE --create 16
$FSCK $FS_IMAGE

echo -e "\nCreating small test filesystem image...\n"
$MKFS --fs $FS_IMAGE --create 1024
$FSCK $FS_IMAGE

echo -e "\nCopying in test files...\n"
$MKFS --fs $FS_IMAGE --in ./root /
$FSCK $FS_IMAGE

