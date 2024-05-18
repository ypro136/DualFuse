#!/bin/sh
set -e
. ./iso.sh
 
qemu-system-$(./target-triplet-to-arch.sh $HOST) -cdrom DualFuse.iso -serial file:serial.log #-s -S &
#gdb isodir/boot/DualFuse.kernel

# if debuging is needed:
# uncomment gdb and the -s -S &
# then run (gdb)> target remote :1234
# then run (gdb)> n (dont kill the process)
# then run (gdb)> c (for continue) ,s (for step), help (for help)
# as needed
# then run (gdb)> quit (to simply quit)