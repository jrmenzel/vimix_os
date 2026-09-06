#!/usr/bin/sh

echo disable sync on virtio0, ignore errors if no such device is present
echo -n 0 > /sys/dev/virtio_disk0/sync

echo starting grind with 2 threads for 5000 iterations
grind 2 5000
shutdown -h
