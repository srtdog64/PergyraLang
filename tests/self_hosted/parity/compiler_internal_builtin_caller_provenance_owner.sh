#!/usr/bin/env bash
# common caller registry and parser-owned declaration module provenance reject exact wrong-path impersonation in C/LLVM legacy and artifact routes while the production driver bootstrap requires typed --source and rejects its parser/AST-text detour.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PYTHON="${PYTHON:-python3}"
BUILD_DIR="${PGY_COMPILER_INTERNAL_CALLER_TEST_DIR:-$ROOT_DIR/.tmp/self_hosted/compiler_internal_caller}"
SEMANTIC_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"
OWNER_ROUTINE="src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy"
OWNER_ARENA="src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy"
WRONG_PATH="tests/self_hosted/semantic/fixture/compiler_retire_array_storage_owner_impersonation_rejected.pgy"
EXTERNAL="tests/self_hosted/semantic/fixture/compiler_retire_array_storage_external_rejected.pgy"
ARTIFACT_FIXTURE="$ROOT_DIR/tests/self_hosted/semantic/fixture/compiler_internal_builtin_artifact_provenance.pgy"
DRIVER_BOOTSTRAP="$ROOT_DIR/tests/self_hosted/parity/driver_bootstrap.sh"

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[compiler-internal-caller] missing compiler: $PGY" >&2; exit 1; }
command -v "$PYTHON" >/dev/null 2>&1 || {
    echo "[compiler-internal-caller] missing python: $PYTHON" >&2
    exit 1
}
mkdir -p "$BUILD_DIR"

grep -Fq '"$CODEGEN_BIN" --source "$driver_rel"' "$DRIVER_BOOTSTRAP" || {
    echo "[compiler-internal-caller] driver bootstrap does not consume the typed source artifact" >&2
    exit 1
}
if grep -Fq 'PARSER_BIN=' "$DRIVER_BOOTSTRAP" ||
    grep -Fq 'driver_bootstrap.ast.txt' "$DRIVER_BOOTSTRAP"; then
    echo "[compiler-internal-caller] driver bootstrap restored the provenance-free AST-text bypass" >&2
    exit 1
fi

"$PYTHON" "$ROOT_DIR/scripts/render_compiler_internal_builtin_caller_registry.py" \
    "$ROOT_DIR/src/common/compiler_internal_builtin_caller_registry.def" \
    "$ROOT_DIR/src/self_hosted/semantic/compiler_internal_builtin_caller_registry_owner.pgy" \
    --check

compile_self_host() {
    local backend="$1"
    local source="$2"
    local output="$3"
    local log="$4"
    local native_subject=""
    [[ "$backend" == "llvm" ]] && native_subject="--native-pipeline"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" $native_subject \
        -o "$(pgy_path_for_compiler "$PGY" "$output")" \
        >"$log" 2>&1) || {
        cat "$log" >&2
        echo "[compiler-internal-caller] $backend compile failed: $source" >&2
        exit 1
    }
}

run_semantic_case() {
    local checker="$1"
    local source="$2"
    local expected="$3"
    local label="$4"
    local out="$BUILD_DIR/${label}.out"
    local rc=0
    (cd "$ROOT_DIR" && "$checker" --check "$source" >"$out" 2>&1) || rc="$?"
    tr -d '\r' <"$out" >"${out}.normalized"
    if [[ "$expected" == "ok" ]]; then
        [[ "$rc" -eq 0 ]] && grep -Fq 'Status: ok' "${out}.normalized" || {
            cat "${out}.normalized" >&2
            echo "[compiler-internal-caller] admitted owner rejected: $label" >&2
            exit 1
        }
    else
        [[ "$rc" -ne 0 ]] && grep -Fq 'compiler_internal_builtin' "${out}.normalized" || {
            cat "${out}.normalized" >&2
            echo "[compiler-internal-caller] wrong-path caller did not fail closed: $label" >&2
            exit 1
        }
    fi
}

for backend in c llvm; do
    checker="$BUILD_DIR/semantic_${backend}.exe"
    compile_self_host "$backend" "$SEMANTIC_SOURCE" "$checker" \
        "$BUILD_DIR/semantic_${backend}.compile.log"
    run_semantic_case "$checker" "$OWNER_ROUTINE" ok "${backend}_owner_routine"
    run_semantic_case "$checker" "$OWNER_ARENA" ok "${backend}_owner_arena"
    run_semantic_case "$checker" "$WRONG_PATH" reject "${backend}_wrong_path"
    run_semantic_case "$checker" "$EXTERNAL" reject "${backend}_external"

    artifact="$BUILD_DIR/artifact_${backend}.exe"
    compile_self_host "$backend" "$ARTIFACT_FIXTURE" "$artifact" \
        "$BUILD_DIR/artifact_${backend}.compile.log"
    (cd "$ROOT_DIR" && "$artifact" >"$BUILD_DIR/artifact_${backend}.out" 2>&1)
    tr -d '\r' <"$BUILD_DIR/artifact_${backend}.out" > \
        "$BUILD_DIR/artifact_${backend}.normalized"
    grep -Fxq 'compiler-internal artifact provenance: ok' \
        "$BUILD_DIR/artifact_${backend}.normalized" || {
        cat "$BUILD_DIR/artifact_${backend}.normalized" >&2
        echo "[compiler-internal-caller] artifact provenance failed: $backend" >&2
        exit 1
    }
done

echo "[compiler-internal-caller] C/LLVM legacy and artifact provenance PASS"
