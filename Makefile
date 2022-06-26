.PHONY: clean

TARGET=invaders
TEST_TARGET=test

CC=cc
CFLAGS=-std=c17 -Wall -Wextra -pedantic -g -O0 $(shell sdl2-config --cflags)
LN_FLAGS=$(shell sdl2-config --libs)

BUILD_DIR=./build
SRC_DIR=./src

SOURCE = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCE))
TEST_OBJECTS = build/8080.o build/disassembler_8080.o build/test.o

# Gcc/Clang will create these .d files containing dependencies.
DEP = $(OBJECTS:%.o=%.d)

default: $(TARGET)

$(TARGET): $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LN_FLAGS) $^ -o $@

$(TEST_TARGET): $(BUILD_DIR)/$(TEST_TARGET)

$(BUILD_DIR)/$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

-include $(DEP)

# The potential dependency on header files is covered
# by calling `-include $(DEP)`.
# The -MMD flags additionaly creates a .d file with
# the same name as the .o file.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

build/test.o: test/test.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	-rm -rf $(BUILD_DIR)

run: $(TARGET)
	$(BUILD_DIR)/$(TARGET)

run_tests: $(TEST_TARGET)
	$(BUILD_DIR)/$(TEST_TARGET)
