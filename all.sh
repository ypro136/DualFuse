#!/bin/sh


export LOG_DIR=logs

. ./config.sh

export PATH="$HOME/src/cross/$ARCH-elf/CROSS_compiler/bin/:$PATH"

./clean.sh | tee $LOG_DIR/clean.log

./headers.sh | tee $LOG_DIR/headers.log

./iso.sh | tee $LOG_DIR/iso.log

./boot.sh | tee $LOG_DIR/qemu.log
