#!/usr/bin/env bash
# Native analyzers inspect unsafe supplied sources without executing them.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-descriptor-effect "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/collection-descriptor-effect.XXXXXX)"
trap 'status=$?; echo "[collection-descriptor-effect] failed at $LINENO status=$status evidence=$WORK" >&2' ERR
SOURCE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
UNIT=tests/self_hosted/fixtures/collection_descriptor_effect_identity_unit.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
inputs=(
    descriptor_text_escape_read_positive.pgy
    descriptor_text_escape_loop_positive.pgy
    descriptor_text_escape_forward_positive.pgy
    descriptor_text_escape_recursive_positive.pgy
    descriptor_text_escape_mixed_negative.pgy
    descriptor_text_escape_forward_mixed_negative.pgy
    descriptor_text_escape_owned_drop_negative.pgy
    descriptor_text_escape_opaque_negative.pgy
    owned_element_text_escapes_result_negative.pgy
    inout_element_escape_drop_negative.pgy
    formal_read_cycle_escape_negative.pgy
    inout_index_after_drop_negative.pgy
    inout_index_string_lambda_negative.pgy
    inout_index_string_async_negative.pgy
)
sha256sum "$PGY" >"$WORK/native.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
sha256sum "$SOURCE" "$UNIT" "${BASH_SOURCE[0]}" "${inputs[@]/#/$FIXTURES/}" >"$WORK/inputs.sha256"
if [[ -n "${SOURCE_PROBE_DIR:-}" ]]; then
    SOURCE_PROBE_DIR="$(cd "$SOURCE_PROBE_DIR" && pwd -P)"
    for manifest in native source inputs probes; do
        [[ -s "$SOURCE_PROBE_DIR/$manifest.sha256" ]]
        sha256sum --quiet -c "$SOURCE_PROBE_DIR/$manifest.sha256"
    done
    cmp "$SOURCE_PROBE_DIR/source.sha256" "$WORK/imports.sha256"
    [[ "$(cut -d ' ' -f 1 "$SOURCE_PROBE_DIR/native.sha256")" == "$(cut -d ' ' -f 1 "$WORK/native.sha256")" ]]
    grep -Fxq "$(sha256sum "$SOURCE")" "$SOURCE_PROBE_DIR/inputs.sha256"
    for backend in c llvm; do
        [[ -s "$SOURCE_PROBE_DIR/$backend-source.exe" ]]
        grep -Fxq "$(sha256sum "$SOURCE_PROBE_DIR/$backend-source.exe")" "$SOURCE_PROBE_DIR/probes.sha256"
    done
fi
for backend in c llvm; do
    if [[ -n "${SOURCE_PROBE_DIR:-}" ]]; then
        cp "$SOURCE_PROBE_DIR/$backend-source.exe" "$WORK/$backend-source.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$SOURCE" "--backend=$backend" --opt=dev \
            -o "$WORK/$backend-source.exe" >"$WORK/$backend-source.compile" 2>&1
    fi
    timeout 120 "$PGY" --native-pipeline "$UNIT" "--backend=$backend" --opt=dev \
        -o "$WORK/$backend-unit.exe" >"$WORK/$backend-unit.compile" 2>&1
    sha256sum "$WORK/$backend-source.exe" "$WORK/$backend-unit.exe" >>"$WORK/binaries.sha256"
    for input in "${inputs[@]}"; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input.raw" 2>"$WORK/$backend-$input.err"
        tr -d '\r' <"$WORK/$backend-$input.raw" >"$WORK/$backend-$input.run"
        [[ ! -s "$WORK/$backend-$input.err" ]]
        expected=false
        if [[ "$input" == *_positive.pgy ]]; then expected=true; fi
        grep -Fxq "body_ok=$expected" "$WORK/$backend-$input.run"
        if [[ "$expected" == false ]]; then
            grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$input.run"
        fi
    done
    for mode in 0 1 2 3 4 5 6 7; do
        timeout 30 "$WORK/$backend-unit.exe" "$FIXTURES/descriptor_text_escape_read_positive.pgy" "$mode" \
            >"$WORK/$backend-unit-$mode.raw" 2>"$WORK/$backend-unit-$mode.err"
        tr -d '\r' <"$WORK/$backend-unit-$mode.raw" >"$WORK/$backend-unit-$mode.run"
        [[ ! -s "$WORK/$backend-unit-$mode.err" ]]
        [[ "$(cat "$WORK/$backend-unit-$mode.run")" == true ]]
    done
    for input in descriptor_text_escape_loop_positive.pgy descriptor_text_escape_forward_positive.pgy \
        descriptor_text_escape_recursive_positive.pgy; do
        timeout 30 "$WORK/$backend-unit.exe" "$FIXTURES/$input" 0 \
            >"$WORK/$backend-$input-facets.raw" 2>"$WORK/$backend-$input-facets.err"
        tr -d '\r' <"$WORK/$backend-$input-facets.raw" >"$WORK/$backend-$input-facets.run"
        [[ ! -s "$WORK/$backend-$input-facets.err" ]]
        [[ "$(cat "$WORK/$backend-$input-facets.run")" == true ]]
    done
done
sha256sum --quiet -c "$WORK/native.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
sha256sum --quiet -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/binaries.sha256"
if [[ -n "${SOURCE_PROBE_DIR:-}" ]]; then
    for manifest in native source inputs probes; do
        sha256sum --quiet -c "$SOURCE_PROBE_DIR/$manifest.sha256"
    done
fi
echo "[collection-descriptor-effect] PASS C/LLVM: 14 source refusals/admissions, 8 identity and 3 forwarding facet checks; $WORK"
