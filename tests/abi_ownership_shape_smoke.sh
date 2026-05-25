#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[abi-ownership-shape] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing ownership ABI file: $rel"
}

require_term() {
    local rel="$1"
    local term="$2"
    local path="$ROOT_DIR/$rel"
    grep -Fq "$term" "$path" || fail "$rel missing term: $term"
}

for rel in \
    "src/runtime/pgy_abi_spec.h" \
    "src/runtime/slot_manager.h" \
    "src/runtime/slot_manager_pin.c" \
    "src/runtime/pgy_runtime_plain_slot_inline.h" \
    "src/runtime/pgy_runtime_slot_macros.h" \
    "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" \
    "src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_secure_slot_exports.h" \
    "src/test_abi_spec.c" \
    "src/test_memory_layout.c" \
    "src/test_security.c" \
    "src/codegen/transpiler_mir_pin_emit.h" \
    "src/codegen/transpiler_block_emit.h" \
    "src/codegen/llvm_runtime.c" \
    "src/codegen/llvm_runtime_secure_slot_decl.c" \
    "src/codegen/llvm_mir_block_emit.h" \
    "src/compiler/mir_cfg_contract_pin.h" \
    "src/compiler/mir_cfg_contract_validate.h" \
    "src/compiler/mir_cfg_contract_validate_cleanup.h" \
    "src/compiler/mir_cfg_contract_validate_cleanup.c" \
    "tests/cfg_body_dataflow_smoke.sh" \
    "tests/compare_backends.sh" \
    "docs/74_slot_pinning_caching.md" \
    "docs/100_beta_readiness_checklist.md" \
    "docs/107_beta_stable_subset.md" \
    "docs/118_slot_model_rigor_audit.md"; do
    require_file "$rel"
done

require_term "src/runtime/pgy_abi_spec.h" "typedef struct { uint64_t id; bool can_write; bool can_read; } pgy_abi_token_int_rel;"
require_term "src/runtime/pgy_abi_spec.h" "typedef struct { int32_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_int_rel;"
require_term "src/runtime/pgy_abi_spec.h" "pgy_abi_pinned_slot_view_int"
require_term "src/runtime/pgy_abi_spec.h" "pgy_abi_pinned_secure_slot_view_int"
require_term "src/test_abi_spec.c" "sizeof(PgyPinnedSlotView_Int) == sizeof(pgy_abi_pinned_slot_view_int)"
require_term "src/test_abi_spec.c" "offsetof(PgyPinnedSecureSlotView_Int, token) == offsetof(pgy_abi_pinned_secure_slot_view_int, token)"

require_term "src/runtime/slot_manager.h" "PgyPinnedView"
require_term "src/runtime/slot_manager.h" "User-facing language syntax must keep PgyPinnedView scope-bound."
require_term "src/runtime/slot_manager_pin.c" "entry->pinThreadAffinity = tid"
require_term "src/runtime/slot_manager_pin.c" "entry->pinGeneration = handle->generation"
require_term "src/runtime/slot_manager_pin.c" "entry->pinThreadAffinity != tid"
require_term "src/runtime/slot_manager_pin.c" "entry->pinGeneration != view->generation"
require_term "src/runtime/slot_manager.c" "return (uintptr_t)pthread_self();"
require_term "src/runtime/slot_manager_internal.h" "uintptr_t current_thread_id(void);"
require_term "src/runtime/slot_manager.h" "uintptr_t pinThreadAffinity;"
if grep -Fq "0xffffffffu" "$ROOT_DIR/src/runtime/slot_manager.c"; then
    fail "slot pin thread affinity must not truncate pthread_self to 32 bits"
fi
require_term "src/runtime/slot_manager_pin.c" "SlotValidateToken(manager, handle, token)"
require_term "src/runtime/slot_manager.c" "if (entry->pinCount > 0)"
require_term "src/runtime/slot_manager.c" "return SLOT_ERROR_PINNED"
require_term "src/runtime/slot_manager.c" "slot_reset_entry_locked(entry)"
require_term "src/runtime/slot_manager_core_ops.c" "entry->pinMode == (uint32_t)PGY_SLOT_PIN_WRITE"
require_term "src/tests/security/test_security_runtime.cases.h" "Write-pinned plain slot rejects concurrent read"

require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "PgyPinnedSlotView_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_pin_read_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_pin_write_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_unpin_cleanup_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "PgyPinnedSecureSlotView_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "pgy_secure_unpin_cleanup_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_unpin_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_pin_read_init_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_pin_write_init_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_unpin_##Suffix"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_pin_read_init_##Suffix"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_pin_write_init_##Suffix"

require_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_pin_%s_%s"
require_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_unpin_%s(&%s);"
require_term "src/codegen/transpiler_block_emit.c" "__attribute__((cleanup(pgy_unpin_cleanup_%s)))"
require_term "src/codegen/llvm_runtime.c" "llvm_runtime_slot_name"
require_term "src/codegen/llvm_runtime.c" "pin_read"
require_term "src/codegen/llvm_runtime.c" "pin_write"
require_term "src/codegen/llvm_runtime.c" "pin_read_init"
require_term "src/codegen/llvm_runtime.c" "pin_write_init"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "llvm_runtime_secure_slot_name"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "secure_pin_read_init"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "secure_pin_write_init"
require_term "src/codegen/llvm_runtime.c" "unpin"
require_term "src/codegen/llvm_mir_pin_region.c" "pgy_pin_%s_init_%s"
require_term "src/codegen/llvm_mir_pin_region.c" "pgy_secure_pin_%s_init_%s"
require_term "src/codegen/llvm_mir_pin_region.c" "pgy_unpin_%s"

require_term "src/compiler/mir_cleanup_fact_names.h" "pin-unpin-cleanup-edge"
require_term "src/compiler/mir_cleanup_fact_names.h" "MIR_CLEANUP_FACT_PIN_UNPIN_EDGE"
require_term "src/compiler/mir_cfg_contract_validate_cleanup.c" "pin-region block[%zu] missing pin-unpin cleanup fact"
require_term "tests/cfg_body_dataflow_smoke.sh" "ReleaseAfterUnpin(slot, all_cfg_exits)"
require_term "tests/cfg_body_dataflow_smoke.sh" "WriteView requires exclusive slot view access"
require_term "tests/compare_backends.sh" "tests/cases/backend_compare/pin_read_view_block"
require_term "tests/compare_backends.sh" "tests/cases/backend_compare/pin_break_cleanup_block"
require_term "tests/compare_backends.sh" "tests/cases/backend_compare/pin_secure_param_read_view_block"

require_term "docs/74_slot_pinning_caching.md" "Pin/Lease is a typed lexical lease"
require_term "docs/100_beta_readiness_checklist.md" "Slot/Pin/Zone-bound handle/runtime-none/raw escape"
require_term "docs/107_beta_stable_subset.md" "Non-pin handle expiration is not claimed as a single-mechanism proof"
require_term "docs/118_slot_model_rigor_audit.md" "Zone-Bound Handle typing"

echo "[abi-ownership-shape] Slot/Pin ABI shape, cleanup, and docs contract are gated"
