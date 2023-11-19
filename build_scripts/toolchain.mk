toolchain: toolchain_binutils toolchain_gcc

BINUTILS_URL = https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz

toolchain_binutils:
	mkdir toolchain 
	cd toolchain && wget $(BINUTILS_URL)
	cd toolchain && tar -xf binutils-2.41.tar.xz
	mkdir toolchain/binutils-build-2.41
	cd toolchain/binutils-build-2.41 && 

