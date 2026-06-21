#!/usr/bin/env bash
#
# IR minimality adequacy smoke.
#
# The Coq model in docs/semantics/proofs/IRMinimality.v proves minimality for
# the declared reads-from graph. This smoke binds that model to live compiler
# source facts so the proof cannot drift into a stale architecture diagram.
#
# This is source-consistency evidence, not whole-architecture soundness.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_file() {
    local path="$1"
    local label="$2"
    if [[ ! -e "$ROOT_DIR/$path" ]]; then
        echo "[ir-minimality] missing $label: $path" >&2
        exit 1
    fi
}

require_text() {
    local path="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$path"; then
        echo "[ir-minimality] $path missing: $text" >&2
        exit 1
    fi
}

line_no() {
    local path="$1"
    local text="$2"
    local line
    line="$(grep -nF -- "$text" "$ROOT_DIR/$path" | head -n 1 | cut -d: -f1 || true)"
    if [[ -z "$line" ]]; then
        echo "[ir-minimality] $path missing ordered term: $text" >&2
        exit 1
    fi
    printf '%s\n' "$line"
}

last_line_no() {
    local path="$1"
    local text="$2"
    local line
    line="$(grep -nF -- "$text" "$ROOT_DIR/$path" | tail -n 1 | cut -d: -f1 || true)"
    if [[ -z "$line" ]]; then
        echo "[ir-minimality] $path missing ordered term: $text" >&2
        exit 1
    fi
    printf '%s\n' "$line"
}

require_before() {
    local path="$1"
    local first="$2"
    local second="$3"
    local first_line
    local second_line
    first_line="$(line_no "$path" "$first")"
    second_line="$(line_no "$path" "$second")"
    if (( first_line >= second_line )); then
        echo "[ir-minimality] $path order drift: '$first' must precede '$second'" >&2
        exit 1
    fi
}

require_before_last() {
    local path="$1"
    local first="$2"
    local second="$3"
    local first_line
    local second_line
    first_line="$(line_no "$path" "$first")"
    second_line="$(last_line_no "$path" "$second")"
    if (( first_line >= second_line )); then
        echo "[ir-minimality] $path order drift: '$first' must precede the last '$second'" >&2
        exit 1
    fi
}

require_no_codegen_dependency() {
    local matches
    matches="$(
        grep -RInE -- \
            '\b(AIRProgram|DIRProgram|air_(synthesize|verify|collect|append|boundary|evidence|dump)|dir_(lower|validate|find|dump))\b' \
            "$ROOT_DIR/src/codegen" \
            || true
    )"
    if [[ -n "$matches" ]]; then
        echo "[ir-minimality] backend codegen reopened DIR/AIR as a codegen dependency" >&2
        printf '%s\n' "$matches" >&2
        exit 1
    fi
}

require_file "docs/semantics/proofs/IRMinimality.v" "IR minimality Coq proof"
require_file "docs/semantics/proofs/IRMinimality.md" "IR minimality audit"
require_file "src/compiler/driver_app.c" "compiler driver"
require_file "src/compiler/rir.h" "RIR API"
require_file "src/compiler/rir_flow.c" "RIR flow enrichment"
require_file "src/compiler/rir_validation.c" "RIR validator"
require_file "src/compiler/mir.h" "MIR API"
require_file "src/compiler/mir.c" "MIR lowering"
require_file "src/compiler/air.h" "AIR API"
require_file "src/compiler/compiler.h" "backend IR bundle"

require_text "docs/semantics/proofs/IRMinimality.v" "Inductive Reads : Node -> Node -> Prop"
require_text "docs/semantics/proofs/IRMinimality.v" "ReadsRIR_HIR : Reads NRIR NHIR"
require_text "docs/semantics/proofs/IRMinimality.v" "ReadsMIR_HIR : Reads NMIR NHIR"
require_text "docs/semantics/proofs/IRMinimality.v" "ReadsMIR_RIR : Reads NMIR NRIR"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem codegen_minimum_is_three"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem two_layers_suffice_when_deferred"
require_text "docs/semantics/proofs/IRMinimality.v" "Inductive VerificationRequirement"
require_text "docs/semantics/proofs/IRMinimality.v" "AIRWitness"
require_text "docs/semantics/proofs/IRMinimality.v" "FunctorHKTWitness"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem air_witness_adequate"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem functor_hkt_not_adequate"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem air_is_minimal_witness_set"

require_text "docs/semantics/proofs/IRMinimality.md" "three codegen IRs (HIR/RIR/MIR)"
require_text "docs/semantics/proofs/IRMinimality.md" "AIR is *not* a codegen IR"
require_text "docs/semantics/proofs/IRMinimality.md" "flow-sensitive resource checking happens at the resource (RIR) layer"
require_text "docs/semantics/proofs/IRMinimality.md" "AIR vs HKT/Functor: minimal verifier, not bigger abstraction"
require_text "docs/semantics/proofs/IRMinimality.md" "AIR, not HKT/Functor, is the minimal verification"
require_text "docs/semantics/proofs/IRMinimality.md" "minimum verifier surface"
require_text "docs/semantics/proofs/IRMinimality.md" "Live overfit test"
require_text "docs/semantics/README.md" "AIR witness minimality claim"
require_text "docs/04_generic_design.md" "AIR is the minimal verifier for intent/effect/authority/coordination"
require_text "docs/04_generic_design.md" "functor_hkt_not_adequate"

require_text "src/compiler/rir.h" "rir_enrich_with_hir_flow(RIRProgram *rir, const HIRProgram *hir"
require_text "src/compiler/rir_flow.c" "rir_enrich_scope_with_hir_flow(RIRScope *scope, const HIRRoutine *hir_routine)"
require_text "src/compiler/rir_flow.c" "!hir_routine->has_cfg"
require_text "src/compiler/rir_flow.c" "hir_routine->cfg.blocks[i].rpo_index"
require_text "src/compiler/rir_flow.c" "rir_merge_states_for_kind(summary->resource_kind"
require_text "src/compiler/rir_flow.c" "merged_from_join"
require_text "src/compiler/rir_flow.c" "widened_by_loop"
require_text "src/compiler/rir_validation.c" "rir_validate(const RIRProgram *rir"
require_text "src/compiler/rir_validation.c" "rir_merge_state_for_kind"

require_text "src/compiler/mir.h" "mir_lower(const HIRProgram *hir, const RIRProgram *rir"
require_text "src/compiler/mir.c" "mir_lower(const HIRProgram *hir, const RIRProgram *rir"
require_text "src/compiler/mir.c" "mir_find_matching_rir_scope(rir, hir_routine)"
require_text "src/compiler/mir.c" "hir_routine->has_cfg"

require_text "src/compiler/air.h" "AIRProgram *air_synthesize(const HIRProgram *hir"
require_text "src/compiler/air.h" "const DIRProgram *dir"
require_text "src/compiler/air.h" "const RIRProgram *rir"
require_text "src/compiler/compiler.h" "const HIRProgram *hir"
require_text "src/compiler/compiler.h" "const DIRProgram *dir"
require_text "src/compiler/compiler.h" "const RIRProgram *rir"
require_text "src/compiler/compiler.h" "const MIRProgram *mir"
if grep -Fq -- "AIRProgram *air" "$ROOT_DIR/src/compiler/compiler.h"; then
    echo "[ir-minimality] CompilerIRBundle must not carry AIR into backend codegen" >&2
    exit 1
fi

require_before "src/compiler/driver_app.c" "hir = hir_lower" "rir_enrich_with_hir_flow(rir, hir"
require_before "src/compiler/driver_app.c" "rir = rir_lower" "rir_enrich_with_hir_flow(rir, hir"
require_before "src/compiler/driver_app.c" "rir_enrich_with_hir_flow(rir, hir" "rir_validate(rir"
require_before "src/compiler/driver_app.c" "rir_validate(rir" "mir = mir_lower(hir, rir"
require_before "src/compiler/driver_app.c" "air = air_synthesize(hir, dir, rir" "air_verify(air"
require_before "src/compiler/driver_app.c" "mir = mir_lower(hir, rir" "air_collect_mir_evidence(air, mir"
require_before_last "src/compiler/driver_app.c" "air_collect_mir_evidence(air, mir" "air_verify(air"

require_no_codegen_dependency

echo "[ir-minimality] Coq reads-from model is bound to live compiler dependency shape"
