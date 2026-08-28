# Intent declaration rows BRIDGE closure — 2026-08-29

Status: `DONE` (local closure green; exact-head CI publication pending)

This directive fixes the next executable objective. It is temporary
coordination, not semantic authority, a registry verdict, or completion
evidence. Formatter checkpoint `1aed1291` is on `origin/main`; exact-head CI
run `33197495199` completed GREEN, so this bounded executable rung is released.

## Objective card

- Objective: change the existing `selfhost.intent_declaration_rows` authority
  from `BRIDGE` to `CLOSED` on the real installed/public source-C path. This
  must reduce the census from `CLOSED=52 BRIDGE=35 ACTIVE=1` to
  `CLOSED=53 BRIDGE=34 ACTIVE=1`; adding another authority row is forbidden.
- Priority: stable intent/routine identity, one complete admitted transition
  view, consumer migration, missing-fact failure before artifact publication,
  old reconstruction deletion, then patch size.
- Fact owner: semantic intent signature/transition facts own declaration
  identity, while the existing `SelfDirIntentFacts.steps` rows already carry
  same-epoch action/nested-call targets, guard/expect/post node identities,
  participant/authority names, and ordered compensation identities. The exact
  missing seam is carriage of those admitted DIR facts into
  `CodegenIntentExecutionView`; duplicating them as new semantic arrays would
  create a second source of truth. `MirIntentExecutionPlan` remains the later
  plan-owned view for the direct-MIR slice. Extend the existing execution view
  with admitted DIR facts; do not add a top-level authority.
- Direct bypass to delete: source-C currently supplies
  `CodegenIntentExecutionViewEmpty(true)` from
  `program_admitted_semantic_owner.pgy`; `CodegenIntentDefinitionBlock` then
  walks intent/step AST children, reparses step headers with
  `SemanticAstIntentStepHeaderFromText`, and reconstructs compensation arrays.
- Last legitimate consumer: `CodegenIntentDefinitionBlock` may emit from one
  admitted codegen view. It may not remain a co-owner of transition shape.
- Forbidden fallback: a present intent accepted with an empty execution view,
  AST-child step discovery, source-text header parsing, compensation AST
  rescanning, mixed plan/AST reads, or success after missing/crossed identity.
- Verification gate: extend
  `tests/self_hosted/parity/intent_guard_post_compensation_execution_owner.sh`
  through installed/public source-C. Require success/failure/ordered reverse
  compensation parity, missing/drifted/crossed fact mutations with no artifact,
  and static rejection of the deleted reconstruction terms.

## Completion evidence

- Source DIR is sealed to every carried row and the typed artifact epoch;
  codegen admission uses the sealed receipt without reopening AST/header data.
- Action execution uses `receiver_aliases`; `who_names` is retained separately
  for observability attribution, including multi-participant ranges.
- Fresh installed-driver parity, 11 negative modes, component/hard contracts,
  root compile, SoT census, progress, shell syntax, and diff checks are green.
- The authority row moved in place from `BRIDGE` to `CLOSED`; no authority row
  was added. Census is `CLOSED=53 BRIDGE=34 ACTIVE=1`.

## Edit boundary

- The active implementation scope, once released, is limited to the existing
  DIR intent fact/readiness owners, the intent execution codegen view, the
  semantic/direct-MIR adapters that construct that view,
  `program_admitted_semantic_owner.pgy`, the final intent codegen consumer, the
  focused gate, component ratchets, registry/Coq status projection, and
  progress/handoff documents.
- Module composition, query/cache, ABI observability, performance, and new
  authority rows are outside this rung. Parallel implementation on the same
  consumer is forbidden; read-only review may be delegated after the first
  executable slice is green.
