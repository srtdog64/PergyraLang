@echo off
setlocal enabledelayedexpansion

set LOGFILE=build\build.log
echo === Pergyra Build (Windows) ===
echo === Pergyra Build (Windows) === > %LOGFILE%
echo Started: %date% %time% >> %LOGFILE%

set CC=gcc
set CFLAGS=-Wall -Wextra -std=c11 -O2 -g -Isrc -DPGY_PROJECT_ROOT=\"E:/PergyraLang\" -DPGY_SRC_DIR=\"E:/PergyraLang/src\" -DPGY_RUNTIME_DIR=\"E:/PergyraLang/src/runtime\" -DPGY_RUNTIME_LIB_C=\"E:/PergyraLang/src/runtime/pgy_runtime_lib.c\" -DPGY_LLVM_ENABLED -Ithird_party
set LDFLAGS=-L"C:/Program Files/LLVM/lib" -lLLVM-C -lwinpthread -lm -liphlpapi -ladvapi32

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

:: Source files (dynamically detected)
set SOURCES=
for %%D in (common lexer parser semantic codegen compiler runtime) do (
    for /f "delims=" %%F in ('dir /b /s src\%%D\*.c') do (
        set "SRC=%%F"
        set "REL=!SRC:%CD%\=!"
        set "SOURCES=!SOURCES! !REL!"
    )
)
set SOURCES=!SOURCES! src\pgy_driver.c

:: Create a temporary file list
set "SOURCES_LIST=build\sources.txt"
if exist "%SOURCES_LIST%" del "%SOURCES_LIST%"

for %%D in (common lexer parser semantic codegen compiler runtime) do (
    for /f "delims=" %%F in ('dir /b /s src\%%D\*.c') do (
        set "SRC=%%F"
        set "REL=!SRC:%CD%\=!"
        echo !REL!>> "%SOURCES_LIST%"
    )
)
echo src\pgy_driver.c>> "%SOURCES_LIST%"

:: Compile each source
set "OBJECTS_LIST=build\objects.txt"
if exist "%OBJECTS_LIST%" del "%OBJECTS_LIST%"

set FAIL=0
for /f "usebackq delims=" %%F in ("%SOURCES_LIST%") do (
    set "SRC=%%F"
    set "OBJ=!SRC:src\=build\!"
    set "OBJ=!OBJ:.c=.o!"

    :: Create nested directories in build\ if they don't exist
    for %%I in ("!OBJ!") do (
        if not exist "%%~dpI" mkdir "%%~dpI"
    )

    echo [CC] %%F
    %CC% %CFLAGS% -c -o "!OBJ!" "%%F"
    if errorlevel 1 (
        echo FAILED: %%F
        set FAIL=1
    )
    set "OBJ=!OBJ:\=/!"
    echo !OBJ!>> "%OBJECTS_LIST%"
)

if !FAIL!==1 (
    echo === Build failed ===
    exit /b 1
)

:: Link
echo [LD] bin\pgy.exe
%CC% %CFLAGS% -o bin\pgy.exe @%OBJECTS_LIST% %LDFLAGS%
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

set "TEST_OBJECTS_LIST=build\test_objects.txt"
if exist "%TEST_OBJECTS_LIST%" del "%TEST_OBJECTS_LIST%"
for /f "usebackq delims=" %%O in ("%OBJECTS_LIST%") do (
    set "OBJ_PATH=%%O"
    if "!OBJ_PATH:pgy_driver=!"=="!OBJ_PATH!" (
        echo %%O>> "%TEST_OBJECTS_LIST%"
    )
)

%CC% %CFLAGS% -o bin\test_semantic.exe build\test_semantic.o @%TEST_OBJECTS_LIST% %LDFLAGS%
if errorlevel 1 (
    echo test_semantic link failed
) else (
    echo === test_semantic built ===
)

endlocal
