#!/usr/bin/env bash
# Runtime-bitcode twin contract gate (docs/189 C4/C5/C6).
#
# The inlined-bitcode leg is only safe while three structural properties
# hold; each regressed silently at least once, so pin them:
#   1. the .bc build mirrors the semantics-relevant codegen flags
#      (-fwrapv, -fno-strict-aliasing) of the native runtime legs;
#   2. the strip list covers the stateful/panic-carrying runtime families
#      and the exclusion loop enforces external-linkage before stripping;
#   3. bitcode freshness is a directory scan, not a hand-maintained header
#      list (every list omission was a stale-but-"fresh" blind spot).
#
# Pins are stable identifiers (function/flag names), never arithmetic or
# prose spellings (docs/188 R2 pin doctrine).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[runtime-bc-contract] $1" >&2
    exit 1
}

require_term() {
    local file="$1"
    local term="$2"
    local why="$3"
    grep -Fq -- "$term" "$ROOT_DIR/$file" \
        || fail "$file lost required term '$term' ($why)"
}

reject_term() {
    local file="$1"
    local term="$2"
    local why="$3"
    if grep -Fq -- "$term" "$ROOT_DIR/$file"; then
        fail "$file regained rejected term '$term' ($why)"
    fi
}

# 1. flag mirror (docs/189 C4)
require_term scripts/build_runtime_bc.sh "-fwrapv" \
    "bitcode must define signed overflow as wrap like both native legs"
require_term scripts/build_runtime_bc.sh "-fno-strict-aliasing" \
    "bitcode must disable TBAA like both native legs"

# 2. strip coverage + structural linkage guard (docs/189 C5/C7)
require_term src/codegen/llvm_runtime_attrs.c "pgy_checked_f2i_" \
    "checked float->int guard must be hardened like div/mod"
require_term src/codegen/llvm_runtime_attrs.c "llvm_fn_is_stateful_runtime" \
    "stateful runtime families must have a strip predicate"
require_term src/codegen/llvm_runtime_attrs.c "pgy_zone_authority_" \
    "zone-authority checks carry an inline panic body that mis-lowers"
require_term src/codegen/llvm_runtime_attrs.c "pgy_clock_" \
    "virtual clock state must stay single-instance in the runtime object"
require_term src/codegen/llvm_runtime_attrs.c "pgy_channel_" \
    "channel waits feed the pool compensation tick's shared state"
require_term src/codegen/llvm_runtime_attrs.c "pgy_pool_" \
    "pool lifecycle flags must stay single-instance"
require_term src/codegen/llvm_api.c "llvm_fn_is_stateful_runtime" \
    "exclusion loop must consult the stateful predicate"
require_term src/codegen/llvm_api.c "LLVMGetLinkage" \
    "stripping must be gated on external linkage (static strip = link break)"
require_term src/codegen/llvm_api.c "LLVMGetFirstGlobal(runtime_module)" \
    "bitcode must enumerate external runtime global definitions"
require_term src/codegen/llvm_api.c "LLVMSetInitializer(gv, NULL)" \
    "the linked runtime object must remain the sole global-definition owner"

# 3. freshness is a directory scan (docs/189 C6)
require_term src/codegen/llvm_runtime_bitcode_freshness.c \
    "llvm_runtime_dir_has_newer_source" \
    "freshness must scan the runtime directory, not a hand list"
require_term src/codegen/llvm_runtime_bitcode_freshness.c "opendir" \
    "directory scan mechanism"
reject_term src/codegen/llvm_runtime_bitcode_freshness.c \
    "pgy_runtime_channel_lane_inline.h" \
    "a hand-maintained per-header dep list is the blind spot this gate closes"

# 4. checked float->int twins exist in BOTH runtime homes (lockstep)
require_term src/runtime/pgy_runtime_lib_checked_arith_core.h \
    "pgy_checked_f2i_i32_export" "native-object home of the f2i guard"
require_term src/runtime/pgy_runtime_lib_checked_arith_core.h \
    "pgy_checked_f2i_i64_export" "native-object home of the f2i guard"
require_term src/runtime/pgy_runtime_panic_checked_inline.h \
    "pgy_checked_f2i_i32_export" "C-leg inline home of the f2i guard"
require_term src/runtime/pgy_runtime_panic_checked_inline.h \
    "pgy_checked_f2i_i64_export" "C-leg inline home of the f2i guard"

echo "[runtime-bc-contract] bitcode twin contract pins hold"
