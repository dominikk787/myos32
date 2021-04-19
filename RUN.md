# myos32 running
## creating image
create empty disk image 32MB:
```
dd if=/dev/zero of=disk.img bs=1M count=32
```

create partition table:
```
fdisk disk.img
o
n
(default options)
w
```

create loop device:
```
sudo kpartx -av disk.img
```
create fs on partition:
```
mkfs.ext2 /dev/mapper/loopXp1
```
mount fs:
```
mkdir -p mnt
sudo mount /dev/mapper/loopXp1 mnt
```
install grub on disk:
```
sudo grub-install --no-floppy --install-modules="biosdisk part_msdos ext2 configfile normal multiboot2" --boot-directory=/path/to/mnt /dev/loopX
```
create grub.cfg:
```
sudo nano mnt/grub/grub.cfg
```
```
set default=0
set timeout=0

menuentry 'MyOS32' {
    multiboot2 /kernel-i386.bin
}
```
umount disk and delete loop:
```
sudo umount mnt
sudo kpartx -d disk.img
rm -r mnt
```
## updating image and running
mount image and copy kernel:
```
mkdir -p mnt
sudo mount disk.img mnt -o loop,offset=1M
sudo cp dist/kernel-i386.bin mnt/
sudo umount mnt
rm -r mnt
```
run with qemu:
```
qemu-system-i386 -drive file=disk.img,format=raw,index=0,media=disk
```
if you want to run qemu from make:
```
mv disk.img dist/
```
```
make run
```