#!/bin/sh
set -e

export LOG_DIR=logs

. ./config.sh

export PATH="$HOME/src/cross/$ARCH-elf/CROSS_compiler/bin/:$PATH"

. ./clean.sh | tee $LOG_DIR/clean.log

. ./headers_${ARCH}.sh | tee $LOG_DIR/headers.log

. ./build_${ARCH}.sh | tee $LOG_DIR/build.log

. ./iso_${ARCH}.sh | tee $LOG_DIR/iso.log

. ./boot.sh | tee $LOG_DIR/boot.log
