# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

ERROR_OUT_FILE?=errorout.log
ERROR_OUT_TEXT?="comand out \\/ \\/ \\/ ============= command /\\ /\\ /\\"

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
	-mno-red-zone \
    -ffreestanding \
	-fcommon \
	-nostartfiles \
	-nostdlib \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
	-fno-lto \
	-Wno-unused-parameter \
	-fpermissive \
	-z max-page-size=0x1000 \
	-fno-exceptions \
	-fno-rtti \
    -fno-lto \
	-std=gnu11 \
    -march=x86-64 \
    -mno-80387 \
    -m64 \
	-fno-stack-protector \
    -fno-stack-check \
    -mcmodel=kernel \
	-fno-pic
# -fPIE 
# -fpie 
#	-fstack-protector-all 
	

override LDFLAGS := -static -nostdlib -z text -Wl,--no-relax,-Y,$(SYSROOT)/usr/lib

override CFLAGS:=$(CFLAGS) -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-declarations \
            -Wredundant-decls -Winline -Wno-long-long \
            -Wconversion

CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -D__is_x86_64 -Iinclude	-Iinclude/ -I./include/
LIBS:=$(LIBS) -lc

ARCHDIR=arch/$(ARCH)

ARCH_LIBS_DIR=$(ARCHDIR)/include/*

UTILITY_LIBS_DIR=$(ARCH_LIBS_DIR)/utility
KERNEL_LIBS_DIR=$(ARCH_LIBS_DIR)/kernel
MEMORY_LIBS_DIR=$(KERNEL_LIBS_DIR)/memory

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)
KERNEL_LIBS:=$(LIBS)

# Use "find" to glob all *.cpp *.c, *.S, and *.asm files in the tree and obtain the
# object and header dependency file names.
override KERNELFILES := $(shell cd kernel && find -L * -type f -name '*.cpp' | LC_ALL=C sort)
override CPPFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.cpp' ! -name "malloc.cpp" ! -name "printf.cpp" | LC_ALL=C sort)
override CFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.c' | LC_ALL=C sort)
override ASFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.S' ! -name "crti.S" ! -name "crtn.S" | LC_ALL=C sort)
override NASMFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.asm' | LC_ALL=C sort)
override OBJ := $(addprefix obj/,$(KERNELFILES:.cpp=.cpp.o) $(CFILES:.c=.c.o) $(NASMFILES:.asm=.asm.o) $(CPPFILES:.cpp=.cpp.o) $(ASFILES:.S=.S.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d) $(CPPFILES:.cpp=.cpp.d) $(KERNELFILES:.cpp=.cpp.d))

CRTI_OBJ=obj/crti.S.o
CRTBEGIN_OBJ:=$(shell $(CPP) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CPP) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=obj/crtn.S.o

override LIBC := -lc



KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
obj/kernel.cpp.o \

C_EXTRA_OBJS = obj/printf.cpp.o

OBJS=\
$(CRTI_OBJ) \
obj/crtbegin.o \
$(OBJ) \
$(KERNEL_OBJS) \
$(C_EXTRA_OBJS) \
obj/crtend.o \
$(CRTN_OBJ) \

LINK_LIST=\
$(OBJ) \
$(LIBS) 

# LINK_LIST=\
# $(CRTI_OBJ) \
# $(OBJ) \
# $(CRTN_OBJ) \
# $(LIBC) 


.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S .cpp

all: bin/$(OUTPUT).kernel

bin/$(OUTPUT).kernel: clean install-headers $(OBJ) GNUmakefile
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LINK_LIST) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	@$(CPP) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LINK_LIST) 2>> $(ERROR_OUT_FILE)
	@echo "\n"

obj/crtbegin.o:
	@echo "$(CPP) $(CFLAGS) $(CPPFLAGS) -print-file-name=$(@F) && cp $(CRTBEGIN_OBJ) $@ \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) $(CFLAGS) -print-file-name=$(@F) && cp $(CRTBEGIN_OBJ) $@
	@echo "\n"


obj/crtend.o:
	@echo "$(CPP) $(CFLAGS) $(CPPFLAGS) -print-file-name=$(@F) && cp $(CRTEND_OBJ) $@ \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) $(CFLAGS) -print-file-name=$(@F) && cp $(CRTEND_OBJ) $@
	@echo "\n"


# Include header dependencies.
-include $(HEADER_DEPS)

obj/printf.cpp.o:arch/x86_64/drivers/printf.cpp
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) -DPRINTF_INCLUDE_CONFIG_H=1 \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) -DPRINTF_INCLUDE_CONFIG_H=1 
	@echo "\n"

obj/malloc.cpp.o:memory/malloc.cpp
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) -DHAVE_MMAP=0 -DLACKS_TIME_H=1 -DLACKS_SYS_PARAM_H=1 -LACKS_STRING_H=0 -Dmalloc_getpagesize=4096 -DNO_MALLOC_STATS=1 -DMORECORE_CONTIGUOUS=0 -DUSE_LOCKS=2 \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) -DHAVE_MMAP=0 -DLACKS_TIME_H=1 -DLACKS_SYS_PARAM_H=1 -LACKS_STRING_H=0 -Dmalloc_getpagesize=4096 -DNO_MALLOC_STATS=1 -DMORECORE_CONTIGUOUS=0 -DUSE_LOCKS=2
	@echo "\n"


# Compilation rules for *.c files.
obj/%.c.o: $(ARCHDIR)/%.c GNUmakefile install-headers
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MMD -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MMD -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for kernel cpp files.
obj/%.cpp.o: kernel/%.cpp 
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS)
	@echo "\n"

# Compilation rules for *.cpp files.
obj/%.cpp.o: $(ARCHDIR)/%.cpp GNUmakefile install-headers
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MMD -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MMD -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for *.S files.
obj/%.S.o: $(ARCHDIR)/%.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
	@echo "\n"

# Compilation rules for *.asm (nasm) files.
obj/%.asm.o: $(ARCHDIR)/%.asm GNUmakefile
	mkdir -p "$$(dirname $@)"
	@echo "nasm -f $(NASM_ARCH) $< -o $@ \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	nasm -f $(NASM_ARCH) $< -o $@
	@echo "\n"

clean:
	rm -f bin/$(OUTPUT).kernel
	rm -rf bin obj
	rm -f errorout.log


install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR)/. $(DESTDIR)$(INCLUDEDIR)/.
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR) $(DESTDIR)$(INCLUDEDIR)/.
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR)/*/. $(DESTDIR)$(INCLUDEDIR)/.
	rm -r $(DESTDIR)$(INCLUDEDIR)/kernel
	rm -r $(DESTDIR)$(INCLUDEDIR)/utility
	rm -r $(DESTDIR)$(INCLUDEDIR)/boot
	rm -r $(DESTDIR)$(INCLUDEDIR)/cpu
	rm -r $(DESTDIR)$(INCLUDEDIR)/disk
	rm -r $(DESTDIR)$(INCLUDEDIR)/memory
	rm -r $(DESTDIR)$(INCLUDEDIR)/multitasking
	rm -r $(DESTDIR)$(INCLUDEDIR)/drivers


install-kernel: all
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp bin/$(OUTPUT).kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)