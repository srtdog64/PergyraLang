#!/usr/bin/env bash
#
# AST-read surface observability gate (self-host maturity metric).
#
# The declaration-inventory migration drives backend codegen off the AST and
# onto MIR-owned metadata. This gate makes the remaining AST coupling a
# measured, ratchet-only metric: the per-kind count of direct AST reads in
# src/codegen may only stay the same or shrink, never grow. When a count
# reaches its floor it is locked there, catching any regression that reaches
# back into the AST.
#
# Runtime evidence (recorded 2026, 40 enum programs x C+LLVM backends): the
# enum AST reads are MIR-first fallback arms that fired 0 times -- the MIR
# variant metadata is always present in MIR-active builds. The structural
# counts below bound how much AST surface remains so the fallbacks can be
# retired kind by kind.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# kind=accessor-prefix : baseline (ratchet ceiling; lower it as reads are removed)
declare -A BASELINE=(
    [enum]=17
)
declare -A PATTERN=(
    [enum]="ast_enum_variant"
)

status=0
for kind in "${!BASELINE[@]}"; do
    count="$(grep -rho "${PATTERN[$kind]}" src/codegen --include=*.c 2>/dev/null | wc -l | tr -d ' ')"
    ceiling="${BASELINE[$kind]}"
    if [ "$count" -gt "$ceiling" ]; then
        echo "[ast-read-surface] FAIL ${kind}: ${count} AST reads > ceiling ${ceiling} (surface grew)" >&2
        grep -rn "${PATTERN[$kind]}" src/codegen --include=*.c | sed 's/^/    /' >&2
        status=1
    else
        echo "[ast-read-surface] ${kind}: ${count}/${ceiling} AST reads (ratchet ok)"
    fi
done

if [ "$status" -eq 0 ]; then
    echo "[ast-read-surface] backend AST-read surface within ratchet ceilings"
fi
exit "$status"
