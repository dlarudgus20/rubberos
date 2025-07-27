TARGET_NAME := kernel
all: build

LD_SCRIPT := src/linker.ld

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

TARGET_BINARY := $(DIR_BIN)/kernel.sys

# rules

build: $(TARGET_BINARY)

$(TARGET_BINARY): $(TARGET_ELF)
	$(TOOLSET_OBJCOPY) -O elf64-x86-64 -j .startup -j .text -j .rodata -j .data -j .bss -j .stack -S $< $@
