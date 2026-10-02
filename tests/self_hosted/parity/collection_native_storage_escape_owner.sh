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
# Freeze every native source/header, including the owner/flow-state ABI.
(cd "$ROOT"; git ls-files -z -- src Makefile | while IFS= read -r -d '' path; do
    case "$path" in *.c|*.h|Makefile) sha256sum "$path" ;; esac
done) >"$WORK/native-sources.sha256"
sha256sum "$PGY" >"$WORK/native-binary.sha256"
NEGATIVES=(
    'field_ctor_clone_drop_negative|ArrayDropOwnedStrings|values'
    'field_ctor_empty_drop_negative|ArrayDropOwnedStrings|values'
    'field_ctor_alias_drop_negative|ArrayDropOwnedStrings|renamed'
    'field_ctor_own_formal_drop_negative|ArrayDropOwnedStrings|values'
    'field_ctor_owned_push_negative|ArrayPushOwnedString|values'
    'field_ctor_alias_owned_push_negative|ArrayPushOwnedString|renamed'
    'field_ctor_clone_own_negative|own|values'
    'field_ctor_alias_own_negative|own|renamed'
    'field_ctor_map_keys_alias_own_negative|own|renamed'
    'field_ctor_branch_fresh_assign_negative|ArrayDropOwnedStrings|values'
    'field_ctor_deferred_fresh_assign_negative|ArrayDropOwnedStrings|values'
    'field_ctor_member_rebind_drop_negative|ArrayDropOwnedStrings|values'
)
POSITIVES=(field_ctor_readonly_positive field_ctor_duplicate_readonly_positive
    field_ctor_indexed_readonly_positive field_ctor_fresh_assign_positive
    own_named_clone_positive native_unescaped_string_storage_positive)
for entry in "${NEGATIVES[@]}"; do
    IFS='|' read -r input operation receiver <<<"$entry"
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    if (cd "$ROOT"; timeout 30 "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err"; then
        fail "accepted $input; rejected input was not executed"
    else
        result=$?
    fi
    [[ "$result" == 1 ]] || fail "non-semantic failure exit=$result for $input"
    [[ ! -s "$WORK/$input.mir.json" ]] || fail "published MIR for $input"
    if [[ "$operation" == own ]]; then
        expected="An own Array argument cannot transfer escaped array storage from '$receiver'"
    elif [[ "$operation" == ArrayPushOwnedString ]]; then
        expected="$operation cannot mutate escaped array storage in '$receiver'"
    else
        expected="$operation cannot release escaped array storage in '$receiver'"
    fi
    grep -Fq "$expected" "$WORK/$input.err" || fail "wrong refusal for $input"
done
for input in "${POSITIVES[@]}"; do
    sha256sum "$ROOT/$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"
    (cd "$ROOT"; timeout 30 "$PGY" --native-pipeline --mir-json "$FIXTURES/$input.pgy") \
        >"$WORK/$input.mir.json" 2>"$WORK/$input.err" || fail "rejected safe $input"
    [[ -s "$WORK/$input.mir.json" ]] || fail "missing safe MIR for $input"
    grep -Fxq '0 error(s), 0 warning(s)' <(tr -d '\r' <"$WORK/$input.err") || fail "safe diagnostic drift for $input"
done
# No escaped-control program is executed. Only two independent owner controls
# run; readonly construction/fresh replacement above is admission-only evidence.
for input in own_named_clone_positive native_unescaped_string_storage_positive; do
    (cd "$ROOT"; timeout 30 "$PGY" --native-pipeline --emit-c "$FIXTURES/$input.pgy" -o "$REL/$input.c") \
        >"$WORK/$input.emit.out" 2>"$WORK/$input.emit.err" || fail "safe C emit failed for $input"
    command=("$CC" -std=c11 "$WORK/$input.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/$input.c"; then
        command+=("-I$ROOT/src" "-I$ROOT/src/runtime" -pthread)
    fi
    command+=(-o "$WORK/$input.exe")
    "${command[@]}" >"$WORK/$input.compile.out" 2>"$WORK/$input.compile.err" || fail "safe C compile failed for $input"
    (cd "$ROOT"; timeout 30 "$WORK/$input.exe") >"$WORK/$input.raw" 2>"$WORK/$input.run.err" || fail "safe C execution failed for $input"
    [[ ! -s "$WORK/$input.run.err" ]] || fail "runtime stderr for $input"
    tr -d '\r' <"$WORK/$input.raw" >"$WORK/$input.run"
    if [[ "$input" == own_named_clone_positive ]]; then printf 'named-clone-retired\n' >"$WORK/expected";
    else printf '1\nsafe-owned-storage\n' >"$WORK/expected"; fi
    cmp "$WORK/expected" "$WORK/$input.run" || fail "runtime output for $input"
done
(cd "$ROOT"; sha256sum --quiet -c "$WORK/native-sources.sha256")
sha256sum --quiet -c "$WORK/native-binary.sha256"
sha256sum --quiet -c "$WORK/inputs.sha256"
echo "[$LABEL] ${#NEGATIVES[@]} pre-MIR refusals, ${#POSITIVES[@]} source admissions, two safe native-C executions PASS"
