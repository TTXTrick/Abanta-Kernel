# ===============================
#   Abanta Kernel - Final Makefile
# ===============================

TARGET = abanta
BUILD = build
ISO   = iso
BIN   = bin

SRC  = src
OBJ  = $(BUILD)/boot.o $(BUILD)/kernel.o

CC = cc
AS = nasm
LD = ld

CFLAGS = -m64 -ffreestanding -fno-pie -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -I.
LDFLAGS = -nostdlib -static -T linker.ld

OVMF_CODE_SYS = /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_SYS = /usr/share/OVMF/OVMF_VARS_4M.fd

# Local copies stored in repo
OVMF_CODE_LOCAL = OVMF_CODE.fd
OVMF_VARS_LOCAL = OVMF_VARS.fd


# ===============================
#            Build
# ===============================

all: $(BUILD)/kernel.elf

$(BUILD)/boot.o: $(SRC)/boot.S
	mkdir -p $(BUILD)
	$(AS) -f elf64 $(SRC)/boot.S -o $(BUILD)/boot.o

$(BUILD)/kernel.o: $(SRC)/kernel.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $(SRC)/kernel.c -o $(BUILD)/kernel.o

$(BUILD)/kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $(BUILD)/kernel.elf


# ===============================
#             ISO
# ===============================

iso: $(BUILD)/kernel.elf
	rm -rf $(ISO)
	mkdir -p $(ISO)/boot/grub
	cp $(BUILD)/kernel.elf $(ISO)/boot/kernel.elf
	cp grub.cfg $(ISO)/boot/grub/grub.cfg
	mkdir -p $(BIN)
	grub-mkrescue -o $(BIN)/$(TARGET).iso $(ISO) 2>/dev/null || \
	    (echo "grub-mkrescue failed — install xorriso + grub-mkrescue" && false)


# ===============================
#             UEFI RUN
# ===============================

# Copy OVMF files (first run only)
ovmf:
	@if [ ! -f "$(OVMF_CODE_LOCAL)" ]; then \
	    echo "Copying OVMF_CODE..."; \
	    cp $(OVMF_CODE_SYS) $(OVMF_CODE_LOCAL); \
	fi
	@if [ ! -f "$(OVMF_VARS_LOCAL)" ]; then \
	    echo "Copying OVMF_VARS (writable)..."; \
	    cp $(OVMF_VARS_SYS) $(OVMF_VARS_LOCAL); \
	    chmod 666 $(OVMF_VARS_LOCAL); \
	fi

run: iso ovmf
	@echo "Running QEMU EFI..."
	qemu-system-x86_64 \
	    -m 512M \
	    -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE_LOCAL) \
	    -drive if=pflash,format=raw,file=$(OVMF_VARS_LOCAL) \
	    -drive format=raw,file=$(BIN)/$(TARGET).iso \
	    -nographic


# ===============================
#          Clean
# ===============================

clean:
	rm -rf $(BUILD) $(ISO)

distclean: clean
	rm -rf $(BIN)
	rm -f $(OVMF_CODE_LOCAL) $(OVMF_VARS_LOCAL)

.PHONY: all iso run clean distclean ovmf
