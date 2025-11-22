TARGET     := abanta.efi
OBJ        := build/abanta.o

EFI_CFLAGS := -I/usr/include/efi -I/usr/include/efi/protocol \
              -fshort-wchar -mno-red-zone -fno-stack-protector \
              -fpic -Wall -Wextra -O2

EFI_LDFLAGS := -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -nostdlib \
               /usr/lib/crt0-efi-x86_64.o -L/usr/lib -lefi -lgnuefi

all: build $(TARGET)

build:
	mkdir -p build

$(OBJ): src/main.c
	gcc $(EFI_CFLAGS) -c src/main.c -o $(OBJ)

$(TARGET): $(OBJ)
	ld $(EFI_LDFLAGS) $(OBJ) -o build/abanta.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* \
	        --target=efi-app-x86_64 \
	        build/abanta.so build/$(TARGET)

# Copy OVMF files into repo (must be writable)
setup:
	cp /usr/share/OVMF/OVMF_CODE_4M.fd OVMF_CODE.fd
	cp /usr/share/OVMF/OVMF_VARS_4M.fd OVMF_VARS.fd
	chmod +w OVMF_VARS.fd
	@echo "Setup complete."

run: all
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=OVMF_CODE.fd \
		-drive if=pflash,format=raw,file=OVMF_VARS.fd \
		-drive format=raw,file=fat:rw:build \
		-m 512M

clean:
	rm -rf build
	rm -f *.fd
