#!/usr/bin/env bash
# Current source analyzers run; supplied inputs never emit or execute.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here member-indexed-read "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/member-indexed-read.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[member-indexed-read] $*; evidence=$REL" >&2; exit 1; }
"${PYTHON_BIN:-python3}" scripts/source_size_count.py --caps <<'CAPS'
120	src/self_hosted/semantic/ast_collection_member_read_permission_owner.pgy
180	src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy
CAPS
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
observe() {
    "$observer" "$FIXTURES/$1.pgy" >"$WORK/$backend-$1.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$WORK/$backend-$1.raw" >"$WORK/$backend-$1.log"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/array_storage_call_preservation_probe.pgy \
        -o "$observer" >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer did not compile"
    for name in member_indexed_read_default_positive member_indexed_read_ref_positive member_indexed_read_forwarded_positive \
        member_indexed_read_nested_forwarded_positive member_composed_views_positive; do
        observe "$name"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused $name"
    done
    for name in member_indexed_read_root_copy_negative member_indexed_read_root_own_negative \
        member_indexed_read_root_inout_negative member_indexed_read_deferred_negative \
        member_indexed_read_forwarded_alias_negative member_indexed_read_forwarded_move_negative \
        member_indexed_read_nested_alias_negative member_composed_views_alias_negative; do
        observe "$name"
        grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost ownership diagnosis"
        grep -Fq 'unproved_formal_indexed_read_entry' "$WORK/$backend-$name.log" || fail "$name did not reach indexed-read admission"
    done
    observe member_indexed_read_moved_negative
    grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-member_indexed_read_moved_negative.log" || fail 'moved field lost transfer diagnosis'
    grep -Fq 'member_use_after_move' "$WORK/$backend-member_indexed_read_moved_negative.log" || fail 'moved field bypassed ordered field owner'
    observe member_indexed_read_mutator_negative
    observe member_indexed_read_value_formal_negative
    grep -Fxq 'body_ok=false' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'readonly member crossed a value formal'
    grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal lost borrow diagnosis'
    grep -Fq 'boundary: default' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal bypassed the sequence boundary'
    grep -Fq 'callee: ReadValue' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal lost callee identity'
    grep -Fq 'argument_index: 0' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal lost ordinal identity'
    grep -Fxq 'body_diagnostic=inout_argument_not_variable' "$WORK/$backend-member_indexed_read_mutator_negative.log" || fail 'member read became copy-out authority'
done
echo "[member-indexed-read] PASS (C/LLVM analysis: 5 positives and 11 compile-only refusals each; not installed-driver proof); evidence=$REL"
