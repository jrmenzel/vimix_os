#!/usr/bin/sh

echo -n 256 > /sys/kmem/bio/min
echo -n 0 > /sys/kmem/bio/max_free
meminfo

echo starting usertests
cd /
usertests
meminfo
shutdown -h
