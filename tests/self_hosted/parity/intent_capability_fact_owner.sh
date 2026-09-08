#!/usr/bin/env bash
# Native-C execution of purpose identity and capability-fact mutations.
# Reads the common body's exact action/nested-purpose masks and mutates
# wrong/missing callable rows in the executed probe, not a source inventory.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here intent-capability-fact "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/intent-capability-fact.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
echo "[intent-capability-fact] evidence: $REL"
sha256sum "$PGY" tests/self_hosted/fixtures/intent_capability_fact.pgy \
    tests/concept_semantics/intent/capability_fixture.pgy \
    tests/concept_semantics/intent/capability_nested_valid.pgy >"$WORK/inputs.sha256"
timeout 30 "$PGY" --native-pipeline --ast tests/concept_semantics/intent/capability_nested_valid.pgy \
    >"$WORK/source.ast" 2>"$WORK/source.err"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/fixtures/intent_capability_fact.pgy -o "$REL/probe.exe" \
    >"$WORK/compile.log" 2>&1
timeout 30 "$WORK/probe.exe" "$REL/source.ast" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<10; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[intent-capability-fact] 10 native-C identity/missing-row controls PASS'
