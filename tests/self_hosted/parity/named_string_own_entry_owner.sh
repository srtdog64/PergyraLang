#!/usr/bin/env bash
# Exact named String consumer admission; no general own-mode permission.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=named-string-own-entry
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/named-string-own-entry.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
F=tests/self_hosted/parity/fixture/collection_field_lifetime
INPUT="$F/owned_string_named_actual_positive.pgy"
NEGATIVE=(owned_string_named_actual_after owned_string_named_actual_duplicate
    owned_string_named_actual_alias owned_string_named_actual_borrowed
    owned_string_named_actual_reassigned owned_string_named_actual_deferred
    owned_string_literal_transfer_aliased_result
    owned_string_literal_transfer_aliased_concat_result
    owned_string_literal_transfer_borrowed owned_string_literal_transfer_after_use
    owned_string_local_literal_transfer_alias owned_string_local_literal_transfer_reuse
    owned_string_local_literal_transfer_conditional)
ASYNC="$F/owned_string_named_actual_async_negative.pgy"
sha256sum "$PGY" "$PROBE" "$INPUT" "$ASYNC" "${BASH_SOURCE[0]}" >"$B/input.sha256"
for name in "${NEGATIVE[@]}"; do sha256sum "$F/${name}_negative.pgy" >>"$B/input.sha256"; done
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
printf 'NAMED STRING OWN ENTRY PASS\n' >"$B/expected"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INPUT" -o "$B/$backend-value.exe" >"$B/$backend-value.compile" 2>&1 || fail "$backend value did not build"
    timeout 30 "$B/$backend-value.exe" >"$B/$backend-value.raw" 2>"$B/$backend-value.err" || fail "$backend value failed"
    test ! -s "$B/$backend-value.err" || fail "$backend value stderr"
    tr -d '\r' <"$B/$backend-value.raw" >"$B/$backend-value.run"
    cmp "$B/expected" "$B/$backend-value.run" || fail "$backend value drift"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer did not build"
    timeout 60 "$B/$backend-observer.exe" "$INPUT" diagnostic >"$B/$backend-positive.raw" 2>"$B/$backend-positive.err" || fail "$backend positive observation failed"
    test ! -s "$B/$backend-positive.err" || fail "$backend positive stderr"
    tr -d '\r' <"$B/$backend-positive.raw" >"$B/$backend-positive.run"
    grep -Fxq 'body_ok=true' "$B/$backend-positive.run" || fail "$backend rejected named owner"
    grep -Fxq 'formal_ready=true' "$B/$backend-positive.run" || fail "$backend formal carrier invalid"
    for name in "${NEGATIVE[@]}"; do
        timeout 60 "$B/$backend-observer.exe" "$F/${name}_negative.pgy" diagnostic >"$B/$backend-$name.raw" 2>"$B/$backend-$name.err" || fail "$backend $name observation failed"
        test ! -s "$B/$backend-$name.err" || fail "$backend $name stderr"
        tr -d '\r' <"$B/$backend-$name.raw" >"$B/$backend-$name.run"
        grep -Fxq 'body_ok=false' "$B/$backend-$name.run" || fail "$backend admitted $name"
        grep -Eq '^body_diagnostic=(borrow_boundary_escape|move_from_released)$' "$B/$backend-$name.run" || fail "$backend $name diagnosis drift"
    done
    if timeout 60 "$B/$backend-observer.exe" "$ASYNC" diagnostic >"$B/$backend-async.raw" 2>"$B/$backend-async.err"; then
        fail "$backend admitted unsupported detached async source"
    else
        status=$?
    fi
    test "$status" -eq 1 || fail "$backend async observation did not refuse at parsing"
    test ! -s "$B/$backend-async.err" || fail "$backend async parser stderr"
    grep -Fq 'Code: statement_kind_unsupported' "$B/$backend-async.raw" || fail "$backend async parser diagnosis drift"
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] C/LLVM named owning value, thirteen source lifetime refusals and one separate parser refusal per backend PASS; evidence=$B"
