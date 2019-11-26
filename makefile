BUILD = build
INCLUDE = include
SRC = src

CC=gcc
SHARED_FLAGS = -fno-builtin  -nostdinc -nostdlib -ffreestanding -g -Wall -Wextra \
            -O3 -I$(INCLUDE) -MMD -mno-red-zone -mcmodel=kernel -fno-pie -fno-permissive -fno-exceptions -fno-rtti
CFLAGS = $(SHARED_FLAGS)
ASFLAGS = $(SHARED_FLAGS) -Wa,--divide

_OBJS = boot.o kernel.o lib.o gdt.o ports.o interrupts.o interruptstubs.o keyboard.o

OBJS = $(patsubst %, $(BUILD)/%, $(_OBJS))

$(BUILD)/%.o: $(SRC)/%.S
	bash -c "$(CC) $(ASFLAGS) -c -o $@ $<"

$(BUILD)/%.o: $(SRC)/%.cpp
	bash -c "$(CC) $(CFLAGS) -c -o $@ $<"

kernel.bin: $(OBJS)
	bash -c "cd build && $(CC) -z max-page-size=0x1000 $(CFLAGS) -no-pie -Wl,--build-id=none -T ../$(SRC)/linker.ld -o ../$@ $(_OBJS)"

kernel.iso: kernel.bin
	mkdir -p iso/boot/grub
	cp $< iso/boot/
	echo set timeout=0 							>> iso/boot/grub/grub.cfg
	echo set default=0 							>> iso/boot/grub/grub.cfg
	echo 										>> iso/boot/grub/grub.cfg
	echo 'menuentry "SickOS" {' 				>> iso/boot/grub/grub.cfg
	echo "    multiboot /boot/$<" 				>> iso/boot/grub/grub.cfg
	echo "    boot" 							>> iso/boot/grub/grub.cfg
	echo } 										>> iso/boot/grub/grub.cfg
	bash -c "grub-mkrescue --output=$@ iso"
	rm -rf iso

.PHONY: all
all: kernel.iso ;

.PHONY: run
run: clean kernel.iso
	qemu-system-x86_64 -cdrom $(word 2, $^) -serial stdio -m 1024M

.PHONY: clean
clean: 
	rm -rf build
	rm -f kernel.iso
	rm -f kernel.bin
	mkdir build