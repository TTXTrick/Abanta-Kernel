# Abanta (multiboot2 / x86_64) kernel build
# Default: build ELF64 kernel (kernel.elf). Set KFORMAT=bin to build legacy ELF32 kernel.bin
# Requires: nasm, gcc, ld, objcopy, grub-mkrescue, xorriso, qemu-system-x86_64

KFORMAT ?= elf    # elf => ELF64 kernel (recommended). bin => legacy 32-bit kernel.bin

OUTDIR := build
BINDIR := bin
ISODIR := iso

SRCDIR := src
ASM := nasm
CC := gcc
LD := ld
OBJCOPY := objcopy

ASM64 := $(SRCDIR)/boot64.S
ASM32 := $(SRCDIR)/boot32.S    # optional if we ever want 32-bit bootstrap
KERNEL_C := $(SRCDIR)/kernel.c

CFLAGS64 := -std=gnu11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wextra -mno-red-zone -m64
LDFLAGS64 := -nostdlib -static -T linker64.ld

CFLAGS32 := -std=gnu11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wextra -m32
LDFLAGS32 := -nostdlib -static -T linker32.ld

# Outputs
KERNEL_ELF := $(OUTDIR)/kernel.elf
KERNEL_BIN := $(OUTDIR)/kernel.bin   # legacy 32-bit multiboot

.PHONY: all clean iso run

all: $(OUTDIR) $(BINDIR) $(if $(filter elf,$(KFORMAT)), $(KERNEL_ELF), $(KERNEL_BIN))

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# 64-bit build path
$(OUTDIR)/boot64.o: $(ASM64) | $(OUTDIR)
	$(ASM) -f elf64 $< -o $@

$(OUTDIR)/kernel.o: $(KERNEL_C) | $(OUTDIR)
	$(CC) $(CFLAGS64) -c $< -o $@

# link ELF64 kernel (multiboot2)
$(KERNEL_ELF): $(OUTDIR)/boot64.o $(OUTDIR)/kernel.o linker64.ld | $(OUTDIR)
	$(LD) $(LDFLAGS64) $(OUTDIR)/boot64.o $(OUTDIR)/kernel.o -o $@
	@echo "Built $@"

# 32-bit legacy multiboot (optional)
$(OUTDIR)/boot32.o: $(ASM32) | $(OUTDIR)
	$(ASM) -f elf32 $< -o $@

$(OUTDIR)/kernel32.o: $(KERNEL_C) | $(OUTDIR)
	$(CC) $(CFLAGS32) -c $< -o $@

$(KERNEL_BIN): $(OUTDIR)/boot32.o $(OUTDIR)/kernel32.o linker32.ld | $(OUTDIR)
	$(LD) $(LDFLAGS32) $(OUTDIR)/boot32.o $(OUTDIR)/kernel32.o -o $(OUTDIR)/kernel32.elf
	$(OBJCOPY) -O binary $(OUTDIR)/kernel32.elf $(KERNEL_BIN)
	@echo "Built $(KERNEL_BIN)"

# Create ISO with GRUB2, multiboot2 supports ELF64 if GRUB is modern
GRUB_CFG := grub.cfg
ISO := $(BINDIR)/abanta.iso

$(ISO): $(if $(filter elf,$(KFORMAT)), $(KERNEL_ELF), $(KERNEL_BIN)) $(GRUB_CFG) | $(BINDIR) $(ISODIR)
	rm -rf $(ISODIR)
	mkdir -p $(ISODIR)/boot/grub
ifeq ($(KFORMAT),elf)
	cp $(KERNEL_ELF) $(ISODIR)/boot/kernel.elf
	echo "Multiboot2 ELF64 kernel placed at /boot/kernel.elf"
else
	cp $(KERNEL_BIN) $(ISODIR)/boot/kernel.bin
endif
	cp $(GRUB_CFG) $(ISODIR)/boot/grub/grub.cfg
	# build ISO (grub-mkrescue uses xorriso)
	grub-mkrescue -o $(ISO) $(ISODIR) 2>/dev/null || (echo "grub-mkrescue failed — ensure grub-mkrescue/xorriso are installed." && false)

iso: $(ISO)

run: $(ISO)
	# Boot ISO under QEMU in UEFI or BIOS depending on the kernel format.
ifeq ($(KFORMAT),elf)
	# For ELF64 multiboot2 we use BIOS GRUB (GRUB will load ELF64 and should jump to 64-bit entry).
	# QEMU boots the ISO (BIOS GRUB). If you prefer OVMF (UEFI) adjust accordingly.
	qemu-system-x86_64 -m 1024 -cdrom $(ISO)
else
	qemu-system-x86_64 -m 1024 -cdrom $(ISO)
endif

clean:
	rm -rf $(OUTDIR) $(BINDIR) $(ISODIR)
