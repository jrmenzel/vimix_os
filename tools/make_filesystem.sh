#!/bin/bash

set -e

BUILD_DIR=$1
BUILD_DIR_HOST=$2

MKFS="${BUILD_DIR_HOST}/root/usr/bin/mkfs"
FSCK="${BUILD_DIR_HOST}/root/usr/bin/fsck.vimixfs"
FS_IMAGE="${BUILD_DIR}/filesystem.img"

META_DEFAULT="--uid 0 --gid 0 --dmode 0755 --fmode 0644"
META_BINARIES="--uid 0 --gid 0 --dmode 0755 --fmode 0755"
META_PROTECTED_BINARIES="--uid 0 --gid 42 --dmode 0755 --fmode 4710"
META_ROOT="--uid 0 --gid 0 --dmode 0750 --fmode 0640"
META_USER="--uid 1000 --gid 1000 --dmode 0755 --fmode 0644"
META_ADA="--uid 1001 --gid 1001 --dmode 0755 --fmode 0644"

$MKFS --fs $FS_IMAGE --create 12288
$MKFS --fs $FS_IMAGE --in ./root/ / $META_DEFAULT
$MKFS --fs $FS_IMAGE --in ${BUILD_DIR}/root/usr/bin/ /usr/bin/ $META_BINARIES
$MKFS --fs $FS_IMAGE --in ${BUILD_DIR}/root/usr/local/bin/ /usr/local/bin/ $META_BINARIES
$MKFS --fs $FS_IMAGE --in ./README.md /README.md $META_DEFAULT
$MKFS --fs $FS_IMAGE --meta /root/ $META_ROOT
$MKFS --fs $FS_IMAGE --meta /home/user/ $META_USER
$MKFS --fs $FS_IMAGE --meta /home/ada/ $META_ADA
$MKFS --fs $FS_IMAGE --meta /tmp/ --uid 0 --gid 0 --dmode 1777
$MKFS --fs $FS_IMAGE --meta /usr/bin/su $META_PROTECTED_BINARIES
$MKFS --fs $FS_IMAGE --meta /usr/bin/usertests $META_PROTECTED_BINARIES
$MKFS --fs $FS_IMAGE --meta /usr/bin/shutdown $META_PROTECTED_BINARIES
$MKFS --fs $FS_IMAGE --meta /etc/shadow --uid 0 --gid 0 --fmode 600

if [ -f ${BUILD_DIR}/root/autoexec.sh ] 
then
    $MKFS --fs $FS_IMAGE --in ${BUILD_DIR}/root/autoexec.sh /autoexec.sh $META_DEFAULT
fi

if [ -f ${BUILD_DIR}/boot/kernel-vimix.xdbg ] 
then
    $MKFS --fs $FS_IMAGE --in ${BUILD_DIR}/boot/kernel-vimix.xdbg /kernel-vimix.xdbg $META_DEFAULT
fi

#$FSCK $FS_IMAGE
