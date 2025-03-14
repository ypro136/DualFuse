#!/bin/sh
set -x # show cmds
set -e # fail globally
 
mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/limine
 
cp sysroot/boot/DualFuse.kernel isodir/boot/DualFuse.kernel
cp -v limine.conf limine/limine-bios.sys limine/limine-bios-cd.bin \
      limine/limine-uefi-cd.bin isodir/boot/limine/


# Create the EFI boot tree and copy Limine's EFI executables over.
mkdir -p isodir/EFI/BOOT
cp -v limine/BOOTX64.EFI isodir/EFI/BOOT/
cp -v limine/BOOTIA32.EFI isodir/EFI/BOOT/

# Create the bootable ISO. xorriso should be instaled with the enviroment
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        isodir -o DualFuse.iso

# Install Limine stage 1 and 2 for legacy BIOS boot.
./limine/limine bios-install DualFuse.iso