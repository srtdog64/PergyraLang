#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:mir-program-routine-index] missing compiler: $PGY" >&2
    exit 1
fi

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/mir_program_routine_index_owner.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_program_routine_index_owner}"
mkdir -p "$BUILD_DIR"

DECLARATION_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_declaration_index_owner.pgy"
ROUTINE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_routine_index_owner.pgy"
INPUT_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/mir_json_input_owner.pgy"
MACHINE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/machine_layer_fact_owner.pgy"

fail() {
    echo "[self-host-parity:mir-program-routine-index] $*" >&2
    exit 1
}

lines_at_most() {
    local file="$1"
    local limit="$2"
    [[ "$(wc -l <"$file" | tr -d ' ')" -le "$limit" ]] ||
        fail "${file#"$ROOT_DIR/"} exceeds $limit lines"
}

pgy_function_body() {
    local file="$1"
    local signature="$2"
    awk -v signature="$signature" '
        index($0, signature) == 1 { found = 1 }
        found && /^func / && index($0, signature) != 1 { exit }
        found { print }
    ' "$file"
}

lines_at_most "$DECLARATION_OWNER" 120
lines_at_most "$ROUTINE_OWNER" 510
lines_at_most "$MACHINE_OWNER" 440

declaration_body="$(pgy_function_body \
    "$DECLARATION_OWNER" \
    'func BuildMirProgramDeclarationIndexFromTable(')"
grep -Fq 'JsonArrayObjectFactTableReady(declarations)' <<<"$declaration_body" ||
    fail "declaration index must consume a ready document-owned table"
grep -Fq 'i + 1 == array_end' "$DECLARATION_OWNER" ||
    fail "declaration table end must be exact"
grep -Fq 'MirDeclArrayBounds(' <<<"$declaration_body" &&
    fail "declaration index reopened raw document bounds"

routine_body="$(pgy_function_body \
    "$ROUTINE_OWNER" 'func BuildMirProgramRoutineIndexFromTable(')"
grep -Fq 'declarations.valid && declarations.field_identities.valid' \
    <<<"$routine_body" || fail "routine index ignored carried declarations"
for forbidden in \
    'BuildMirProgramDeclarationIndex(' \
    'BuildMirProgramDeclarationIndexFromTable(' \
    'BuildMirDocumentFactIndex(' \
    'MirDeclArrayBounds('; do
    grep -Fq "$forbidden" <<<"$routine_body" &&
        fail "routine index reintroduced forbidden declaration read: $forbidden"
done
grep -Fq 'BuildMirProgramDeclarationIndexFromTable(document.declarations)' \
    "$INPUT_OWNER" || fail "production input did not carry declaration table"
grep -Fq 'document.routines, declarations' "$MACHINE_OWNER" ||
    fail "machine admission did not carry declaration index"
grep -Fq 'document.declarations.end + 1' "$SOURCE" ||
    fail "missing extended declaration-bound negative"
grep -Fq 'cross_document_index.valid' "$SOURCE" ||
    fail "missing cross-document declaration negative"

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/mir_program_routine_index_${backend}.exe"
    local compile_log="$BUILD_DIR/mir_program_routine_index_${backend}.compile.log"
    local run_err="$BUILD_DIR/mir_program_routine_index_${backend}.err"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1); then
        cat "$compile_log" >&2
        return 1
    fi
    pgy_require_runnable_binary_here \
        "self-host-parity:mir-program-routine-index:$backend" "$bin" || return 1
    (cd "$ROOT_DIR" && "$bin" 2>"$run_err") | tr -d '\r'
}

C_OUT="$(run_backend c)"
[[ "$C_OUT" == "mir-program-routine-index-owner-ok" ]] || {
    fail "C mismatch: $C_OUT"
}
LLVM_OUT="$(run_backend llvm)"
[[ "$LLVM_OUT" == "$C_OUT" ]] || {
    fail "LLVM mismatch: $LLVM_OUT"
}

echo "[self-host-parity:mir-program-routine-index] C/LLVM structure parity ok"
