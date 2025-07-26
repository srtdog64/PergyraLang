@echo off
REM Quick build script for parser test on Windows

echo Building Pergyra Parser Test...

REM Create directories
if not exist build\lexer mkdir build\lexer
if not exist build\parser mkdir build\parser
if not exist bin mkdir bin

REM Compile lexer
echo Compiling lexer...
gcc -Wall -c src\lexer\lexer.c -o build\lexer\lexer.o
if errorlevel 1 goto error

REM Compile parser
echo Compiling parser...
gcc -Wall -c src\parser\parser.c -o build\parser\parser.o
if errorlevel 1 goto error

gcc -Wall -c src\parser\ast.c -o build\parser\ast.o
if errorlevel 1 goto error

REM Compile test
echo Compiling test...
gcc -Wall -c src\test_parser.c -o build\test_parser.o
if errorlevel 1 goto error

REM Link
echo Linking...
gcc -Wall build\lexer\lexer.o build\parser\parser.o build\parser\ast.o build\test_parser.o -o bin\parser_test.exe
if errorlevel 1 goto error

echo Build complete! Run with: bin\parser_test.exe
goto end

:error
echo Build failed!

:end
