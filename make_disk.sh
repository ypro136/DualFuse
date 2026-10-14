#!/bin/sh
set -e

IMG=disk.img
FS_DIR=fs
SIZE_MB=64

if command -v "${TARGET}-gcc" >/dev/null 2>&1; then
    mkdir -p fs/progs
    "${TARGET}-gcc" --sysroot="$SYSROOT" -static -no-pie -fno-pie \
        -o fs/progs/hello userspace/hello.c
    echo "[disk] compiled userspace/hello.c -> fs/progs/hello"
else
    echo "[disk] warning: ${TARGET}-gcc not found, skipping userspace compile"
fi

echo "[disk] creating ${SIZE_MB}MB FAT32 image..."
dd if=/dev/zero of=$IMG bs=1M count=$SIZE_MB 2>/dev/null
mkfs.fat -F 32 -n "DUALFUSE" $IMG

export MTOOLS_SKIP_CHECK=1

mmd -i $IMG ::assets
mmd -i $IMG ::desktop
mmd -i $IMG ::progs
mmd -i $IMG ::src

for dir in assets desktop progs src; do
    find $FS_DIR/$dir -maxdepth 1 -type f | while read f; do
        mcopy -i $IMG "$f" "::${dir}/"
    done
done

echo "[disk] done. contents:"
mdir -i $IMG -/ ::