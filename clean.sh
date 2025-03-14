#!/bin/sh
set -x # show cmds
#set -e # fail globally

for PROJECT in $PROJECTS; do
  (cd $PROJECT && $MAKE -f Makefile clean && $MAKE -f GNUmakefile clean)
done

cd $ENVIROMENT_DIR
#make clean
cd $PROJECT_ROOT

#rm -rf sysroot
rm -rf isodir
rm -rf DualFuse.iso