# myos32
Project of small 32-bit OS

## building newlib

Required tools: i686-elf GCC Cross-Toolchain ([Build as Here](wiki.osdev.org/GCC_Cross-Compiler)), make.

Download newest newlib (4.1.0) and untar. 
Create build dir:
```
mkdir build-newlib
cd build-newlib
```
Test i686-elf toolchain can be found:
```
whereis i686-elf-gcc
```
If whereis can't found toolchain, make sure it's in PATH and is relative to `/`.

Configure and build newlib:
```
../newlib-x.y.z/configure --prefix=/usr --target=i686-elf
make all
```
Install in sysroot:
```
make DESTDIR=/path/to/sysroot/ install
```
Create required links in sysroot:
```
cd /path/to/sysroot/usr
ln -s i686-elf/include .
ln -s i686-elf/lib .
```

## building kernel

Building requires newlib, i686-elf GCC Cross-Toolchain, nasm, make.

Edit Makefile for your toolchain and sysroot:
```
SYSROOT := /path/to/sysroot
TOOLCHAIN := /path/to/toolchain/bin/i686-elf-
```
Build kernel:
```
make build
```
Ready kernel image is in dist/kernel-i386.bin

Can be booted from grub cli:
```
multiboot2 /path/to/kernel-i386.bin
boot
```