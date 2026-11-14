#!/usr/bin/env bash
# One backend shard: retain row Strings independently, then free the input.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT/bin/pgy}")"
pgy_require_runnable_binary_here mir-row-borrow-retention "$PGY"
backend="${1:?expected c or llvm backend shard}"
case "$backend" in c) suffix=c; emit=--emit-c ;; llvm) suffix=ll; emit=--emit-llvm ;; *) exit 2 ;; esac
CC="${CLANG_BIN:-clang-18}"
cd "$ROOT"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/mir-row-borrow-retention.XXXXXX)"
SOURCE=tests/self_hosted/fixtures/mir_row_borrow_retention_probe.pgy
OBSERVER=tests/self_hosted/parity/mir/mir_row_borrow_release_observer.c
sha256sum "$PGY" "$SOURCE" "$OBSERVER" >"$WORK/inputs.sha256"
find src/self_hosted src/runtime -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
timeout 240 "$PGY" --native-pipeline "$SOURCE" "$emit" -o "$WORK/program.$suffix" \
    >"$WORK/emit.out" 2>"$WORK/emit.err"
flags=(-O0 -g0 -fwrapv -fno-strict-aliasing -fsanitize=address -fno-omit-frame-pointer -pthread -Isrc -Isrc/runtime)
extra=()
if [[ "$backend" == llvm ]]; then
    timeout 45 "$CC" "${flags[@]}" -DPGY_LLVM_ENABLED -c src/runtime/pgy_runtime_lib.c \
        -o "$WORK/runtime-san.o" >"$WORK/runtime.out" 2>"$WORK/runtime.err"
    extra+=("$WORK/runtime-san.o")
fi
for control in normal suppressed; do
    define=()
    [[ "$control" != suppressed ]] || define+=(-DPGY_SUPPRESS_INPUT_FREE)
    timeout 45 "$CC" "${flags[@]}" "$WORK/program.$suffix" "${extra[@]}" "$OBSERVER" \
        "${define[@]}" -Wl,--wrap=malloc,--wrap=free -lm -o "$WORK/$control.exe" \
        >"$WORK/$control.link.out" 2>"$WORK/$control.link.err"
    status=0
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 timeout 5 "$WORK/$control.exe" \
        >"$WORK/$control.out" 2>"$WORK/$control.err" || status=$?
    if [[ "$control" == normal ]]; then
        [[ "$status" == 0 && "$(cat "$WORK/$control.out")" == retained-row-ok ]]
        printf 'borrow_input_frees=1\n' >"$WORK/expected.err"
    else
        [[ "$status" == 89 ]]
        printf 'borrow_input_frees=0\n' >"$WORK/expected.err"
    fi
    cmp "$WORK/expected.err" "$WORK/$control.err"
done
sha256sum --quiet -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
echo "[mir-row-borrow-retention] $backend PASS: physical input free, retained row read and missing-free control; $WORK"
