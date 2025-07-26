# Pergyra 언어 컴파일러 빌드 시스템
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
ASMFLAGS = -f elf64

# 디렉토리
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
LEXER_DIR = $(SRC_DIR)/lexer
PARSER_DIR = $(SRC_DIR)/parser
RUNTIME_DIR = $(SRC_DIR)/runtime
CODEGEN_DIR = $(SRC_DIR)/codegen
JVM_DIR = $(SRC_DIR)/jvm_bridge

# 소스 파일
LEXER_SOURCES = $(LEXER_DIR)/lexer.c
PARSER_SOURCES = $(PARSER_DIR)/ast.c $(PARSER_DIR)/parser.c
RUNTIME_SOURCES = $(RUNTIME_DIR)/slot_manager.c $(RUNTIME_DIR)/memory_pool.c
CODEGEN_SOURCES = $(CODEGEN_DIR)/codegen.c
JVM_SOURCES = $(JVM_DIR)/jni_bridge.c
MAIN_SOURCE = $(SRC_DIR)/main.c

# 오브젝트 파일
LEXER_OBJECTS = $(LEXER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PARSER_OBJECTS = $(PARSER_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
RUNTIME_OBJECTS = $(RUNTIME_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CODEGEN_OBJECTS = $(CODEGEN_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
JVM_OBJECTS = $(JVM_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

ALL_OBJECTS = $(LEXER_OBJECTS) $(PARSER_OBJECTS) $(RUNTIME_OBJECTS) $(CODEGEN_OBJECTS) $(MAIN_OBJECT)

# 실행 파일
TARGET = $(BIN_DIR)/pergyra
LEXER_TEST = $(BIN_DIR)/lexer_test

# 기본 타겟
all: $(TARGET) $(LEXER_TEST)

# 실행 파일 빌드
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# 렉서 테스트 빌드
$(LEXER_TEST): $(LEXER_OBJECTS) $(MAIN_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# 오브젝트 파일 빌드
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/lexer $(BUILD_DIR)/parser $(BUILD_DIR)/runtime $(BUILD_DIR)/codegen $(BUILD_DIR)/jvm_bridge
	$(CC) $(CFLAGS) -c -o $@ $<

# 디렉토리 생성
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

# 테스트 실행
test: $(LEXER_TEST)
	@echo "=== 렉서 테스트 실행 ==="
	./$(LEXER_TEST)

# 청소
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# 부분 청소
clean-objects:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*/*.o

# 디버그 빌드
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# 릴리즈 빌드  
release: CFLAGS += -DNDEBUG -O3
release: $(TARGET)

# 정적 분석
analyze:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)

# 의존성 생성
depend:
	makedepend -Y -p$(BUILD_DIR)/ $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c 2>/dev/null

# 설치
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# 문서 생성
docs:
	doxygen docs/Doxyfile

.PHONY: all test clean clean-objects debug release analyze depend install docs

# 개별 컴포넌트 빌드
lexer: $(LEXER_OBJECTS)
parser: $(PARSER_OBJECTS)
runtime: $(RUNTIME_OBJECTS)
codegen: $(CODEGEN_OBJECTS)
jvm: $(JVM_OBJECTS)

.PHONY: lexer parser runtime codegen jvm