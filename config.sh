#!/bin/sh

SYSTEM_HEADER_PROJECTS="libc kernel"
PROJECTS="libc kernel"
 
export MAKE=${MAKE:-make}
export HOST=${HOST:-$(./default-host.sh)}

export ARCH="x86_64"
#export ARCH="i686"
 
#export AR=${HOST}-ar
#export AS=${HOST}-as
#export CC=${HOST}-gcc
#export CPP=${HOST}-g++ 

export AR=${ARCH}-elf-ar
export AS=${ARCH}-elf-as
export CC=${ARCH}-elf-gcc
export CPP=${ARCH}-elf-g++ 

export CPP32="i686-elf-g++"

export PREFIX32="$HOME/src/cross/i686-elf/CROSS_compiler/bin"

export CPP32=${PREFIX32}/${CPP32}
 
export PREFIX=/usr
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include
 
export CFLAGS='-O2 -g'
export CPPFLAGS=''
 
# Configure the cross-compiler to use the desired system root.
export SYSROOT="$(pwd)/sysroot"
export CC="$CC --sysroot=$SYSROOT"
export CPP="$CPP --sysroot=$SYSROOT"
 
# Work around that the -elf gcc targets doesn't have a system include directory
# because it was configured with --without-headers rather than --with-sysroot.
if echo "$HOST" | grep -Eq -- '-elf($|-)'; then
  export CC="$CC -isystem=$INCLUDEDIR"
  export CPP="$CPP -isystem=$INCLUDEDIR"
fi