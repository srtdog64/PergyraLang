#!/usr/bin/env bash
# Source-level SoT guard for budget-charge twin parity (external red-team R6/R2).
#
# Several metered allocation paths have a C-inline twin and a separate LLVM-export
# twin (the inline-vs-linkable split fundamental to the dual backend). The budget
# charge for a kind must appear in BOTH twins or the LLVM and C backends diverge
# in enforcement -- the exact silent C/LLVM gap that disabled the LLVM budget
# gate earlier this session. The runtime matrix (run_runtime_enforce.sh) catches
# this behaviorally for the types it exercises; this gate catches it at the
# source for EVERY twin file, including types the runtime fixtures don't cover,
# and fails the build the moment a charge is dropped from one side of a twin.
#
# Pure textual check -- no compiler needed, so it is always load-bearing in CI.
set -u

HERE="$(cd "$(dirname "$0")/../.." && pwd)"
RT="$HERE/src/runtime"
fail=0

file_consumes_charge() {
    local token="$1" file="$2"
    local include

    if grep -q "$token" "$RT/$file"; then
        return 0
    fi

    while IFS= read -r include; do
        if [ -f "$RT/$include" ] && grep -q "$token" "$RT/$include"; then
            return 0
        fi
    done < <(sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*"\([^"]*\)".*/\1/p' "$RT/$file")

    return 1
}

# require_charge <kind-token> <label> <file...> -- every file must contain the token
require_charge() {
    local token="$1" label="$2"; shift 2
    local f
    for f in "$@"; do
        if [ ! -f "$RT/$f" ]; then
            echo "[FAIL] $label: twin file missing: $f"; fail=1; continue
        fi
        if file_consumes_charge "$token" "$f"; then
            echo "[PASS] $label charge present in $f"
        else
            echo "[FAIL] $label charge ($token) absent in $f -- C/LLVM budget twin drift"; fail=1
        fi
    done
}

# CHANNEL_COUNT: C-inline twins (channel_inline, channel_string_inline) +
# LLVM-export twins (channel_int_exports, channel_string_exports).
require_charge "PGY_BUDGET_CHANNEL_COUNT" "channel-count" \
    pgy_runtime_channel_inline.h \
    pgy_runtime_channel_string_inline.h \
    pgy_runtime_lib_channel_int_exports.h \
    pgy_runtime_lib_channel_string_exports.h

# ALLOC charge on the list paths: C-inline (list_set_inline, list_generic_inline)
# + LLVM-export (lib_list_raw_exports). Either the choke-point wrapper or the raw
# ALLOC_BYTES kind counts as charging.
require_charge "pgy_budget_charge_alloc\|PGY_BUDGET_ALLOC_BYTES" "list-alloc" \
    pgy_runtime_list_set_inline.h \
    pgy_runtime_list_generic_inline.h \
    pgy_runtime_lib_list_raw_exports.h

if [ "$fail" -eq 0 ]; then echo "ALL PASS (0 budget-twin drift)"; exit 0; fi
echo "FAILED"; exit 1
