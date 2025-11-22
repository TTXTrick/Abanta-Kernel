# ===============================
# Abanta UEFI Kernel (GNU-EFI)
# Fully working Makefile for Debian 13
# ===============================

TARGET = abanta
SRC = src/main.c

EFIINC = /usr/include/efi
EFILIB = /usr/lib

# Compiler flags for GNU-EFI
CFLAGS = -I$(EFIINC) \
         -I$(EFIINC)/protocol \
         -fshort-wchar \
         -mno-red-zone \
         -fno-stack-protector \
         -fpic \
         -Wall -Wextra -O2

# Linker flags for GNU-EFI
LDFLAGS = -T $(EFILIB)/elf_x86_64_efi.lds \
          -shared \
          -Bsymbolic \
          -nostdlib \
          $(EFILIB)/crt0-efi-x86_64.o \
          -L$(EFILIB) \
          -lefi -lgnuefi

# Output directories
OBJ = build/$(TARGET).o
SO = build/$(TARGET).so
EFI = build/$(TARGET).efi

all: $(EFI)

build:
	mkdir -p build

$(OBJ): $(SRC) | build
	gcc $(CFLAGS) -c $(SRC) -o $(OBJ)

$(SO): $(OBJ)
	ld $(LDFLAGS) $(OBJ) -o $(SO)

$(EFI): $(SO)
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* \
	        --target=efi-app-x86_64 \
	        $(SO) $(EFI)

clean:
	rm -rf build

# ===============================
# QEMU RUN (Debian 13 OVMF 4M files)
# ===============================

run: all
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
		-drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS_4M.fd \
		-drive format=raw,file=fat:rw:build \
		-m 512M
