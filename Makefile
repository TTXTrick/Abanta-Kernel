# Makefile for Abanta UEFI kernel (GNU-EFI)
# Uses system GNU-EFI headers in /usr/include/efi and /usr/include/efi/protocol

GNU_EFI_DIR ?= /usr
BINUTILS_PREFIX ?=

CC = $(BINUTILS_PREFIX)gcc
AR = $(BINUTILS_PREFIX)ar
LD = $(BINUTILS_PREFIX)ld
OBJCOPY = $(BINUTILS_PREFIX)objcopy

SRC = src
BUILD = build
BIN = bin

# Use system gnu-efi headers
CFLAGS = -I$(GNU_EFI_DIR)/include -I$(GNU_EFI_DIR)/include/efi -I$(GNU_EFI_DIR)/include/efi/protocol -fshort-wchar -mno-red-zone -fvisibility=hidden -Wall -Wextra -O2 -fPIC
# linker flags for gnu-efi
LDFLAGS = -nostdlib -znocombreloc -T $(GNU_EFI_DIR)/lib/elf_x86_64_efi.lds -shared -Bsymbolic

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))

EFI = $(BIN)/abanta.efi

.PHONY: all clean run

all: $(EFI)

$(BUILD):
	mkdir -p $(BUILD)

$(BIN):
	mkdir -p $(BIN)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(EFI): $(OBJS) | $(BIN)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) \
	  -L$(GNU_EFI_DIR)/lib -lefi -lgnuefi

clean:
	rm -rf $(BUILD) $(BIN) fat.img

# QEMU/OVMF run (adjust OVMF paths on your distro)
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd

run: all
	# create a small FAT image and put EFI binary at /EFI/BOOT/BOOTX64.EFI
	rm -f fat.img
	dd if=/dev/zero of=fat.img bs=1M count=64
	mkfs.vfat fat.img
	# use mtools to avoid sudo/mount
	mmd -i fat.img ::/EFI
	mmd -i fat.img ::/EFI/BOOT
	mcopy -i fat.img bin/abanta.efi ::/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 -m 1024 \
	  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	  -drive if=pflash,format=raw,file=$(OVMF_VARS) \
	  -hda fat.img
