#!/usr/bin/env bash
# Native bootstrap ratchet for one straight-line aggregate release/restore.
# The self-host aggregate analyzer remains the semantic call-site authority.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=collection-native-member-release-restore
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT/bin/pgy}")"
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
mkdir -p "$ROOT/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT/.tmp/self_hosted/native-member-release.XXXXXX")"
cd "$ROOT"

POSITIVES=(
    tests/concept_semantics/hashmap/inout_field_drop_restore_valid.pgy
    "$FIXTURES/callable_table_owned_release_positive.pgy"
)
NEGATIVES=(
    "$FIXTURES/aggregate_release_field_writeback_missing_negative.pgy"
    "$FIXTURES/aggregate_release_wrong_field_writeback_negative.pgy"
    "$FIXTURES/aggregate_release_branch_drop_negative.pgy"
    "$FIXTURES/native_aggregate_release_intervening_call_negative.pgy"
)
sha256sum "$PGY" "${BASH_SOURCE[0]}" "${POSITIVES[@]}" "${NEGATIVES[@]}" \
    >"$WORK/inputs.sha256"

index=0
for input in "${POSITIVES[@]}"; do
    timeout 30 "$PGY" --native-pipeline --mir-json "$input" \
        >"$WORK/positive-$index.mir.json" 2>"$WORK/positive-$index.err" || \
        fail "rejected straight-line release/restore input=$input"
    [[ -s "$WORK/positive-$index.mir.json" ]] || \
        fail "missing MIR for positive input=$input"
    grep -Fxq '0 error(s), 0 warning(s)' \
        <(tr -d '\r' <"$WORK/positive-$index.err") || \
        fail "diagnostic drift for positive input=$input"
    for backend in c llvm; do
        timeout 120 "$PGY" --native-pipeline "--backend=$backend" "$input" \
            -o "$WORK/positive-$index-$backend.exe" \
            >"$WORK/positive-$index-$backend.compile.out" \
            2>"$WORK/positive-$index-$backend.compile.err" || \
            fail "$backend compile failed for positive input=$input"
        timeout 30 "$WORK/positive-$index-$backend.exe" \
            >"$WORK/positive-$index-$backend.raw" \
            2>"$WORK/positive-$index-$backend.run.err" || \
            fail "$backend execution failed for positive input=$input"
        [[ ! -s "$WORK/positive-$index-$backend.run.err" ]] || \
            fail "$backend execution wrote stderr for positive input=$input"
        tr -d '\r' <"$WORK/positive-$index-$backend.raw" \
            >"$WORK/positive-$index-$backend.run"
        if [[ "$index" == 0 ]]; then
            printf '1\n0\n' >"$WORK/positive-$index.expected"
        else
            printf 'owned-table-retired\n' >"$WORK/positive-$index.expected"
        fi
        cmp "$WORK/positive-$index.expected" \
            "$WORK/positive-$index-$backend.run" || \
            fail "$backend output drift for positive input=$input"
    done
    index=$((index + 1))
done

index=0
for input in "${NEGATIVES[@]}"; do
    if timeout 30 "$PGY" --native-pipeline --mir-json "$input" \
        >"$WORK/negative-$index.mir.json" 2>"$WORK/negative-$index.err"; then
        fail "admitted unproved release/restore input=$input"
    else
        result=$?
    fi
    [[ "$result" == 1 ]] || \
        fail "non-semantic failure exit=$result input=$input"
    [[ ! -s "$WORK/negative-$index.mir.json" ]] || \
        fail "published MIR for negative input=$input"
    grep -Fq 'ArrayDropOwnedStrings cannot release escaped array storage' \
        "$WORK/negative-$index.err" || \
        fail "wrong refusal for negative input=$input"
    index=$((index + 1))
done

sha256sum --quiet -c "$WORK/inputs.sha256"
echo "[$LABEL] two C/LLVM executions and four pre-MIR refusals PASS; evidence=${WORK#"$ROOT/"}"
