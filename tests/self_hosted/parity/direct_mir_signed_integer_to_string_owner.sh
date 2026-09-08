#!/usr/bin/env bash
# One signed-integer conversion identity retains Int/Long width on both targets.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-signed-integer-to-string
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
SOURCE=tests/self_hosted/fixtures/direct_mir_signed_integer_to_string.pgy
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -n "$PYTHON_BIN" ]] || fail "Python is required"
for tool in "$CC" "$CLANG" timeout; do
    command -v "$tool" >/dev/null || fail "missing $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/signed-integer-tostr.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $REL"
printf '%s\n' 17 0 2147483648 -2147483649 9223372036854775807 \
    -9223372036854775808 4294967296 -7 true false signed-ready >"$WORK/expected.run"
check_run() {
    local stem="$1"
    timeout 30s "$WORK/$stem.exe" | tr -d '\r' >"$WORK/$stem.run" ||
        fail "$stem execution failed"
    cmp -s "$WORK/expected.run" "$WORK/$stem.run" || {
        diff -u "$WORK/expected.run" "$WORK/$stem.run" >&2 || true
        fail "$stem signed-width/control output differs"
    }
}
for origin in native public; do
    for backend in c llvm; do
        stem="$origin-$backend"
        command=("$PGY" "$SOURCE")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        command+=("--backend=$backend" --opt=dev -o "$REL/$stem.exe")
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
            "${command[@]}") >"$WORK/$stem.compile.log" 2>&1 || {
            cat "$WORK/$stem.compile.log" >&2; fail "$stem compilation failed"
        }
        if [[ "$origin" == public ]]; then
            ! grep -Fq '[pipeline timing]' "$WORK/$stem.compile.log" ||
                fail "$stem used the native pipeline"
        fi
        check_run "$stem"
    done
done
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$REL/program.json") >"$WORK/producer.log" 2>&1 || {
    cat "$WORK/producer.log" >&2; fail "MIR production failed"
}
mir_sha="$(sha256sum "$WORK/program.json" | cut -d' ' -f1)"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/direct_mir_signed_integer_to_string_mutations.py" \
    "$WORK/program.json" "$WORK"
bads=(missing-operand wrong-operand missing-binding wrong-builtin-runtime \
    wrong-builtin-target wrong-result-type)
for backend in c llvm; do
    suffix=c; [[ "$backend" == llvm ]] && suffix=ll
    for good in program display-only; do
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$REL/$good.json" \
            -o "$REL/$good.$suffix") >"$WORK/$good.$backend.project.log" 2>&1 || {
            cat "$WORK/$good.$backend.project.log" >&2; fail "$backend rejected $good"
        }
    done
    cmp -s "$WORK/program.$suffix" "$WORK/display-only.$suffix" ||
        fail "$backend recovered types from display text"
    stem="direct-$backend"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$WORK/program.c")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/program.c"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$WORK/$stem.exe")
        grep -Fq 'pgy_tostr(long long v)' "$WORK/program.c" || fail "C conversion width drifted"
    else
        command=("$CLANG" -x ir "$WORK/program.ll" -o "$WORK/$stem.exe")
        grep -Fq 'define internal ptr @pgy_tostr(i64 %value)' "$WORK/program.ll" ||
            fail "LLVM conversion width drifted"
    fi
    "${command[@]}" >"$WORK/$stem.compile.log" 2>&1 || {
        cat "$WORK/$stem.compile.log" >&2; fail "$stem artifact compilation failed"
    }
    check_run "$stem"
    for bad in "${bads[@]}"; do
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$REL/$bad.json" \
            -o "$REL/$bad.$suffix") >"$WORK/$bad.$backend.log" 2>&1; then
            fail "$backend accepted $bad"
        fi
        [[ ! -e "$WORK/$bad.$suffix" ]] || fail "$backend published $bad"
        grep -Fq 'CODEGEN ERROR' "$WORK/$bad.$backend.log" ||
            fail "$backend $bad lacked an explicit diagnostic"
    done
done
[[ "$(sha256sum "$WORK/program.json" | cut -d' ' -f1)" == "$mir_sha" ]] ||
    fail "projection mutated producer input"
echo "[$LABEL] eleven values on native/public/direct C/LLVM, display identity and 12 refusals: PASS"
