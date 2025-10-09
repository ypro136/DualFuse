#!/bin/sh
set -x # show cmds
set -e # fail globally
  
for PROJECT in $SYSTEM_HEADER_PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE -f GNUmakefile install-headers -j13)
done