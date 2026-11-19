#!/usr/bin/env bash
# The general MIR emitter compiles a routine that returns an Array<Int> to its
# caller on C and LLVM, gives the same artifact when the routine rows swap,
# and refuses thirteen forged entrypoint, call target, return, result, use,
# ABI and CFG rows without publishing an artifact. This program used to be
# claimed by a per-program-shape route, deleted on 2026-11-20.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-array-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_array_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/array_return_literal.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_array_return_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR" || fail "MIR mutations could not be derived"

project() {
    local input="$1" target="$2" suffix="$3" code=0
    rm -f "$WORK_DIR/$input.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$input.$suffix") \
        >"$WORK_DIR/$input.$target.out" 2>"$WORK_DIR/$input.$target.err" || code=$?
    [[ "$code" -ne 0 ]] && return "$code"
    ! grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$input.$target.out" \
        "$WORK_DIR/$input.$target.err"
}

"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -O0 -g \
    -fsanitize=address -fno-omit-frame-pointer \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.log" 2>&1 || fail "LLVM runtime did not compile"

# The general emitter does not yet retire the returned Array<Int> (S1 of the
# owned-value lifetime plan), so leak detection stays off; ASan still refuses
# any freed-memory access.
run_artifact() {
    local stem="$1" target="$2" suffix="$3" exe="$WORK_DIR/$1-$2.exe"
    if [[ "$target" == c ]]; then
        (cd "$ROOT_DIR" && "$CC" -x c -std=c11 -O0 -g -fsanitize=address \
            -fno-omit-frame-pointer -Isrc -Isrc/runtime -pthread \
            "$WORK_DIR/$stem.c" -lm -o "$exe") >"$WORK_DIR/$stem-$target.compile" 2>&1 ||
            fail "$stem C did not compile"
    else
        "$CLANG" -x ir -O0 -g -fsanitize=address -fno-omit-frame-pointer \
            "$WORK_DIR/$stem.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
            -o "$exe" >"$WORK_DIR/$stem-$target.compile" 2>&1 ||
            fail "$stem LLVM did not compile"
    fi
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$exe" 2>"$WORK_DIR/$stem-$target.asan" |
        tr -d '\r' >"$WORK_DIR/$stem-$target.run" || fail "$stem $target failed under ASan"
}

bads=(entrypoint-name call-target producer-return-kind producer-return-type \
    missing-producer-return duplicate-producer-return stale-result-definition \
    stale-use abi-offset abi-field-shape-repaired-id unreachable-main \
    producer-successor forged-log-result)
printf '4\n3\n' >"$WORK_DIR/expected.run"
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in program routine-order-swap; do
        project "$good" "$target" "$suffix" || fail "$target rejected $good"
        [[ -s "$WORK_DIR/$good.$suffix" ]] || fail "$target published no $good artifact"
    done
    cmp -s "$WORK_DIR/program.$suffix" "$WORK_DIR/routine-order-swap.$suffix" ||
        fail "$target artifact depends on routine row order"
    run_artifact program "$target" "$suffix"
    cmp -s "$WORK_DIR/program-$target.run" "$WORK_DIR/expected.run" ||
        fail "$target printed other output"
    for bad in "${bads[@]}"; do
        if project "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(CODEGEN ERROR|MIR)' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target refused $bad without a diagnostic"
    done
done

echo "[$LABEL] general emitter: C/LLVM 4/3 under ASan, row order irrelevant; ${#bads[@]} forged MIR refused with no artifact"
