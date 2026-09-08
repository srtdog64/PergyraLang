#!/usr/bin/env bash
# Native-C execution of the Pergyra fact mutation witness. Production self-host
# admission/runtime parity is separately owned by capability_admission.sh.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here capability-fact "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/capability-fact.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
sha256sum "$PGY" tests/self_hosted/fixtures/capability_admission_fact.pgy >"$WORK/inputs.sha256"
echo "[capability-fact] evidence: $REL"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/fixtures/capability_admission_fact.pgy -o "$REL/probe.exe" \
    >"$WORK/compile.log" 2>&1
timeout 30 "$WORK/probe.exe" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<11; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[capability-fact] 11 native-C mutation/identity/diagnostic controls PASS'
