#!/usr/bin/env bash
# Native-C fact witness; public source admission has its own paired gate.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here callable-capability-fact "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/callable-capability-fact.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
echo "[callable-capability-fact] evidence: $REL"
sha256sum "$PGY" tests/self_hosted/fixtures/callable_capability_fact.pgy \
    tests/concept_semantics/authority_effect/callable_capability_{fixture,isolated_valid}.pgy >"$WORK/inputs.sha256"
timeout 30 "$PGY" --native-pipeline --ast \
    tests/concept_semantics/authority_effect/callable_capability_isolated_valid.pgy >"$WORK/control.ast" 2>"$WORK/control.err"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/fixtures/callable_capability_fact.pgy -o "$REL/probe.exe" >"$WORK/compile.log" 2>&1
timeout 30 "$WORK/probe.exe" "$REL/control.ast" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<10; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[callable-capability-fact] 10 native-C substitution/identity/deferred-use controls PASS'
