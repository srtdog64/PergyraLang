#!/usr/bin/env bash
#
# source_ast/source_decl SoT-debt inventory (Phase 2 dashboard).
#
# Reports, per file, how many declarations still reach through MIRProgram's
# source_ast provenance pin or the declaration-header source_decl accessor
# instead of MIR-owned metadata. This is a report, not a gate:
# tests/ast_read_surface_smoke.sh owns the ratchet that forbids regression.
# This view ranks where the remaining migration work lives so the Phase 2
# cutover can be driven hotspot first.
#
# Two groups are separated. The codegen group is the migration frontier: each
# read should move to a MIRDeclHeader accessor. The compiler group is the
# source_ast field's own plumbing (set on capture, freed in lifecycle, walked
# by the provenance shape) and only empties when the field itself is removed
# in the final Phase 2 step, after every codegen reader is gone.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

report_pattern_group() {
    group_label="$1"
    pattern="$2"
    shift
    shift
    total=0
    printf '  %s\n' "$group_label"
    for dir in "$@"; do
        while IFS= read -r file; do
            [ -n "$file" ] || continue
            n="$(grep -o "$pattern" "$file" 2>/dev/null | wc -l | tr -d " ")"
            printf '    %4d  %s\n' "$n" "$file"
            total=$((total + n))
        done <<EOF
$(grep -rl "$pattern" "$dir" --include=*.c 2>/dev/null | sort)
EOF
    done
    printf '    ----  group total: %d\n\n' "$total"
    return 0
}

echo "[source-ast-inventory] Phase 2 single-SoT debt by file"
echo
report_pattern_group "codegen source_ast (migration frontier: closed)" \
    source_ast src/codegen
report_pattern_group "compiler source_ast (field/scalar provenance tail)" \
    source_ast src/compiler
report_pattern_group "codegen source_decl (declaration-header payload boundary)" \
    mir_decl_header_source_decl src/codegen
report_pattern_group "compiler source_decl (declaration-header payload boundary)" \
    mir_decl_header_source_decl src/compiler
report_pattern_group "codegen routine source_decl (routine payload compatibility boundary)" \
    mir_routine_source_decl_of_type src/codegen

frontier_cluster="$((grep -rl source_ast src/codegen --include=*.c 2>/dev/null || true) \
    | { grep -E 'slot_view|inventory' || true; } \
    | xargs -r grep -o source_ast 2>/dev/null \
    | wc -l | tr -d ' ')"
echo "[source-ast-inventory] codegen inventory/slot_view hotspot: ${frontier_cluster} reads"
if [ "$frontier_cluster" -eq 0 ]; then
    echo "[source-ast-inventory] codegen inventory/slot_view hotspot closed"
else
    echo "[source-ast-inventory] drive these to 0 first, then lower the codegen ratchet ceiling"
fi
