CONFIG ?= debug

ifeq ($(CONFIG), debug)
else ifeq ($(CONFIG), release)
else
$(error [conf.mk] '$(CONFIG)': unknown configuration.)
endif

ifeq ($(TOOLSET), )

TOOLSET_PREFIX			?= x86_64-elf
TOOLSET_GCC				?= $(TOOLSET_PREFIX)-gcc
TOOLSET_AR				?= $(TOOLSET_PREFIX)-gcc-ar
TOOLSET_NASM			?= nasm
TOOLSET_OBJCOPY			?= $(TOOLSET_PREFIX)-objcopy
TOOLSET_OBJDUMP			?= $(TOOLSET_PREFIX)-objdump
TOOLSET_NM				?= $(TOOLSET_PREFIX)-gcc-nm
TOOLSET_GDB				?= $(TOOLSET_PREFIX)-gdb

TOOLSET_QEMU			?= qemu-system-x86_64
TOOLSET_BOCHS			?= bochs
TOOLSET_GRUB_MKRESCUE	?= grub-mkrescue

CFLAGS += -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-std=c17 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=unused-function
NASMFLAGS += -f elf64
LDFLAGS += -ffreestanding -nostdlib -Xlinker --gc-sections

else ifeq ($(TOOLSET), host)

TOOLSET_GCC				?= gcc
TOOLSET_AR				?= gcc-ar
TOOLSET_NASM			?= nasm
TOOLSET_OBJCOPY			?= objcopy
TOOLSET_OBJDUMP			?= objdump
TOOLSET_NM				?= gcc-nm
TOOLSET_GDB				?= gdb

CFLAGS += -std=c17 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=unused-function
NASMFLAGS +=
LDFLAGS +=

TEST_GXX				?= g++

TEST_CXXFLAGS += -std=c++17 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=unused-function
TEST_LDFLAGS +=

ifeq ($(CONFIG), debug)
TEST_CXXFLAGS += -DDEBUG -ggdb
else ifeq ($(CONFIG), release)
TEST_CXXFLAGS += -DNDEBUG -O3 -ggdb
endif

else
$(error [conf.mk] '$(TOOLSET)': unknown toolset.)
endif

ifeq ($(CONFIG), debug)
CFLAGS += -DDEBUG -ggdb
else ifeq ($(CONFIG), release)
CFLAGS += -DNDEBUG -O3 -flto -ggdb
endif

CFLAGS += -masm=intel -iquote include
NASMFLAGS +=
LDFLAGS +=
OBJDUMP_FLAGS += -M intel -C
NM_FLAGS += -C --line-numbers --print-size --print-armap --numeric-sort

ifeq ($(TOOLSET), )
DIR_INFIX := $(CONFIG)
else
DIR_INFIX := $(TOOLSET)/$(CONFIG)
endif

DIR_SRC := src
DIR_TEST := tests

ifneq ($(and $(filter $(TOOLSET), host), $(wildcard $(DIR_TEST)/*)), )
IS_TEST := 1
endif
	
DIR_BIN := bin/$(DIR_INFIX)
DIR_OBJ := obj/$(DIR_INFIX)
DIR_DEP := dep/$(DIR_INFIX)
ifdef IS_TEST
DIR_BIN_TEST := $(DIR_BIN)/tests
DIR_OBJ_TEST := $(DIR_OBJ)/tests
DIR_DEP_TEST := $(DIR_DEP)/tests
DIRS := $(DIR_BIN_TEST) $(DIR_OBJ_TEST) $(DIR_DEP_TEST)
else
DIRS := $(DIR_BIN) $(DIR_OBJ) $(DIR_DEP)
endif

PHONY_TARGETS += clean_dirs

$(DIRS):
ifdef IS_TEST
	mkdir -p $(DIR_BIN_TEST)
	mkdir -p $(DIR_OBJ_TEST)
	mkdir -p $(DIR_DEP_TEST)
else
	mkdir -p $(DIR_BIN)
	mkdir -p $(DIR_OBJ)
	mkdir -p $(DIR_DEP)
endif

clean_dirs:
	-rm -rf bin
	-rm -rf obj
	-rm -rf dep
