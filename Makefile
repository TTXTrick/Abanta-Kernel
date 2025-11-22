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

LDFLAGS = -T /usr/lib/elf_x86_64_efi.lds \
          -shared \
          -Bsymbolic \
          -nostdlib \
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
	        -j .reltext -j .reldata -j .relrodata \
	        --target=efi-app-x86_64 \
	        build/$(TARGET).so build/$(TARGET).efi

clean:
	rm -rf build
