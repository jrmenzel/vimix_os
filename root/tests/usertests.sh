#!/usr/bin/sh

meminfo
echo starting usertests
cd /
usertests
meminfo
shutdown -h
