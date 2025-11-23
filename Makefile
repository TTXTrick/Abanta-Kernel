# Simple build for Abanta kernel skeleton (GRUB iso)
# Requires: gcc, ld, nasm, grub-mkrescue (and xorriso), qemu-system-x86_64 (for `make run`)

KSRC = src
BUILD = build
ISO = iso
BIN = bin

KERNEL_OBJ = $(BUILD)/boot.o $(BUILD)/kernel.o
KERNEL_ELF = $(BUILD)/kernel.elf
ISO_IMAGE = $(BIN)/abanta.iso

# Paths to OVMF (adjust if necessary)
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd

CFLAGS = -m64 -ffreestanding -fno-pie -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -I.
LDFLAGS = -nostdlib -T linker.ld

.PHONY: all clean iso run

all: $(KERNEL_ELF)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: $(KSRC)/boot.S | $(BUILD)
	nasm -f elf64 $< -o $@

$(BUILD)/kernel.o: $(KSRC)/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJ)
	ld $(LDFLAGS) -o $@ $(KERNEL_OBJ)

iso: $(KERNEL_ELF)
	# prepare iso tree
	rm -rf $(ISO)
	mkdir -p $(ISO)/boot/grub
	cp $(KERNEL_ELF) $(ISO)/boot/kernel.elf
	cp grub.cfg $(ISO)/boot/grub/grub.cfg
	# create ISO (uses grub-mkrescue which needs xorriso on many distros)
	mkdir -p $(BIN)
	grub-mkrescue -o $(ISO_IMAGE) $(ISO) 2>/dev/null || \
		(echo "grub-mkrescue failed — ensure grub-mkrescue and xorriso are installed." && false)

run: iso
	# Run under QEMU using OVMF (UEFI). Adjust OVMF paths if needed.
	if [ ! -f "$(OVMF_CODE)" ]; then echo "OVMF_CODE not found at $(OVMF_CODE)"; exit 1; fi
	if [ ! -f "$(OVMF_VARS)" ]; then echo "OVMF_VARS not found at $(OVMF_VARS)"; exit 1; fi
	qemu-system-x86_64 -enable-kvm -m 1024 \
	  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	  -drive if=pflash,format=raw,file=$(OVMF_VARS) \
	  -cdrom $(ISO_IMAGE) \
	  -boot d

clean:
	rm -rf $(BUILD) $(ISO) $(BIN)
