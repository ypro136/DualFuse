#!/bin/sh
# make_disk.sh - rebuild disk.img from the fs/ directory
# Run from project root. Requires: mtools, dosfstools

set -e

IMG=disk.img
FS_DIR=fs
SIZE_MB=64

echo "[disk] creating ${SIZE_MB}MB FAT32 image..."
dd if=/dev/zero of=$IMG bs=1M count=$SIZE_MB 2>/dev/null
mkfs.fat -F 32 -n "DUALFUSE" $IMG

export MTOOLS_SKIP_CHECK=1

# Create directory structure
mmd -i $IMG ::assets
mmd -i $IMG ::desktop
mmd -i $IMG ::progs
mmd -i $IMG ::src

# Copy contents of each folder if they have files
for dir in assets desktop progs src; do
    find $FS_DIR/$dir -maxdepth 1 -type f | while read f; do
        mcopy -i $IMG "$f" "::${dir}/"
    done
done

echo "[disk] done. contents:"
mdir -i $IMG -/ ::