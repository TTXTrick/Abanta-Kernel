TARGET = abanta
SRC = src/main.c

EFIINC = /usr/include/efi
EFILIB = /usr/lib

CFLAGS = -I$(EFIINC) \
         -I$(EFIINC)/protocol \
         -fshort-wchar \
         -mno-red-zone \
         -fno-stack-protector \
         -fpic \
         -Wall -Wextra -O2

LDFLAGS = -T $(EFILIB)/elf_x86_64_efi.lds \
          -shared \
          -Bsymbolic \
          -nostdlib \
          $(EFILIB)/crt0-efi-x86_64.o \
          -L$(EFILIB) \
          -lefi -lgnuefi

all: build/$(TARGET).efi

build/$(TARGET).o: $(SRC)
	mkdir -p build
	gcc $(CFLAGS) -c $(SRC) -o build/$(TARGET).o

build/$(TARGET).so: build/$(TARGET).o
	ld $(LDFLAGS) build/$(TARGET).o -o build/$(TARGET).so

build/$(TARGET).efi: build/$(TARGET).so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* \
	        --target=efi-app-x86_64 \
	        build/$(TARGET).so build/$(TARGET).efi

clean:
	rm -rf build

#
# Run the EFI binary in QEMU’s UEFI firmware
#
run: all
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.fd \
		-drive if=pflash,format=raw,file=/usr/share/edk2/x64/OVMF_VARS.fd \
		-drive format=raw,file=fat:rw:build \
		-m 512M
