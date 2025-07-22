all: build

include mkfiles/conf.mk

KERNEL_ELF := kernel/$(DIR_BIN)/kernel.elf
KERNEL_BINARY := kernel/$(DIR_BIN)/kernel.sys

GRUB_CFG := grub/boot/grub/grub.cfg
TARGET_IMAGE := $(DIR_BIN)/boot.iso

QEMU_DRIVES := -cdrom "$(TARGET_IMAGE)"
QEMU_FLAGS := -L . -m 64 $(QEMU_DRIVES) -rtc base=localtime -M pc -serial stdio
BOCHSRC := bochsrc.bxrc

SUBDIRS := kernel

.PHONY: all build re rebuild run rerun dbg debug gdb bochs test mostlyclean clean distclean

build:
	for dir in $(SUBDIRS); do \
		make build -C $$dir || exit 1; \
	done
	make $(TARGET_IMAGE)

re: rebuild
rebuild: clean build

run: build
	$(TOOLSET_QEMU) $(QEMU_FLAGS)

rerun: clean run

dbg: debug
debug: build
	$(TOOLSET_QEMU) $(QEMU_FLAGS) -S -gdb tcp:127.0.0.1:1234 \
		-fw_cfg name=opt/org.starrios.debug,string=1

gdb:
	$(TOOLSET_GDB) $(KERNEL_ELF) "-ex=target remote :1234"

bochs: build
	CONFIG=$(CONFIG) $(TOOLSET_BOCHS) -qf $(BOCHSRC)

test:
	for dir in $(SUBDIRS); do \
		make test -C $$dir || exit 1; \
	done

mostlyclean:
	for dir in $(SUBDIRS); do \
		make mostlyclean -C $$dir || exit 1; \
	done

clean:
	-rm -rf $(DIR_BIN)/*
	for dir in $(SUBDIRS); do \
		make clean -C $$dir || exit 1; \
	done

distclean: clean_dirs
	for dir in $(SUBDIRS); do \
		make distclean -C $$dir || exit 1; \
	done

$(TARGET_IMAGE): $(KERNEL_BINARY) $(GRUB_CFG)
	mkdir -p $(DIR_BIN)
	mkdir -p $(DIR_OBJ)
	cp -r grub/ $(DIR_OBJ)
	cp $(KERNEL_BINARY) $(DIR_OBJ)/grub/boot/
	$(TOOLSET_GRUB_MKRESCUE) -o $(TARGET_IMAGE) $(DIR_OBJ)/grub
