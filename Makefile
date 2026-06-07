ISO_DIR := iso
BUILD   := build

CC := gcc
AS := nasm
LD := ld

CFLAGS := -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -O2 -Wall -Wextra
LDFLAGS := -m elf_i386

SRC_C := src/kmain.c src/idt.c src/keyboard.c src/mouse.c src/console.c src/timer.c src/heap.c src/memory.c src/paging.c src/fs.c src/process.c src/window.c src/mathlib.c src/events.c src/security.c src/privacy.c src/service.c src/vfs.c src/net.c src/gui.c src/jobs.c src/tests.c src/shell.c src/editor.c src/loader.c src/gfx.c src/taskman.c src/fd.c src/sched.c src/fb.c src/object.c src/block.c src/lang.c src/project.c src/kernel_modules.c src/policy.c src/research.c src/science.c
SRC_S := src/boot.S src/isr.S

OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC_C)) \
       $(patsubst src/%.S,$(BUILD)/%.o,$(SRC_S))

.PHONY: all clean iso run run-gui run-gtk run-sdl run-text

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

run: run-sdl

run-gui: run-sdl

run-sdl: iso
	qemu-system-i386 -cdrom myos.iso -display sdl -vga std -serial file:serial.log

run-gtk: iso
	qemu-system-i386 -cdrom myos.iso -display gtk -vga std -serial file:serial.log

run-text: iso
	qemu-system-i386 -cdrom myos.iso -display curses -serial file:serial.log

clean:
	rm -rf $(BUILD) myos.iso $(ISO_DIR)
