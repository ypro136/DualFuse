#!/bin/sh
set -x # show cmds
set -e # fail globally

#kill last qemu instance
#killall -q qemu-system-x86_64 || true

#or  -serial stdio for terminal output
#or -serial file:logs/serial.log for file output

export BOOT_COMMAND="qemu-system-x86_64 -d int,cpu_reset -D logs/qemu.log -no-reboot -no-shutdown -smp 4 -cdrom DualFuse.iso -serial file:logs/serial.log -m 4G -enable-kvm -s -S"
# add this later " -d int,cpu_reset -D logs/qemu.log"

. ./all.sh | tee $PROJECT_ROOT/logs/all.log