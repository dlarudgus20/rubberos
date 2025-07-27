TARGET_NAME := kernel
TARGET_TYPE := executable
PROJECT_REFS := libbuddy libslab

all: build

LD_SCRIPT := src/linker.ld

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

TARGET_STRIPPED := $(DIR_BIN)/kernel.sys

# rules

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -O elf64-x86-64 -j .startup -j .text -j .rodata -j .data -j .bss -j .stack -S $< $@
