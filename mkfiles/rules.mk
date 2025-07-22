LIBRARIES +=

C_SOURCES += $(wildcard $(DIR_SRC)/*.c)
C_OBJECTS := $(patsubst $(DIR_SRC)/%.c, $(DIR_OBJ)/%.c.o, $(C_SOURCES))

AS_SOURCES += $(wildcard $(DIR_SRC)/*.asm)
AS_OBJECTS := $(patsubst $(DIR_SRC)/%.asm, $(DIR_OBJ)/%.asm.o, $(AS_SOURCES))

DEPENDENCIES += $(patsubst $(DIR_SRC)/%.c, $(DIR_DEP)/%.c.d, $(C_SOURCES))

ifdef TARGET_NAME
TARGET_ELF := $(DIR_BIN)/$(TARGET_NAME).elf
endif

PHONY_TARGETS += all build test rebuild mostlyclean clean distclean cleanimpl
.PHONY: $(PHONY_TARGETS)

test:

rebuild:
	make clean
	make build

mostlyclean:
	-rm -rf $(DIR_OBJ)
	-rm -rf $(DIR_DEP)
	-make cleanimpl

clean: mostlyclean
	-rm -rf $(DIR_BIN)

distclean: clean clean_dirs

ifdef TARGET_NAME
$(TARGET_ELF): $(LD_SCRIPT) $(C_OBJECTS) $(AS_OBJECTS) $(LIBRARIES) | $(DIRS)
	$(TOOLSET_GCC) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(C_OBJECTS) $(AS_OBJECTS) $(LIBRARIES) \
		-Xlinker -Map=$(DIR_OBJ)/$(TARGET_NAME).map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $(DIR_OBJ)/$(TARGET_NAME).nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ)/$(TARGET_NAME).total.disasm
ifdef CODE_SECTIONS
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $(patsubst .%, -j .%, $(CODE_SECTIONS)) -D $@ > $(DIR_OBJ)/$(TARGET_NAME).code.disasm
endif

	$(TOOLSET_NM) -C --numeric-sort $@ \
		| perl -p -e 's/([0-9a-fA-F]*) ([0-9a-fA-F]* .|.) ([^\s]*)(^$$|.*)/\1 \3/g' \
		> $(DIR_OBJ)/$(TARGET_NAME).sym
endif

$(DIR_OBJ)/%.c.o: $(DIR_SRC)/%.c | $(DIRS)
	$(TOOLSET_GCC) $(CFLAGS) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ)/$*.c.dump

$(DIR_DEP)/%.c.d: $(DIR_SRC)/%.c | $(DIRS)
	$(TOOLSET_GCC) $(CFLAGS) $< -MM -MT $(DIR_OBJ)/$*.c.o \
		| sed 's@\($(DIR_OBJ)/$*.c.o\)[ :]*@\1 $@ : @g' > $@

$(DIR_OBJ)/%.asm.o: $(DIR_SRC)/%.asm | $(DIRS)
	$(TOOLSET_NASM) $(NASMFLAGS) $< -o $@ -l $(DIR_OBJ)/$*.asm.lst
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ)/$*.asm.dump

ifeq ($(filter $(subst build, , $(PHONY_TARGETS)), $(MAKECMDGOALS)), )
include $(DEPENDENCIES)
endif
