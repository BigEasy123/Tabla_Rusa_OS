TARGET = myos
CC = gcc
LD = ld
CFLAGS = -m32 -ffreestanding -nostdlib -fno-pic -fno-pie
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = boot.o kernel.o console.o keyboard.o

all: $(TARGET).iso

boot.o: boot.s
	$(CC) $(CFLAGS) -c $< -o $@

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

grub.cfg:
	mkdir -p iso/boot/grub
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "MyOS" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/$(TARGET).bin' >> iso/boot/grub/grub.cfg
	echo '  boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg

$(TARGET).iso: $(TARGET).bin grub.cfg
	cp $(TARGET).bin iso/boot/
	grub-mkrescue -o $(TARGET).iso iso

run: $(TARGET).iso
	qemu-system-i386 -cdrom $(TARGET).iso

clean:
	rm -rf *.o *.bin *.iso iso
