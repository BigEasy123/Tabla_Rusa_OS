ISO_DIR := iso
BUILD   := build

CC := gcc
AS := nasm
LD := ld

CFLAGS := -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -O2 -Wall -Wextra
LDFLAGS := -m elf_i386

SRC_C := src/kmain.c src/idt.c src/keyboard.c src/console.c src/timer.c src/heap.c src/memory.c src/paging.c
SRC_S := src/boot.S src/isr.S

OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC_C)) \
       $(patsubst src/%.S,$(BUILD)/%.o,$(SRC_S))

.PHONY: all clean iso run

all: $(BUILD)/kernel.elf

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.elf: $(OBJ) src/linker.ld
	$(LD) $(LDFLAGS) -T src/linker.ld -o $@ $(OBJ)

iso: all
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(BUILD)/kernel.elf $(ISO_DIR)/boot/kernel.elf
	@echo "set timeout=0" > $(ISO_DIR)/boot/grub/grub.cfg
	@echo "set default=0" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "menuentry \"myos\" {" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "    multiboot2 /boot/kernel.elf" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "    boot" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "}" >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso $(ISO_DIR)

run: iso
	qemu-system-i386 -cdrom myos.iso -serial stdio -display curses

clean:
	rm -rf $(BUILD) myos.iso $(ISO_DIR)
