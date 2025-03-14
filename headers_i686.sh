#!/bin/sh
set -x # show cmds
set -e # fail globally

. ./config.sh
 
mkdir -p "$SYSROOT"
 
for PROJECT in $SYSTEM_HEADER_PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE -f Makefile install-headers)
done