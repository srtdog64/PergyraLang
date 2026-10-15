#!/usr/bin/env bash
# Supplied sources are analyzed only; no unsafe input is emitted or executed.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here member-completed-read "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/member-completed-read.XXXXXX)"
trap 'status=$?; echo "[member-completed-read] failed line=$LINENO status=$status evidence=$WORK" >&2' ERR
SOURCE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
UNIT=tests/self_hosted/fixtures/collection_member_completed_read_identity_unit.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
inputs=(member_completed_read_before_alias_positive member_completed_read_shared_backing_positive
    member_completed_read_literal_text_positive
    member_completed_read_after_alias_negative member_completed_read_same_statement_negative
    member_completed_read_loop_negative member_completed_read_defer_negative
    member_completed_read_retaining_negative member_completed_read_borrowed_text_negative
    member_completed_read_async_negative)
sha256sum "$PGY" >"$WORK/native.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
sha256sum "$SOURCE" "$UNIT" "${BASH_SOURCE[0]}" >"$WORK/inputs.sha256"
for input in "${inputs[@]}"; do sha256sum "$FIXTURES/$input.pgy" >>"$WORK/inputs.sha256"; done
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
        grep -Fxq "$(sha256sum "$SOURCE_PROBE_DIR/$backend-source.exe")" "$SOURCE_PROBE_DIR/probes.sha256"
    done
fi
for backend in c llvm; do
    if [[ -n "${SOURCE_PROBE_DIR:-}" ]]; then
        cp "$SOURCE_PROBE_DIR/$backend-source.exe" "$WORK/$backend-source.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$SOURCE" "--backend=$backend" --opt=dev -o "$WORK/$backend-source.exe" >"$WORK/$backend-source.compile" 2>&1
    fi
    timeout 120 "$PGY" --native-pipeline "$UNIT" "--backend=$backend" --opt=dev -o "$WORK/$backend-unit.exe" >"$WORK/$backend-unit.compile" 2>&1
    sha256sum "$WORK/$backend-source.exe" "$WORK/$backend-unit.exe" >>"$WORK/binaries.sha256"
    for input in "${inputs[@]}"; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input.pgy" diagnostic >"$WORK/$backend-$input.raw" 2>"$WORK/$backend-$input.err"
        tr -d '\r' <"$WORK/$backend-$input.raw" >"$WORK/$backend-$input.run"
        [[ ! -s "$WORK/$backend-$input.err" ]]
        expected=false; if [[ "$input" == *_positive ]]; then expected=true; fi
        grep -Fxq "body_ok=$expected" "$WORK/$backend-$input.run"
    done
    for mode in {0..30}; do
        timeout 30 "$WORK/$backend-unit.exe" "$FIXTURES/member_completed_read_before_alias_positive.pgy" "$mode" >"$WORK/$backend-unit-$mode.raw" 2>"$WORK/$backend-unit-$mode.err"
        tr -d '\r' <"$WORK/$backend-unit-$mode.raw" >"$WORK/$backend-unit-$mode.run"
        [[ ! -s "$WORK/$backend-unit-$mode.err" ]]
        [[ "$(cat "$WORK/$backend-unit-$mode.run")" == true ]]
    done
done
for manifest in native imports inputs binaries; do sha256sum --quiet -c "$WORK/$manifest.sha256"; done
if [[ -n "${SOURCE_PROBE_DIR:-}" ]]; then
    for manifest in native source inputs probes; do sha256sum --quiet -c "$SOURCE_PROBE_DIR/$manifest.sha256"; done
fi
echo "[member-completed-read] PASS C/LLVM: 10 sources and 31 direct controls; $WORK"
