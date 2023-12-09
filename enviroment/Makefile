CROSS=$(HOME)/src/cross
#for Cross-Compiler build

GCC_BUILD=$(CROSS)/build-gcc

SYS_compiler=$(CROSS)/SYS_compiler

BINUTILS_BUILD=$(CROSS)/binutils-build

MPC_BUILD=$(CROSS)/build-mpc

GCC_CROSS=$(CROSS)/GCC_cross

CROSS_compiler=$(CROSS)/CROSS_compiler

SYS_BINUTILS_BUILD=$(CROSS)/sys-binutils-build

SYS_compiler=$(GCC_BUILD)/bin/i686-elf-g++

TARGET=i686-elf


bootstrap: clean binutils gcc
	@echo run setup-gcc
	sh setup-gcc
	@echo run build-gcc
	sh build-gcc
	@echo run GCC_cross_build
	sh GCC_cross_build
	cd $(CROSS) && \
	ls
	ls

# test to add Positional Parameters to the compile scripts (arguments to be implemented)
bootstrap-test:
	@echo run build-gcc-test
	sh build-gcc-test $(CROSS) $(TARGET)
	@echo run GCC_cross_build-test
	sh GCC_cross_build-test $(CROSS) $(TARGET) $(SYS_compiler)


.PHONY: binutils
binutils: binutils-2.38
	mkdir -m=rwx -p $(BINUTILS_BUILD)
	-cp binutils-2.38 $(CROSS) -r -n

	mkdir -m=rwx -p $(SYS_BINUTILS_BUILD)

	chmod ugo+rwx $(CROSS)/binutils-2.38/*

.PHONY: gcc
gcc: gcc-11.3.0
	
	mkdir -m=rwx -p $(GCC_BUILD)
	-cp gcc-11.3.0 $(CROSS) -r -n

	mkdir -m=rwx -p $(GCC_CROSS)

	mkdir -m=rwx -p $(CROSS)/SYS_compiler

	mkdir -m=rwx -p $(CROSS)/CROSS_compiler

	chmod ugo+rwx $(CROSS)/gcc-11.3.0/*

test_dawnload: gcc-11.3.0


binutils-2.38: binutils-2.38.tar.xz
	tar -vxf $<



gcc-11.3.0: gcc-11.3.0.tar.xz
	tar -vxf $<

binutils-2.38.tar.xz:
	wget http://ftpmirror.gnu.org/binutils/binutils-2.38.tar.xz



gcc-11.3.0.tar.xz:
	wget http://ftpmirror.gnu.org/gcc/gcc-11.3.0/gcc-11.3.0.tar.xz

# test gcc
test_sys_gcc: bootstrap-test
	/home/yasser/src/cross/build-gcc/bin/gcc test_sys_gcc.cpp -o test_sys_gcc
	./test_sys_gcc
	

.PHONY: 
clean: clean_GCC clean_SYS_BINUTILS clean_BINUTILS clean_GCC_CROSS


.PHONY: 
clean_GCC_CROSS:
	-rm -f -r $(GCC_CROSS)
	-rm -f -r $(CROSS_compiler)

	@echo ">>>>>> GCC-cross-build is removed"

.PHONY: 
clean_GCC:
	-rm -f -r $(GCC_BUILD)
	-rm -f -r $(SYS_compiler)

	@echo ">>>>>> GCC-sys-build is removed"

.PHONY: 
clean_BINUTILS:
	-rm -f -r $(BINUTILS_BUILD)

	@echo ">>>>>> binutils-build is removed"

.PHONY: 
clean_SYS_BINUTILS:
	-rm -f -r $(SYS_BINUTILS_BUILD)

	@echo ">>>>>> binutils-sys-build is removed"