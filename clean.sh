#!/bin/sh
set -e

. ./config.sh
 
for PROJECT in $PROJECTS; do
  (cd $PROJECT && $MAKE -f Makefile clean && $MAKE -f GNUmakefile clean)
done
 
rm -rf sysroot
rm -rf isodir
rm -rf DualFuse.iso