c_sources := $(shell find src -name *.c)
c_objects := $(patsubst src/%.c, build/%.o, $(c_sources))
headers := $(shell find inc -name *.h)

asm_sources := $(shell find src -name *.asm)
asm_objects := $(patsubst src/%.asm, build/%.asm.o, $(asm_sources))

objects := $(c_objects) $(asm_objects)

C_FLAGS := -I inc -m32 -O2 -fno-pie -fno-builtin -fomit-frame-pointer -march=pentium3 -fno-stack-protector -fno-asynchronous-unwind-tables

.SILENT: run-i386 $(objects) dist/kernel-i386.elf dist/kernel-i386.bin size-i386 mount umount dist/disk.img clean
.PHONY: size-i386 mount umount build run build-i386 run-i386

build: build-i386
run: run-i386

run-i386: dist/disk.img
	qemu-system-i386 -drive file=dist/disk.img,format=raw,index=0,media=disk -monitor stdio

$(c_objects): build/%.o : src/%.c $(headers)
	mkdir -p $(dir $@)
	printf "building %s\n" $(patsubst build/%.o, %.c, $@)
	gcc $(patsubst build/%.o, src/%.c, $@) -c -o $@ $(C_FLAGS)

$(asm_objects): build/%.asm.o : src/%.asm
	mkdir -p $(dir $@)
	printf "building %s\n" $(patsubst build/%.asm.o, %.asm, $@)
	nasm -f elf32 $(patsubst build/%.asm.o, src/%.asm, $@) -o $@

build-i386: dist/kernel-i386.bin dist/disk.img size-i386
dist/kernel-i386.elf: $(c_objects) $(asm_objects) linker-i386.ld
	mkdir -p dist
	ld -o dist/kernel-i386.elf -T linker-i386.ld $(asm_objects) $(c_objects) -Map=dist/kernel-i386.map
	objdump -d dist/kernel-i386.elf -M i386 -M intel > dist/kernel-i386.disasm

dist/kernel-i386.bin: dist/kernel-i386.elf
	objcopy -O binary dist/kernel-i386.elf dist/kernel-i386.bin -R .bss.*

size-i386: dist/kernel-i386.elf
	size dist/kernel-i386.elf

mount:
	mkdir -p dist/mnt
	sudo mount dist/disk.img dist/mnt -o loop,offset=1M || true
umount:
	sudo umount dist/mnt || true
	rm -r dist/mnt || true
dist/disk.img: dist/kernel-i386.bin
	printf "updating disk\n"
	mkdir -p dist/mnt
	sudo mount dist/disk.img dist/mnt -o loop,offset=1M || true
	sudo cp dist/kernel-i386.bin dist/mnt
	sudo umount dist/mnt || true
	rm -r dist/mnt || true

clean:
	printf "cleaning\n"
	rm -rf build/
	rm -f dist/kernel*