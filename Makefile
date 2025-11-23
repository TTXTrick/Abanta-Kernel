TARGET = kernel.elf
ISO    = bin/abanta.iso

CC     = gcc
LD     = ld
NASM   = nasm

CFLAGS = -m64 -ffreestanding -fno-pie -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -I.
LDFLAGS = -nostdlib -T linker.ld

SRC_DIR = src
BUILD_DIR = build

C_SOURCES = $(SRC_DIR)/kernel.c
ASM_SOURCES = $(SRC_DIR)/boot.S

OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o

# -----------------------------
# Build rules
# -----------------------------

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: $(SRC_DIR)/boot.S | $(BUILD_DIR)
	$(NASM) -f elf64 $(SRC_DIR)/boot.S -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/kernel.o: $(SRC_DIR)/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/kernel.c -o $(BUILD_DIR)/kernel.o

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $(BUILD_DIR)/$(TARGET) $(OBJS)
	cp $(BUILD_DIR)/$(TARGET) .

# -----------------------------
# ISO creation
# -----------------------------

iso: all
	rm -rf iso
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/kernel.elf
	cp grub.cfg iso/boot/grub/grub.cfg

	mkdir -p bin
	grub-mkrescue -o $(ISO) iso

# -----------------------------
# Run under BIOS QEMU
# -----------------------------

run: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 512M

clean:
	rm -rf $(BUILD_DIR) kernel.elf iso bin

.PHONY: all clean iso run
