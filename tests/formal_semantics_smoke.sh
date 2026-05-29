#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"

require_file() {
    local path="$1"
    local label="$2"

    if [[ ! -e "$path" ]]; then
        echo "missing $label" >&2
        exit 1
    fi
}

require_term() {
    local path="$1"
    local label="$2"
    local term="$3"

    if [[ "$path" == "$CHECKLIST_PATH" ]]; then
        if ! pgy_beta_checklist_contains "$term"; then
            echo "$label shards missing required term: $term" >&2
            exit 1
        fi
        return 0
    fi

    if ! grep -Fq -- "$term" "$path"; then
        echo "$label missing required term: $term" >&2
        exit 1
    fi
}

forbid_term() {
    local path="$1"
    local label="$2"
    local term="$3"

    if grep -Fq -- "$term" "$path"; then
        echo "$label contains forbidden term: $term" >&2
        exit 1
    fi
}

require_terms() {
    local path="$1"
    local label="$2"
    local term

    while IFS= read -r term; do
        [[ -z "$term" ]] && continue
        require_term "$path" "$label" "$term"
    done
}

INDEX_PATH="$ROOT_DIR/docs/102_formal_semantics_and_proof_obligations.md"
PROOF_DIR="$ROOT_DIR/docs/semantics"
RIGOR_AUDIT_PATH="$ROOT_DIR/docs/118_slot_model_rigor_audit.md"
CHECKLIST_PATH="$ROOT_DIR/docs/100_beta_readiness_checklist.md"
SEMANTIC_DESIGN_PATH="$ROOT_DIR/docs/14_semantic_analyzer_design.md"
SLOT_MANAGER_PATH="$ROOT_DIR/src/runtime/slot_manager.h"
SLOT_MACROS_PATH="$ROOT_DIR/src/runtime/pgy_runtime_slot_macros.h"
TODO_PATH="$ROOT_DIR/TODO.md"
CI_PATH="$ROOT_DIR/.github/workflows/ci.yml"
README_PATH="$ROOT_DIR/README.md"
SLOT_COQ="$PROOF_DIR/proofs/SlotCalculus.v"
EFFECT_SOURCE_PATH="$ROOT_DIR/src/semantic/type_effects.c"
EFFECT_FLOW_PATH="$ROOT_DIR/src/semantic/type_checker_flow_effects.c"
EFFECT_TEST_A_PATH="$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_a.cases.h"
EFFECT_TEST_B_PATH="$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_b.cases.h"
B0_PROVENANCE_TEST_PATH="$ROOT_DIR/src/tests/semantic/test_semantic_b0_provenance.cases.h"
BUILTINS_STDLIB_BODY_PATH="$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_body.c"
FUNC_DECL_PATH="$ROOT_DIR/src/semantic/type_checker_func_decl.c"

require_file "$INDEX_PATH" "docs/102_formal_semantics_and_proof_obligations.md"
require_file "$PROOF_DIR" "docs/semantics proof folder"
require_file "$RIGOR_AUDIT_PATH" "docs/118_slot_model_rigor_audit.md"
require_file "$CHECKLIST_PATH" "docs/100_beta_readiness_checklist.md"
require_file "$SEMANTIC_DESIGN_PATH" "docs/14_semantic_analyzer_design.md"
require_file "$SLOT_MANAGER_PATH" "src/runtime/slot_manager.h"
require_file "$SLOT_MACROS_PATH" "src/runtime/pgy_runtime_slot_macros.h"
require_file "$TODO_PATH" "TODO.md"
require_file "$CI_PATH" ".github/workflows/ci.yml"
require_file "$README_PATH" "README.md"
require_file "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v"
require_file "$EFFECT_SOURCE_PATH" "src/semantic/type_effects.c"
require_file "$EFFECT_FLOW_PATH" "src/semantic/type_checker_flow_effects.c"
require_file "$EFFECT_TEST_A_PATH" "src/tests/semantic/test_semantic_effects_part_a.cases.h"
require_file "$EFFECT_TEST_B_PATH" "src/tests/semantic/test_semantic_effects_part_b.cases.h"
require_file "$B0_PROVENANCE_TEST_PATH" "src/tests/semantic/test_semantic_b0_provenance.cases.h"
require_file "$BUILTINS_STDLIB_BODY_PATH" "src/semantic/type_checker_builtins_stdlib_body.c"
require_file "$FUNC_DECL_PATH" "src/semantic/type_checker_func_decl.c"

require_terms "$PROOF_DIR/README.md" "docs/semantics/README.md" <<'TERMS'
Status: `beta-proof-obligation`
Every stable beta feature must be represented in this folder
Stable proof scope:
Out of beta proof scope:
Regression tests, smoke tests, and backend compare runs are proof evidence, not proof itself.
Borrow-checker-equivalent safety: only through the combined ownership
Slot alone is not advertised as a borrow checker.
08_slot_capability_calculus.md
proofs/SlotCalculus.v
not beta-closure evidence unless a CI
Generic contracts
Ownership: anchored slot-handle boundary subset only.
Runtime observability
Backends: MIR-equivalent C and LLVM behavior
AIR abstraction safety
Slot capability calculus
Full quantum resource model.
Higher-kinded types and full FP functor/applicative/monad laws.
GPU/Spray, Skia/render graph
TERMS

require_terms "$PROOF_DIR/00_proof_contract.md" "docs/semantics/00_proof_contract.md" <<'TERMS'
## Semantic Domains
## Core Judgments
## Static Boundary vs Runtime Existence
static rejection  = unsafe transition across a known boundary
runtime validate  = dynamic existence/state of a resource handle
static proofs cover unsafe transitions; runtime proofs cover dynamic
### Type Preservation
### Progress
### Failure Separation
### Backend Observational Equivalence
`PanicState`
TERMS

require_terms "$PROOF_DIR/01_intent_world_zone.md" "docs/semantics/01_intent_world_zone.md" <<'TERMS'
Keywords: `intent`, `world`, `zone`, `subject`, `authority`, `handoff`.
## Theorem: Authority Soundness
## Theorem: Intent Step Progress
## Theorem: World/Zone Frontier Termination
TERMS

require_terms "$PROOF_DIR/02_relation_effect_projection.md" "docs/semantics/02_relation_effect_projection.md" <<'TERMS'
Keywords: `relation`, `effect`, `projection`, `refresh`, `publish`, `bind`.
## Theorem: Projection Freshness
## Theorem: Effect Conflict Soundness
Beta-stable partial order contract:
closure        = collapse >= nondeterministic
join(a, b)     = closure(a) union closure(b)
meet(a, b)     = closure(a) intersection closure(b)
authority      = secure requires authority provenance
resource edge  = secure | remote | collapse touch a resource boundary
conflict       = secure conflicts with remote | collapse | nondeterministic
type_effect_mask_conflicts
## Theorem: Projection Diagnostic Completeness
TERMS

require_terms "$EFFECT_SOURCE_PATH" "src/semantic/type_effects.c" <<'TERMS'
type_effect_mask_closure
type_effect_mask_join
type_effect_mask_meet
type_effect_mask_requires_authority
type_effect_mask_touches_resource_boundary
type_effect_mask_conflicts
EFFECT_COLLAPSE
EFFECT_NONDETERMINISTIC
EFFECT_SECURE
EFFECT_REMOTE
TERMS

require_terms "$EFFECT_FLOW_PATH" "src/semantic/type_checker_flow_effects.c" <<'TERMS'
semantic_warning_with_hints
PGY_CODE_SEM_EFFECT_CONFLICT
PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO
PGY_FIX_SPLIT_EFFECT_FAMILIES
type_effect_mask_conflicts
Control-flow branch/join combines conflicting effect classes
Reason:
Fix:
TERMS

require_terms "$EFFECT_TEST_A_PATH" "src/tests/semantic/test_semantic_effects_part_a.cases.h" <<'TERMS'
effect-partial-order: collapse is a superset of nondeterministic by closure
effect-partial-order: secure and remote are incomparable
effect-partial-order: joined secure|remote is a superset of each side
type_effect_mask_requires_authority
type_effect_mask_touches_resource_boundary
type_effect_mask_conflicts
TERMS

require_terms "$EFFECT_TEST_B_PATH" "src/tests/semantic/test_semantic_effects_part_b.cases.h" <<'TERMS'
effect-partial-order: disjoint branch effects join into combined contract
effect-conflict: secure and remote combination emits warning
effect-conflict: secure and collapse combination emits warning
effect-conflict: non-adjacent branch combination still emits warning
TERMS

require_terms "$B0_PROVENANCE_TEST_PATH" "src/tests/semantic/test_semantic_b0_provenance.cases.h" <<'TERMS'
branch effect conflict warning reports reason and fix
if branch effect conflict warning reports branch provenance
then branch contributes 'secure'
else branch contributes 'remote'
TERMS

forbid_term "$BUILTINS_STDLIB_BODY_PATH" "src/semantic/type_checker_builtins_stdlib_body.c" "EFFECT_NONDETERMINISTIC | EFFECT_COLLAPSE"
forbid_term "$FUNC_DECL_PATH" "src/semantic/type_checker_func_decl.c" "semantic_warning_code(ctx, PGY_CODE_SEM_EFFECT_CONFLICT"

require_terms "$PROOF_DIR/03_generics_modules_dag.md" "docs/semantics/03_generics_modules_dag.md" <<'TERMS'
Keywords and surfaces: `where`, `ability`, generic parameters, default type arguments, module imports/exports, type-resolution DAG.
## Theorem: Generic Contract Soundness
## Theorem: DAG Soundness
## Theorem: Module Visibility Non-Interference
TERMS

require_terms "$PROOF_DIR/04_ownership_abi.md" "docs/semantics/04_ownership_abi.md" <<'TERMS'
Keywords and surfaces: `own`, `ref`, anchored slot handles, slot boundaries, runtime ABI ownership.
## Theorem Boundary: Slot Runtime Safety Is Not Borrow Safety
`Slot runtime safety`
`Borrow-checker-equivalent safety`
## Theorem: Anchored Ownership Safety
## Theorem: Secure Token Unforgeability
## Theorem: Authority Transfer Single-Owner
## Theorem: Arena Lifetime Non-Escape
## Theorem: ABI Ownership Parity
TERMS

require_terms "$PROOF_DIR/05_parallel_execution.md" "docs/semantics/05_parallel_execution.md" <<'TERMS'
Keywords: `parallel`
## Theorem: Parallel Conflict Soundness
## Theorem: Execution Backend Parity
TERMS

require_terms "$PROOF_DIR/06_backend_parity.md" "docs/semantics/06_backend_parity.md" <<'TERMS'
Surfaces: MIR, declaration inventory, C backend, LLVM backend, runtime ABI.
## Theorem: MIR Source-of-Truth
## Theorem: Backend Observational Equivalence
## Theorem: Runtime Panic Parity
## Theorem: Structured Backend Failure
TERMS

require_terms "$PROOF_DIR/07_air_abstraction_safety.md" "docs/semantics/07_air_abstraction_safety.md" <<'TERMS'
Stable surface: AIR (Abstraction Intent Representation)
## Theorem: AIR Synthesis Read-Only
## Theorem: Intent Node Coverage
## Theorem: Boundary Closure
## Theorem: Drift Detection Soundness
## Theorem: Codegen Non-Impact
PGY_SEM_INTENT_BOUNDARY_DRIFT
TERMS

require_terms "$PROOF_DIR/08_slot_capability_calculus.md" "docs/semantics/08_slot_capability_calculus.md" <<'TERMS'
Status: `IN PROGRESS / PROOF-SKETCH`
## Stable Surface
## Negative Claim: Slot Is Not A Borrow Checker
Slot = runtime capability + generation + token + pin-state safety.
borrow-checker-equivalent is the static ownership/CFG layer above Slot.
static rejection  = unsafe transition across a known boundary
runtime validate  = dynamic existence/state of a resource handle
Runtime validation covers generation freshness
the ownership/CFG/AIR layers prove which boundary transitions are accepted
## Semantic Domains
## Theorem: ABA Safety
## Theorem: Token Unforgeability
## Theorem: Pin Non-Eviction
## Bridge Obligation: Borrow-Checker-Equivalent Safety
NoEscape(view, region)
NoSuspend(view, region)
WriteExclusive(slot, region)
proof sketch, not completed
Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` now reaches
HIR and MIR as explicit pin-region metadata
pin-unpin-cleanup-edge
PgyPinnedSlotView_*
PgyPinnedSecureSlotView_*
pgy_pin_read_*
pgy_pin_write_*
pgy_unpin_*
TERMS

require_terms "$CHECKLIST_PATH" "docs/100_beta_readiness_checklist.md" <<'TERMS'
docs/102_formal_semantics_and_proof_obligations.md
docs/semantics/
Do not advertise mechanized proof for beta
Do not advertise "Slot as borrow checker"
Canonical semantic split
runtime validation covers dynamic existence/state
TERMS

require_terms "$TODO_PATH" "TODO.md" <<'TERMS'
docs/102_formal_semantics_and_proof_obligations.md
docs/semantics/
proof evidence
beta proof line honest
TERMS

require_terms "$INDEX_PATH" "docs/102_formal_semantics_and_proof_obligations.md" <<'TERMS'
docs/semantics/README.md
docs/semantics/08_slot_capability_calculus.md
docs/semantics/proofs/SlotCalculus.v
docs/118_slot_model_rigor_audit.md
TERMS

forbid_term "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" "Status: Beta"
forbid_term "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" "Level 4 Mechanized Proof"
forbid_term "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" "Safe Core Mechanized Proof"

require_terms "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" <<'TERMS'
Status: proof-sketch; not beta-closure evidence unless checked by CI
Negative scope: this file does not prove Rust-style borrow checking
Require Import Coq.Arith.PeanoNat.
Lemma stale_handle_read_impossible
Lemma handle_read_requires_issued_token
Lemma unissued_token_read_impossible
Lemma handle_pin_requires_issued_token
Lemma unissued_token_pin_impossible
Definition FreshSlotId
Lemma zero_slot_id_claim_impossible
Lemma max_slot_id_claim_impossible
Lemma tampered_view_unpin_impossible
Lemma double_unpin_impossible
Lemma pin_non_eviction
Qed.
TERMS

for stale_doc in \
    "$PROOF_DIR/00_proof_contract.md" \
    "$PROOF_DIR/08_slot_capability_calculus.md" \
    "$PROOF_DIR/README.md"; do
    forbid_term "$stale_doc" "$stale_doc" "proof sketch for one invariant"
    forbid_term "$stale_doc" "$stale_doc" "proof sketch for two invariants"
    forbid_term "$stale_doc" "$stale_doc" "minimal mechanized sketch for two invariants"
done

require_terms "$SEMANTIC_DESIGN_PATH" "docs/14_semantic_analyzer_design.md" <<'TERMS'
Slot Resource-Boundary Analyzer
Slot analysis is not Rust-style lifetime analysis.
Slot is the source-level modular resource boundary.
TERMS

require_terms "$SLOT_MANAGER_PATH" "src/runtime/slot_manager.h" <<'TERMS'
source-level resource boundary
pointer/address ownership
backend
TERMS

require_terms "$SLOT_MACROS_PATH" "src/runtime/pgy_runtime_slot_macros.h" <<'TERMS'
source-level resource boundary
pointer/address ownership
backend
TERMS

require_terms "$RIGOR_AUDIT_PATH" "docs/118_slot_model_rigor_audit.md" <<'TERMS'
Slot Is Not a Borrow Checker
Slot Is A Modular Resource Boundary
Pergyra does not expose memory as address ownership.
Pergyra exposes memory as a modular resource boundary.
A Slot is the stable language-level boundary; the backend handle below it is replaceable.
Slot = address abstraction + ownership boundary + capability gate + replaceable backend handle
The borrow-checker-equivalent in Pergyra is not Slot
Current `WriteView<T>` same-slot exclusivity is enforced
source-level typed-view pin blocks reject suspension and transport boundaries
Active source-level typed-view pin blocks; `docs/74`; diagnostics + backend compare
The block-scoped source `pin` surface is active for typed views.
pin_read_view_block
pin_secure_read_view_block
pin_mixed_read_view_sequence
pin_write_view_block
pin_secure_write_view_block
straight-line typed-view read/write parity across
The comparison to Rust 1.0 at launch is a scope-bounded analogy
TERMS

for path in "$README_PATH" "$TODO_PATH"; do
    forbid_term "$path" "$path" "Slot Lifetime Analyzer"
    forbid_term "$path" "$path" "slot???"
    forbid_term "$path" "$path" "Slot is Pergyra's borrow checker"
    forbid_term "$path" "$path" "Slot proves borrow safety"
    forbid_term "$path" "$path" "Slot proves Rust-style borrow checking"
    forbid_term "$path" "$path" "Pergyra provides Rust-level memory safety"
    forbid_term "$path" "$path" "Pergyra guarantees Rust-level memory safety"
    forbid_term "$path" "$path" "pin blocks statically reject crossing await"
    forbid_term "$path" "$path" "WriteView<T> exclusive is not enforced"
done

global_forbid_terms="$(mktemp)"
global_forbid_files="$(mktemp)"
global_forbid_matches="$(mktemp)"
cat >"$global_forbid_terms" <<'TERMS'
Slot Lifetime Analyzer
slot???
Slot is Pergyra's borrow checker
Slot proves borrow safety
Slot proves Rust-style borrow checking
Pergyra provides Rust-level memory safety
Pergyra guarantees Rust-level memory safety
pin blocks reject crossing await
pin blocks statically reject crossing await
WriteView<T> exclusive is not enforced
WriteView<T> is not enforced
TERMS

while IFS= read -r -d '' path; do
    case "$path" in
        "$CHECKLIST_PATH"|"$RIGOR_AUDIT_PATH"|"$PROOF_DIR/08_slot_capability_calculus.md"|"$PROOF_DIR/README.md")
            continue
            ;;
    esac
    printf '%s\0' "$path" >>"$global_forbid_files"
done < <(find "$ROOT_DIR/docs" -name '*.md' -print0)

if [[ -s "$global_forbid_files" ]] &&
    xargs -0 grep -nF -f "$global_forbid_terms" -- \
        <"$global_forbid_files" >"$global_forbid_matches"; then
    cat "$global_forbid_matches" >&2
    echo "docs contain forbidden Slot safety claim(s)" >&2
    exit 1
fi

require_terms "$CI_PATH" ".github/workflows/ci.yml" <<'TERMS'
sudo apt-get install -y gcc make llvm-dev llvm coq
make ci-linux
TERMS

echo "formal semantics smoke: ok"

if command -v coqc >/dev/null 2>&1; then
    coq_timeout="${PGY_COQ_SMOKE_TIMEOUT_SECONDS:-60}"
    if command -v timeout >/dev/null 2>&1; then
        (cd "$ROOT_DIR" && timeout "$coq_timeout" coqc docs/semantics/proofs/SlotCalculus.v)
    else
        (cd "$ROOT_DIR" && coqc docs/semantics/proofs/SlotCalculus.v)
    fi
    echo "formal semantics Coq smoke: ok"
else
    echo "formal semantics Coq smoke: skipped (coqc not found)"
fi
