#!/bin/sh
set -x # show cmds
set -e # fail globally

 
for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE -f GNUmakefile install -j8)
done