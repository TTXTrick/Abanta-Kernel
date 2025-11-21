# Makefile for Abanta x86_64
AS=nasm
CC=gcc
LD=ld
OBJCOPY=objcopy

CFLAGS = -m64 -nostdlib -fno-builtin -fno-stack-protector -fno-pic -ffreestanding -O2 -Wall
LDFLAGS = -m elf_x86_64 -T link.ld

SRC = src
BUILD = build
BIN = bin

SOURCES_C = $(wildcard $(SRC)/*.c)
OBJS_C = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SOURCES_C))
OBJS_ASM = $(BUILD)/boot.o

.PHONY: all clean iso run

all: $(BIN)/abanta.elf $(BIN)/abanta.bin

$(BUILD):
	mkdir -p $(BUILD)

$(BIN):
	mkdir -p $(BIN)

# assemble NASM 32-bit stub (object format elf32)
$(BUILD)/boot.o: $(SRC)/boot.S | $(BUILD)
	nasm -f elf32 -o $@ $<

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC) -c $< -o $@

$(BIN)/abanta.elf: $(OBJS_ASM) $(OBJS_C) | $(BIN)
	$(LD) $(LDFLAGS) -o $@ $(OBJS_ASM) $(OBJS_C)

$(BIN)/abanta.bin: $(BIN)/abanta.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD) $(BIN) isodir abanta.iso

iso: all
	mkdir -p isodir/boot/grub
	cp $(BIN)/abanta.bin isodir/boot/abanta.bin
	printf 'set timeout=0\nset default=0\nmenuentry "Abanta x86_64" {\n  multiboot /boot/abanta.bin\n  boot\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o abanta.iso isodir

run: iso
	qemu-system-x86_64 -cdrom abanta.iso -m 512M -no-reboot -no-shutdown
