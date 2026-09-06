#!/usr/bin/sh

echo disable sync on virtio0, ignore errors if no such device is present
echo -n 0 > /sys/dev/virtio_disk0/sync

echo -n 256 > /sys/kmem/bio/min
echo -n 0 > /sys/kmem/bio/max_free
meminfo

echo starting often failing usertests
cd /
usertests -m 64
meminfo
shutdown -h
