#!/bin/bash
set -x # show cmds
set -e # fail globally


# musl is a C standard library implementation for Linux. 
# Some of musl’s major advantages over glibc and uClibc/uClibc-ng are its size, correctness, static linking support, and clean code.
# https://wiki.musl-libc.org/   <<<<<<<<<<<<<<<<<<<<<<<<<
# also check mussel the shortest and fastest script available today to build working cross compilers that target musl libc. https://github.com/firasuke/mussel/tree/main <<<<<<<<<<<<<<<<<<<<<<<<<

MUSL_RELEASE="musl-1.2.5"


cd $MUSL_DIR

# --noreplace -> won't re-compile if it finds libc
if test -f "$SYSROOT/usr/lib/libc.a"; then
	if [ "$#" -eq 1 ]; then
		exit 0
	fi
fi

# get sources
if ! test -f "$MUSL_DIR/$MUSL_RELEASE/README"; then
	wget -nc "https://musl.libc.org/releases/$MUSL_RELEASE.tar.gz"
	tar -xpf "$MUSL_RELEASE.tar.gz"
	rm -f -r $MUSL_RELEASE/src/complex # remove complex operations support assumed to be a bug in gcc 11.3.0
	mkdir -p $MUSL

fi

# Ensure the toolchain is in PATH
if [[ ":$PATH:" != *":$HOME/src/cross/$TARGET/CROSS_compiler/bin:"* ]]; then
	export PATH=$HOME/src/cross/$TARGET/CROSS_compiler/bin:$PATH
fi

if ! test -f "$SYSROOT/usr/include/stdio.h"; then
	cd $MUSL
	mkdir -p $SYSROOT/usr
	cp -r "$MUSL_DIR/$MUSL_RELEASE/include" "$SYSROOT/usr"
fi

# WARNING delet src files when editing this it dose not delet them automaticly
# build musl
if test -f "$PREFIX/bin/${TARGET}-gcc"; then
	cd $MUSL
	if ! test -f "$MUSL/Makefile"; then
		rm -f -r obj/src/complex # remove complex operations support assumed to be a bug in gcc 11.3.0
		CC=$TARGET-gcc ARCH=x86_64 CROSS_COMPILE=$TARGET- CFLAGS="-O2 -g -mcmodel=large" LDFLAGS="-static-pie" "../$MUSL_RELEASE/configure" --target=$TARGET --prefix="$SYSROOT/usr" --syslibdir="/lib" --disable-shared --enable-static --enable-debug
	fi
	make clean 
	make install-headers $MAKE_ARGS
	make all $MAKE_ARGS
	make install 

	#   musl builds directly to sysroot
	#	cp $MUSL/lib/libc.a $SYSROOT/lib/libc.a

	# Copy libraries (and update headers)
	#   musl builds directly to sysroot
	# cp -r "${MUSL_DIR}/$MUSL_RELEASE/include" "$SYSROOT/usr"
	# cp -r "$MUSL/lib" "$SYSROOT/usr/"

	# required for proper dynamic linking
	#cp "$SYSROOT/usr/lib/libc.so" "$SYSROOT/usr/lib/ld64.so.1"
fi

cd $CROSS

