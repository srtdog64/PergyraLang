#!/usr/bin/env bash
# ABI projection and removed List-only guess; execution is in hashmap_admission.sh.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here hashmap-runtime "$PGY"
cd "$ROOT_DIR"
"${PYTHON_BIN:-python3}" scripts/render_hashmap_key_abi.py --check
if grep -Fq 'func CollectionListRuntimeHashMapCValueType(' src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy; then
    echo '[hashmap-runtime] retired List-only HashMap ABI guess returned' >&2
    exit 1
fi
grep -Fq 'CollectionHashMapRuntimeFactFromTypeName(element_type)' src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy
grep -Fq 'CodegenHashMapCallProtocol(graph, call)' src/self_hosted/codegen/emission/expr_semantic_call_type_owner.pgy
grep -Fq 'collection_protocol.family == "HashMap"' src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-runtime.XXXXXX)"
echo "[hashmap-runtime] evidence: $WORK"
sha256sum "$PGY" tests/self_hosted/fixtures/hashmap_runtime_fact.pgy >"$WORK/inputs.sha256"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev tests/self_hosted/fixtures/hashmap_runtime_fact.pgy -o "$WORK/probe.exe" >"$WORK/compile.log" 2>&1
timeout 10 "$WORK/probe.exe" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<26; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[hashmap-runtime] 26 ABI/key/header/negative controls PASS'
sha256sum tests/self_hosted/fixtures/hashmap_normalized_fact.pgy >>"$WORK/inputs.sha256"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev tests/self_hosted/fixtures/hashmap_normalized_fact.pgy -o "$WORK/normalized.exe" >"$WORK/normalized.compile.log" 2>&1
timeout 10 "$WORK/normalized.exe" >"$WORK/normalized.raw" 2>"$WORK/normalized.err"
tr -d '\r' <"$WORK/normalized.raw" >"$WORK/normalized.actual"
for ((i=0; i<12; i++)); do printf 'true\n'; done >"$WORK/normalized.expected"
[[ ! -s "$WORK/normalized.err" ]]
cmp "$WORK/normalized.expected" "$WORK/normalized.actual"
echo '[hashmap-runtime] 12 normalized signature/identity/negative controls PASS'
