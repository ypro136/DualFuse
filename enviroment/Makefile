CROSS=$(HOME)/src/cross
#for Cross-Compiler build

GCC_BUILD=$(CROSS)/build-gcc

SYS_compiler=$(CROSS)/SYS_compiler

BINUTILS_BUILD=$(CROSS)/binutils-build

MPC_BUILD=$(CROSS)/build-mpc

GCC_CROSS=$(CROSS)/GCC_cross

CROSS_compiler=$(CROSS)/CROSS_compiler

SYS_BINUTILS_BUILD=$(CROSS)/sys-binutils-build

TIME_LOG= $(HOME)/progs/DualFuse/enviroment/log.txt

#SYS_compiler=$(GCC_BUILD)/bin/i686-elf-g++

TARGET=i686-elf

CORE_COUNT= 12

MAKE_ARGS= -j$(CORE_COUNT)

BINUTILS_VERSION=2.41
GCC_VERSION=13.2.0


# test gcc
test_sys_gcc: bootstrap
	$(CROSS_compiler)/bin/i686-elf-g++ test_sys_gcc.cpp -o test_sys_gcc -nostdlib -nostartfiles
	./test_sys_gcc
	date +%T >> log.txt


test_echo:
	@echo $(CORE_COUNT)


bootstrap: clean set_up_log binutils gcc
	date +%T >> log.txt
	@echo ">>> run setup-gcc\n" >> log.txt
	sh setup-gcc $(TIME_LOG)

# date +%T >> log.txt
# @echo ">>> run build-gcc $(MAKE_ARGS)\n" >> log.txt
# sh build-gcc $(MAKE_ARGS) $(TIME_LOG)

	date +%T >> log.txt
	@echo ">>> run GCC_cross_build $(MAKE_ARGS)\n" >> log.txt
	sh GCC_cross_build $(MAKE_ARGS) $(TIME_LOG)

	cd $(CROSS) && \
	ls

.PHONY:
remove_setup:
	sh remove-setup

# test to add Positional Parameters to the compile scripts (arguments to be implemented)
bootstrap-test:
	@echo run build-gcc-test
	sh build-gcc-test $(CROSS) $(TARGET)
	@echo run GCC_cross_build-test
	sh GCC_cross_build-test $(CROSS) $(TARGET) $(SYS_compiler)


.PHONY: binutils
binutils: #binutils_src
	mkdir -m=rwx -p $(BINUTILS_BUILD)
	-cp binutils_src $(CROSS) -r -n

	mkdir -m=rwx -p $(SYS_BINUTILS_BUILD)

	chmod ugo+rwx $(CROSS)/binutils_src/*

	@echo ">>> binutils directores set up\n" >> log.txt

.PHONY: gcc
gcc: #gcc_src
	
	mkdir -m=rwx -p $(GCC_BUILD)
	-cp gcc_src $(CROSS) -r -n

	mkdir -m=rwx -p $(GCC_CROSS)

	mkdir -m=rwx -p $(CROSS)/SYS_compiler

	mkdir -m=rwx -p $(CROSS)/CROSS_compiler

	chmod ugo+rwx $(CROSS)/gcc_src/*

	@echo ">>> gcc directores set up\n" >> log.txt

test_dawnload: 


binutils_src: binutils-$(BINUTILS_VERSION).tar.xz
	tar -vxf $<
	-cp binutils-$(BINUTILS_VERSION) binutils_src -r -n
	@echo ">>> binutils-$(BINUTILS_VERSION) extracted\n" >> log.txt



gcc_src: gcc-$(GCC_VERSION).tar.xz
	tar -vxf $<
	-cp gcc-$(GCC_VERSION) gcc_src -r -n
	@echo ">>> gcc-$(GCC_VERSION) extracted\n" >> log.txt

binutils-$(BINUTILS_VERSION).tar.xz:
	-rm -f -r binutils-*.tar.xz
	@echo ">>> binutils tar files are removed\n" >> log.txt

	wget http://ftpmirror.gnu.org/binutils/binutils-$(BINUTILS_VERSION).tar.xz
	@echo ">>> binutils-$(BINUTILS_VERSION).tar.xz downloaded\n" >> log.txt



gcc-$(GCC_VERSION).tar.xz:
	-rm -f -r gcc-*.tar.xz
	@echo ">>> gcc tar files are removed\n" >> log.txt
	
	wget http://ftpmirror.gnu.org/gcc/gcc-$(GCC_VERSION)/gcc-$(GCC_VERSION).tar.xz
	@echo ">>> gcc-$(GCC_VERSION).tar.xz downloaded\n" >> log.txt


.PHONY:
set_up_log:
	date +%T > log.txt
	@echo ">>> init log <<<\n" >> log.txt
	

.PHONY: 
clean: clean_src clean_GCC clean_SYS_BINUTILS clean_BINUTILS clean_GCC_CROSS #clean_tars


.PHONY: 
clean_GCC_CROSS:
	-rm -f -r $(GCC_CROSS)
	-rm -f -r $(CROSS_compiler)

	@echo ">>> GCC-cross-build is removed\n" >> log.txt

.PHONY: 
clean_GCC:
	-rm -f -r $(GCC_BUILD)
	-rm -f -r $(SYS_compiler)

	@echo ">>> GCC-sys-build is removed\n" >> log.txt

.PHONY: 
clean_BINUTILS:
	-rm -f -r $(BINUTILS_BUILD)

	@echo ">>> binutils-build is removed\n" >> log.txt

.PHONY: 
clean_SYS_BINUTILS:
	-rm -f -r $(SYS_BINUTILS_BUILD)

	@echo ">>> binutils-sys-build is removed\n" >> log.txt

.PHONY: 
clean_tars:
	-rm -f -r *.tar.xz

	@echo ">>> tar files are removed\n" >> log.txt

.PHONY: 
clean_src:
	-rm -f -r $(CROSS)/binutils_src
	-rm -f -r $(CROSS)/gcc_src

	@echo ">>> src files are removed\n" >> log.txt