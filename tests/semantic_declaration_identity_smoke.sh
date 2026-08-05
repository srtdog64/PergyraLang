#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   semantic declaration identity drifted.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[semantic-decl-identity] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_semantic_decl_identity.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq "$text" "$ROOT_DIR/$file"; then
        echo "[semantic-decl-identity] missing $file contract: $text" >&2
        exit 1
    fi
}

require_text "src/semantic/symbol_table.h" "uint32_t   decl_syntax_id;"
require_text "src/semantic/symbol_table.h" "bool       is_forward_placeholder;"
require_text "src/semantic/symbol_table.c" "symbol_is_forward_declaration_for"
require_text "src/semantic/symbol_table.c" "symbol->decl_syntax_id == syntax_id"
require_text "src/semantic/type_checker_program.c" \
    "symbol_mark_declaration(s, ast_node_stable_id(stmt), true);"
require_text "src/semantic/type_checker_program.c" \
    "if (ast_node_stable_id(program) == 0"
require_text "src/semantic/type_checker_program.c" \
    "&& !ast_assign_stable_ids(program))"
require_text "src/semantic/type_checker_func_decl.c" \
    "symbol_is_forward_declaration_for(existing,"
require_text "src/semantic/type_checker_class_decl.c" \
    "SYMBOL_CLASS, ast_node_stable_id(node)"
require_text "src/semantic/type_checker_ability_decl.c" \
    "SYMBOL_ABILITY, ast_node_stable_id(node)"

if grep -R -n -E 'existing->decl_(line|col)' "$ROOT_DIR/src/semantic" \
    >"$WORK_DIR/legacy_identity_reads"; then
    echo "[semantic-decl-identity] line/column declaration identity returned" >&2
    cat "$WORK_DIR/legacy_identity_reads" >&2
    exit 1
fi

positive="$ROOT_DIR/tests/cases/stable_identity_import_merge/main.pgy"
positive_arg="$(pgy_path_for_compiler "$PGY" "$positive")"
positive_out="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/positive.c")"
"$PGY" "$positive_arg" --backend=c --emit-c -o "$positive_out" \
    >"$WORK_DIR/positive.out" 2>"$WORK_DIR/positive.err" || {
        cat "$WORK_DIR/positive.err" >&2
        exit 1
    }

expect_duplicate() {
    local case_name="$1"
    local cause="$2"
    local source="$ROOT_DIR/tests/cases/semantic_declaration_identity/$case_name/main.pgy"
    local source_arg
    local output_arg
    local log="$WORK_DIR/$case_name.log"

    source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
    output_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$case_name.c")"
    if "$PGY" "$source_arg" --backend=c --emit-c --error-format=json \
        -o "$output_arg" >"$log" 2>&1; then
        echo "[semantic-decl-identity] $case_name unexpectedly succeeded" >&2
        exit 1
    fi
    for required in \
        '"stage":"semantic"' \
        '"code":"PGY_SEM_REDECLARATION"' \
        "\"cause_ir\":\"$cause\""; do
        if ! grep -Fq "$required" "$log"; then
            echo "[semantic-decl-identity] $case_name missing: $required" >&2
            cat "$log" >&2
            exit 1
        fi
    done
    if grep -Fq '"stage":"mir_validate"' "$log"; then
        echo "[semantic-decl-identity] duplicate escaped to MIR validation" >&2
        cat "$log" >&2
        exit 1
    fi
}

expect_duplicate "duplicate_function" "semantic:function:duplicate_name"
expect_duplicate "duplicate_class" "semantic:class:duplicate_name"

echo "[semantic-decl-identity] SyntaxNodeId-owned placeholders reject cross-module coalescing"
