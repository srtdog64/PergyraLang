#!/usr/bin/env bash
# Source-level SoT guard for the channel runtime twin op-set (target #4, the
# dual-backend-unified-consumption beta-closure item; sibling of
# run_budget_twin_parity.sh).
#
# Each channel payload type has a C-inline twin (self-contained C output) and a
# hand-expanded LLVM-export twin (the linked runtime). Every operation the
# C-inline twin exposes MUST have a matching LLVM export, or a program that uses
# that op compiles on C but fails to link / diverges on LLVM -- the exact silent
# C/LLVM gap class this beta targets. compare_backends.sh catches BEHAVIORAL
# drift for the ops its fixtures exercise; this gate catches OP-SET drift (an op
# added to one twin and forgotten in the other) at the source for EVERY op,
# including those no fixture exercises, and fails the build the moment the twins
# desync.
#
# Direction: C-inline ops MUST be a subset of LLVM exports (the C surface must be
# linkable on LLVM). Extra LLVM-only exports are allowed (e.g. *_status_code
# helpers). Pure textual check -- no compiler needed, always load-bearing in CI.
#
# Int C-inline twin is macro-generated (PGY_CHANNEL_DEFINE, ops spelled
# pgy_channel_<op>_##SuffixName); String C-inline twin is hand-written
# (pgy_channel_<op>_String). Both export twins spell ops literally.
set -u

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v grep >/dev/null 2>&1 \
    || ! command -v sed >/dev/null 2>&1 \
    || ! command -v sort >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

HERE="$(cd "$(dirname "$0")/../.." && pwd)"
RT="$HERE/src/runtime"
fail=0

# inline_ops_int -- op tokens the Int C macro generates (strip _##SuffixName)
inline_ops_int() {
    grep -oE "pgy_channel_[a-z_]+_##SuffixName" "$RT/pgy_runtime_channel_inline.h" \
        | sed 's/_##SuffixName//' | sort -u
}
# inline_ops_string -- op tokens the String C-inline twin defines literally
inline_ops_string() {
    grep -oE "pgy_channel_[a-z_]+_String\b" "$RT/pgy_runtime_channel_string_inline.h" \
        | sed 's/_String$//' | sort -u
}

# check_subset <type> <inline-op-lister> <exports-file>
check_subset() {
    local type="$1" lister="$2" exports="$3"
    local op missing=0 n=0

    if [ ! -f "$RT/$exports" ]; then
        echo "[FAIL] channel-$type: exports twin missing: $exports"; fail=1; return
    fi
    # If the exports twin instantiates the shared PGY_CHANNEL_DEFINE macro for
    # this type, every C-inline op is generated from the SAME macro body -- the
    # twins cannot drift by construction (target #4 unification), so the per-op
    # literal check is moot (and would mis-fire, since the ops are no longer
    # spelled out). Drift-proof, not merely drift-checked.
    # Two unification shapes count as drift-proof: the Int twin instantiates
    # PGY_CHANNEL_DEFINE(Int,...); the String twin #includes the C-inline header
    # with PGY_CH_STR_STORAGE=extern to reuse the same op bodies.
    if grep -qE "PGY_CHANNEL_DEFINE\(${type}," "$RT/$exports" \
        || grep -qE "define PGY_CH_STR_STORAGE|PGY_CH_STR_STORAGE" "$RT/$exports"; then
        echo "[PASS] channel-$type: unified -- ops generated from the shared C-inline body (drift-proof by construction)"
        return
    fi
    while IFS= read -r op; do
        [ -n "$op" ] || continue
        n=$((n + 1))
        if ! grep -qE "\b${op}_${type}\b" "$RT/$exports"; then
            echo "[FAIL] channel-$type: C-inline op '${op}_${type}' has NO LLVM export in $exports -- twin op-set drift"
            fail=1; missing=$((missing + 1))
        fi
    done < <("$lister")
    if [ "$missing" -eq 0 ]; then
        echo "[PASS] channel-$type: all $n C-inline ops have matching LLVM exports"
    fi
}

check_subset Int    inline_ops_int    pgy_runtime_lib_channel_int_exports.h
check_subset String inline_ops_string pgy_runtime_lib_channel_string_exports.h

if [ "$fail" -eq 0 ]; then echo "ALL PASS (0 channel-twin op-set drift)"; exit 0; fi
echo "FAILED"; exit 1
