# Pergyra Programming Language
#
# Copyright (c) 2025 Pergyra Language Project
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the Pergyra Language Project nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# Pergyra Language Compiler Build System
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
ASMFLAGS = -f elf64

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
LEXER_DIR = $(SRC_DIR)/lexer
PARSER_DIR = $(SRC_DIR)/parser
RUNTIME_DIR = $(SRC_DIR)/runtime
ASYNC_DIR = $(RUNTIME_DIR)/async
CODEGEN_DIR = $(SRC_DIR)/codegen
JVM_DIR = $(SRC_DIR)/jvm_bridge

# Source files
LEXER_SOURCES = $(LEXER_DIR)/lexer.c
PARSER_SOURCES = $(PARSER_DIR)/ast.c $(PARSER_DIR)/parser.c $(PARSER_DIR)/parser_async.c
RUNTIME_SOURCES = $(RUNTIME_DIR)/slot_manager.c $(RUNTIME_DIR)/slot_pool.c $(RUNTIME_DIR)/slot_security.c
ASYNC_SOURCES = $(ASYNC_DIR)/fiber.c $(ASYNC_DIR)/scheduler.c $(ASYNC_DIR)/async_scope.c
RUNTIME_ASM_SOURCES = $(RUNTIME_DIR)/slot_asm.s
CODEGEN_SOURCES = $(CODEGEN_DIR)/codegen.c
JVM_SOURCES = $(JVM_DIR)/jni_bridge.c
MAIN_SOURCE = $(SRC_DIR)/main.c
TEST_DATASTRUCTURES_SOURCE = $(SRC_DIR)/test_datastructures.c
TEST_SECURITY_SOURCE = $(SRC_DIR)/test_security.c

# Object files
LEXER_OBJECTS = $(LEXER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PARSER_OBJECTS = $(PARSER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
RUNTIME_OBJECTS = $(RUNTIME_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ASYNC_OBJECTS = $(ASYNC_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
RUNTIME_ASM_OBJECTS = $(RUNTIME_ASM_SOURCES:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
CODEGEN_OBJECTS = $(CODEGEN_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
JVM_OBJECTS = $(JVM_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_DATASTRUCTURES_OBJECT = $(TEST_DATASTRUCTURES_SOURCE:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_SECURITY_OBJECT = $(TEST_SECURITY_SOURCE:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

ALL_OBJECTS = $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(RUNTIME_OBJECTS) $(ASYNC_OBJECTS) \
              $(RUNTIME_ASM_OBJECTS) $(CODEGEN_OBJECTS) $(MAIN_OBJECT)

# Executables
TARGET = $(BIN_DIR)/pergyra
LEXER_TEST = $(BIN_DIR)/lexer_test
DATASTRUCTURES_TEST = $(BIN_DIR)/test_datastructures
SECURITY_TEST = $(BIN_DIR)/test_security

# Default target
all: $(TARGET) $(LEXER_TEST) $(PARSER_TEST) $(DATASTRUCTURES_TEST) $(SECURITY_TEST)

# Main executable build
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lssl -lcrypto

# Lexer test build
$(LEXER_TEST): $(LEXER_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Parser test build
$(PARSER_TEST): $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(TEST_PARSER_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Data structures test build
$(DATASTRUCTURES_TEST): $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) $(TEST_DATASTRUCTURES_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# Security test build
$(SECURITY_TEST): $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) $(TEST_SECURITY_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lssl -lcrypto

# C source compilation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/lexer $(BUILD_DIR)/parser \
                   $(BUILD_DIR)/runtime $(BUILD_DIR)/codegen $(BUILD_DIR)/jvm_bridge
	$(CC) $(CFLAGS) -c -o $@ $<

# Assembly source compilation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)/runtime
	nasm $(ASMFLAGS) -o $@ $<

# Directory creation
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/lexer: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lexer

$(BUILD_DIR)/parser: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/parser

$(BUILD_DIR)/runtime: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/runtime

$(BUILD_DIR)/codegen: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/codegen

$(BUILD_DIR)/jvm_bridge: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/jvm_bridge

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Test execution
test: $(LEXER_TEST)
	@echo "=== Running Pergyra Lexer Test ==="
	./$(LEXER_TEST)

# Parser test execution
test-parser: $(PARSER_TEST)
	@echo "=== Running Pergyra Parser Test ==="
	./$(PARSER_TEST)

# Security test execution
test-security: $(SECURITY_TEST)
	@echo "=== Running Pergyra Security Test Suite ==="
	./$(SECURITY_TEST)

# All tests
test-all: test test-parser test-security
	@echo "=== All Pergyra Tests Completed ==="

# Clean targets
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

clean-objects:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*/*.o

# Build variants
debug: CFLAGS += -DDEBUG -g3 -O0
debug: $(TARGET)

release: CFLAGS += -DNDEBUG -O3 -flto
release: $(TARGET)

# Static analysis
analyze:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)

# Dependency generation
depend:
	makedepend -Y -p$(BUILD_DIR)/ $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c 2>/dev/null

# Installation
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Documentation generation
docs:
	doxygen docs/Doxyfile

# Format code (requires clang-format)
format:
	find $(SRC_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

# Individual component builds
lexer: $(LEXER_OBJECTS)
	@echo "Lexer component built successfully"

parser: $(PARSER_OBJECTS)
	@echo "Parser component built successfully"

runtime: $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS)
	@echo "Runtime component built successfully"

codegen: $(CODEGEN_OBJECTS)
	@echo "Codegen component built successfully"

jvm: $(JVM_OBJECTS)
	@echo "JVM bridge component built successfully"

# Benchmark target
benchmark: release
	@echo "Running performance benchmarks..."
	./$(TARGET) --benchmark

# Memory check (requires valgrind)
memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(LEXER_TEST)

# Coverage analysis (requires gcov)
coverage: CFLAGS += --coverage
coverage: $(TARGET)
	./$(LEXER_TEST)
	gcov $(SRC_DIR)/*.c

.PHONY: all test clean clean-objects debug release analyze depend install \
        docs format lexer parser runtime codegen jvm benchmark memcheck coverage