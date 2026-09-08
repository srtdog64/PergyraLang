#!/usr/bin/env bash
# Executable specialization of protocol rows; not backend-availability evidence.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here hashmap-signature "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-signature.XXXXXX)"
echo "[hashmap-signature] evidence: $WORK"
"${PYTHON_BIN:-python3}" scripts/render_collection_key_policy.py --check
sha256sum "$PGY" tests/self_hosted/fixtures/hashmap_signature_fact.pgy >"$WORK/inputs.sha256"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/fixtures/hashmap_signature_fact.pgy -o "$WORK/probe.exe" >"$WORK/compile.log" 2>&1
timeout 10 "$WORK/probe.exe" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<18; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[hashmap-signature] 18 contextual/protocol/key-policy/negative controls PASS'
