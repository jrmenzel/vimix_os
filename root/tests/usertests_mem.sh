#!/usr/bin/sh

echo -n 256 > /sys/kmem/bio/min
echo -n 0 > /sys/kmem/bio/max_free
meminfo

echo starting memory related usertests
cd /
usertests -m 2
meminfo
shutdown -h
