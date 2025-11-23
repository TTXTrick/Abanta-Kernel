# ============================
# Abanta OS Makefile
# ============================

FORMAT ?= elf          # Valid: elf | bin
KERNEL_NAME := kernel.$(FORMAT)

CC = gcc
LD = ld
AS = nasm

CFLAGS = -std=gnu11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wextra -mno-red-zone -m64
ASFLAGS = -f elf64
LDFLAGS = -nostdlib -T linker.ld

all: build/$(KERNEL_NAME)

# -------------------------------------------------------------
# Build kernel.elf
# -------------------------------------------------------------
build/kernel.elf: build/boot64.o build/kernel.o
	$(LD) $(LDFLAGS) -o build/kernel.elf build/boot64.o build/kernel.o

# -------------------------------------------------------------
# Optional: build kernel.bin (raw flat binary)
# -------------------------------------------------------------
build/kernel.bin: build/kernel.elf
	objcopy -O binary build/kernel.elf build/kernel.bin

# -------------------------------------------------------------
# Object files
# -------------------------------------------------------------
build/boot64.o: src/boot64.S
	mkdir -p build
	$(AS) $(ASFLAGS) src/boot64.S -o build/boot64.o

build/kernel.o: src/kernel.c
	mkdir -p build
	$(CC) $(CFLAGS) -c src/kernel.c -o build/kernel.o

# -------------------------------------------------------------
# ISO BUILDER
# -------------------------------------------------------------
bin/abanta.iso: build/$(KERNEL_NAME) grub.cfg
	rm -rf iso
	mkdir -p iso/boot/grub
	mkdir -p bin
	cp build/$(KERNEL_NAME) iso/boot/$(KERNEL_NAME)
	cp grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o bin/abanta.iso iso

iso: bin/abanta.iso

# -------------------------------------------------------------
# Run under QEMU (BIOS mode - safe)
# -------------------------------------------------------------
run: bin/abanta.iso
	qemu-system-x86_64 -cdrom bin/abanta.iso

clean:
	rm -rf build iso bin
