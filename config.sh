#!/bin/sh
set -x # show cmds
set -e # fail globally 

# set up dir names that have header files.
SYSTEM_HEADER_PROJECTS="kernel" # add libc if needed
# set up dir names that have src code.
PROJECTS="kernel"  # add libc if needed

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "${SCRIPTPATH}"
export PROJECT_ROOT="$SCRIPTPATH"
export ENVIROMENT_DIR="$PROJECT_ROOT/enviroment"


# set up make command and host(if used).
export MAKE=${MAKE:-make}
export HOST=${HOST:-$(./default-host.sh)}

# set up the program versions. check version compatibilty at https://wiki.osdev.org/Cross-Compiler_Successful_Builds
export BINUTILS_VERSION=2.41 # 2.38
export GCC_VERSION=13.2.0 # 11.3.0
# set up the program versions. check the required versions for binutils and gcc
export AUTOMAKE_VERTION=1.15.1
export AUTOCONF_VERTION=2.69

# set up core count for make multithreading.
export CORE_COUNT=$(nproc)

# for 64 bit target. modifying this is not tested
export ARCH="x86_64"
export TARGET_OS=elf # or dualfuse. test on elf then try dualfuse
export TARGET=${ARCH}-${TARGET_OS}

# set up toolchain location.
export CROSS=$HOME/src/cross/$TARGET

# set up extra c and c++ flags.
export CFLAGS=''
export CPPFLAGS=''

# set up location for sysroot and musl src
export SYSROOT="$PROJECT_ROOT/sysroot"
export MUSL_DIR="$PROJECT_ROOT/libc"

# ?
export PREFIX=/usr
export EXEC_PREFIX=/usr
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=/usr/include

# path
export PREFIX="$CROSS/CROSS_compiler"
export PATH="$PREFIX/bin:$PATH"
 
# Configure the cross-compiler to use the desired system root and musl
export SYS_INCLUDEDIR="$SYSROOT/usr/include"
export MUSL="$MUSL_DIR/musl"

# set up location for the log directory.
export LOG_DIR=$CROSS/logs




