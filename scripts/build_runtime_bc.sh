#!/usr/bin/env bash
#
# build_runtime_bc.sh
#
# Compile the Pergyra C runtime to LLVM bitcode so the LLVM backend can inline
# runtime primitives (Substring, StringConcat, pgy_array_*, ...) the way the C
# backend's static-inline runtime is folded by gcc. The LLVM backend links this
# bitcode into each module before optimization when PGY_RUNTIME_BC (env) or the
# build-time PGY_RUNTIME_LIB_BC define points at it; absent that, the backend
# keeps calling the runtime as external symbols, so this step is optional.
#
# Requires clang matching the libLLVM the compiler links against (LLVM 15 here).
# clang is NOT a build dependency for pgy itself: generate this artifact once
# (here or in CI) and gcc-only users consume the committed .bc.
#
# Usage:
#   scripts/build_runtime_bc.sh [output.bc]
#   CLANG=clang-15 scripts/build_runtime_bc.sh
#
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLANG="${CLANG:-clang}"
OUT="${1:-$ROOT/src/runtime/pgy_runtime_lib.bc}"

if ! command -v "$CLANG" >/dev/null 2>&1; then
    echo "build_runtime_bc: '$CLANG' not found; set CLANG to a clang matching libLLVM" >&2
    exit 1
fi

# The entire runtime translation unit is wrapped in `#ifdef PGY_LLVM_ENABLED`
# (it holds the non-inline symbols emitted only for LLVM linking), so the define
# is REQUIRED -- without it clang compiles the file to an empty module and the
# resulting bitcode has zero functions, silently defeating the whole optimization.
DEFS=(
    -DPGY_LLVM_ENABLED
    -DPGY_ZONE_THREADSAFE
    # This is the .bc build: declare the shared gate state extern so it is not
    # duplicated against the native cache object (see authority_file_core.h).
    -DPGY_RUNTIME_BC_BUILD
    -DPGY_PROJECT_ROOT="\"$ROOT\""
    -DPGY_SRC_DIR="\"$ROOT/src\""
    -DPGY_RUNTIME_DIR="\"$ROOT/src/runtime\""
    -DPGY_RUNTIME_LIB_C="\"$ROOT/src/runtime/pgy_runtime_lib.c\""
)
TARGET_FLAGS=()

# On a mingw/MSYS host the runtime pulls in mingw headers (<pthread.h>) and must
# match the gcc-built program's ABI, so target the mingw triple and add the
# mingw system include. clang's own headers do not ship pthread.h.
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*|Windows*)
        DEFS+=(-D__USE_MINGW_ANSI_STDIO=1)
        TARGET_FLAGS+=(--target="${PGY_RUNTIME_BC_TARGET:-x86_64-w64-mingw32}")
        # Locate the mingw system include that holds pthread.h.
        mingw_inc="${PGY_MINGW_INCLUDE:-}"
        if [ -z "$mingw_inc" ]; then
            for cand in \
                /c/ProgramData/mingw64/mingw64/x86_64-w64-mingw32/include \
                /mingw64/x86_64-w64-mingw32/include \
                /mingw64/include; do
                if [ -f "$cand/pthread.h" ]; then mingw_inc="$cand"; break; fi
            done
        fi
        if [ -n "$mingw_inc" ]; then
            TARGET_FLAGS+=(-isystem "$mingw_inc")
        else
            echo "build_runtime_bc: WARNING: no mingw include with pthread.h found;" >&2
            echo "  set PGY_MINGW_INCLUDE to the dir containing pthread.h" >&2
        fi
        ;;
    *)
        # POSIX hosts: glibc feature-test macros, default (host) target.
        DEFS+=(-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE)
        ;;
esac

# Mirror the macro/define set the runtime .c is compiled with for the C backend
# so the bitcode is ABI-identical to the externally linked runtime object.
"$CLANG" "${TARGET_FLAGS[@]}" -emit-llvm -O2 -g0 -std=c11 \
    "${DEFS[@]}" \
    -I"$ROOT/src" -I"$ROOT/third_party" \
    -c "$ROOT/src/runtime/pgy_runtime_lib.c" -o "$OUT"

# Guard against the silent-empty-module failure: a valid runtime bitcode must
# define the hot primitives (Substring et al.). Refuse to "succeed" with an
# empty artifact that would make the optimization a no-op.
if command -v llvm-nm >/dev/null 2>&1; then
    SYMBOLS="$(llvm-nm "$OUT" 2>/dev/null)"
    if [ "$(printf '%s\n' "$SYMBOLS" | grep -cE ' T ')" -eq 0 ]; then
        echo "build_runtime_bc: ERROR: bitcode has no defined functions" >&2
        echo "  (is -DPGY_LLVM_ENABLED reaching the compile? is the target/include right?)" >&2
        exit 1
    fi
    for symbol in pgy_text_builder_new_export \
                  pgy_text_builder_append_export \
                  pgy_text_builder_finish_export \
                  pgy_text_builder_drop_export; do
        if ! printf '%s\n' "$SYMBOLS" | grep -Eq " T ${symbol}$"; then
            echo "build_runtime_bc: ERROR: missing TextBuilder export '$symbol'" >&2
            exit 1
        fi
    done
fi

echo "build_runtime_bc: wrote $OUT"
echo "enable it with:  PGY_RUNTIME_BC=$OUT  pgy <file>.pgy --backend=llvm"
echo "  (use a native path, e.g. E:/path/...; a /unix/path is not openable by a Windows pgy)"
