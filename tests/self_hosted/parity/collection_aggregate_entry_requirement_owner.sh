#!/usr/bin/env bash
# Conditional field-entry facts only; no Release/MIR/runtime permission claim.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=collection-aggregate-entry-requirement
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/aggregate-entry-units.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/collection_aggregate_entry_requirement_probe.pgy
INPUT=tests/self_hosted/parity/fixture/collection_field_lifetime/callable_table_owned_release_positive.pgy
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$PROBE" "$INPUT" >"$WORK/inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
echo "[$LABEL] evidence=$REL; input source is parsed, never emitted/run"
printf 'true\n' >"$WORK/expected"
for backend in c llvm; do
    timeout 240 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-entry.exe" >"$WORK/$backend-entry.compile" 2>&1 || fail "$backend fact probe did not build"
    for mutation in $(seq 0 34); do
        timeout 30 "$WORK/$backend-entry.exe" "$INPUT" "$mutation" \
            >"$WORK/$backend-$mutation.raw" 2>"$WORK/$backend-$mutation.err" || fail "$backend mutation=$mutation refused"
        [[ ! -s "$WORK/$backend-$mutation.err" ]] || fail "$backend mutation=$mutation stderr"
        tr -d '\r' <"$WORK/$backend-$mutation.raw" >"$WORK/$backend-$mutation.run"
        cmp "$WORK/expected" "$WORK/$backend-$mutation.run" || fail "$backend mutation=$mutation drifted"
    done
    for value in -1 35 invalid 01; do
        if "$WORK/$backend-entry.exe" "$INPUT" "$value" >"$WORK/$backend-cli-$value.out" 2>"$WORK/$backend-cli-$value.err"; then
            fail "$backend accepted invalid mutation=$value"
        fi
        [[ ! -s "$WORK/$backend-cli-$value.err" ]] || fail "$backend invalid CLI stderr"
        tr -d '\r' <"$WORK/$backend-cli-$value.out" >"$WORK/$backend-cli-$value.run"
        grep -Fxq 'invalid entry mutation' "$WORK/$backend-cli-$value.run" || fail "$backend invalid CLI diagnostic"
    done
done
for mutation in $(seq 0 34); do cmp "$WORK/c-$mutation.run" "$WORK/llvm-$mutation.run"; done
for manifest in native inputs imports; do sha256sum --quiet -c "$WORK/$manifest.sha256"; done
sha256sum "$WORK/c-entry.exe" "$WORK/llvm-entry.exe" >"$WORK/binaries.sha256"
echo "[$LABEL] 35 fact units + 4 invalid CLI cases per C/LLVM PASS; actual aggregate lifetime remains unproved"
