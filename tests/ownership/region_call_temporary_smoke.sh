#!/usr/bin/env bash
# Checked borrowed Concat calls retire their storage on native C and LLVM.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here region-call-temporary "$PGY"
for tool in gcc clang timeout sha256sum cmp; do command -v "$tool" >/dev/null; done
mkdir -p "$ROOT_DIR/.tmp"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/region_call_temporary.XXXXXX")"
SOURCE="$ROOT_DIR/tests/cases/backend_compare/region_call_temporary/main.pgy"
EXPECTED="$ROOT_DIR/tests/cases/backend_compare/region_call_temporary/expected.txt"
cd "$ROOT_DIR"
sha256sum "$PGY" "$SOURCE" "$EXPECTED" src/runtime/pgy_runtime_lib.c >"$WORK/inputs.before"
timeout 45s clang -O0 -g -fsanitize=address -DPGY_LLVM_ENABLED -Isrc -Isrc/runtime \
    -c src/runtime/pgy_runtime_lib.c -o "$WORK/runtime.o" >"$WORK/runtime.out" 2>"$WORK/runtime.err"
for backend in c llvm; do
    suffix=c; [[ "$backend" == c ]] || suffix=ll
    timeout 30s "$PGY" --native-pipeline "$SOURCE" "--emit-$backend" \
        -o "$WORK/program.$suffix" >"$WORK/$backend.emit.out" 2>"$WORK/$backend.emit.err"
    if [[ "$backend" == c ]]; then
        timeout 30s gcc -O0 -g -fsanitize=address -Isrc -Isrc/runtime -pthread \
            "$WORK/program.c" -lm -o "$WORK/program-c" >"$WORK/c.link.out" 2>"$WORK/c.link.err"
    else
        timeout 30s clang -O0 -g -fsanitize=address -pthread "$WORK/program.ll" \
            "$WORK/runtime.o" -lm -o "$WORK/program-llvm" >"$WORK/llvm.link.out" 2>"$WORK/llvm.link.err"
    fi
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 LSAN_OPTIONS=exitcode=99 \
        timeout 10s "$WORK/program-$backend" >"$WORK/$backend.run.out" 2>"$WORK/$backend.run.err"
    cmp "$EXPECTED" "$WORK/$backend.run.out"
done
sha256sum "$PGY" "$SOURCE" "$EXPECTED" src/runtime/pgy_runtime_lib.c >"$WORK/inputs.after"
cmp "$WORK/inputs.before" "$WORK/inputs.after"
echo "[region-call-temporary] C/LLVM output and ASan/LSan: PASS; evidence: $WORK"
