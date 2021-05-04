SYSROOT := /home/dominik/myos-env32/ksysroot
TOOLCHAIN := /home/dominik/myos-env32/cross/bin/i686-elf-

c_sources := $(shell find src -name *.c)
c_objects := $(patsubst %.c, build/%.o, $(c_sources))
c_other_sources := $(shell find other -name *.c)
c_other_objects := $(patsubst %.c, build/%.o, $(c_other_sources))
headers := $(shell find inc -name *.h) $(shell find other -name *.h)

asm_sources := $(shell find src -name *.asm)
asm_objects := $(patsubst %.asm, build/%.asm.o, $(asm_sources))

objects := $(c_objects) $(asm_objects) $(c_other_objects)

warnings_src := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wno-missing-prototypes -Wno-missing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wstrict-prototypes

warnings_other := -Wall -Wextra -Wshadow 

C_FLAGS := -I inc -I other -m32 -O2 -fno-pie -fno-builtin -fomit-frame-pointer -march=pentium3 -fno-stack-protector -fno-asynchronous-unwind-tables
C_FLAGS += -ffunction-sections -fdata-sections -masm=intel -ffreestanding -I $(SYSROOT)/usr/include
LD_FLAGS := -Wl,--gc-sections -nostdlib -ffreestanding -lgcc --sysroot=$(SYSROOT) -lc -lm

.SILENT: run-i386 $(objects) dist/kernel-i386.elf dist/kernel-i386.bin size-i386 mount umount dist/disk_exfat.vdi clean
.PHONY: size-i386 mount umount build run build-i386 run-i386

build: build-i386
run: run-i386

CC := $(TOOLCHAIN)gcc
OBJDUMP := $(TOOLCHAIN)objdump
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size

run-i386: build-i386 dist/disk_exfat.vdi
	qemu-system-i386 -hda dist/disk_exfat.vdi -monitor stdio -rtc base=utc
	# VBoxManage startvm myos

$(c_objects): build/%.o : %.c $(headers)
	mkdir -p $(dir $@)
	printf "building %s\n" $(patsubst build/%.o, %.c, $@)
	$(CC) $(patsubst build/%.o, %.c, $@) -c -o $@ $(C_FLAGS) $(warnings_src)

$(c_other_objects): build/other/%.o : other/%.c $(headers)
	mkdir -p $(dir $@)
	printf "building %s\n" $(patsubst build/%.o, %.c, $@)
	$(CC) $(patsubst build/%.o, %.c, $@) -c -o $@ $(C_FLAGS) $(warnings_others)

$(asm_objects): build/%.asm.o : %.asm
	mkdir -p $(dir $@)
	printf "building %s\n" $(patsubst build/%.asm.o, %.asm, $@)
	nasm -f elf32 $(patsubst build/%.asm.o, %.asm, $@) -o $@

build-i386: dist/kernel-i386.bin size-i386
dist/kernel-i386.elf: $(c_objects) $(asm_objects) $(c_other_objects) linker-i386.ld
	printf "linking %s\n" $@
	mkdir -p dist
	$(CC) -o dist/kernel-i386.elf -T linker-i386.ld $(asm_objects) $(c_objects) $(c_other_objects) -Wl,-Map=dist/kernel-i386.map $(LD_FLAGS)
	$(OBJDUMP) -d dist/kernel-i386.elf -M i386 -M intel > dist/kernel-i386.disasm

dist/kernel-i386.bin: dist/kernel-i386.elf
	$(OBJCOPY) -O binary dist/kernel-i386.elf dist/kernel-i386.bin 

size-i386: dist/kernel-i386.elf
	printf "size\n"
	$(SIZE) dist/kernel-i386.elf

mount:
	mkdir -p dist/mnt
	guestmount -w -a dist/disk_exfat.vdi -m /dev/sda1 dist/mnt
umount:
	guestunmount dist/mnt
	rm -r dist/mnt || true
dist/disk_exfat.vdi: dist/kernel-i386.bin
	printf "updating disk\n"
	mkdir -p dist/mnt
	guestmount -w -a dist/disk_exfat.vdi -m /dev/sda1 dist/mnt
	cp dist/kernel-i386.bin dist/mnt
	guestunmount dist/mnt
	rm -r dist/mnt || true
	sleep 1

clean:
	printf "cleaning\n"
	rm -rf build/
	rm -f dist/kernel*