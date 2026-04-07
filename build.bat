@echo off
setlocal enabledelayedexpansion

set LOGFILE=build\build.log
echo === Pergyra Build (Windows) ===
echo === Pergyra Build (Windows) === > %LOGFILE%
echo Started: %date% %time% >> %LOGFILE%

set CC=gcc
set CFLAGS=-Wall -Wextra -std=c11 -O2 -g -Isrc -DPGY_PROJECT_ROOT=\"E:/PergyraLang\" -DPGY_SRC_DIR=\"E:/PergyraLang/src\" -DPGY_RUNTIME_DIR=\"E:/PergyraLang/src/runtime\" -DPGY_RUNTIME_LIB_C=\"E:/PergyraLang/src/runtime/pgy_runtime_lib.c\" -DPGY_LLVM_ENABLED -Ithird_party
set LDFLAGS=-L"C:/Program Files/LLVM/lib" -lLLVM-C -lwinpthread -lm

:: Create build directories
if not exist build\common mkdir build\common
if not exist build\lexer mkdir build\lexer
if not exist build\parser mkdir build\parser
if not exist build\semantic mkdir build\semantic
if not exist build\codegen mkdir build\codegen
if not exist build\compiler mkdir build\compiler
if not exist build\runtime mkdir build\runtime
if not exist build\runtime\async mkdir build\runtime\async
if not exist build\lsp mkdir build\lsp
if not exist bin mkdir bin

:: Source files
set SOURCES=^
 src\common\arena.c^
 src\lexer\lexer.c^
 src\parser\ast.c^
 src\parser\ast_print.c^
 src\parser\parser.c^
 src\parser\parser_expr.c^
 src\parser\parser_stmt.c^
 src\parser\parser_decl.c^
 src\parser\parser_intent.c^
 src\parser\parser_domain.c^
 src\parser\parser_async.c^
 src\semantic\type_system.c^
 src\semantic\symbol_table.c^
 src\semantic\type_checker.c^
 src\semantic\type_checker_builtins.c^
 src\semantic\type_checker_flow.c^
 src\semantic\slot_analyzer.c^
 src\semantic\semantic.c^
 src\codegen\transpiler.c^
 src\compiler\compiler.c^
 src\compiler\dir.c^
 src\compiler\rir.c^
 src\compiler\mir.c^
 src\compiler\hir.c^
 src\compiler\module_loader.c^
 src\compiler\module_normalizer.c^
 src\compiler\import_resolver.c^
 src\compiler\driver_app.c^
 src\compiler\path_utils.c^
 src\compiler\llvm_runner.c^
 src\compiler\c_runner.c^
 src\compiler\repl.c^
 src\compiler\fmt.c^
 src\compiler\pkg.c^
 src\compiler\debugger.c^
 src\codegen\llvm_backend.c^
 src\codegen\llvm_expr.c^
 src\codegen\llvm_stmt.c^
 src\codegen\llvm_decl.c^
 src\codegen\llvm_domain.c^
 src\runtime\pgy_runtime_lib.c^
 src\pgy_driver.c

:: Compile each source
set OBJS=
set FAIL=0
for %%F in (%SOURCES%) do (
    set "SRC=%%F"
    set "OBJ=!SRC:src\=build\!"
    set "OBJ=!OBJ:.c=.o!"
    echo [CC] %%F
    echo [CC] %%F >> %LOGFILE%
    %CC% %CFLAGS% -c -o "!OBJ!" "%%F" 2>> %LOGFILE%
    if errorlevel 1 (
        echo FAILED: %%F
        echo FAILED: %%F >> %LOGFILE%
        set FAIL=1
    )
    set "OBJS=!OBJS! !OBJ!"
)

if !FAIL!==1 (
    echo === Build failed ===
    exit /b 1
)

:: Link
echo [LD] bin\pgy.exe
%CC% %CFLAGS% -o bin\pgy.exe %OBJS% %LDFLAGS%
if errorlevel 1 (
    echo === Link failed ===
    exit /b 1
)

echo === Build successful: bin\pgy.exe ===
echo Finished: %date% %time% >> %LOGFILE%
echo === Build successful === >> %LOGFILE%
echo.
echo Build log: %LOGFILE%

:: Also build test_semantic
echo [CC] test_semantic
%CC% %CFLAGS% -c -o build\test_semantic.o src\test_semantic.c
set "TEST_OBJS="
for %%F in (%OBJS%) do (
    echo %%F | findstr /v "pgy_driver" >nul && set "TEST_OBJS=!TEST_OBJS! %%F"
)
%CC% %CFLAGS% -o bin\test_semantic.exe build\test_semantic.o !TEST_OBJS! %LDFLAGS%
if errorlevel 1 (
    echo test_semantic link failed
) else (
    echo === test_semantic built ===
)

endlocal
