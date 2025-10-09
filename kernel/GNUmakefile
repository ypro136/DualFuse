# Nuke built-in rules and variables.
MAKEFLAGS += -rR

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


override CFLAGS:=$(CFLAGS) -O2 -pipe -g -std=c++11


PREFIX?=/usr
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

# Internal C flags that should not be changed by the user.
override CFLAGS:=$(CFLAGS) \
    -ffreestanding \
	-ffunction-sections \
    -fdata-sections \
	-fno-common \
	-fno-lto \
	-fpermissive \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
    -fno-stack-check \
	-fno-omit-frame-pointer \
	-fno-PIC \
	-mno-red-zone \
    -mno-mmx \
    -mno-sse \
	-mno-sse2 \
	-mno-sse3 \
	-mno-ssse3 \
	-mno-sse4 \
	-mno-avx \
	-mno-avx2 \
	-mno-avx512f \
    -mcmodel=kernel \
    -march=x86-64 \
    -mno-80387 \
    -m64 \
    -Wall \
    -Wextra \
	-Wno-unused-parameter \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-align \
    -Wwrite-strings \
	-Wmissing-declarations \
    -Wredundant-decls \
	-Winline \
	-Wno-long-long \
    -Wconversion \
	-nostdlib \
	-z max-page-size=0x1000 \
	-z noexecstack
	


override LDFLAGS := -static -z text -Wl,--no-relax,-Y,$(SYSROOT)/usr/lib,--gc-sections

# debuging options -DDEBUG_MEMORY -DDEBUG_CONSOLE -DDEBUG_TIMER -DDEBUG_PCI

CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -D__is_x86_64 -DLIMINE_API_REVISION=1 -Iinclude	-Iinclude/ -I./include/
LIBS:=$(LIBS) -lc -lgcc

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
override CRTI_OBJ := $(shell x86_64-elf-gcc $(CPP) $(CFLAGS) --print-file-name=crti.o)
CRTBEGIN_OBJ:=$(shell $(CPP) $(CFLAGS) -print-file-name=crtbegin.o)
KERNEL_OBJS= $(KERNEL_ARCH_OBJS) obj/kernel.cpp.o 
C_EXTRA_OBJS = obj/printf.cpp.o
override KERNELFILES := $(shell cd kernel && find -L * -type f -name '*.cpp' | LC_ALL=C sort)
override CPPFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.cpp' ! -name "malloc.cpp" ! -name "printf.cpp" | LC_ALL=C sort)
override CFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.c' | LC_ALL=C sort)
override ASFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.S' ! -name "crti.S" ! -name "crtn.S" | LC_ALL=C sort)
override NASMFILES := $(shell cd $(ARCHDIR) && find -L * -type f -name '*.asm' | LC_ALL=C sort)
override OBJ := $(addprefix obj/,$(KERNELFILES:.cpp=.cpp.o) $(CFILES:.c=.c.o) $(NASMFILES:.asm=.asm.o) $(CPPFILES:.cpp=.cpp.o) $(ASFILES:.S=.S.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d) $(CRTI_SOURCE:.S=.S.o) $(CRTN_SOURCE:.S=.S.o) $(CPPFILES:.cpp=.cpp.d) $(KERNELFILES:.cpp=.cpp.d))
CRTEND_OBJ:=$(shell $(CPP) $(CFLAGS) -print-file-name=crtend.o)
override CRTN_OBJ := $(shell x86_64-elf-gcc $(CPP) $(CFLAGS) --print-file-name=crtn.o)


OBJS=\
$(CRTI_OBJ) \
$(CRTBEGIN_OBJ) \
$(C_EXTRA_OBJS) \
$(OBJ) \
$(CRTEND_OBJ) \
$(CRTN_OBJ) 

LINK_LIST=\
$(OBJS) \
$(LIBS) 


.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S .cpp .asm

all: bin/$(OUTPUT).kernel

bin/$(OUTPUT).kernel: install-headers $(OBJS) GNUmakefile
	echo "LINK_LIST $(LINK_LIST) \n"
	echo "linking $@"
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LINK_LIST) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	@$(CPP) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LINK_LIST) 2>> $(ERROR_OUT_FILE)
	@echo "\n"
	-touch bin/all_dump.txt
	-touch bin/kernel_main_dump.txt
	objdump -D bin/DualFuse.kernel >> bin/all_dump.txt
	objdump -D bin/DualFuse.kernel --start-address=0xffffffff80000000 -M intel | head -n 200 >> bin/kernel_main_dump.txt

obj/crtbegin.o:
	echo "building crtbegin.o file $<"
	@echo "$(CPP) $(CFLAGS) $(CPPFLAGS) -print-file-name=$(@F) && cp $(CRTBEGIN_OBJ) $@ \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) $(CFLAGS) -print-file-name=$(@F) && cp $(CRTBEGIN_OBJ) $@ 2>> $(ERROR_OUT_FILE)
	@echo "\n"


obj/crtend.o:
	echo "building crtend.o file $<"
	@echo "$(CPP) $(CFLAGS) $(CPPFLAGS) -print-file-name=$(@F) && cp $(CRTEND_OBJ) $@ \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) $(CFLAGS) -print-file-name=$(@F) && cp $(CRTEND_OBJ) $@ 2>> $(ERROR_OUT_FILE)
	@echo "\n"


# Include header dependencies.
-include $(HEADER_DEPS)

obj/printf.cpp.o:arch/x86_64/drivers/printf.cpp
	echo "building printf cpp file $<"
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) -DPRINTF_INCLUDE_CONFIG_H=1 \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) -DPRINTF_INCLUDE_CONFIG_H=1 2>> $(ERROR_OUT_FILE)
	@echo "\n"

# obj/malloc.cpp.o:memory/malloc.cpp
# 	mkdir -p "$$(dirname $@)"
# 	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) -DHAVE_MMAP=0 -DLACKS_TIME_H=1 -DLACKS_SYS_PARAM_H=1 -LACKS_STRING_H=0 -Dmalloc_getpagesize=4096 -DNO_MALLOC_STATS=1 -DMORECORE_CONTIGUOUS=0 -DUSE_LOCKS=2 \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
# 	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) -DHAVE_MMAP=0 -DLACKS_TIME_H=1 -DLACKS_SYS_PARAM_H=1 -LACKS_STRING_H=0 -Dmalloc_getpagesize=4096 -DNO_MALLOC_STATS=1 -DMORECORE_CONTIGUOUS=0 -DUSE_LOCKS=2 2>> $(ERROR_OUT_FILE)
# 	@echo "\n"


# Compilation rules for *.c files.
obj/%.c.o: $(ARCHDIR)/%.c GNUmakefile install-headers
	echo "building c file $<"
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) 2>> $(ERROR_OUT_FILE)
	@echo "\n"

# Compilation rules for kernel cpp files.
obj/%.cpp.o: kernel/%.cpp 
	echo "building kernel cpp file $<"
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(KERNEL_LIBS) 2>> $(ERROR_OUT_FILE)
	@echo "\n"

# Compilation rules for *.cpp files.
obj/%.cpp.o: $(ARCHDIR)/%.cpp GNUmakefile install-headers
	echo "building cpp file $<"
	mkdir -p "$$(dirname $@)"
	@echo "$(CPP) -MMD -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CPP) -MMD -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) 2>> $(ERROR_OUT_FILE)
	@echo "\n"

#Compilation rules for *.S files.
obj/%.S.o: $(ARCHDIR)/%.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	@echo "$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	$(CC) -MMD -c $< -o $@ $(CFLAGS) $(CPPFLAGS) 2>> $(ERROR_OUT_FILE)
	@echo "\n"

# Compilation rules for *.asm (nasm) files.
obj/%.asm.o: $(ARCHDIR)/%.asm GNUmakefile
	echo "building nasm file $<"
	mkdir -p "$$(dirname $@)"
	@echo "nasm -g -f $(NASM_ARCH) $< -o $@ \n $(ERROR_OUT_TEXT)" | tee -a $(ERROR_OUT_FILE)
	nasm -g -f $(NASM_ARCH) $< -o $@ 2>> $(ERROR_OUT_FILE)
	@echo "\n"

clean:
	echo "Clean"
	-rm -f bin/$(OUTPUT).kernel
	-rm -f $(DESTDIR)$(BOOTDIR)$(OUTPUT).kernel
#	-rm -rf bin obj
	-rm -f errorout.log


install: clean install-headers install-kernel

install-headers:
	echo "objs $(OBJS) \n"
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR)/. $(DESTDIR)$(INCLUDEDIR)/.
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR) $(DESTDIR)$(INCLUDEDIR)/.
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR)/*/. $(DESTDIR)$(INCLUDEDIR)/.
	cp -R --preserve=timestamps $(PROJECT_ROOT)/limine/limine.h $(DESTDIR)$(INCLUDEDIR)/limine.h
	rm -r $(DESTDIR)$(INCLUDEDIR)/kernel
	rm -r $(DESTDIR)$(INCLUDEDIR)/utility
	rm -r $(DESTDIR)$(INCLUDEDIR)/boot
	rm -r $(DESTDIR)$(INCLUDEDIR)/cpu
	rm -r $(DESTDIR)$(INCLUDEDIR)/disk
	rm -r $(DESTDIR)$(INCLUDEDIR)/memory
	rm -r $(DESTDIR)$(INCLUDEDIR)/multitasking
	rm -r $(DESTDIR)$(INCLUDEDIR)/drivers


install-kernel: all
	mkdir -p $(DESTDIR)
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp bin/$(OUTPUT).kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)