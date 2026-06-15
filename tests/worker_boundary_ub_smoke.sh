#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_term() {
    local file="$1"
    local term="$2"

    if ! grep -Fq "$term" "$ROOT_DIR/$file"; then
        echo "[worker-boundary-ub] missing term in $file: $term" >&2
        exit 1
    fi
}

reject_term() {
    local file="$1"
    local term="$2"

    if grep -Fq "$term" "$ROOT_DIR/$file"; then
        echo "[worker-boundary-ub] forbidden term in $file: $term" >&2
        exit 1
    fi
}

require_term "AGENTS.md" 'Do not keep a borrowed `LLVMVarEntry *`'
require_term "AGENTS.md" "Do not pass growable runtime container storage"
require_term "AGENTS.md" 'Do not let detached `async { ... }` capture local storage by pointer'
require_term "AGENTS.md" "Rehash/grow plus concurrent read is UB"

require_term "src/common/worker_boundary_storage_policy.h" "PGY_WORKER_BOUNDARY_STORAGE_ARRAY"
require_term "src/common/worker_boundary_storage_policy.h" "PGY_WORKER_BOUNDARY_STORAGE_SLICE"
require_term "src/common/worker_boundary_storage_policy.h" "PGY_WORKER_BOUNDARY_STORAGE_HASHMAP"
require_term "src/common/worker_boundary_storage_policy.h" "PGY_WORKER_BOUNDARY_STORAGE_CHANNEL"
require_term "src/common/worker_boundary_storage_policy.c" "PgyWorkerBoundaryStorageSpec"
require_term "src/common/worker_boundary_storage_policy.c" "pgy_worker_boundary_storage_kind_name"
require_term "src/common/worker_boundary_storage_policy.c" "pgy_worker_boundary_storage_kind_from_constructor_name"
require_term "src/common/worker_boundary_storage_policy.c" "pgy_worker_boundary_storage_kind_from_type_name"
require_term "src/common/worker_boundary_storage_policy.c" "strcmp(type_name, spec->constructor_name) == 0"
require_term "src/tests/transpile/test_transpile_core_part_0.cases.h" "worker-boundary policy rejects raw storage constructor names"

require_term "src/semantic/type_checker_helpers_resources.c" "../common/worker_boundary_storage_policy.h"
require_term "src/semantic/type_checker_helpers_resources.c" "worker_boundary_storage_display_name"
require_term "src/semantic/type_checker_helpers_resources.c" "detached_worker_boundary_storage_display_name"
require_term "src/semantic/type_checker_flow_parallel.c" "worker_boundary_storage_display_name(sym->type)"
require_term "src/semantic/type_checker_flow_parallel.c" "Parallel task cannot capture mutable collection"
require_term "src/semantic/type_checker_async_channel.c" "semantic_report_worker_storage_boundary"
require_term "src/semantic/type_checker_async_channel.c" "type_is_detached_worker_boundary_unsafe_storage"

require_term "src/codegen/transpiler_type_mapping.c" "../common/worker_boundary_storage_policy.h"
require_term "src/codegen/transpiler_type_mapping.c" "codegen_worker_boundary_storage_kind_from_type_name"
require_term "src/codegen/transpiler_type_mapping.c" "pgy_worker_boundary_storage_kind_name"
require_term "src/codegen/transpiler_async_parallel_emit.c" "codegen_worker_boundary_storage_kind_from_type_name(type_name, false)"
require_term "src/codegen/transpiler_async_parallel_emit.c" "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)"
require_term "src/codegen/transpiler_async_parallel_emit.c" "cannot share mutable collection"
require_term "src/codegen/transpiler_async_parallel_emit.c" "async block cannot capture Channel<T> local"
require_term "src/codegen/transpiler_spawn_channel_emit.c" "transpiler_spawn_reject_worker_storage"
require_term "src/codegen/transpiler_spawn_channel_emit.c" "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)"

require_term "src/codegen/llvm_stmt_parallel_async.c" "codegen_worker_boundary_storage_kind_from_constructor_name"
require_term "src/codegen/llvm_stmt_parallel_async.c" '"Array/Slice", false, true'
require_term "src/codegen/llvm_stmt_parallel_async.c" "LLVM async block cannot capture Channel<T> local"
require_term "src/codegen/llvm_expr_spawn_worker_boundary.c" "llvm_spawn_reject_worker_storage_param"
require_term "src/codegen/llvm_expr_spawn_worker_boundary.c" "llvm_spawn_reject_worker_storage_arg"
require_term "src/codegen/llvm_expr_spawn_worker_boundary.c" "codegen_worker_boundary_storage_kind_from_type_name("
require_term "src/codegen/llvm_expr_spawn_worker_boundary.c" "param_type_name, true"
require_term "src/codegen/llvm_expr_spawn_worker_boundary.c" '"Array/Slice", true, true'

require_term "src/codegen/llvm_internal_api.h" "llvm_scope_lookup_snapshot"
require_term "src/codegen/llvm_registry.c" "llvm_scope_cache_invalidate(ctx)"
reject_term "src/codegen/llvm_internal_api.h" "LLVMVarEntry *llvm_scope_lookup"
scope_lookup_escape="$(
    grep -R -n --include='*.c' --include='*.h' \
        'llvm_scope_lookup(ctx,' "$ROOT_DIR/src/codegen" \
        | grep -Fv 'src/codegen/llvm_registry.c:' || true
)"
if [[ -n "$scope_lookup_escape" ]]; then
    echo "[worker-boundary-ub] borrowed LLVM scope lookup escaped the registry owner" >&2
    echo "$scope_lookup_escape" >&2
    exit 1
fi

require_term "src/codegen/codegen_hashmap_key_policy.c" "if (name == NULL || name[0] == '\\0')"
require_term "src/codegen/codegen_hashmap_key_policy.c" "pgy_hashmap_key_c_infix"
require_term "src/codegen/codegen_hashmap_key_policy.c" "pgy_hashmap_key_policy_type_text"
require_term "src/codegen/transpiler_expr_stdlib_map_builtin.c" "transpiler_map_require_supported_key"
require_term "src/codegen/llvm_expr_call_collections_extended.c" "pgy_hashmap_key_policy_type_text()"

require_term "src/runtime/pgy_runtime_scalar_std_inline.h" "pthread_mutex_lock(&pgy_runtime_rng_mutex)"
require_term "src/runtime/pgy_runtime_scalar_std_inline.h" "pthread_mutex_unlock(&pgy_runtime_rng_mutex)"
require_term "src/runtime/pgy_runtime_lib_std_exports.h" "pthread_mutex_lock(&pgy_runtime_lib_rng_mutex)"
require_term "src/runtime/pgy_runtime_lib_std_exports.h" "pthread_mutex_unlock(&pgy_runtime_lib_rng_mutex)"

require_term "src/tests/semantic/test_semantic_misc_a_part_b_1.cases.h" "CFG parallel rejects borrowed Slice capture"
require_term "src/tests/semantic/test_semantic_misc_a_part_b_1.cases.h" "CFG parallel rejects HashMap storage capture"
require_term "src/tests/semantic/test_semantic_misc_a_part_b_1.cases.h" "CFG spawn rejects HashMap storage boundary crossing"

if grep -R -n --include='*.c' --include='*.h' \
        -E 'return "(Array/Slice|Array|Slice|List|Queue|Set|HashMap|Channel)"' \
        "$ROOT_DIR/src/semantic" "$ROOT_DIR/src/codegen"; then
    echo "[worker-boundary-ub] worker-boundary display strings must stay in the common policy owner" >&2
    exit 1
fi

echo "[worker-boundary-ub] source contracts ok"
