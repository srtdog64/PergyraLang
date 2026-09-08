#!/usr/bin/env bash
# Bounded effect facts on the shared call graph; no invalid source is executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here effect-call-fact "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/effect-call-fact.XXXXXX)"
echo "[effect-call-fact] evidence: $WORK"
sha256sum "$PGY" tests/self_hosted/fixtures/effect_call_fact.pgy \
    tests/concept_semantics/authority_effect/effect_callable_isolated_valid.pgy >"$WORK/inputs.sha256"
timeout 30 "$PGY" --native-pipeline --ast \
    tests/concept_semantics/authority_effect/effect_callable_isolated_valid.pgy >"$WORK/control.ast" 2>"$WORK/control.err"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/fixtures/effect_call_fact.pgy -o "$WORK/probe.exe" >"$WORK/compile.log" 2>&1
timeout 30 "$WORK/probe.exe" "$WORK/control.ast" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<17; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[effect-call-fact] 17 effect/isolation/unknown/missing-row controls PASS'
