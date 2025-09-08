#!/bin/sh
set -x # show cmds
set -e # fail globally

. ./config.sh

mkdir -m=rwx -p $LOG_DIR

. ./clean.sh | tee $LOG_DIR/clean.log

if ! test -f "$PREFIX/bin/${TARGET}-gcc"; then
	cd $ENVIROMENT_DIR
	make | tee make.log
  cd $PROJECT_ROOT
fi

"$MUSL_DIR/build_musl.sh" --noreplace

# exporting the new toolchain
export LINKER=${TARGET}-ld
export AR=${TARGET}-ar
export AS=${TARGET}-as
export CC=${TARGET}-gcc
export CPP=${TARGET}-g++ 

export CC="$CC --sysroot=$SYSROOT"
export CPP="$CPP --sysroot=$SYSROOT"


cd $PROJECT_ROOT
. ./headers_${ARCH}.sh | tee $LOG_DIR/headers.log
cd $PROJECT_ROOT
. ./build_${ARCH}.sh | tee $LOG_DIR/build.log
cd $PROJECT_ROOT
. ./iso_${ARCH}.sh | tee $LOG_DIR/iso.log
cd $PROJECT_ROOT
. ./boot.sh | tee $LOG_DIR/boot.log
