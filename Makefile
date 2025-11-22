# Abanta UEFI Kernel — GNU-EFI build

ARCH = x86_64
GNU_EFI = /usr/lib/$(ARCH)-linux-gnu/gnu-efi

CC = gcc
LD = ld
OBJCOPY = objcopy

SRC = src
BUILD = build
BIN = bin

CFLAGS = \
	-I/usr/include/efi \
	-I/usr/include/efi/protocol \
	-fshort-wchar \
	-mno-red-zone \
	-fno-stack-protector \
	-fpic \
	-fno-pic \
	-Wall -Wextra -O2

LDFLAGS = \
	-nostdlib \
	-T $(GNU_EFI)/elf_$(ARCH)_efi.lds \
	-shared -Bsymbolic \
	-L$(GNU_EFI) \
	-lefi -lgnuefi

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))

EFI = $(BIN)/BOOTX64.EFI

all: $(EFI)

$(BUILD):
	mkdir -p $(BUILD)

$(BIN):
	mkdir -p $(BIN)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(EFI): $(OBJS) | $(BIN)
	$(LD) $(LDFLAGS) -o $(BIN)/abanta.so $(OBJS)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic \
	-j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* \
	--target=efi-app-$(ARCH) $(BIN)/abanta.so $(EFI)

clean:
	rm -rf $(BUILD) $(BIN)
