CODE_SECTIONS ?= .text

C_SOURCES += $(wildcard $(DIR_SRC)/*.c)
C_OBJECTS := $(patsubst $(DIR_SRC)/%.c, $(DIR_OBJ)/%.c.o, $(C_SOURCES))

AS_SOURCES += $(wildcard $(DIR_SRC)/*.asm)
AS_OBJECTS := $(patsubst $(DIR_SRC)/%.asm, $(DIR_OBJ)/%.asm.o, $(AS_SOURCES))

TESTS_SOURCES += $(wildcard $(DIR_TEST)/*.cpp)
TESTS_EXECUTABLES += $(patsubst $(DIR_TEST)/%.cpp, $(DIR_BIN_TEST)/%, $(TESTS_SOURCES))

DEPENDENCIES += $(patsubst $(DIR_SRC)/%.c, $(DIR_DEP)/%.c.d, $(C_SOURCES))

REFS_LIBS := $(foreach ref, $(PROJECT_REFS), ../$(ref)/$(DIR_BIN)/$(ref).a)
REFS_INCS := $(foreach ref, $(PROJECT_REFS), ../$(ref)/include)

LIBRARIES += $(REFS_LIBS)
INCLUDES += $(REFS_INCS)

INCLUDE_FLAGS += $(patsubst %, -I %, $(INCLUDES))

ifeq ($(TARGET_TYPE), )
ifneq ($(TARGET_NAME), )
TARGET_TYPE := executable
endif
else
ifeq ($(TARGET_NAME), )
$(error [rules.mk] TARGET_NAME is missing.)
endif
endif

ifeq ($(TARGET_TYPE), executable)
ifeq ($(LD_SCRIPT), )
$(error [rules.mk] LD_SCRIPT is missing.)
endif
TARGET := $(DIR_BIN)/$(TARGET_NAME).elf
else ifeq ($(TARGET_TYPE), static-lib)
TARGET := $(DIR_BIN)/$(TARGET_NAME).a
else ifneq ($(TARGET_NAME), )
$(error '$(TARGET_TYPE)': unknown target type.)
endif

PHONY_TARGETS += all build test rebuild mostlyclean clean distclean cleanimpl
.PHONY: $(PHONY_TARGETS) .FORCE
.FORCE:

ifeq ($(TOOLSET), host)
test: $(REFS_LIBS) $(TESTS_EXECUTABLES)
else
test:
	TOOLSET=host make test
endif

rebuild:
	make clean
	make build

mostlyclean: cleanimpl
	-rm -rf $(DIR_OBJ)
	-rm -rf $(DIR_DEP)

clean: mostlyclean
	-rm -rf $(DIR_BIN)

distclean: cleanimpl clean_dirs

ifeq ($(TARGET_TYPE), executable)
$(TARGET): $(LD_SCRIPT) $(C_OBJECTS) $(AS_OBJECTS) $(LIBRARIES) | $(DIRS)
	$(TOOLSET_GCC) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(C_OBJECTS) $(AS_OBJECTS) $(LIBRARIES) \
		-Xlinker -Map=$(DIR_OBJ)/$(TARGET_NAME).map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $(DIR_OBJ)/$(TARGET_NAME).nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ)/$(TARGET_NAME).total.disasm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $(patsubst .%, -j .%, $(CODE_SECTIONS)) -D $@ > $(DIR_OBJ)/$(TARGET_NAME).code.disasm

	$(TOOLSET_NM) -C --numeric-sort $@ \
		| perl -p -e 's/([0-9a-fA-F]*) ([0-9a-fA-F]* .|.) ([^\s]*)(^$$|.*)/\1 \3/g' \
		> $(DIR_OBJ)/$(TARGET_NAME).sym
else ifeq ($(TARGET_TYPE), static-lib)
$(TARGET): $(C_OBJECTS) $(AS_OBJECTS) | $(DIRS)
	$(TOOLSET_AR) rcs $@ $^
endif

$(DIR_OBJ)/%.c.o: $(DIR_SRC)/%.c | $(DIRS)
	$(TOOLSET_GCC) $(CFLAGS) $(INCLUDE_FLAGS) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ)/$*.c.dump

$(DIR_DEP)/%.c.d: $(DIR_SRC)/%.c | $(DIRS)
	$(TOOLSET_GCC) $(CFLAGS) $(INCLUDE_FLAGS) $< -MM -MT $(DIR_OBJ)/$*.c.o \
		| sed 's@\($(DIR_OBJ)/$*.c.o\)[ :]*@\1 $@ : @g' > $@

$(DIR_OBJ)/%.asm.o: $(DIR_SRC)/%.asm | $(DIRS)
	$(TOOLSET_NASM) $(NASMFLAGS) $< -o $@ -l $(DIR_OBJ)/$*.asm.lst
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ)/$*.asm.dump

$(DIR_BIN_TEST)/%: $(TARGET) $(LIBRARIES) .FORCE | $(DIRS)
	g++ $(TEST_CXXFLAGS) -iquote include $(INCLUDE_FLAGS) $(DIR_TEST)/$*.cpp $(LIBRARIES) $(TARGET) $(LIBRARIES) -lgtest -lgtest_main -o $@
	./$@

$(REFS_LIBS): .FORCE
	for dir in $(PROJECT_REFS); do \
		make build -C ../$$dir || exit 1; \
	done

ifeq ($(filter $(subst build, , $(PHONY_TARGETS)), $(MAKECMDGOALS)), )
include $(DEPENDENCIES)
endif
