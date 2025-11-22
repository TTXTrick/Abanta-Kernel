# Makefile for Abanta "kernel-like" UEFI app (gnu-efi)
# Adjust OVMF paths and GNU_EFI paths if needed.

GNU_EFI_DIR ?= /usr
BINUTILS_PREFIX ?=

CC = $(BINUTILS_PREFIX)gcc
LD = $(BINUTILS_PREFIX)ld
OBJCOPY = $(BINUTILS_PREFIX)objcopy

SRC = src
BUILD = build
BIN = build        # we put fat dir directly under build for easier run target

CFLAGS = -I$(GNU_EFI_DIR)/include -I$(GNU_EFI_DIR)/include/efi -I$(GNU_EFI_DIR)/include/efi/protocol \
         -fshort-wchar -mno-red-zone -fno-stack-protector -fpic -Wall -Wextra -O2

LDFLAGS = -nostdlib -znocombreloc -T $(GNU_EFI_DIR)/lib/elf_x86_64_efi.lds -shared -Bsymbolic

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))

EFI_SO = $(BUILD)/abanta.so
EFI = $(BUILD)/abanta.efi

.PHONY: all clean run image

all: $(EFI)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(EFI_SO): $(OBJS) | $(BUILD)
	$(LD) $(LDFLAGS) /usr/lib/crt0-efi-x86_64.o -L$(GNU_EFI_DIR)/lib -lefi -lgnuefi $(OBJS) -o $@

# keep .so target for objcopy step
$(EFI): $(EFI_SO)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic \
	  -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* \
	  --target=efi-app-x86_64 \
	  $(EFI_SO) $(EFI)

clean:
	rm -rf $(BUILD)

# Run in QEMU with OVMF (edit these to the OVMF files on your system)
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd

# compact run target: create a small FAT image inside build/ and run QEMU
run: all image
	qemu-system-x86_64 -enable-kvm -m 1024 \
	  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	  -drive if=pflash,format=raw,file=$(OVMF_VARS) \
	  -drive format=raw,file=fat:rw:$(BUILD) \
	  -net none

# Build a tiny FAT-formatted folder usable by qemu's fat driver (no sudo required)
# Copies EFI binary to /EFI/BOOT/BOOTX64.EFI inside $(BUILD)
image: $(EFI)
	rm -rf $(BUILD)/EFI
	mkdir -p $(BUILD)/EFI/BOOT
	cp $(EFI) $(BUILD)/EFI/BOOT/BOOTX64.EFI
