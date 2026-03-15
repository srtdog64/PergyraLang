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

CC      = gcc
OPENMP_FLAGS = -fopenmp
TMPDIR ?= /tmp
export TMPDIR
CFLAGS  = -Wall -Wextra -std=c11 -O2 -g $(OPENMP_FLAGS) -I$(SRC_DIR)
ASMFLAGS = -f elf64
NASM    := $(shell command -v nasm 2>/dev/null)

# -----------------------------------------------------------------
# LLVM backend (optional)
#   make LLVM_ENABLED=1          — build with LLVM native backend
#   make                         — build without LLVM (C transpiler only)
# -----------------------------------------------------------------
LLVM_DIR     = third_party
LLVM_INSTALL = C:/Program Files/LLVM

ifdef LLVM_ENABLED
  CFLAGS  += -DPGY_LLVM_ENABLED -I$(LLVM_DIR)
  LDFLAGS_LLVM = -L"$(LLVM_INSTALL)/lib" -lLLVM-C
endif

# -----------------------------------------------------------------
# Directories
# -----------------------------------------------------------------
SRC_DIR      = src
BUILD_DIR    = build
BIN_DIR      = bin
LEXER_DIR    = $(SRC_DIR)/lexer
PARSER_DIR   = $(SRC_DIR)/parser
RUNTIME_DIR  = $(SRC_DIR)/runtime
ASYNC_DIR    = $(RUNTIME_DIR)/async
SEMANTIC_DIR = $(SRC_DIR)/semantic
CODEGEN_DIR  = $(SRC_DIR)/codegen
COMPILER_DIR = $(SRC_DIR)/compiler

# -----------------------------------------------------------------
# Source groups
# -----------------------------------------------------------------
LEXER_SOURCES    = $(LEXER_DIR)/lexer.c
PARSER_SOURCES   = $(PARSER_DIR)/ast.c \
                   $(PARSER_DIR)/parser.c \
                   $(PARSER_DIR)/parser_async.c
RUNTIME_SOURCES  = $(RUNTIME_DIR)/slot_manager.c \
                   $(RUNTIME_DIR)/slot_pool.c \
                   $(RUNTIME_DIR)/slot_security.c
RUNTIME_ASM_SOURCES = $(RUNTIME_DIR)/slot_asm.s
SEMANTIC_SOURCES = $(SEMANTIC_DIR)/type_system.c \
                   $(SEMANTIC_DIR)/symbol_table.c \
                   $(SEMANTIC_DIR)/type_checker.c \
                   $(SEMANTIC_DIR)/slot_analyzer.c \
                   $(SEMANTIC_DIR)/semantic.c
CODEGEN_SOURCES  = $(CODEGEN_DIR)/transpiler.c
COMPILER_SOURCES = $(COMPILER_DIR)/compiler.c \
                   $(COMPILER_DIR)/hir.c

# LLVM backend sources (only compiled when LLVM_ENABLED=1)
ifdef LLVM_ENABLED
  LLVM_BACKEND_SOURCES = $(CODEGEN_DIR)/llvm_backend.c
  RUNTIME_LIB_SOURCES  = $(RUNTIME_DIR)/pgy_runtime_lib.c
  LLVM_BACKEND_OBJECTS = $(LLVM_BACKEND_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
  RUNTIME_LIB_OBJECTS  = $(RUNTIME_LIB_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
else
  LLVM_BACKEND_OBJECTS =
  RUNTIME_LIB_OBJECTS  =
endif

# Test & driver sources
MAIN_SOURCE             = $(SRC_DIR)/main.c
PARSER_TEST_SOURCE      = $(SRC_DIR)/test_parser.c
TEST_DATASTRUCTURES_SRC = $(SRC_DIR)/test_datastructures.c
TEST_SECURITY_SRC       = $(SRC_DIR)/test_security.c
TEST_SEMANTIC_SRC       = $(SRC_DIR)/test_semantic.c
TEST_TRANSPILE_SRC      = $(SRC_DIR)/test_transpile.c
TEST_MEMORY_SRC         = $(SRC_DIR)/test_memory_layout.c
TEST_CONCURRENCY_SRC    = $(SRC_DIR)/test_concurrency.c
TEST_HIR_SRC            = $(SRC_DIR)/test_hir.c
DRIVER_SRC              = $(SRC_DIR)/pgy_driver.c
LSP_SRC                 = $(SRC_DIR)/lsp/pgy_lsp.c

# -----------------------------------------------------------------
# Object files
# -----------------------------------------------------------------
LEXER_OBJECTS    = $(LEXER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PARSER_OBJECTS   = $(PARSER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
RUNTIME_OBJECTS  = $(RUNTIME_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ifeq ($(NASM),)
RUNTIME_ASM_OBJECTS =
else
RUNTIME_ASM_OBJECTS = $(RUNTIME_ASM_SOURCES:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
endif
SEMANTIC_OBJECTS = $(SEMANTIC_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CODEGEN_OBJECTS  = $(CODEGEN_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
COMPILER_OBJECTS = $(COMPILER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

MAIN_OBJECT            = $(BUILD_DIR)/main.o
PARSER_TEST_OBJECT     = $(BUILD_DIR)/test_parser.o
TEST_DATASTRUCTURES_OBJ = $(BUILD_DIR)/test_datastructures.o
TEST_SECURITY_OBJ      = $(BUILD_DIR)/test_security.o
TEST_SEMANTIC_OBJ      = $(BUILD_DIR)/test_semantic.o
TEST_TRANSPILE_OBJ     = $(BUILD_DIR)/test_transpile.o
TEST_MEMORY_OBJ        = $(BUILD_DIR)/test_memory_layout.o
TEST_CONCURRENCY_OBJ   = $(BUILD_DIR)/test_concurrency.o
TEST_HIR_OBJ           = $(BUILD_DIR)/test_hir.o
DRIVER_OBJ             = $(BUILD_DIR)/pgy_driver.o
LSP_OBJ                = $(BUILD_DIR)/lsp/pgy_lsp.o

# Common frontend objects used by many targets
FRONTEND_OBJECTS = $(LEXER_OBJECTS) $(PARSER_OBJECTS) \
                   $(SEMANTIC_OBJECTS) $(CODEGEN_OBJECTS) $(COMPILER_OBJECTS) \
                   $(LLVM_BACKEND_OBJECTS) $(RUNTIME_LIB_OBJECTS)

# -----------------------------------------------------------------
# Executables
# -----------------------------------------------------------------
LEXER_TEST          = $(BIN_DIR)/lexer_test
PARSER_TEST         = $(BIN_DIR)/test_parser
DATASTRUCTURES_TEST = $(BIN_DIR)/test_datastructures
SECURITY_TEST       = $(BIN_DIR)/test_security
SEMANTIC_TEST       = $(BIN_DIR)/test_semantic
TRANSPILE_TEST      = $(BIN_DIR)/test_transpile
MEMORY_TEST         = $(BIN_DIR)/test_memory_layout
CONCURRENCY_TEST    = $(BIN_DIR)/test_concurrency
HIR_TEST            = $(BIN_DIR)/test_hir
PGY                 = $(BIN_DIR)/pgy
PGY_LSP             = $(BIN_DIR)/pgy-lsp

# -----------------------------------------------------------------
# Default target — build the driver and all tests
# -----------------------------------------------------------------
all: $(PGY) $(PGY_LSP) $(LEXER_TEST) $(PARSER_TEST) $(SEMANTIC_TEST) $(TRANSPILE_TEST) $(MEMORY_TEST) $(CONCURRENCY_TEST) $(HIR_TEST)

pgy: $(PGY)

# -----------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------

# pgy compiler driver
$(PGY): $(FRONTEND_OBJECTS) $(DRIVER_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) -lpthread

# Lexer smoke-test (original main.c)
$(LEXER_TEST): $(LEXER_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Parser test
$(PARSER_TEST): $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(PARSER_TEST_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Data structures test
$(DATASTRUCTURES_TEST): $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
                         $(TEST_DATASTRUCTURES_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# Security test
$(SECURITY_TEST): $(RUNTIME_OBJECTS) $(RUNTIME_ASM_OBJECTS) \
                   $(TEST_SECURITY_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lssl -lcrypto

# Semantic analyzer test
$(SEMANTIC_TEST): $(FRONTEND_OBJECTS) $(TEST_SEMANTIC_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) -lpthread

# C backend test
$(TRANSPILE_TEST): $(FRONTEND_OBJECTS) $(TEST_TRANSPILE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_LLVM) -lpthread

# Memory layout test (runtime-only, no frontend)
$(MEMORY_TEST): $(TEST_MEMORY_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Concurrency runtime test
$(CONCURRENCY_TEST): $(TEST_CONCURRENCY_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# HIR lowering test
$(HIR_TEST): $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(BUILD_DIR)/compiler/hir.o $(TEST_HIR_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# LSP server
$(PGY_LSP): $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(SEMANTIC_OBJECTS) $(LSP_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# -----------------------------------------------------------------
# Compilation rules
# -----------------------------------------------------------------

# C sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Assembly sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(NASM) $(ASMFLAGS) -o $@ $<

# -----------------------------------------------------------------
# Directory creation
# -----------------------------------------------------------------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) \
		$(BUILD_DIR)/lexer \
		$(BUILD_DIR)/parser \
		$(BUILD_DIR)/semantic \
		$(BUILD_DIR)/codegen \
		$(BUILD_DIR)/compiler \
		$(BUILD_DIR)/runtime \
		$(BUILD_DIR)/runtime/async \
		$(BUILD_DIR)/lsp

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# -----------------------------------------------------------------
# Test execution targets
# -----------------------------------------------------------------
test: $(LEXER_TEST)
	@echo "=== Lexer Test ==="
	./$(LEXER_TEST)

test-parser: $(PARSER_TEST)
	@echo "=== Parser Test ==="
	./$(PARSER_TEST)

test-security: $(SECURITY_TEST)
	@echo "=== Security Test ==="
	./$(SECURITY_TEST)

test-semantic: $(SEMANTIC_TEST)
	@echo "=== Semantic Analyzer Test ==="
	./$(SEMANTIC_TEST)

test-transpile: $(TRANSPILE_TEST)
	@echo "=== C Backend Test ==="
	./$(TRANSPILE_TEST)

test-memory: $(MEMORY_TEST)
	@echo "=== Memory Layout Test ==="
	./$(MEMORY_TEST)

test-concurrency: $(CONCURRENCY_TEST)
	@echo "=== Concurrency Test ==="
	./$(CONCURRENCY_TEST)

test-hir: $(HIR_TEST)
	@echo "=== HIR Test ==="
	./$(HIR_TEST)

test-all: test test-parser test-semantic test-transpile test-memory test-concurrency test-hir
	@echo "=== All Frontend Tests Completed ==="

# -----------------------------------------------------------------
# pgy driver convenience targets
# -----------------------------------------------------------------
example-hello: $(PGY)
	./$(PGY) examples/hello.pgy --run -v

example-slots: $(PGY)
	./$(PGY) examples/slots.pgy --run -v

# Emit C only
emit-c-%: $(PGY)
	./$(PGY) examples/$*.pgy --emit-c -v

# -----------------------------------------------------------------
# Maintenance
# -----------------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

clean-objects:
	find $(BUILD_DIR) -name "*.o" -delete 2>/dev/null || true

debug: CFLAGS += -DDEBUG -g3 -O0
debug: $(PGY)

release: CFLAGS += -DNDEBUG -O3 -flto
release: $(PGY)

analyze:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)

format:
	find $(SRC_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TRANSPILE_TEST)

lsp: $(PGY_LSP)

.PHONY: all clean clean-objects debug release analyze format memcheck \
        test test-parser test-security test-semantic test-transpile test-memory test-concurrency test-hir test-all \
        example-hello example-slots lsp
