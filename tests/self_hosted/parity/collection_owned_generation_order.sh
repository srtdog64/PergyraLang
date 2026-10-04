#!/usr/bin/env bash
# Fresh-generation own-entry and post-transfer scope falsifiers.
# Current C/LLVM analyzers run; supplied source inputs never emit or execute.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-owned-generation "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-owned-generation.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[collection-owned-generation] $*; evidence=$REL" >&2; exit 1; }
"${PYTHON_BIN:-python3}" scripts/source_size_count.py --caps <<'CAPS'
110	src/self_hosted/semantic/ast_collection_owned_generation_order_owner.pgy
150	src/self_hosted/semantic/ast_collection_owned_argument_admission_owner.pgy
230	src/self_hosted/semantic/ast_collection_ownership_argument_transfer_owner.pgy
CAPS
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
SOURCE_PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
observe() {
    "$observer" "$FIXTURES/$1.pgy" diagnostic >"$WORK/$backend-$1.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$WORK/$backend-$1.raw" >"$WORK/$backend-$1.log"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$SOURCE_PROBE" \
        -o "$observer" >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer did not compile"
    for name in own_generation_cleanup_return_positive own_generation_cleanup_continue_positive \
        inout_event_loop_fresh_generation_positive own_storage_live_empty_positive; do
        observe "$name"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused $name"
    done
    for name in own_generation_double_transfer_negative own_generation_nonterminal_cleanup_negative \
        own_generation_return_use_negative; do
        observe "$name"
        grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
        grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-$name.log" || fail "$name lost transfer-use diagnosis"
        grep -Fq 'owned_argument_use_after_move' "$WORK/$backend-$name.log" || fail "$name did not reach ordered transfer owner"
    done
    for name in own_generation_nested_loop_negative own_storage_loop_negative \
        own_storage_deferred_negative own_storage_retired_empty_negative own_storage_child_retired_negative \
        own_assign_stale_required_owned_negative inout_event_loop_fresh_defer_negative; do
        observe "$name"
        grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
        grep -Eq '^body_diagnostic=(borrow_boundary_escape|move_from_released)$' "$WORK/$backend-$name.log" || fail "$name lost ownership diagnosis"
    done
done
echo "[collection-owned-generation] PASS (C/LLVM analysis: 4 positives and 10 compile-only refusals each; not installed-driver proof); evidence=$REL"
