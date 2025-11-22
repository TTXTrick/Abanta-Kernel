# Simple build for Abanta minimal kernel (multiboot2 + GRUB)
# Requirements: nasm, gcc, ld, grub-mkrescue (or grub-install tools), xorriso, qemu-system-x86

OUTDIR = build
SRCDIR = src

KERNEL_ELF = $(OUTDIR)/kernel.elf
KERNEL_BIN = $(OUTDIR)/kernel.bin
ISO = $(OUTDIR)/abanta.iso
GRUB_DIR = $(OUTDIR)/iso/boot/grub

AS = nasm
CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-pic
LDFLAGS = -T linker.ld -nostdlib

OBJS = $(OUTDIR)/boot.o $(OUTDIR)/kernel.o

.PHONY: all clean run iso

all: $(ISO)

$(OUTDIR):
	mkdir -p $(OUTDIR)

# assemble boot (boot.S)
$(OUTDIR)/boot.o: $(SRCDIR)/boot.S | $(OUTDIR)
	$(AS) -f elf64 $< -o $@

# compile kernel C
$(OUTDIR)/kernel.o: $(SRCDIR)/kernel.c $(SRCDIR)/kernel.h | $(OUTDIR)
	$(CC) -c $(CFLAGS) $< -o $@

# link ELF kernel (multiboot2 header is in boot.o)
$(KERNEL_ELF): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# convert ELF to raw bin (not strictly necessary for grub; keep for inspection)
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# make a grub iso
$(ISO): $(KERNEL_ELF)
	mkdir -p $(GRUB_DIR)
	# place kernel in iso/boot
	cp $< $(OUTDIR)/iso/boot/kernel.elf
	# grub.cfg
	printf 'set timeout=5\nset default=0\nmenuentry "Abanta kernel" { multiboot2 /boot/kernel.elf }\n' > $(GRUB_DIR)/grub.cfg
	# build iso (requires grub-mkrescue + xorriso)
	grub-mkrescue -o $@ $(OUTDIR)/iso 2>/dev/null || (echo "grub-mkrescue failed - try installing grub2-common / grub-mkrescue / xorriso"; false)

run: $(ISO)
	qemu-system-x86_64 -m 512M -cdrom $(ISO) -boot d -serial stdio

clean:
	rm -rf $(OUTDIR)
