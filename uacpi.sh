#!/bin/sh

set -x # show cmds
set -e # fail globally


cd $PROJECT_ROOT/kernel/arch/x86_64/

TARGET_VERSION="3.1.0"

fetchUACPI() {
	rm -rf uACPI/ repo/ uacpi/ ../include/uacpi/
	mkdir -p acpi/uacpi/
	cd acpi
	wget "https://github.com/uACPI/uACPI/archive/refs/tags/$TARGET_VERSION.tar.gz" -O "$TARGET_VERSION.tar.gz"
	tar xpf "$TARGET_VERSION.tar.gz"
	rm -f "$TARGET_VERSION.tar.gz"
	mv "uACPI-$TARGET_VERSION" repo/
	mv "repo/source/"* uacpi/
	mv "repo/include/"* ../include/
	rm -rf uACPI/ repo/
}

fetchUACPI

if ! test -f "$MUSL_DIR/$MUSL_RELEASE/README"; then
	fetchUACPI
fi

