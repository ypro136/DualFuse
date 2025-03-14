# The GNU Compiler Collection (GCC) is a toolkit for compiling and assembling programs for a wide variety of platforms and languages. 
# It is one of the core utilities in the Free Software Foundation's GNU Project. https://wiki.osdev.org/GCC      <<<<<<<<<<<<<<<<<<<<<<<<<
# this is a make file that configers gcc and build's it for a custume target called x86_64-dualfuse to be used dualfuse os

# modify enviroment variables in config.sh in project root
PATCH_DIR= $(ENVIROMENT_DIR)/patches
AUTOMAKE_BUILD= $(AUTOTOOLS_dir)/automake
CROSS_compiler=$(CROSS)/CROSS_compiler
TAR_DIR= $(ENVIROMENT_DIR)/tars
AUTOMAKE_SRC= $(ENVIROMENT_DIR)/automake-$(AUTOMAKE_VERTION)
BUILD_DIR=$(CROSS)/build
AUTOTOOLS_dir=$(ENVIROMENT_DIR)/autotools
AUTOMAKE=$(AUTOTOOLS_dir)/automake/bin/automake

LOG_DIR=$(CROSS)/logs
TIME_LOG=$(ENVIROMENT_DIR)/log.txt

GCC_TAR= gcc-$(GCC_VERSION).tar.xz
BINUTILS_TAR= binutils-$(BINUTILS_VERSION).tar.xz
AUTOMAKE_TAR= automake-$(AUTOMAKE_VERTION).tar.gz

AUTOTOOLS= $(AUTOMAKE_SRC)

#MAKE_ARGS= -j$(CORE_COUNT)
MAKE_ARGS= -j8


# test gcc
.PHONY: test_sys_gcc
test_sys_gcc: bootstrap #clean_src clean_tars
	$(CROSS_compiler)/bin/$(TARGET)-gcc test_sys_gcc.cpp -o test_sys_gcc -nostdlib -nostartfiles
	./test_sys_gcc
	date +%T >> $(TIME_LOG)

.PHONY: bootstrap
bootstrap: clean set_up_log binutils gcc
	date +%T >> $(TIME_LOG)
	@echo ">>> run setup-gcc\n" >> $(TIME_LOG)
	@echo ">>> cross is $(CROSS)\n" >> $(TIME_LOG)
#	sh setup-gcc $(TIME_LOG)

	date +%T >> $(TIME_LOG)
	@echo ">>> run GCC_cross_build $(MAKE_ARGS)\n" >> $(TIME_LOG)
	sh GCC_cross_build $(MAKE_ARGS) $(TIME_LOG)

.PHONY: binutils
binutils: $(AUTOMAKE_BUILD) binutils_src 
	-cp binutils_src $(CROSS) -r --update=none
	chmod ugo+rwx $(CROSS)/binutils_src/*
	@echo ">>> binutils directores set up\n" >> $(TIME_LOG)

.PHONY: gcc
gcc: $(AUTOMAKE_BUILD) gcc_src 
	-cp gcc_src $(CROSS) -r --update=none
	chmod ugo+rwx $(CROSS)/gcc_src/*
	@echo ">>> gcc directores set up\n" >> $(TIME_LOG)

.PHONY: autotools
autotools: $(AUTOTOOLS)
	sh $(AUTOMAKE) --version
	sh $(AUTOCONF) --version

$(AUTOMAKE_BUILD): #$(AUTOMAKE_TAR)
	tar -xf $(AUTOMAKE_TAR)
	mkdir -m=rwx -p $(AUTOMAKE_BUILD)
	sh autotools.sh "$(AUTOMAKE_BUILD)" "$(AUTOMAKE_SRC)" "--target="$(HOST)" --build="$(HOST)" --host="$(HOST)" --prefix="$(AUTOMAKE_BUILD)""

binutils_src: $(BINUTILS_TAR)
	tar -xf $(BINUTILS_TAR)
	cp binutils-$(BINUTILS_VERSION) binutils_src -r --update=none
	sh patch_binutils $(PATCH_DIR) | tee patch_binutils.log
	@echo ">>> binutils-$(BINUTILS_VERSION) extracted and patched\n" >> log.txt

gcc_src: $(GCC_TAR)
	tar -xf $(GCC_TAR)
	cp gcc-$(GCC_VERSION) gcc_src -r --update=none
	sh patch_gcc $(PATCH_DIR) | tee patch_gcc.log
	@echo ">>> gcc-$(GCC_VERSION) extracted and patched and download_prerequisites ran\n" >> log.txt

$(BINUTILS_TAR):
	wget http://ftpmirror.gnu.org/binutils/binutils-$(BINUTILS_VERSION).tar.xz
	@echo ">>> $(BINUTILS_TAR) downloaded\n" >> log.txt

$(GCC_TAR):
	wget http://ftpmirror.gnu.org/gcc/gcc-$(GCC_VERSION)/gcc-$(GCC_VERSION).tar.xz
	@echo ">>> $(GCC_TAR) downloaded\n" >> log.txt

$(AUTOMAKE_TAR):
	wget https://ftp.gnu.org/gnu/automake/automake-$(AUTOMAKE_VERTION).tar.gz
	@echo ">>> $(AUTOMAKE_TAR) downloaded\n" >> log.txt


.PHONY:$LOG_DIR
set_up_log:
	date +%T > log.txt
	mkdir -p $(LOG_DIR)
	@echo ">>> init log <<<\n" >> log.txt

.PHONY: 
clean: clean_GCC_CROSS clean_sys_root #clean_tars

.PHONY:
clean_sys_root:
	-rm -f -r $(SYSROOT)/*

.PHONY: 
clean_GCC_CROSS:
	-rm -f -r $(CROSS)/binutils_src
	-rm -f -r $(CROSS)/gcc_src
	-rm -f -r $(BUILD_DIR)
	-rm -f -r $(CROSS_compiler)
	-rm -f -r $(LOG_DIR)
	@echo ">>> GCC-cross-build is removed\n" >> log.txt

.PHONY: 
clean_tars:
	-rm -f -r *.tar.xz
	@echo ">>> tar files are removed\n" >> log.txt

.PHONY: 
clean_src:
	-rm -f -r binutils_src
	-rm -f -r gcc_src
	@echo ">>> src files are removed\n" >> log.txt