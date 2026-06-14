#!/usr/bin/env bash
#
# AST-read surface observability gate (self-host maturity metric).
#
# The declaration-inventory migration drives backend codegen off the AST and
# onto MIR-owned metadata. This gate makes the remaining AST coupling a
# measured, ratchet-only metric: the per-kind count of direct AST reads in
# the named scope may only stay the same or shrink, never grow. When a count
# reaches its floor it is locked there, catching any regression that reaches
# back into the AST.
#
# Two metric families are tracked. The first is per-decl-kind direct AST
# accessor reads in codegen (e.g. enum variant metadata fallbacks). The
# second is the source_ast provenance backing that Phase 2 (single source of
# truth) retires: every MIRProgram declaration still pinned to its origin
# ASTNode. As declaration metadata moves onto MIR the source_ast count falls,
# so locking it here turns Phase 2 progress into a ratchet that cannot regress.
# Broad emission reads (zone/world body lowering off the AST) are deliberately
# not counted: those are legitimate lowering, not declaration-metadata debt.
#
# Runtime evidence (recorded 2026, 40 enum programs x C+LLVM backends): the
# enum AST reads are MIR-first fallback arms that fired 0 times -- the MIR
# variant metadata is always present in MIR-active builds. The structural
# counts below bound how much AST surface remains so the fallbacks can be
# retired kind by kind.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

status=0
while IFS='|' read -r kind pattern ceiling scope; do
    [ -n "$kind" ] || continue
    [ -n "$scope" ] || scope="src/codegen"
    count="$(grep -rho "$pattern" $scope --include=*.c 2>/dev/null | wc -l | tr -d ' ')"
    if [ "$count" -gt "$ceiling" ]; then
        echo "[ast-read-surface] FAIL ${kind}: ${count} AST reads > ceiling ${ceiling} in ${scope} (surface grew)" >&2
        grep -rn "$pattern" $scope --include=*.c | sed 's/^/    /' >&2
        status=1
    else
        echo "[ast-read-surface] ${kind}: ${count}/${ceiling} AST reads (ratchet ok)"
    fi
done <<'RATCHET_CEILINGS'
enum|ast_enum_variant|17|src/codegen
source_ast_codegen|source_ast|76|src/codegen
source_ast_compiler|source_ast|73|src/compiler
RATCHET_CEILINGS

if [ "$status" -eq 0 ]; then
    echo "[ast-read-surface] backend AST-read surface within ratchet ceilings"
fi
exit "$status"
