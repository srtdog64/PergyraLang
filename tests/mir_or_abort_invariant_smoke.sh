#!/usr/bin/env bash
#
# MIR-or-abort invariant gate (single source of truth precondition).
#
# The backend slot/inventory views carry AST-fallback arms guarded by
# requires_mir_metadata, which resolves to active_has_mir(ctx), i.e.
# ctx->mir != NULL. Those fallbacks are dead in production only because the
# driver aborts compilation when MIR lowering yields no program: codegen never
# runs with a NULL MIR. This gate locks that precondition. If the driver is
# ever changed to degrade to an AST-only path instead of aborting, the dead
# fallbacks become live again and the source_ast retirement is unsafe; this
# test fails first and explains why.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

DRIVER="src/compiler/driver_app.c"
status=0

if [ ! -f "$DRIVER" ]; then
    echo "[mir-or-abort] FAIL: $DRIVER not found" >&2
    exit 1
fi

# The lowering call and its NULL guard must both be present.
if ! grep -q 'mir = mir_lower(' "$DRIVER"; then
    echo "[mir-or-abort] FAIL: mir_lower call not found in driver" >&2
    status=1
fi

# Extract the block immediately following the mir_lower assignment and confirm
# a NULL check that diverts to cleanup (abort), not a fall-through to codegen.
guard="$(awk '
    /mir = mir_lower\(/ { seen = 1; window = 0 }
    seen { lines[++window] = $0 }
    seen && window > 12 { exit }
    END {
        joined = ""
        for (i = 1; i <= window; i++) joined = joined "\n" lines[i]
        print joined
    }
' "$DRIVER")"

if ! printf '%s' "$guard" | grep -q 'mir == NULL'; then
    echo "[mir-or-abort] FAIL: no 'mir == NULL' guard after mir_lower" >&2
    status=1
fi
if ! printf '%s' "$guard" | grep -Eq 'goto cleanup|stage_fail'; then
    echo "[mir-or-abort] FAIL: NULL-MIR path does not abort (no cleanup/stage_fail)" >&2
    status=1
fi

if [ "$status" -eq 0 ]; then
    echo "[mir-or-abort] driver aborts on NULL MIR; backend AST fallbacks remain dead"
fi
exit "$status"
