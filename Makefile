# Makefile — Option B kernel (GRUB ELF kernel)
# Usage:
#   make         -> build kernel ELF (build/kernel.bin)
#   make iso     -> build bootable ISO (build/abanta.iso) using grub-mkrescue
#   make run     -> boot the ISO in qemu (requires grub-mkrescue and qemu-system-x86_64)

CC = gcc
LD = ld
NASM = nasm
OBJCOPY = objcopy

SRC = src
BUILD = build
ISO_DIR = iso
GRUB_DIR = $(ISO_DIR)/boot/grub

KERN = build/kernel.bin
ISO = build/abanta.iso

CFLAGS = -m64 -ffreestanding -fno-builtin -fno-pic -O2 -Wall -Wextra -nostdlib -fno-stack-protector
NASMFLAGS = -f elf64

.PHONY: all clean iso run

all: $(KERN)

$(BUILD):
	mkdir -p $(BUILD)

# assemble boot (NASM)
$(BUILD)/boot.o: $(SRC)/boot.S | $(BUILD)
	$(NASM) $(NASMFLAGS) $< -o $@

# compile kernel C
$(BUILD)/kernel.o: $(SRC)/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# link kernel (ELF)
$(KERN): $(BUILD)/boot.o $(BUILD)/kernel.o linker.ld
	$(LD) -nostdlib -T linker.ld -o $(KERN) $(BUILD)/boot.o $(BUILD)/kernel.o

# build ISO using grub-mkrescue (if available)
iso: $(KERN)
	rm -rf $(ISO_DIR) build_iso
	mkdir -p $(GRUB_DIR)
	cp $(KERN) $(ISO_DIR)/boot/kernel.bin
	cp grub/grub.cfg $(GRUB_DIR)/grub.cfg
	# Use grub-mkrescue to build a bootable ISO (common on Debian/Ubuntu)
	@if command -v grub-mkrescue >/dev/null 2>&1; then \
	  grub-mkrescue -o $(ISO) $(ISO_DIR) >/dev/null 2>&1 || { echo "grub-mkrescue failed"; exit 1; } \
	else \
	  echo "Error: grub-mkrescue not found. Install grub-pc-bin (Debian) or build the ISO manually."; exit 1; \
	fi

# run ISO in QEMU
run: iso
	@echo "Starting QEMU..."
	# For BIOS boot (GRUB ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 512M

clean:
	rm -rf $(BUILD) $(ISO_DIR) $(ISO)
