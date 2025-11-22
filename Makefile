# Simple build for Abanta kernel (GRUB / Multiboot2)
# Targets: all, iso, run, clean

OUTDIR = build
SRCDIR = src
KERNEL = $(OUTDIR)/kernel.elf
ISO    = $(OUTDIR)/abanta.iso

AS = gcc
CC = gcc
LD = ld
OBJCOPY = objcopy
GRUB_MKRESCUE = grub-mkrescue

CFLAGS = -Wall -Wextra -O2 -fno-builtin -fno-stack-protector -ffreestanding -mno-red-zone -m64
ASFLAGS = -fno-asynchronous-unwind-tables -fno-exceptions -m64
LDFLAGS = -nostdlib -T link.ld

SRCS = $(SRCDIR)/kernel.c
ASMS = $(SRCDIR)/boot.S

.PHONY: all iso run clean

all: $(KERNEL)

$(OUTDIR):
	mkdir -p $(OUTDIR)

# assemble C and assembly then link
$(OUTDIR)/boot.o: $(ASMS) | $(OUTDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OUTDIR)/kernel.o: $(SRCS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OUTDIR)/boot.o $(OUTDIR)/kernel.o | $(OUTDIR)
	$(LD) $(LDFLAGS) $(OUTDIR)/boot.o $(OUTDIR)/kernel.o -o $@
	$(OBJCOPY) --strip-all -O elf64-x86-64 $@ $@ # ensure ELF64 output (keeps symbols minimal)

# iso layout expected by grub-mkrescue
iso: all | $(OUTDIR)
	mkdir -p $(OUTDIR)/iso/boot/grub
	cp $(KERNEL) $(OUTDIR)/iso/boot/
	cp grub/grub.cfg $(OUTDIR)/iso/boot/grub/
	# create standalone iso (grub-mkrescue typically uses xorriso internally)
	$(GRUB_MKRESCUE) -o $(ISO) $(OUTDIR)/iso 2>/dev/null || (echo "grub-mkrescue failed: ensure grub-mkrescue installed"; exit 1)
	@echo "ISO created: $(ISO)"

run: iso
	qemu-system-x86_64 -m 512M -serial stdio -drive file=$(ISO),format=raw,if=virtio

clean:
	rm -rf $(OUTDIR)
