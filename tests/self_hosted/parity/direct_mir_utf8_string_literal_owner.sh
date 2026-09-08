#!/usr/bin/env bash
# docs/110 byte semantics through public and one-MIR C/LLVM projections.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-utf8-string-literal
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
CASES="$ROOT_DIR/tests/self_hosted/parity/direct_mir_utf8_string_literal_cases.py"
SOURCE=tests/self_hosted/fixtures/direct_mir_utf8_string_literal.pgy
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -n "$PYTHON_BIN" ]] || fail "Python is required"
for tool in "$CC" "$CLANG" timeout; do
    command -v "$tool" >/dev/null || fail "missing $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/utf8-string-literal.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $REL"
check_run() {
    local stem="$1" variant="$2"
    timeout 30s "$WORK/$stem.exe" >"$WORK/$stem.run" 2>"$WORK/$stem.run.err" ||
        fail "$stem execution failed"
    "$PYTHON_BIN" "$CASES" compare "$WORK/$stem.run" "$variant" ||
        fail "$stem byte preservation/length/normalization differs"
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
        check_run "$stem" program
    done
done
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$REL/program.json") >"$WORK/producer.log" 2>&1 || {
    cat "$WORK/producer.log" >&2; fail "MIR production failed"
}
mir_sha="$(sha256sum "$WORK/program.json" | cut -d' ' -f1)"
"$PYTHON_BIN" "$CASES" prepare "$WORK/program.json" "$WORK"
bads=(missing-quote unsupported-escape unescaped-quote raw-tab raw-del \
    wrong-literal-kind unexpected-binding)
for backend in c llvm; do
    suffix=c; [[ "$backend" == llvm ]] && suffix=ll
    for good in program display-only semantic-change; do
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$REL/$good.json" \
            -o "$REL/$good.$suffix") >"$WORK/$good.$backend.project.log" 2>&1 || {
            cat "$WORK/$good.$backend.project.log" >&2; fail "$backend rejected $good"
        }
    done
    cmp -s "$WORK/program.$suffix" "$WORK/display-only.$suffix" ||
        fail "$backend used display text as literal identity"
    ! cmp -s "$WORK/program.$suffix" "$WORK/semantic-change.$suffix" ||
        fail "$backend ignored a typed UTF-8 payload change"
    if [[ "$backend" == llvm ]]; then
        grep -Fq 'c"\ED\95\9C\EA\B8\80\F0\9F\99\82\00"' "$WORK/program.ll" ||
            fail "LLVM did not retain the exact Korean/emoji bytes"
    fi
    for good in program semantic-change; do
        stem="direct-$backend-$good"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 "$WORK/$good.c")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/$good.c"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$WORK/$stem.exe")
        else
            command=("$CLANG" -x ir "$WORK/$good.ll" -o "$WORK/$stem.exe")
        fi
        "${command[@]}" >"$WORK/$stem.compile.log" 2>&1 || {
            cat "$WORK/$stem.compile.log" >&2; fail "$stem artifact compilation failed"
        }
        check_run "$stem" "$good"
    done
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
echo "[$LABEL] six-path byte/length/normalization parity, semantic/display identity and 14 refusals: PASS"
