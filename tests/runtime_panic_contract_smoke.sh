#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"
CONTRACT_CHECK_DONE=0

require_literal() {
    local rel="$1"
    local term="$2"
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
        "src/runtime/pgy_runtime_result_option_inline.h"
        "src/codegen/transpiler_expr_core_emit.h"
        "src/codegen/llvm_expr_scalar_core.h"
        "src/codegen/llvm_expr_scalar_core.c"
        "src/codegen/llvm_runtime.c"
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
    require_literal "src/codegen/transpiler_expr_core_emit.h" "pgy_checked_div_i32_export"
    require_literal "src/codegen/llvm_expr_scalar_core.c" "pgy_checked_mod_i32_export"
    require_literal "src/codegen/llvm_runtime.c" "pgy_runtime_panic_internal_invariant_export"
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
lib_top = root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h"
slot_c = root / "src" / "runtime" / "pgy_runtime_lib_intent_slot_core_exports.h"
slot_export = root / "src" / "runtime" / "pgy_runtime_lib_slot_exports.h"
slot_array_export = root / "src" / "runtime" / "pgy_runtime_lib_slot_array_io_string_exports.h"
device_slot_export = root / "src" / "runtime" / "pgy_runtime_lib_device_slot_exports.h"
secure_slot_export = root / "src" / "runtime" / "pgy_runtime_lib_secure_slot_exports.h"
docs = [
    root / "docs" / "100_beta_readiness_checklist.md",
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
    root / "src" / "codegen" / "transpiler_expr_core_emit.h",
    root / "src" / "codegen" / "llvm_expr_scalar_core.c",
    root / "src" / "codegen" / "llvm_runtime.c",
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
    root / "src" / "codegen" / "llvm_runtime.c": [
        "pgy_runtime_panic_internal_invariant_export",
    ],
}
for path, tokens in unwrap_lowering_paths.items():
    text = path.read_text(encoding="utf-8")
    for token in tokens:
        if token not in text:
            raise SystemExit(f"{path.relative_to(root)} missing unwrap panic token {token}")

array_lowering_paths = {
    root / "src" / "codegen" / "transpiler_expr_stdlib_builtin.h": ["pgy_array_set_"],
    root / "src" / "codegen" / "transpiler_expr_dispatch_emit.h": ["pgy_array_get_", "pgy_slice_get_"],
    root / "src" / "codegen" / "llvm_expr.c": ["pgy_array_get_", "pgy_slice_get_", "llvm_emit_checked_collection_get"],
    root / "src" / "codegen" / "llvm_expr_array_calls.c": ["pgy_array_set_"],
    root / "src" / "codegen" / "llvm_runtime.c": ["pgy_array_get_", "pgy_array_set_", "pgy_slice_get_"],
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

for path in docs:
    text = path.read_text(encoding="utf-8")
    missing = [
        term for term in [
            "Runtime Panic Parity",
            "invalid-secure-token",
            "authority-mismatch",
            "released-slot",
        ]
        if term not in text
    ]
    if missing:
        raise SystemExit(
            f"{path.relative_to(root)} missing panic contract term(s): "
            + ", ".join(missing)
        )

print("[runtime-panic-contract] hard-fail panic surface is contract-backed")
PY
fi
