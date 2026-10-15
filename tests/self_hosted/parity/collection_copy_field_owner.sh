#!/usr/bin/env bash
# Native analyzers inspect unsafe supplied sources without executing them.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-copy-field "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/collection-copy-field.XXXXXX)"
trap 'status=$?; echo "[collection-copy-field] failed at $LINENO status=$status evidence=$WORK" >&2' ERR
SOURCE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
UNIT=tests/self_hosted/fixtures/collection_copy_field_identity_unit.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
inputs=(
    copy_field_disjoint_holder_positive.pgy
    copy_field_disjoint_sibling_positive.pgy
    copy_field_overlapping_holder_negative.pgy
    copy_field_overlapping_sibling_negative.pgy
    copy_field_same_source_two_landings_negative.pgy
    copy_field_loop_holder_negative.pgy
    copy_field_deferred_holder_negative.pgy
    copy_field_self_root_negative.pgy
    copy_field_whole_holder_negative.pgy
    copy_field_all_shared_negative.pgy
    copy_field_unshared_source_later_read_negative.pgy
    copy_field_unshared_source_metadata_positive.pgy
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
    for mode in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28; do
        timeout 30 "$WORK/$backend-unit.exe" "$FIXTURES/copy_field_disjoint_holder_positive.pgy" "$mode" \
            >"$WORK/$backend-unit-$mode.raw" 2>"$WORK/$backend-unit-$mode.err"
        tr -d '\r' <"$WORK/$backend-unit-$mode.raw" >"$WORK/$backend-unit-$mode.run"
        [[ ! -s "$WORK/$backend-unit-$mode.err" ]]
        [[ "$(cat "$WORK/$backend-unit-$mode.run")" == true ]]
    done
done
sha256sum --quiet -c "$WORK/native.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
sha256sum --quiet -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/binaries.sha256"
cp "$WORK/imports.sha256" "$WORK/source.sha256"
sha256sum "$ROOT_DIR/$WORK/c-source.exe" "$ROOT_DIR/$WORK/llvm-source.exe" >"$WORK/probes.sha256"
sha256sum --quiet -c "$WORK/source.sha256"
sha256sum --quiet -c "$WORK/probes.sha256"
if [[ -n "${SOURCE_PROBE_DIR:-}" ]]; then
    for manifest in native source inputs probes; do
        sha256sum --quiet -c "$SOURCE_PROBE_DIR/$manifest.sha256"
    done
fi
echo "[collection-copy-field] PASS C/LLVM: 12 source refusals/admissions and 29 copy-field identity checks; $WORK"
