#!/usr/bin/env bash
# Build analyzers only; supplied ownership falsifiers are never emitted/run.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-map-payload "$PGY"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/map-payload.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
INPUTS=(
    map_payload_replace_negative.pgy map_payload_remove_negative.pgy
    map_payload_read_before_positive.pgy map_payload_disjoint_positive.pgy
    map_payload_clone_positive.pgy map_payload_int_positive.pgy
    map_payload_string_alias_negative.pgy map_payload_map_alias_negative.pgy
    map_payload_field_alias_negative.pgy map_payload_shadow_positive.pgy
    map_payload_loop_backedge_negative.pgy map_payload_forwarded_negative.pgy
    map_payload_nested_argument_negative.pgy map_payload_unproved_formal_negative.pgy
    map_payload_unrelated_unknown_positive.pgy
    map_payload_clone_string_negative.pgy map_payload_array_escape_negative.pgy
    map_payload_formal_read_positive.pgy
    map_payload_key_copy_positive.pgy map_payload_remove_key_positive.pgy
    map_payload_value_copy_unsupported_negative.pgy
    map_payload_assigned_formal_before_negative.pgy map_payload_assigned_formal_after_negative.pgy
    indexed_string_alias_after_owned_drop_negative.pgy
    indexed_string_copy_before_owned_drop_positive.pgy
    indexed_string_tostring_passthrough_after_owned_drop_negative.pgy
)
sha256sum "$PGY" >"$WORK/native.sha256"
find src/self_hosted -type f -name '*.pgy' -print0 | sort -z | xargs -0 sha256sum >"$WORK/source.sha256"
{ sha256sum "$PROBE" "${BASH_SOURCE[0]}"; for input in "${INPUTS[@]}"; do sha256sum "$FIXTURES/$input"; done; } >"$WORK/inputs.sha256"
SOURCE_PAIR_DIR="${SOURCE_PROBE_DIR:-}"
validate_source_pair_receipt() {
    local manifest backend
    for manifest in native source inputs probes; do
        [[ -s "$SOURCE_PAIR_DIR/$manifest.sha256" ]] || {
            echo "source-only reuse is missing $manifest receipt" >&2; return 1;
        }
        sha256sum --quiet -c "$SOURCE_PAIR_DIR/$manifest.sha256"
    done
    cmp "$SOURCE_PAIR_DIR/source.sha256" "$WORK/source.sha256"
    [[ "$(cut -d ' ' -f 1 "$SOURCE_PAIR_DIR/native.sha256")" == "$(cut -d ' ' -f 1 "$WORK/native.sha256")" ]]
    grep -Fxq "$(sha256sum "$PROBE")" "$SOURCE_PAIR_DIR/inputs.sha256"
    for backend in c llvm; do
        [[ -s "$SOURCE_PAIR_DIR/$backend-source.exe" ]] || {
            echo "source-only reuse is missing $backend source binary" >&2; return 1;
        }
        grep -Fxq "$(sha256sum "$SOURCE_PAIR_DIR/$backend-source.exe")" "$SOURCE_PAIR_DIR/probes.sha256"
    done
}
if [[ -n "$SOURCE_PAIR_DIR" ]]; then
    SOURCE_PAIR_DIR="$(cd "$SOURCE_PAIR_DIR" && pwd -P)"
    validate_source_pair_receipt
    sha256sum "$SOURCE_PAIR_DIR/native.sha256" "$SOURCE_PAIR_DIR/source.sha256" \
        "$SOURCE_PAIR_DIR/inputs.sha256" "$SOURCE_PAIR_DIR/probes.sha256" >"$WORK/source-reuse-receipts.sha256"
fi
for backend in ${BACKENDS:-c llvm}; do
    if [[ -n "$SOURCE_PAIR_DIR" ]]; then
        cp "$SOURCE_PAIR_DIR/$backend-source.exe" "$WORK/$backend-source.exe"
        cmp "$SOURCE_PAIR_DIR/$backend-source.exe" "$WORK/$backend-source.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev \
            -o "$REL/$backend-source.exe" >"$WORK/$backend-source.compile" 2>&1
    fi
    sha256sum "$WORK/$backend-source.exe" >>"$WORK/probes.sha256"
    for input in "${INPUTS[@]}"; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input.raw" 2>"$WORK/$backend-$input.err"
        tr -d '\r' <"$WORK/$backend-$input.raw" >"$WORK/$backend-$input.run"
        [[ ! -s "$WORK/$backend-$input.err" ]]
        expected=true
        if [[ "$input" == *_negative.pgy ]]; then expected=false; fi
        grep -Fxq "body_ok=$expected" "$WORK/$backend-$input.run"
        if [[ "$input" == map_payload_*_negative.pgy ]]; then
            grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$input.run"
            grep -Eq '^- boundary: (map_string_|unproved_exclusive)' "$WORK/$backend-$input.run"
        fi
        if [[ "$input" == map_payload_assigned_formal_before_negative.pgy ]]; then
            grep -Fxq 'formal_ready=true' "$WORK/$backend-$input.run"
            grep -Fxq -- '- boundary: map_string_use_after_mutation' "$WORK/$backend-$input.run"
        fi
    done
done
sha256sum -c "$WORK/native.sha256" >"$WORK/native-recheck.log"
sha256sum -c "$WORK/source.sha256" >"$WORK/source-recheck.log"
sha256sum -c "$WORK/inputs.sha256" >"$WORK/input-recheck.log"
sha256sum -c "$WORK/probes.sha256" >"$WORK/probe-recheck.log"
if [[ -n "$SOURCE_PAIR_DIR" ]]; then
    sha256sum --quiet -c "$WORK/source-reuse-receipts.sha256"
    validate_source_pair_receipt
fi
echo "[collection-map-payload] PASS: ${#INPUTS[@]} analyze-only sources per backend; $REL"
