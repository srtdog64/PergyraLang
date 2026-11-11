#!/usr/bin/env bash
# A native residual-boundary ratchet, not self-host field-lifetime proof.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT/tests/pgy_binary_path_helpers.sh"
source "$ROOT/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=collection-native-storage-escape
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
mkdir -p "$ROOT/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT/.tmp/self_hosted/native-storage-escape.XXXXXX")"
REL=".tmp/self_hosted/${WORK##*/}"
echo "[$LABEL] evidence=$REL compiler=$PGY"
SECONDS=0
run_admission_bounded() {
    local remaining=$((300-SECONDS))
    (( remaining > 0 )) || fail "native storage total budget exhausted"
    (( remaining <= 30 )) || remaining=30
    timeout --signal=TERM --kill-after=2s "$remaining" "$@"
}
# Freeze every native source/header, including the owner/flow-state ABI.
(cd "$ROOT"; git ls-files -z -- src Makefile | while IFS= read -r -d '' path; do
    case "$path" in *.c|*.h|Makefile) sha256sum "$path" ;; esac
done) >"$WORK/native-sources.sha256"
sha256sum "$PGY" >"$WORK/native-binary.sha256"
find "$ROOT/src/self_hosted" -type f -name '*.pgy' -print0 | sort -z >"$WORK/import-paths.before"
xargs -0 sha256sum <"$WORK/import-paths.before" >"$WORK/imports.sha256"
finish_guards() {
    local original_status=$? guard_status=0
    for manifest in native-sources native-binary imports inputs; do
        [[ ! -f "$WORK/$manifest.sha256" ]] ||
            (cd "$ROOT"; sha256sum --quiet -c "$WORK/$manifest.sha256") || guard_status=1
    done
    find "$ROOT/src/self_hosted" -type f -name '*.pgy' -print0 | sort -z >"$WORK/import-paths.after"
    cmp "$WORK/import-paths.before" "$WORK/import-paths.after" || guard_status=1
    ((guard_status==0)) || { echo "[$LABEL] input generation drift" >&2; exit 1; }
    exit "$original_status"
}
trap finish_guards EXIT
NEGATIVES=(
    'field_ctor_own_formal_drop_negative|ArrayDropOwnedStrings|values'
    'field_ctor_clone_own_negative|own|values'
    'field_ctor_alias_own_negative|own|renamed'
    'field_ctor_map_keys_alias_own_negative|own|renamed'
    'field_ctor_member_rebind_drop_negative|ArrayDropOwnedStrings|values'
)
# The constructor stores `values` and the caller then drops or grows it, so
# the call takes `Clone(values)` (docs/truth/decisions/
# 2026-10-22_moved_argument_used_again_is_copied.md): the record and the local
# own independent storage. Under the 2026-10-23 decision that safe copy
# landings are positive cases, each is admitted with the Clone in MIR and runs
# on C and LLVM with the record's length, 1. The self-host analyzer does not
# land this copy yet and still refuses these inputs (collection_inout_effect).
COPIED=(field_ctor_clone_drop_negative field_ctor_empty_drop_negative
    field_ctor_alias_drop_negative field_ctor_owned_push_negative
    field_ctor_alias_owned_push_negative field_ctor_branch_fresh_assign_negative
    field_ctor_deferred_fresh_assign_negative)
POSITIVES=(field_ctor_readonly_positive field_ctor_duplicate_readonly_positive
    field_ctor_indexed_readonly_positive field_ctor_fresh_assign_positive
    own_named_clone_positive native_unescaped_string_storage_positive)
# These six are native type/transfer checks only, never target execution or
# runtime allocation/free evidence. Pergyra whole-import body remains separate.
for entry in "${NEGATIVES[@]}"; do
    IFS='|' read -r input operation receiver <<<"$entry"
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    if (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err"; then
        fail "accepted $input; rejected input was not executed"
    else
        result=$?
    fi
    [[ "$result" == 1 ]] || fail "non-semantic failure exit=$result for $input"
    [[ ! -s "$WORK/$input.mir.json" ]] || fail "published MIR for $input"
    if [[ "$operation" == own ]]; then
        expected="An own Array argument requires one current exclusive cleanup authority; '$receiver' is borrowed or aliased"
    elif [[ "$operation" == ArrayPushOwnedString ]]; then
        expected="$operation cannot mutate escaped array storage in '$receiver'"
    else
        expected="$operation cannot release escaped array storage in '$receiver'"
    fi
    grep -Fq "$expected" "$WORK/$input.err" || fail "wrong refusal for $input"
done
for input in "${COPIED[@]}"; do
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err" || fail "rejected copied $input"
    grep -Fq '"Clone"' "$WORK/$input.mir.json" || fail "no Clone in MIR for copied $input"
    for backend in c llvm; do
        (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline "--backend=$backend" \
            "$FIXTURES/$input.pgy" -o "$REL/$input-$backend.exe") \
            >"$WORK/$input-$backend.build.out" 2>&1 || fail "$backend build failed for copied $input"
        (cd "$ROOT"; run_admission_bounded "$WORK/$input-$backend.exe") \
            >"$WORK/$input-$backend.raw" 2>"$WORK/$input-$backend.run.err" || fail "$backend run failed for copied $input"
        [[ ! -s "$WORK/$input-$backend.run.err" ]] || fail "$backend runtime stderr for copied $input"
        [[ "$(tr -d '\r' <"$WORK/$input-$backend.raw")" == 1 ]] || fail "$backend output for copied $input"
    done
done
# A String MapGet returned points into the map's own value; MapSet, MapRemove,
# a hand-off or a reassignment of the map frees it (map_string_payload_view_owner).
# Each negative read freed memory under ASan before the owner refused it.
MAP_VIEW_NEGATIVES=(replace remove string_alias map_alias field_alias forwarded
    nested_argument unproved_formal array_escape assigned_formal_after
    assigned_formal_before loop_backedge)
MAP_VIEW_POSITIVES=(clone disjoint formal_read int key_copy read_before remove_key
    shadow unrelated_unknown)
for name in "${MAP_VIEW_NEGATIVES[@]}"; do
    input="map_payload_${name}_negative"
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    if (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err"; then
        fail "accepted map view $input; rejected input was not executed"
    fi
    [[ ! -s "$WORK/$input.mir.json" ]] || fail "published MIR for $input"
    grep -Eq "holds a String MapGet returned from a HashMap value that was replaced or removed|A String MapGet returned is passed while the same call changes its HashMap" \
        "$WORK/$input.err" || fail "wrong refusal for map view $input"
done
for name in "${MAP_VIEW_POSITIVES[@]}"; do
    input="map_payload_${name}_positive"
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err" || fail "rejected safe map view $input"
done
for input in "${POSITIVES[@]}"; do
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err" || fail "rejected safe $input"
    [[ -s "$WORK/$input.mir.json" ]] || fail "missing safe MIR for $input"
    grep -Fxq '0 error(s), 0 warning(s)' <(tr -d '\r' <"$WORK/$input.err") || fail "safe diagnostic drift for $input"
done
# No escaped-control program is executed. Only two independent owner controls
# run; readonly construction/fresh replacement above is admission-only evidence.
for input in own_named_clone_positive native_unescaped_string_storage_positive; do
    (cd "$ROOT"; run_admission_bounded "$PGY" --native-pipeline --emit-c "$FIXTURES/$input.pgy" -o "$REL/$input.c") \
        >"$WORK/$input.emit.out" 2>"$WORK/$input.emit.err" || fail "safe C emit failed for $input"
    command=("$CC" -std=c11 "$WORK/$input.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/$input.c"; then
        command+=("-I$ROOT/src" "-I$ROOT/src/runtime" -pthread)
    fi
    command+=(-o "$WORK/$input.exe")
    run_admission_bounded "${command[@]}" >"$WORK/$input.compile.out" 2>"$WORK/$input.compile.err" || fail "safe C compile failed for $input"
    (cd "$ROOT"; run_admission_bounded "$WORK/$input.exe") >"$WORK/$input.raw" 2>"$WORK/$input.run.err" || fail "safe C execution failed for $input"
    [[ ! -s "$WORK/$input.run.err" ]] || fail "runtime stderr for $input"
    tr -d '\r' <"$WORK/$input.raw" >"$WORK/$input.run"
    if [[ "$input" == own_named_clone_positive ]]; then printf 'named-clone-retired\n' >"$WORK/expected";
    else printf '1\nsafe-owned-storage\n' >"$WORK/expected"; fi
    cmp "$WORK/expected" "$WORK/$input.run" || fail "runtime output for $input"
done
(cd "$ROOT"; sha256sum --quiet -c "$WORK/native-sources.sha256")
sha256sum --quiet -c "$WORK/native-binary.sha256"
sha256sum --quiet -c "$WORK/inputs.sha256"
echo "[$LABEL] ${#NEGATIVES[@]} pre-MIR refusals, ${#COPIED[@]} copy landings run on C/LLVM, ${#MAP_VIEW_NEGATIVES[@]}+${#MAP_VIEW_POSITIVES[@]} map view refusals/admissions, ${#POSITIVES[@]} source admissions, two safe native-C executions PASS"
