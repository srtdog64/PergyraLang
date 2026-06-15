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

RATCHET="tests/ast_read_surface_ratchet.txt"
if [ ! -f "$RATCHET" ]; then
    echo "[ast-read-surface] FAIL missing ratchet spec: $RATCHET" >&2
    exit 1
fi

tmp_specs="$(mktemp "${TMPDIR:-/tmp}/pgy-ast-read-specs.XXXXXX")"
cleanup_tmp() {
    rm -f "$tmp_specs"
}
trap cleanup_tmp EXIT

bad_line="$(awk -F'|' 'NF != 4 || $1 == "" || $2 == "" || $3 == "" || $4 == "" { print NR; exit }' "$RATCHET")"
if [ -n "$bad_line" ]; then
    echo "[ast-read-surface] FAIL malformed ratchet line ${bad_line}: $RATCHET" >&2
    exit 1
fi

sort -u "$RATCHET" > "$tmp_specs"

status=0
while IFS='|' read -r kind pattern ceiling scope; do
    [ -n "$kind" ] || continue
    count="$((grep -R -F -o "$pattern" "$scope" --include='*.c' 2>/dev/null || true) \
        | wc -l | tr -d ' ')"

    if [ "$count" -gt "$ceiling" ]; then
        echo "[ast-read-surface] FAIL ${kind}: ${count} AST reads > ceiling ${ceiling} in ${scope} (surface grew)" >&2
        grep -rnF "$pattern" "$scope" --include=*.c | sed 's/^/    /' >&2
        status=1
    else
        echo "[ast-read-surface] ${kind}: ${count}/${ceiling} reads (ratchet ok)"
    fi
done < "$tmp_specs"

if [ "$status" -eq 0 ]; then
    echo "[ast-read-surface] backend AST-read surface within ratchet ceilings"
fi
exit "$status"
