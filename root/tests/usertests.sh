#!/usr/bin/sh

meminfo
echo starting usertests
usertests
meminfo
shutdown -h
