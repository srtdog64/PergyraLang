#!/usr/bin/env bash
# Native analyzers inspect unsafe supplied sources without executing them.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/semantic_owner_probe_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-copy-field "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/collection-copy-field.XXXXXX)"
trap 'status=$?; echo "[collection-copy-field] failed at $LINENO status=$status evidence=$WORK" >&2' ERR
SOURCE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
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
sha256sum "$SOURCE" "${BASH_SOURCE[0]}" "${inputs[@]/#/$FIXTURES/}" >"$WORK/inputs.sha256"
for backend in c llvm; do
    pgy_semantic_owner_probe_install "$ROOT_DIR" "$PGY" "$backend" \
        nominal-constructor-source-arity "$WORK/$backend-source.exe"
    sha256sum "$WORK/$backend-source.exe" >>"$WORK/binaries.sha256"
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
done
sha256sum --quiet -c "$WORK/native.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
sha256sum --quiet -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/binaries.sha256"
cp "$WORK/imports.sha256" "$WORK/source.sha256"
sha256sum "$ROOT_DIR/$WORK/c-source.exe" "$ROOT_DIR/$WORK/llvm-source.exe" >"$WORK/probes.sha256"
sha256sum --quiet -c "$WORK/source.sha256"
sha256sum --quiet -c "$WORK/probes.sha256"
echo "[collection-copy-field] PASS C/LLVM: 12 source refusals/admissions; $WORK"
