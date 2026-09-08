#!/usr/bin/env bash
# Native-built execution of the Pergyra fact owner, not bootstrap/substitution.
# Public source/runtime integration is tests/concept_semantics/field_write_admission.sh.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here nominal-field-write-fact "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/nominal-field-write-fact.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
for ((i=0; i<13; i++)); do printf 'true\n'; done >"$WORK/expected"
for backend in c llvm; do
    timeout 60 "$PGY" --native-pipeline tests/self_hosted/fixtures/nominal_field_write_fact.pgy \
        "--backend=$backend" --opt=dev -o "$REL/$backend.exe" >"$WORK/$backend.compile" 2>&1
    timeout 30 "$WORK/$backend.exe" >"$WORK/$backend.raw" 2>"$WORK/$backend.err"
    tr -d '\r' <"$WORK/$backend.raw" >"$WORK/$backend.run"
    [[ ! -s "$WORK/$backend.err" ]]
    cmp "$WORK/expected" "$WORK/$backend.run"
    echo "[nominal-field-write-fact] native-$backend: 13 provenance/missing-row/diagnostic checks PASS"
done
echo "[nominal-field-write-fact] evidence: $REL"
