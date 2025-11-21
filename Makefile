# Makefile for Abanta UEFI kernel (gnu-efi)
# Adjust GNU_EFI and TOOLCHAIN paths if needed.

# Example (Debian/Ubuntu): install packages `gnu-efi` and `efibootmgr`/`ovmf`
# and then run `make`

GNU_EFI_DIR ?= /usr
BINUTILS_PREFIX ?=

CC = $(BINUTILS_PREFIX)gcc
AR = $(BINUTILS_PREFIX)ar
LD = $(BINUTILS_PREFIX)ld
OBJCOPY = $(BINUTILS_PREFIX)objcopy

SRC = src
BUILD = build
BIN = bin

# Add local include directory so src/include/elf.h is found
CFLAGS = -I$(GNU_EFI_DIR)/include -I$(SRC)/include -fshort-wchar -mno-red-zone -fno-exceptions -fno-rtti -fvisibility=hidden -Wall -Wextra -O2 -fPIC
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

# Run in QEMU with OVMF (example - adjust path to OVMF_CODE.fd and OVMF_VARS.fd on your distro)
# On Debian/Ubuntu these files often live in /usr/share/ovmf/
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd

run: all
	# create a small FAT image with the EFI binary in /EFI/BOOT/BOOTX64.EFI
	rm -f fat.img
	fallocate -l 64M fat.img
	# format as FAT (need dosfstools)
	sudo losetup -f --show fat.img >/tmp/loopdevice
	LOOP=$(cat /tmp/loopdevice); sudo mkfs.vfat -n ABANTA $$LOOP
	mkdir -p mnt
	sudo mount $$LOOP mnt
	sudo mkdir -p mnt/EFI/BOOT
	sudo cp $(EFI) mnt/EFI/BOOT/BOOTX64.EFI
	sudo umount $$LOOP
	sudo losetup -d $$LOOP
	qemu-system-x86_64 -enable-kvm -m 1024 \
	  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	  -drive if=pflash,format=raw,file=$(OVMF_VARS) \
	  -hda fat.img
