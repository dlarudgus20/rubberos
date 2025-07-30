CODE_SECTIONS ?= .text

C_SOURCES := $(wildcard $(DIR_SRC)/*.c)
C_OBJECTS := $(patsubst $(DIR_SRC)/%.c, $(DIR_OBJ)/%.c.o, $(C_SOURCES))

AS_SOURCES := $(wildcard $(DIR_SRC)/*.asm)
AS_OBJECTS := $(patsubst $(DIR_SRC)/%.asm, $(DIR_OBJ)/%.asm.o, $(AS_SOURCES))

DEPENDENCIES += $(patsubst $(DIR_SRC)/%.c, $(DIR_DEP)/%.c.d, $(C_SOURCES))

ifdef IS_TEST_ON
TEST_SOURCES := $(wildcard $(DIR_TEST)/*.cpp)
TEST_OBJECTS := $(patsubst $(DIR_TEST)/%.cpp, $(DIR_OBJ_TEST)/%.cpp.o, $(TEST_SOURCES))
TEST_DEPS := $(patsubst $(DIR_TEST)/%.cpp, $(DIR_DEP_TEST)/%.cpp.d, $(TEST_SOURCES))
TEST_EXECUTABLE := $(DIR_BIN_TEST)/test
TEST_CXXFLAGS += -iquote include -iquote $(DIR_TEST)
endif

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

ifneq ($(TARGET_TYPE), static-lib)
ifneq ($(TEST_SOURCES), )
$(error [rules.mk] test is available only for static-lib targets.)
endif
endif

PHONY_TARGETS += all build-test test clean-test build rebuild mostlyclean clean distclean cleanimpl
.PHONY: $(PHONY_TARGETS) .FORCE
.FORCE:

ifndef HAS_TEST
build-test:

test:

clean-test:
else
ifdef IS_TEST_ON
build-test: $(TEST_EXECUTABLE)

test: build-test
	./$(TEST_EXECUTABLE)

clean-test: clean
else
build-test:
	TOOLSET=host make build-test

test:
	TOOLSET=host make test

clean-test:
	TOOLSET=host make clean
endif
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

ifdef IS_TEST_ON

$(DIR_OBJ_TEST)/%.cpp.o: $(DIR_TEST)/%.cpp | $(DIRS)
	$(TEST_GXX) $(TEST_CXXFLAGS) $(TEST_INCLUDE_FLAGS) $(INCLUDE_FLAGS) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ_TEST)/$*.cpp.dump

$(DIR_DEP_TEST)/%.cpp.d: $(DIR_TEST)/%.cpp | $(DIRS)
	$(TEST_GXX) $(TEST_CXXFLAGS) $(TEST_INCLUDE_FLAGS) $(INCLUDE_FLAGS) $< -MM -MT $(DIR_OBJ_TEST)/$*.cpp.o \
		| sed 's@\($(DIR_OBJ_TEST)/$*.cpp.o\)[ :]*@\1 $@ : @g' > $@

$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(TARGET) $(LIBRARIES) | $(DIRS)
	$(TEST_GXX) $(TEST_LDFLAGS) -o $@ $(TEST_OBJECTS) $(TARGET) $(LIBRARIES) -lgtest -lgtest_main \
		-Xlinker -Map=$(DIR_OBJ_TEST)/test.map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $(DIR_OBJ_TEST)/test.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(DIR_OBJ_TEST)/test.total.disasm

endif

$(REFS_LIBS): .FORCE
	for dir in $(PROJECT_REFS); do \
		make build -C ../$$dir || exit 1; \
	done

ifeq ($(filter $(subst build, , $(PHONY_TARGETS)), $(MAKECMDGOALS)), )
include $(DEPENDENCIES)
ifdef IS_TEST_ON
include $(TEST_DEPS)
endif
endif
