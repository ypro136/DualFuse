#!/bin/sh
set -x # show cmds
set -e # fail globally

export BOOT_COMMAND="bochs -f bochs"

. ./all.sh