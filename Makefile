C_SOURCES = $(wildcard kernel/*.cpp drivers/*.cpp cpu/*.cpp)
HEADERS = $(wildcard kernel/*.hpp drivers/*.hpp cpu/*.hpp)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.cpp=.o cpu/interrupt.o} 

# Change this if your cross-compiler is somewhere else
CC = /home/yasser/src/cross/CROSS_compiler/bin/i686-elf-g++
GDB = i686-elf-gdb
# -g: Use debugging symbols in gcc
CFLAGS = -m32 -ffreestanding -mgeneral-regs-only -fpermissive -O2 -Wall -Wextra -fno-exceptions -fno-rtti -lgcc

# First rule is run by default
DualFuse.bin: boot/boot_sector.o kernel/kernel.o
	${CC} -m32 -T linker.ld -o DualFuse.bin -ffreestanding -O2 -nostdlib $^ -lgcc

# '--oformat binary' deletes all symbols as a collateral, so we don't need
# to 'strip' them manually on this case
kernel/kernel.o: kernel/kernel.cpp
	${CC} -c $< -o $@ -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti

run: DualFuse.iso
	qemu-system-i386 -cdrom $<

# Open the connection to qemu and load our kernel-object file with symbols
debug: DualFuse.bin kernel.elf
	qemu-system-i386 -s -cdrom DualFuse.iso -d guest_errors,int &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

DualFuse.iso: DualFuse.bin
	mkdir -p isodir/boot/grub
	cp DualFuse.bin isodir/boot/DualFuse.iso
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o DualFuse.iso isodir

# Generic rules for wildcards
# To make an object, always compile from its .cpp
%.o: %.cpp ${HEADERS}
	${CC} ${CFLAGS} -c $< -o $@ 

%.o: %.asm
	nasm $< -felf32 -o $@

%.bin: %.asm
	nasm $< -f bin -o $@

.PHONY:
clean:
	rm -rf *.bin *.dis *.o DualFuse.bin *.elf
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o
