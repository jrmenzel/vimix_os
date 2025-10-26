#!/usr/bin/sh

meminfo
echo starting quick usertests
cd /
usertests -q
meminfo
shutdown -h
