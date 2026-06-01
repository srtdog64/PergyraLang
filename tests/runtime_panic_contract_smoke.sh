#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"
PYTHON_BIN="${PYTHON_BIN:-}"
CONTRACT_CHECK_DONE=0

require_literal() {
    local rel="$1"
    local term="$2"
    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        pgy_beta_checklist_contains "$term" || {
            echo "[runtime-panic-contract] $rel shards missing term: $term" >&2
            exit 1
        }
        return 0
    fi
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || {
        echo "[runtime-panic-contract] $rel missing term: $term" >&2
        exit 1
    }
}

forbid_literal() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        echo "[runtime-panic-contract] $rel contains forbidden term: $term" >&2
        exit 1
    fi
}

run_literal_contract_smoke() {
    local required_files=(
        "src/runtime/pgy_runtime_panic_contract.h"
        "src/runtime/pgy_runtime_platform_io_core.h"
        "src/runtime/pgy_runtime_allocator_inline.h"
        "src/runtime/pgy_runtime_memory_array_slot_inline.h"
        "src/runtime/pgy_runtime_lib_authority_file_core.h"
        "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h"
        "src/runtime/pgy_runtime_lib_slot_exports.h"
        "src/runtime/pgy_runtime_lib_device_slot_exports.h"
        "src/runtime/pgy_runtime_lib_secure_slot_exports.h"
        "src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h"
        "src/runtime/pgy_runtime_lib_array_map_exports.h"
        "src/runtime/pgy_runtime_panic_checked_inline.h"
        "src/runtime/pgy_runtime_process_exit.h"
        "src/runtime/pgy_runtime_result_option_inline.h"
        "src/runtime/async/fiber.c"
        "src/codegen/transpiler_expr_core_emit.c"
        "src/codegen/llvm_expr_scalar_core.h"
        "src/codegen/llvm_expr_scalar_core.c"
        "src/codegen/llvm_runtime.c"
        "src/codegen/llvm_runtime_core_builtin_decl.c"
        "docs/100_beta_readiness_checklist.md"
        "docs/semantics/06_backend_parity.md"
        "docs/105_runtime_panic_contract.md"
    )
    local panic_tokens=(
        "PGY_RUNTIME_PANIC_CLASS_OOM"
        "PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO"
        "PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS"
        "PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT"
        "PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE"
        "PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN"
        "PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH"
        "PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT"
        "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE"
        "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ"
        "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE"
        "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ"
        "PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT"
        "PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH"
        "PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS"
        "PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS"
        "PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED"
        "PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO"
        "PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR"
        "PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE"
    )

    for rel in "${required_files[@]}"; do
        [[ -f "$ROOT_DIR/$rel" ]] || {
            echo "[runtime-panic-contract] missing contract file: $rel" >&2
            exit 1
        }
    done

    for token in "${panic_tokens[@]}"; do
        require_literal "src/runtime/pgy_runtime_panic_contract.h" "$token"
    done
    require_literal "src/runtime/pgy_runtime_panic_contract.h" "PGY_RUNTIME_PANIC("
    require_literal "src/runtime/pgy_runtime_panic_contract.h" "pgy_runtime_panic_emit"
    require_literal "src/runtime/pgy_runtime_platform_io_core.h" "pgy_runtime_panic_contract.h"
    require_literal "src/runtime/pgy_runtime_lib_authority_file_core.h" "pgy_runtime_panic_contract.h"
    require_literal "src/runtime/pgy_runtime_memory_array_slot_inline.h" "PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT"
    require_literal "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT"
    require_literal "src/runtime/pgy_runtime_lib_slot_exports.h" "PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE"
    require_literal "src/runtime/pgy_runtime_lib_device_slot_exports.h" "PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE"
    require_literal "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE"
    require_literal "src/runtime/pgy_runtime_result_option_inline.h" "PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR"
    require_literal "src/runtime/pgy_runtime_result_option_inline.h" "PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE"
    require_literal "src/runtime/pgy_runtime_panic_checked_inline.h" "PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO"
    require_literal "src/runtime/async/fiber.c" "pgy_runtime_panic_contract.h"
    require_literal "src/runtime/async/fiber.c" "fiber returned after completion"
    forbid_literal "src/runtime/async/fiber.c" "assert("
    require_literal "src/runtime/pgy_runtime_process_exit.h" "pgy_runtime_process_exit"
    require_literal "src/runtime/pgy_runtime_process_exit.h" "exit((int)code)"
    require_literal "src/runtime/pgy_runtime_io_qubit_inline.h" "pgy_runtime_process_exit(code)"
    require_literal "src/runtime/pgy_runtime_lib_io_string_exports.h" "pgy_runtime_process_exit(code)"
    forbid_literal "src/runtime/pgy_runtime_io_qubit_inline.h" "exit((int)code)"
    forbid_literal "src/runtime/pgy_runtime_lib_io_string_exports.h" "exit((int)code)"
    require_literal "src/codegen/transpiler_expr_core_emit.c" "pgy_checked_div_i32_export"
    require_literal "src/codegen/llvm_expr_scalar_core.c" "pgy_checked_mod_i32_export"
    require_literal "src/codegen/llvm_runtime_core_builtin_decl.c" "pgy_runtime_panic_internal_invariant_export"
    require_literal "src/codegen/llvm_runtime_core_builtin_decl.c" "pgy_runtime_panic_out_of_bounds_export"
    require_literal "docs/100_beta_readiness_checklist.md" "Runtime Panic Parity"
    require_literal "docs/105_runtime_panic_contract.md" "invalid-secure-token"
    require_literal "docs/semantics/06_backend_parity.md" "released-slot"

    forbid_literal "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "fprintf(stderr, \"[pgy] slot "
    forbid_literal "src/runtime/pgy_runtime_lib_slot_exports.h" "fprintf(stderr, \"[pgy] slot "

    echo "[runtime-panic-contract] hard-fail panic surface is contract-backed (literal fallback)"
}

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        run_literal_contract_smoke
        CONTRACT_CHECK_DONE=1
    fi
fi

if [[ "$CONTRACT_CHECK_DONE" -eq 0 ]]; then
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
header = root / "src" / "runtime" / "pgy_runtime_panic_contract.h"
inline_top = root / "src" / "runtime" / "pgy_runtime_platform_io_core.h"
inline_panic = root / "src" / "runtime" / "pgy_runtime_memory_array_slot_inline.h"
allocator_inline = root / "src" / "runtime" / "pgy_runtime_allocator_inline.h"
result_option_inline = root / "src" / "runtime" / "pgy_runtime_result_option_inline.h"
process_exit_h = root / "src" / "runtime" / "pgy_runtime_process_exit.h"
fiber_c = root / "src" / "runtime" / "async" / "fiber.c"
lib_top = root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h"
slot_c = root / "src" / "runtime" / "pgy_runtime_lib_intent_slot_core_exports.h"
slot_export = root / "src" / "runtime" / "pgy_runtime_lib_slot_exports.h"
slot_array_export = root / "src" / "runtime" / "pgy_runtime_lib_slot_array_io_string_exports.h"
device_slot_export = root / "src" / "runtime" / "pgy_runtime_lib_device_slot_exports.h"
secure_slot_export = root / "src" / "runtime" / "pgy_runtime_lib_secure_slot_exports.h"
docs = [
    root / "docs" / "100_beta_readiness_checklist.md",
    root / "docs" / "100a_beta_active_status.md",
    root / "docs" / "100b_beta_p0_semantics_systems_air.md",
    root / "docs" / "100c_beta_dag_mir_abi_runtime.md",
    root / "docs" / "100d_beta_execution_log.md",
    root / "docs" / "semantics" / "06_backend_parity.md",
    root / "docs" / "105_runtime_panic_contract.md",
]

required_classes = [
    "PGY_RUNTIME_PANIC_CLASS_OOM",
    "PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO",
    "PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS",
    "PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT",
    "PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE",
    "PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN",
    "PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH",
    "PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT",
]
required_reasons = [
    "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_RELEASE",
    "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE",
    "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ",
    "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE",
    "PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE",
    "PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ",
    "PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT",
    "PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT",
    "PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH",
    "PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS",
    "PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS",
    "PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED",
    "PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY",
    "PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY",
    "PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO",
    "PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR",
    "PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE",
]

if not header.exists():
    raise SystemExit("missing runtime panic contract header")
header_text = header.read_text(encoding="utf-8")
for token in required_classes + required_reasons:
    if token not in header_text:
        raise SystemExit(f"panic contract missing {token}")
if "PGY_RUNTIME_PANIC(" not in header_text or "pgy_runtime_panic_emit" not in header_text:
    raise SystemExit("panic contract does not expose a single panic emitter")

for path in [inline_top, lib_top]:
    text = path.read_text(encoding="utf-8")
    if "pgy_runtime_panic_contract.h" not in text:
        raise SystemExit(f"{path.relative_to(root)} does not include panic contract")

fiber_text = fiber_c.read_text(encoding="utf-8")
if "pgy_runtime_panic_contract.h" not in fiber_text:
    raise SystemExit("fiber runtime does not include panic contract")
if "assert(" in fiber_text:
    raise SystemExit("fiber runtime still uses raw assert instead of panic contract")
for token in [
    "fiber entry received null fiber",
    "fiber entry received null start routine",
    "fiber returned after completion",
]:
    if token not in fiber_text:
        raise SystemExit(f"fiber runtime missing panic reason: {token}")

process_exit_text = process_exit_h.read_text(encoding="utf-8")
if "pgy_runtime_process_exit" not in process_exit_text:
    raise SystemExit("process exit owner missing pgy_runtime_process_exit")
if "exit((int)code)" not in process_exit_text:
    raise SystemExit("process exit owner must retain explicit language Exit(Int)")
for path in [
    root / "src" / "runtime" / "pgy_runtime_io_qubit_inline.h",
    root / "src" / "runtime" / "pgy_runtime_lib_io_string_exports.h",
]:
    text = path.read_text(encoding="utf-8")
    if "pgy_runtime_process_exit(code)" not in text:
        raise SystemExit(f"{path.relative_to(root)} must consume process exit owner")
    if "exit((int)code)" in text:
        raise SystemExit(f"{path.relative_to(root)} still owns raw exit")

inline_text = inline_panic.read_text(encoding="utf-8")
if "PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT" not in inline_text:
    raise SystemExit("inline PGY_PANIC is not backed by the runtime panic contract")

for path in [slot_c, slot_export]:
    text = path.read_text(encoding="utf-8")
    if 'fprintf(stderr, "[pgy] slot ' in text:
        raise SystemExit(f"{path.relative_to(root)} still logs released-slot hard failures")
    for token in [
        "PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT",
        "PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE",
        "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE",
        "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ",
        "PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT",
    ]:
        if token not in text:
            raise SystemExit(f"{path.relative_to(root)} missing {token}")

device_text = device_slot_export.read_text(encoding="utf-8")
device_macro = re.search(
    r"#define PGY_DEFINE_DEVICE_SLOT_EXPORTS\(Suffix, CType, ZeroExpr\)(.*?)"
    r"PGY_DEFINE_DEVICE_SLOT_EXPORTS\(Int, int32_t, 0\)",
    device_text,
    flags=re.S,
)
if device_macro is None:
    raise SystemExit("missing device slot export macro body")
device_body = device_macro.group(1)
for token in [
    "PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE",
    "PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ",
    "PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT",
]:
    if token not in device_body:
        raise SystemExit(f"device slot exports missing {token}")
if "return (ZeroExpr);" in device_body:
    raise SystemExit("device slot export macro still silently returns ZeroExpr")
if "if (s != NULL && s->claimed)" in device_body:
    raise SystemExit("device slot export macro still uses silent guard-only validation")

secure_text = secure_slot_export.read_text(encoding="utf-8")
for token in [
    "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE",
    "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ",
    "PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE",
    "PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE",
    "PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ",
]:
    if token not in secure_text:
        raise SystemExit(f"secure slot exports missing {token}")

secure_macro = re.search(
    r"#define PGY_DEFINE_SECURE_SLOT_EXPORTS\(Suffix, CType, ZeroExpr\)(.*?)"
    r"PGY_DEFINE_SECURE_SLOT_EXPORTS\(Int, int32_t, 0\)",
    secure_text,
    flags=re.S,
)
if secure_macro is None:
    raise SystemExit("missing secure slot export macro body")
macro_body = secure_macro.group(1)
if "return (ZeroExpr);" in macro_body:
    raise SystemExit("secure slot export macro still silently returns ZeroExpr")
if "s != NULL && t != NULL" in macro_body:
    raise SystemExit("secure slot export macro still uses silent guard-only validation")
for token in [
    "atomic_uint_least64_t pgy_secure_token_counter_",
    "atomic_fetch_add_explicit",
    "memory_order_relaxed",
]:
    if token not in macro_body:
        raise SystemExit(f"secure slot token counter is not atomic-gated: {token}")

authority_text = (root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h").read_text(encoding="utf-8")
inline_authority_text = (root / "src" / "runtime" / "pgy_runtime_zone_result_option_inline.h").read_text(encoding="utf-8")
for label, text in [
    ("exported authority", authority_text),
    ("inline authority", inline_authority_text),
]:
    for token in [
        "PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH",
        "PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH",
    ]:
        if token not in text:
            raise SystemExit(f"{label} missing {token}")
if re.search(r"pgy_zone_authority_check_export.*?abort\s*\(", authority_text, flags=re.S):
    raise SystemExit("exported authority check still aborts outside panic contract")

array_text = "\n".join([
    (root / "src" / "runtime" / "pgy_runtime_memory_array_slot_inline.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_builtin_storage_inline.h").read_text(encoding="utf-8"),
])
for token in [
    "PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS",
    "PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS",
    "PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS",
    "pgy_slice_get_",
    "PGY_RUNTIME_PANIC_CLASS_OOM",
    "PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED",
]:
    if token not in array_text:
        raise SystemExit(f"inline array runtime missing {token}")

allocator_text = "\n".join([
    (root / "src" / "runtime" / "pgy_runtime_memory_array_slot_inline.h").read_text(encoding="utf-8"),
    allocator_inline.read_text(encoding="utf-8"),
])
for token in [
    "PGY_RUNTIME_PANIC_CLASS_OOM",
    "PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED",
    "PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY",
    "PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY",
    "alloc->bytes_in_use > SIZE_MAX - size",
    "alloc->allocations != SIZE_MAX",
    "ptr != NULL && size > 0",
]:
    if token not in allocator_text:
        raise SystemExit(f"inline allocator runtime missing {token}")

export_array_text = (root / "src" / "runtime" / "pgy_runtime_lib_slot_array_io_string_exports.h").read_text(encoding="utf-8")
export_array_text += "\n" + (root / "src" / "runtime" / "pgy_runtime_lib_array_map_exports.h").read_text(encoding="utf-8")
for token in [
    "PGY_RUNTIME_PANIC_CLASS_OOM",
    "PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED",
    "PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS",
    "PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS",
    "pgy_slice_get_",
]:
    if token not in export_array_text:
        raise SystemExit(f"exported array runtime missing {token}")

inline_collection_text = "\n".join([
    (root / "src" / "runtime" / "pgy_runtime_queue_inline.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_builtin_storage_inline.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_list_set_inline.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_zone_result_option_inline.h").read_text(encoding="utf-8"),
])
for token in [
    "list index out of bounds",
    "list set index out of bounds",
    "list remove index out of bounds",
    "queue pop from empty queue",
    "map key not found",
    "map remove key not found",
    "PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS",
]:
    if token not in inline_collection_text:
        raise SystemExit(f"inline collection runtime missing hard-fail token {token}")

export_collection_text = "\n".join([
    (root / "src" / "runtime" / "pgy_runtime_lib_list_raw_exports.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_lib_raw_collection_exports.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_lib_raw_map_exports.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_lib_raw_map_key_exports.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_lib_raw_queue_exports.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_lib_raw_set_exports.h").read_text(encoding="utf-8"),
])
for token in [
    "list index out of bounds",
    "list set index out of bounds",
    "list remove index out of bounds",
    "queue pop from empty queue",
    "map key not found",
    "map remove key not found",
    "PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS",
]:
    if token not in export_collection_text:
        raise SystemExit(f"exported collection runtime missing hard-fail token {token}")

runtime_export_text = (root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h").read_text(encoding="utf-8")
inline_runtime_text = "\n".join([
    (root / "src" / "runtime" / "pgy_runtime_memory_array_slot_inline.h").read_text(encoding="utf-8"),
    (root / "src" / "runtime" / "pgy_runtime_panic_checked_inline.h").read_text(encoding="utf-8"),
])
for label, text in [
    ("exported checked arithmetic", runtime_export_text),
    ("inline checked arithmetic", inline_runtime_text),
]:
    for token in [
        "pgy_checked_div_i32_export",
        "pgy_checked_div_i64_export",
        "pgy_checked_mod_i32_export",
        "pgy_checked_mod_i64_export",
        "PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO",
        "PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO",
    ]:
        if token not in text:
            raise SystemExit(f"{label} missing {token}")

for path in [
    root / "src" / "codegen" / "transpiler_expr_core_emit.c",
    root / "src" / "codegen" / "llvm_expr_scalar_core.c",
    root / "src" / "codegen" / "llvm_runtime_core_builtin_decl.c",
]:
    text = path.read_text(encoding="utf-8")
    for token in ["pgy_checked_div_i32_export", "pgy_checked_mod_i32_export"]:
        if token not in text:
            raise SystemExit(f"{path.relative_to(root)} missing checked arithmetic lowering {token}")

unwrap_lowering_paths = {
    result_option_inline: [
        "PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR",
        "PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE",
    ],
    root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h": [
        "pgy_runtime_panic_internal_invariant_export",
        "PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT",
    ],
    root / "src" / "runtime" / "pgy_runtime_panic_checked_inline.h": [
        "pgy_runtime_panic_internal_invariant_export",
        "PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT",
    ],
    root / "src" / "codegen" / "llvm_expr_result_option_calls.c": [
        "llvm_emit_checked_result_option_unwrap",
        "pgy_runtime_panic_internal_invariant_export",
        "Result unwrap on Err value",
        "Option unwrap on None value",
    ],
    root / "src" / "codegen" / "llvm_expr_unary_core.c": [
        "llvm_emit_try_operator_unwrap_panic",
        "llvm_lookup_function",
        "pgy_runtime_panic_internal_invariant_export",
        "Result unwrap on Err value",
    ],
    root / "src" / "codegen" / "llvm_runtime_core_builtin_decl.c": [
        "pgy_runtime_panic_internal_invariant_export",
        "pgy_runtime_panic_out_of_bounds_export",
    ],
}
for path, tokens in unwrap_lowering_paths.items():
    text = path.read_text(encoding="utf-8")
    for token in tokens:
        if token not in text:
            raise SystemExit(f"{path.relative_to(root)} missing unwrap panic token {token}")

array_lowering_paths = {
    root / "src" / "codegen" / "transpiler_expr_stdlib_builtin.c": ["pgy_array_set_"],
    root / "src" / "codegen" / "transpiler_expr_array_access_emit.c": ["pgy_array_get_", "pgy_slice_get_"],
    root / "src" / "codegen" / "llvm_expr_aggregate.c": ["pgy_array_get_", "pgy_slice_get_", "llvm_emit_checked_collection_get"],
    root / "src" / "codegen" / "llvm_expr_call_methods_domain_slice.c": [
        "pgy_runtime_panic_out_of_bounds_export",
        "LLVMIntUGT",
        "remaining = LLVMBuildSub",
    ],
    root / "src" / "codegen" / "llvm_expr_array_calls.c": ["pgy_array_set_"],
    root / "src" / "codegen" / "llvm_runtime.c": ["array_get", "array_set", "slice_get"],
}
for path, tokens in array_lowering_paths.items():
    text = path.read_text(encoding="utf-8")
    for token in tokens:
        if token not in text:
            raise SystemExit(f"{path.relative_to(root)} missing checked array lowering {token}")

compiler_text = "\n".join([
    (root / "src" / "compiler" / "compiler_toolchain.c").read_text(encoding="utf-8"),
    (root / "src" / "compiler" / "compiler_runtime_cache.c").read_text(encoding="utf-8"),
])
for token in [
    'PGY_SRC_DIR "/common/string_compat.h"',
    'PGY_RUNTIME_DIR "/pgy_runtime_lib_slot_array_io_string_exports.h"',
    'PGY_RUNTIME_DIR "/pgy_runtime_builtin_storage_inline.h"',
    'PGY_RUNTIME_DIR "/pgy_runtime_scalar_std_inline.h"',
    'PGY_RUNTIME_DIR "/pgy_runtime_map_int_key_inline.h"',
    'PGY_RUNTIME_DIR "/pgy_runtime_queue_inline.h"',
]:
    if token not in compiler_text:
        raise SystemExit(f"LLVM runtime cache freshness missing split runtime dependency {token}")

docs_text = "\n".join(path.read_text(encoding="utf-8") for path in docs)
missing = [
    term for term in [
        "Runtime Panic Parity",
        "invalid-secure-token",
        "authority-mismatch",
        "released-slot",
    ]
    if term not in docs_text
]
if missing:
    raise SystemExit(
        "runtime panic contract docs missing term(s): "
        + ", ".join(missing)
    )

print("[runtime-panic-contract] hard-fail panic surface is contract-backed")
PY
fi
