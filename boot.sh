#!/bin/sh
set -e

# for bochs
#bochs -f bochs

# for qemu
echo "qemu-system-$(./target-triplet-to-arch.sh ${ARCH}) -cdrom DualFuse.iso -serial file:serial.log --drive id=disk,file=disk.img,if=none -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -m 1g" 
qemu-system-$(./target-triplet-to-arch.sh ${ARCH}) -cdrom DualFuse.iso -serial file:serial.log -drive id=disk,file=disk.img,if=none -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -m 1g -s -S &
#sleep 0.5
#gdb

# if debuging is needed:
# uncomment gdb and the -s -S &
# then run (gdb)> target remote :1234
# then run (gdb)> n (dont kill the process)
# then run (gdb)> c (for continue) ,s (for step), help (for help)
# as needed
# then run (gdb)> quit (to simply quit)