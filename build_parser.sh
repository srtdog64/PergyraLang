#!/bin/bash
# Quick build script for parser test

echo "Building Pergyra Parser Test..."

# Create directories
mkdir -p build/lexer build/parser bin

# Compile lexer
echo "Compiling lexer..."
gcc -Wall -c src/lexer/lexer.c -o build/lexer/lexer.o

# Compile parser
echo "Compiling parser..."
gcc -Wall -c src/parser/parser.c -o build/parser/parser.o
gcc -Wall -c src/parser/ast.c -o build/parser/ast.o

# Compile test
echo "Compiling test..."
gcc -Wall -c src/test_parser.c -o build/test_parser.o

# Link
echo "Linking..."
gcc -Wall build/lexer/lexer.o build/parser/parser.o build/parser/ast.o build/test_parser.o -o bin/parser_test

echo "Build complete! Run with: ./bin/parser_test"
