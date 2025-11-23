# Simple Makefile for Abanta kernel (Multiboot2 + GRUB)
# Builds a 64-bit freestanding kernel and an ISO that boots under GRUB.

# Tools
CC = cc
LD = ld
NASM = nasm
OBJCOPY = objcopy
GRUB_MKRESCUE = grub-mkrescue

# Directories
SRC = src
BUILD = build
ISO = iso
BIN = bin

# Targets
KERNEL = $(BUILD)/kernel.elf
ISO_IMG = $(BIN)/abanta.iso

# Compiler/linker flags for freestanding 64-bit kernel
CFLAGS = -m64 -ffreestanding -fno-pie -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -I.
LDFLAGS = -nostdlib -static -T linker.ld

# NASM
ASMFLAGS = -f elf64

# OVMF/UEFI files for QEMU run (adjust if on your system)
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS_4M.fd

.PHONY: all clean run iso

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(ISO):
	mkdir -p $(ISO)/boot/grub

$(BIN):
	mkdir -p $(BIN)

# Assemble boot.S (multiboot header stub)
$(BUILD)/boot.o: $(SRC)/boot.S | $(BUILD)
	$(NASM) $(ASMFLAGS) $< -o $@

# Compile kernel C
$(BUILD)/kernel.o: $(SRC)/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel ELF64
$(KERNEL): $(BUILD)/boot.o $(BUILD)/kernel.o linker.ld | $(BUILD)
	$(LD) $(LDFLAGS) $(BUILD)/boot.o $(BUILD)/kernel.o -o $(KERNEL)

# Produce ISO with GRUB
$(ISO_IMG): $(KERNEL) | $(ISO) $(BIN)
	cp $(KERNEL) $(ISO)/boot/kernel.elf
	cp grub.cfg $(ISO)/boot/grub/grub.cfg
	# grub-mkrescue may require xorriso / grub-pc-bin packages
	$(GRUB_MKRESCUE) -o $(ISO_IMG) $(ISO) 2>/dev/null || (echo "grub-mkrescue failed — ensure grub-mkrescue and xorriso are installed." && false)

# run: build iso and launch QEMU w/ OVMF (UEFI). If you want BIOS (legacy) editing, modify accordingly.
run: $(ISO_IMG)
	@echo "Running QEMU (OVMF paths):"
	@echo " OVMF_CODE=$(OVMF_CODE)"
	@echo " OVMF_VARS=$(OVMF_VARS)"
	if [ ! -f "$(OVMF_CODE)" ]; then echo "OVMF_CODE not found at $(OVMF_CODE)"; exit 1; fi
	if [ ! -f "$(OVMF_VARS)" ]; then echo "OVMF_VARS not found at $(OVMF_VARS)"; exit 1; fi
	qemu-system-x86_64 \
		-m 512M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS) \
		-drive format=raw,file=$(ISO_IMG) \
		-nographic

clean:
	rm -rf $(BUILD) $(ISO) $(BIN)
