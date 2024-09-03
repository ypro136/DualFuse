#!/bin/sh
set -e
. ./headers_${ARCH}.sh
 
for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE -f Makefile install)
done