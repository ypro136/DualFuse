#!/bin/sh
set -x # show cmds
set -e # fail globally

#kill last qemu instance
#killall -q qemu-system-x86_64 || true

#or  -serial stdio for terminal output
#or -serial file:logs/serial.log for file output

export BOOT_COMMAND="qemu-system-x86_64 -no-shutdown -no-reboot -d int,cpu_reset -D logs/qemu.log -cdrom DualFuse.iso -serial file:logs/serial.log -drive id=disk,file=disk.img,if=none -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -m 4g -s -S"

. ./all.sh | tee $PROJECT_ROOT/logs/all.log