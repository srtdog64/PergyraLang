#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[security-portability] $*" >&2
    exit 1
}

require_term() {
    local rel="$1"
    local term="$2"

    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

require_term "Makefile" "check-security-toolchain:"
require_term "Makefile" "openssl/evp.h"
require_term "Makefile" "-lssl -lcrypto"
require_term "Makefile" "security test preflight requires OpenSSL development headers"

require_term "docs/03_security_mode_design.md" "not a completed third-party cryptographic audit"
require_term "docs/03_security_mode_design.md" "make check-security-toolchain"
require_term "docs/03_security_mode_design.md" "Platform claims must be tied to CI evidence"

require_term "docs/security/01_audit_targets.md" "Slot id exhaustion availability / tombstone flooding"
require_term "docs/security/01_audit_targets.md" "Runtime panic DoS boundary policy"
require_term "docs/security/01_audit_targets.md" "Zone-bound handle escape decision and diagnostics"
require_term "docs/security/01_audit_targets.md" "Do not claim a 128-bit Slot handle"
require_term "docs/security/01_audit_targets.md" "Recovery is a boundary"

require_term "src/runtime/slot_manager.h" "typedef struct"
require_term "src/runtime/slot_manager.h" "SlotFailure;"
require_term "src/runtime/slot_manager.h" "const char *SlotErrorName(SlotError err);"
require_term "src/runtime/slot_manager.h" "SlotFailure SlotFailureFromError(SlotError err, const char *operation,"
require_term "src/runtime/slot_manager_core_ops.c" "SlotErrorName(SlotError err)"
require_term "src/runtime/slot_manager_core_ops.c" "SlotFailureFromError(SlotError err, const char *operation,"
require_term "src/runtime/slot_manager_core_ops.c" "SlotErrorName(err)"
require_term "src/runtime/slot_manager_core_ops.c" "SlotFailure failure = SlotFailureFromError(err, op, handle)"
require_term "src/runtime/pgy_runtime_slot_status.h" "typedef enum"
require_term "src/runtime/pgy_runtime_slot_status.h" "PgyRuntimeSlotStatus;"
require_term "src/runtime/pgy_runtime_slot_status.h" "pgy_runtime_slot_status_name(PgyRuntimeSlotStatus status)"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_try_read_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "PGY_SLOT_TRY_EXPORT_DEFINE"
require_term "tests/runtime_panic_abi_smoke.sh" "inline_try_slot_status"
require_term "tests/runtime_panic_abi_smoke.sh" "exported_try_slot_status"

require_term "TODO.md" "Security/runtime portability needs narrower claims"
require_term "TODO.md" "check-security-toolchain"
require_term "TODO.md" "Red-team security closure target"
require_term "TODO.md" "Slot ID/generation exhaustion"

echo "[security-portability] security dependency and claim-scope contract is gated"
