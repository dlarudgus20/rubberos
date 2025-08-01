TARGET_NAME := kernel
TARGET_TYPE := executable
PROJECT_REFS := libfreec libcoll libbuddy libslab

all: build

ARCH := x86_64
SUBDIRS := boot drivers

SRC_ARCH := arch/$(ARCH)
SRC_SUBDIRS := $(SRC_ARCH) $(foreach dir, $(SUBDIRS), $(SRC_ARCH)/$(dir)) $(SUBDIRS)

LD_SCRIPT := src/$(SRC_ARCH)/linker.ld

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

TARGET_STRIPPED := $(DIR_BIN)/kernel.sys

# rules

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -O elf64-x86-64 -j .startup -j .text -j .rodata -j .data -j .bss -j .stack -S $< $@
