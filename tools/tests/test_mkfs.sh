#!/bin/bash
set -e

BUILD="./build_host"
MKFS="$BUILD/root/usr/bin/mkfs"
FSCK="$BUILD/root/usr/bin/fsck.vimixfs"
FS_IMAGE="$BUILD/test_fs.img"

mkdir -p $BUILD

echo -e "\nCreating tiny test filesystem image...\n"
$MKFS --fs $FS_IMAGE --create 16
$FSCK $FS_IMAGE

rm -f $FS_IMAGE

echo -e "\nCreating small test filesystem image...\n"
$MKFS --fs $FS_IMAGE --create 1024
$FSCK $FS_IMAGE

echo -e "\nCopy in test files...\n"
$MKFS --fs $FS_IMAGE --in ./root/tests/echo.sh /autoexec.sh
$FSCK $FS_IMAGE

$MKFS --fs $FS_IMAGE --out $BUILD/extract1
diff ./root/tests/echo.sh $BUILD/extract1/autoexec.sh

echo -e "\nCopy in test files replacing old file...\n"
$MKFS --fs $FS_IMAGE --in ./root/tests/forktest.sh /autoexec.sh
$FSCK $FS_IMAGE

$MKFS --fs $FS_IMAGE --out $BUILD/extract2
diff ./root/tests/forktest.sh $BUILD/extract2/autoexec.sh


