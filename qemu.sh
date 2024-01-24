#!/bin/sh
set -e
. ./iso.sh
 
qemu-system-$(./target-triplet-to-arch.sh $HOST) -cdrom DualFuse.iso -serial file:serial.log