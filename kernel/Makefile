DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g -std=c11
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra -Wno-unused-parameter -fno-exceptions 
BOOTSTRAP_CFLAGS:=$(CFLAGS)
CFLAGS:=$(CFLAGS) -fstack-protector-all -fpermissive -z max-page-size=0x1000 -fno-rtti -mno-red-zone -mno-mmx -mno-sse -mno-sse2
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -D__is_i686 -Iinclude
LIBS:=$(LIBS) -nostdlib -lgcc


ARCHDIR=arch/$(ARCH)
ARCH_LIBS_DIR=$(ARCHDIR)/include

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)
KERNEL_LIBS:=$(LIBS) -lk


KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
kernel/kernel.o \

OBJS=\
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

LINK_LIST=\
$(LDFLAGS) \
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(KERNEL_LIBS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

BOOTSTRAP_LINK_LIST=\
$(LDFLAGS) \
$(BOOTSTRAP_OBJS) \
$(LIBS) \


.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S .cpp

all: DualFuse.kernel

DualFuse.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CPP) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	grub-file --is-x86-multiboot DualFuse.kernel
	mkdir -p bin
	cp DualFuse.kernel bin/DualFuse.kernel

 
$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CPP) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.cpp.o:
	$(CPP) -MD -c $(CPPFLAGS) $(CFLAGS) $< -o $@ 


.S.o:
	nasm -f $(NASM_ARCH) $< -o $@


clean:
	rm -f DualFuse.kernel
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps $(ARCH_LIBS_DIR)/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: DualFuse.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp DualFuse.kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)