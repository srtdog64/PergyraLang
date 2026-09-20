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
if grep -Fq 'storage_kind_values = {"STRING"' scripts/render_hashmap_key_abi.py ||
    ! grep -Fq 'src/common/hashmap_key_storage_kind.h' scripts/render_hashmap_key_abi.py; then
    echo '[hashmap-runtime] self-host storage tags have an independent numeric authority' >&2
    exit 1
fi
for retired in pgy_map_format_i32_key pgy_map_i32_key_string_export \
    pgy_map_format_i64_key pgy_map_i64_key_string_export \
    pgy_map_format_bool_key pgy_map_bool_key_string_export; do
    if grep -R -Fq --include='*.h' --include='*.c' "$retired" src/runtime; then
        echo "[hashmap-runtime] retired Int string projection returned: $retired" >&2
        exit 1
    fi
done
for term in \
    'pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL)' \
    'PGY_MAP_RAW_BOOL_KEYS(map)[h] == key' \
    'pgy_map_keys_raw_bool_export'; do
    grep -R -Fq "$term" src/runtime || {
        echo "[hashmap-runtime] Bool storage owner term missing: $term" >&2
        exit 1
    }
done
for term in \
    'pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64)' \
    'PGY_MAP_RAW_I64_KEYS(map)[h] == key' \
    'pgy_map_keys_raw_i64_export'; do
    grep -R -Fq "$term" src/runtime || {
        echo "[hashmap-runtime] Long storage owner term missing: $term" >&2
        exit 1
    }
done
for term in \
    'pgy_map_new_raw_export(void *map_ptr, int64_t value_size,' \
    'pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32)' \
    'PGY_MAP_RAW_I32_KEYS(map)[h] == key' \
    'map->deleted_count++'; do
    grep -R -Fq "$term" src/runtime || {
        echo "[hashmap-runtime] Int storage owner term missing: $term" >&2
        exit 1
    }
done
if grep -Fq 'func CollectionListRuntimeHashMapCValueType(' src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy; then
    echo '[hashmap-runtime] retired List-only HashMap ABI guess returned' >&2
    exit 1
fi
grep -Fq 'CollectionHashMapRuntimeFactFromTypeName(element_type)' src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy
grep -Fq 'type_name == CompilerAbiLayoutArrayLongTypeName()' src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy
grep -Fq 'left_type == CompilerAbiLayoutArrayLongTypeName()' src/self_hosted/compiler/direct_mir_scalar_program_logical_record_expression_readiness_owner.pgy
grep -Fq 'CodegenHashMapCallProtocol(graph, call)' src/self_hosted/codegen/emission/expr_semantic_call_type_owner.pgy
grep -Fq 'collection_protocol.family == "HashMap"' src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy
grep -Fq 'pgy_map_drop_raw_export' src/codegen/llvm_runtime_raw_collections.c
grep -Fq 'pgy_map_drop_string_value_raw_export' src/codegen/llvm_runtime_raw_collections.c
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-runtime.XXXXXX)"
echo "[hashmap-runtime] evidence: $WORK"
sha256sum "$PGY" tests/self_hosted/fixtures/hashmap_runtime_fact.pgy >"$WORK/inputs.sha256"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev tests/self_hosted/fixtures/hashmap_runtime_fact.pgy -o "$WORK/probe.exe" >"$WORK/compile.log" 2>&1
timeout 10 "$WORK/probe.exe" >"$WORK/raw" 2>"$WORK/err"
tr -d '\r' <"$WORK/raw" >"$WORK/actual"
for ((i=0; i<36; i++)); do printf 'true\n'; done >"$WORK/expected"
[[ ! -s "$WORK/err" ]]
cmp "$WORK/expected" "$WORK/actual"
echo '[hashmap-runtime] 36 ABI/key/storage/release/header/negative controls PASS'
sha256sum tests/self_hosted/fixtures/hashmap_normalized_fact.pgy >>"$WORK/inputs.sha256"
timeout 240 "$PGY" --native-pipeline --backend=c --opt=dev tests/self_hosted/fixtures/hashmap_normalized_fact.pgy -o "$WORK/normalized.exe" >"$WORK/normalized.compile.log" 2>&1
timeout 10 "$WORK/normalized.exe" >"$WORK/normalized.raw" 2>"$WORK/normalized.err"
tr -d '\r' <"$WORK/normalized.raw" >"$WORK/normalized.actual"
for ((i=0; i<12; i++)); do printf 'true\n'; done >"$WORK/normalized.expected"
[[ ! -s "$WORK/normalized.err" ]]
cmp "$WORK/normalized.expected" "$WORK/normalized.actual"
echo '[hashmap-runtime] 12 normalized signature/identity/negative controls PASS'
