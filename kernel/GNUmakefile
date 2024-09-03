# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

# This is the name that our final executable will have.
# Change as needed.
override OUTPUT := DualFuse

# Convenience macro to reliably declare user overridable variables.
define DEFAULT_VAR =
    ifeq ($(origin $1),default)
        override $(1) := $(2)
    endif
    ifeq ($(origin $1),undefined)
        override $(1) := $(2)
    endif
endef


CFLAGS?=-O2 -g -std=c11
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

# Internal C flags that should not be changed by the user.
override CFLAGS:=$(CFLAGS) \
    -Wall \
    -Wextra \
    -ffreestanding \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
	-Wno-unused-parameter \
	-fpermissive \
	-z max-page-size=0x1000 \
	-fno-exceptions \
	-fno-rtti \
    -fno-lto \
    -std=c++11 \
    -fno-PIC \
    -march=x86-64 \
    -mno-80387 \
    -m64 \
	-fno-stack-protector \
    -fno-stack-check \
	-lk \
	#-fstack-protector-all 

override LDFLAGS := \
    -static \
    -pie \
    -z text \

override CFLAGS:=$(CFLAGS) -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-declarations \
            -Wredundant-decls -Winline -Wno-long-long \
            -Wconversion

CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LIBS:=$(LIBS) -nostdlib -lgcc \


ARCHDIR=arch/$(ARCH)
ARCH_LIBS_DIR=$(ARCHDIR)/include

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)
KERNEL_LIBS:=$(LIBS)

# Use "find" to glob all *.c, *.S, and *.asm files in the tree and obtain the
# object and header dependency file names.
override KERNELFILES := $(shell cd kernel && find -L * -type f -name '*.cpp' | LC_ALL=C sort)
override CPPFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.cpp' | LC_ALL=C sort)
override CFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.c' | LC_ALL=C sort)
override ASFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.S' | LC_ALL=C sort)
override NASMFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.asm' | LC_ALL=C sort)
override OBJ := $(addprefix obj/,$(KERNELFILES:.cpp=.cpp.o) $(CFILES:.c=.c.o) $(NASMFILES:.asm=.asm.o) $(CPPFILES:.cpp=.cpp.o)) # $(ASFILES:.S=.S.o) add s files manualy to put crti and crtn in ther position
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d) $(CPPFILES:.cpp=.cpp.d) $(KERNELFILES:.cpp=.cpp.d))

CRTI_OBJ=obj/crti.S.o
CRTBEGIN_OBJ:=$(shell $(CPP) $(CFLAGS) $(LDFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CPP) $(CFLAGS) $(LDFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=obj/crtn.S.o


KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
obj/kernel.cpp.o \

OBJS=\
$(CRTI_OBJ) \
obj/crtbegin.o \
$(OBJ) \
$(KERNEL_OBJS) \
obj/crtend.o \
$(CRTN_OBJ) \

LINK_LIST=\
$(CRTI_OBJ) \
obj/crtbegin.o \
$(OBJ) \
obj/crtend.o \
$(CRTN_OBJ) \


.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S .cpp

all: bin/$(OUTPUT).kernel

bin/$(OUTPUT).kernel: $(OBJS) GNUmakefile
	mkdir -p "$$(dirname $@)"
	cd $(SYSROOT)/usr/lib/ && ls
	$(CPP) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LDFLAGS) $(KERNEL_LIBS) $(LINK_LIST)
	@echo "\n"

obj/crtbegin.o:
	$(CPP) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F) && cp $(CRTBEGIN_OBJ) $@
	@echo "\n"


obj/crtend.o:
	$(CPP) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F) && cp $(CRTEND_OBJ) $@
	@echo "\n"


# Include header dependencies.
-include $(HEADER_DEPS)

# Compilation rules for *.c files.
obj/%.c.o: $(ARCHDIR)/%.c GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for kernel cpp files.
obj/%.cpp.o: kernel/%.cpp
	mkdir -p "$$(dirname $@)"
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for *.cpp files.
obj/%.cpp.o: $(ARCHDIR)/%.cpp GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for *.S files.
obj/%.S.o: $(ARCHDIR)/%.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for *.asm (nasm) files.
obj/%.asm.o: $(ARCHDIR)/%.asm GNUmakefile
	mkdir -p "$$(dirname $@)"
	nasm -f $(NASM_ARCH) $< -o $@
	@echo "\n"

clean:
	rm -f bin/$(OUTPUT).kernel
	rm -rf bin obj


install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR)/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: all
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp bin/$(OUTPUT).kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)