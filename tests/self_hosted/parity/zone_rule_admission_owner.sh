#!/usr/bin/env bash
# The executed program is a validator over synthetic facts, not invalid source.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here zone-rule-facts "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/zone-rule-facts.XXXXXX)"
echo "[zone-rule-facts] evidence: $WORK"
SOURCE=tests/self_hosted/fixtures/zone_rule_admission_fact.pgy
sha256sum "$PGY" "$SOURCE" >"$WORK/inputs.sha256"
timeout 120 "$PGY" --native-pipeline --backend=c --opt=dev "$SOURCE" -o "$WORK/probe.exe" >"$WORK/compile.log" 2>&1
timeout 10 "$WORK/probe.exe" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<6; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[zone-rule-facts] 6 typed authority/layer controls PASS'
