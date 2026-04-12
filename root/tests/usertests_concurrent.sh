#!/usr/bin/sh

echo -n 256 > /sys/kmem/bio/min
echo -n 0 > /sys/kmem/bio/max_free
meminfo

echo starting concurrency related usertests
cd /
usertests -m 4
meminfo
shutdown -h
