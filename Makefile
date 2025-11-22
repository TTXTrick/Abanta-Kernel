# Abanta UEFI Kernel – Single Makefile Build (EDK2 Headers)

CC = gcc
LD = gcc

SRC = src
BUILD = build
BIN = bin

# Path to EDK2 headers inside your repo
EDK2 = edk2

CFLAGS = \
    -I$(EDK2)/MdePkg/Include \
    -I$(EDK2)/MdePkg/Include/X64 \
    -I$(EDK2)/MdePkg/Include/Protocol \
    -I$(EDK2)/MdePkg/Include/IndustryStandard \
    -fshort-wchar -mno-red-zone -fvisibility=hidden \
    -Wall -Wextra -O2 -fPIC

LDFLAGS = \
    -nostdlib \
    -Wl,--subsystem=efi_application \
    -Wl,--entry=efi_main \
    -shared -Bsymbolic

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
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -rf $(BUILD) $(BIN) fat.img

# QEMU UEFI Test
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd

run: all
	rm -f fat.img
	dd if=/dev/zero of=fat.img bs=1M count=64
	mkfs.vfat fat.img
	mmd -i fat.img ::/EFI
	mmd -i fat.img ::/EFI/BOOT
	mcopy -i fat.img $(EFI) ::/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 -m 1024 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS) \
		-hda fat.img
