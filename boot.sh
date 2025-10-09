#!/bin/sh
set -x # show cmds
set -e # fail globally

if test -f "sysroot/boot/DualFuse.kernel"; then
	echo $BOOT_COMMAND
    eval "$BOOT_COMMAND" | tee $PROJECT_ROOT/logs/serial.log
else
    echo "build  failed, skipping boot" | tee $PROJECT_ROOT/logs/boot.log
    exit 1
fi

