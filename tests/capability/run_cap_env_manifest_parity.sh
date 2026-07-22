#!/usr/bin/env bash
# Source-level parity guard for the capability grant policy (docs/190 A4).
#
# The effective capability grant is env INTERSECT manifest: the host env
# (PGY_CAP_GRANT) and a loader's manifest each restrict independently and the
# more restrictive wins (fail-closed). The C-inline twin
# (pgy_runtime_panic_checked_inline.h) and the LLVM-object twin
# (pgy_runtime_lib_authority_file_core.h) must implement the SAME policy, or the
# two backends diverge on capability decisions -- exactly the divergence A4
# fixed (C used to let the manifest win, LLVM used to let env overwrite the
# manifest). run_runtime_enforce.sh proves the env path behaviourally on both
# backends; this gate pins the intersection in BOTH twins at the source and
# fails the moment either reverts to a one-sided overwrite.
#
# Pure textual check -- no compiler needed, so it is always load-bearing in CI.
set -u

HERE="$(cd "$(dirname "$0")/../.." && pwd)"
RT="$HERE/src/runtime"
fail=0

C_TWIN="pgy_runtime_panic_checked_inline.h"
LLVM_TWIN="pgy_runtime_lib_authority_file_core.h"

require_term() {
    local file="$1" term="$2" label="$3"
    if grep -Fq "$term" "$RT/$file"; then
        echo "[PASS] $label present in $file"
    else
        echo "[FAIL] $label absent in $file -- A4 env-INTERSECT-manifest parity regression"
        fail=1
    fi
}

reject_term() {
    local file="$1" term="$2" label="$3"
    if grep -Fq "$term" "$RT/$file"; then
        echo "[FAIL] $label found in $file -- one-sided overwrite reintroduced (A4 regression)"
        fail=1
    else
        echo "[PASS] $label absent in $file"
    fi
}

# Both twins must compute the intersection of the two restriction components.
require_term "$C_TWIN"    "m->env & m->manifest"              "C twin intersection (env & manifest)"
require_term "$LLVM_TWIN" "m->env & m->manifest" "LLVM twin intersection (env & manifest)"

# Both twins must keep the env and manifest components separate (a single fused
# grant cannot express intersection with reset -- the pre-A4 shape).
require_term "$LLVM_TWIN" "PgyCapMasks *m"       "LLVM twin context mask component"
require_term "$LLVM_TWIN" "pgy_cap_context_masks" "LLVM twin context owner"

# The fused single-mask global from before A4 must be gone on the LLVM twin;
# its presence means the overwrite policy was restored.
reject_term "$LLVM_TWIN" "g_pgy_cap_granted" "pre-A4 fused grant global g_pgy_cap_granted"
reject_term "$LLVM_TWIN" "g_pgy_cap_manifest" "process-global manifest owner"
reject_term "$LLVM_TWIN" "g_pgy_cap_env" "process-global env owner"

if [ "$fail" -ne 0 ]; then
    echo "[cap-env-manifest-parity] FAIL -- the two capability twins disagree on env vs manifest"
    exit 1
fi
echo "[cap-env-manifest-parity] both twins intersect env & manifest (fail-closed, backend-identical)"
exit 0
