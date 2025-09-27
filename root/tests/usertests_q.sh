#!/usr/bin/sh

meminfo
echo starting quick usertests
usertests -q
meminfo
shutdown -h
