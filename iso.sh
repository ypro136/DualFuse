#!/bin/sh
set -e
. ./build.sh
 
mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub
 
cp sysroot/boot/DualFuse.kernel isodir/boot/DualFuse.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "DualFuse" {
	multiboot /boot/DualFuse.kernel
}
EOF
grub-mkrescue -o DualFuse.iso isodir