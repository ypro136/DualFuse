#!/bin/sh
set -x # show cmds
set -e # fail globally

if test -f "sysroot/boot/DualFuse.kernel"; then
	echo $BOOT_COMMAND
    eval "$BOOT_COMMAND"
else
    echo "build  failed, skipping boot"
    exit 1
fi

