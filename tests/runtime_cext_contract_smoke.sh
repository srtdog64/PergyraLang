#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[runtime-cext-contract] $1" >&2
    exit 1
}

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" \
        || fail "$rel lost required term: $term"
}

reject_term() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel regained forbidden term: $term"
    fi
}

reject_term_after_marker() {
    local rel="$1"
    local marker="$2"
    local term="$3"
    if awk -v marker="$marker" 'index($0, marker) { seen=1 } seen { print }' \
        "$ROOT_DIR/$rel" | grep -Fq -- "$term"; then
        fail "$rel regained forbidden term after $marker: $term"
    fi
}

require_program_local_family() {
    local rel="$1"
    local define_term="$2"
    require_term "$rel" "$define_term"
    require_term "$rel" "PGY_RT_PROGRAM_DECL"
    require_term "$rel" "PGY_RT_PROGRAM_BODY"
}

require_term "src/runtime/pgy_runtime_linkage.h" \
    "#define PGY_RT_PROGRAM_DECL static inline"
require_term "src/runtime/pgy_runtime_linkage.h" \
    "#define PGY_RT_PROGRAM_BODY(...) __VA_ARGS__"
require_term "src/runtime/pgy_runtime_platform_io_core.h" \
    "#include \"pgy_runtime_linkage.h\""
require_term "src/runtime/pgy_runtime_cext_lib.c" \
    "#define PGY_RUNTIME_EXTERN_DEFS"
reject_term "src/compiler/compiler_runtime_cache.c" \
    'argv[argc++] = "-DPGY_RUNTIME_EXTERN_DEFS";'

# CType/ErrType for these families may be declared only by the generated TU.
# They must never become declarations whose definitions are expected from the
# separately compiled runtime object.
require_program_local_family \
    "src/runtime/pgy_runtime_result_option_inline.h" "#define PGY_RESULT_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_result_option_inline.h" "#define PGY_OPTION_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_list_generic_inline.h" "#define PGY_LIST_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_set_generic_inline.h" "#define PGY_SET_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_builtin_hashmap_inline.h" "#define PGY_HASHMAP_DEFINE"

for rel in \
    src/runtime/pgy_runtime_result_option_inline.h \
    src/runtime/pgy_runtime_list_generic_inline.h \
    src/runtime/pgy_runtime_set_generic_inline.h; do
    reject_term "$rel" "PGY_RT_DECL"
    reject_term "$rel" "PGY_RT_MACRO_BODY"
done
reject_term_after_marker \
    "src/runtime/pgy_runtime_builtin_hashmap_inline.h" \
    "#define PGY_HASHMAP_DEFINE" "PGY_RT_DECL"
reject_term_after_marker \
    "src/runtime/pgy_runtime_builtin_hashmap_inline.h" \
    "#define PGY_HASHMAP_DEFINE" "PGY_RT_MACRO_BODY"

# Stateful extern families must not leave a private emitted-TU copy beside the
# runtime object's storage. PGY_RT_GLOBAL is a definition in the object/default
# modes and an extern declaration in PGY_RUNTIME_DECLS_ONLY mode.
require_term "src/runtime/pgy_runtime_machine_layer_inline.h" \
    "PGY_RT_GLOBAL void *pgy_machine_layer_mapping_provider_context;"
reject_term "src/runtime/pgy_runtime_machine_layer_inline.h" \
    "static void *pgy_machine_layer_mapping_provider_context;"
require_term "src/runtime/pgy_runtime_platform_io_core.h" \
    "PGY_RT_GLOBAL _Thread_local bool pgy_zone_authority_last_ok"
for name in \
    pgy_zone_authority_last_zone \
    pgy_zone_authority_last_participant \
    pgy_zone_authority_last_code \
    pgy_zone_authority_last_reason; do
    require_term "src/runtime/pgy_runtime_platform_io_core.h" \
        "PGY_RT_GLOBAL _Thread_local char ${name}"
done
reject_term "src/runtime/pgy_runtime_platform_io_core.h" \
    "static _Thread_local bool pgy_zone_authority_last_ok"
reject_term "src/runtime/pgy_runtime_platform_io_core.h" \
    "static _Thread_local char pgy_zone_authority_last_"

# Channel waits execute in the generated TU while task creation installs the
# cancellation hook in the runtime object. They must observe one shared probe.
require_term "src/runtime/pgy_runtime_cancel_probe.h" \
    '#include "pgy_runtime_linkage.h"'
require_term "src/runtime/pgy_runtime_cancel_probe.h" \
    "PGY_RT_GLOBAL _Atomic(bool (*)(void)) g_pgy_cancel_probe;"
require_term "src/runtime/pgy_runtime_cancel_probe.h" \
    "PGY_RT_DECL bool"
require_term "src/runtime/pgy_runtime_cancel_probe.h" \
    "PGY_RT_DECL void"
reject_term "src/runtime/pgy_runtime_cancel_probe.h" \
    "static _Atomic(bool (*)(void)) g_pgy_cancel_probe;"
require_term "src/runtime/pgy_parallel.h" \
    '#include "runtime/pgy_parallel_task_foundation.h"'
require_term "src/runtime/pgy_parallel_task_foundation.h" \
    "pgy_cancel_node_create(PgyCancelNode *parent)"
reject_term "src/runtime/pgy_parallel.h" \
    "pgy_cancel_node_create(PgyCancelNode *parent)"

echo "[runtime-cext-contract] program generics stay local; stateful extern storage has one owner"
