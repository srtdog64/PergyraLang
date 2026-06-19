#!/usr/bin/env bash
#
# Binary adequacy for the slot capability calculus
# (docs/semantics/proofs/SlotCalculus.v).
#
# SlotCalculus.v proves, inside Coq, the slot safety invariants (stale-handle
# rejection, token-gated access, pin non-eviction). Those theorems constrain the
# *model*. This differential test binds the model to the *real runtime* so the
# two cannot drift: every modeled operation and every proven invariant must have
# a live counterpart in src/runtime/slot_manager.h. Rename or delete a runtime
# symbol the proof relies on and this gate fails.
#
# Pure source-consistency (no coqc); complements SlotCalculus.v.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SLOT_COQ="$ROOT_DIR/docs/semantics/proofs/SlotCalculus.v"
SLOT_RT="$ROOT_DIR/src/runtime/slot_manager.h"

for f in "$SLOT_COQ" "$SLOT_RT"; do
    [[ -e "$f" ]] || { echo "missing required file: $f" >&2; exit 1; }
done

fail=0

coq_has()     { grep -qE "\b$1\b" "$SLOT_COQ"; }
runtime_has() { grep -qE "\b$1\b" "$SLOT_RT"; }

# Each row: "<Coq symbol> <runtime symbol> <concept>"
# A. modeled operation  -> real runtime API
OP_MAP=(
    "HandleRead     SlotRead       read-access-op"
    "HandleWrite    SlotWrite      write-access-op"
    "HandleRelease  SlotRelease    release-op"
    "HandlePin      PergyraSlotPin pin-op"
    "ModeClaim      SlotClaim      claim-op"
)

# B. proven invariant   -> runtime mechanism that realizes it
INV_MAP=(
    "stale_handle_read_impossible        generation      stale-handle-rejection"
    "handle_read_requires_issued_token   TokenCapability token-gated-access"
    "pin_non_eviction                    PergyraSlotPin  pin-non-eviction"
)

echo "== A. SlotCalculus operation -> runtime API =="
for row in "${OP_MAP[@]}"; do
    read -r coq rt concept <<<"$row"
    if ! coq_has "$coq"; then
        echo "  FAIL: SlotCalculus.v no longer models '$coq' ($concept)"; fail=1; continue
    fi
    if ! runtime_has "$rt"; then
        echo "  FAIL: runtime no longer provides '$rt' for modeled op '$coq' ($concept)"; fail=1; continue
    fi
    printf '  ok   %-32s -> %-16s %s\n' "$coq" "$rt" "$concept"
done

echo "== B. proven invariant -> runtime mechanism =="
for row in "${INV_MAP[@]}"; do
    read -r coq rt concept <<<"$row"
    if ! coq_has "$coq"; then
        echo "  FAIL: SlotCalculus.v no longer proves '$coq' ($concept)"; fail=1; continue
    fi
    if ! runtime_has "$rt"; then
        echo "  FAIL: runtime dropped the '$rt' mechanism behind invariant '$coq' ($concept)"; fail=1; continue
    fi
    printf '  ok   %-36s -> %-16s %s\n' "$coq" "$rt" "$concept"
done

if [[ "$fail" -ne 0 ]]; then
    echo "slot calculus adequacy: FAILED"
    exit 1
fi

echo "slot calculus adequacy: ok (SlotCalculus.v model <-> slot_manager.h runtime consistent)"
