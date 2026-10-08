# DRV-2 demanded-leaf continuation review

Date: 2026-10-07 KST. Read-only review, rechecked by the integration owner.
Base: main `3658548d24bca3d721e4f1974ac7a10da99f7aa8` plus the preserved dirty
candidate. Original review graph:
`2f2ef0ac4574d110adad2e457429a442c64ccb88ecf2c488a8736522535b36e2`.
This report owns neither language semantics nor a successor rung or closure.

## Actual chain

1. `destructure_fact_owner.pgy` creates independent empty String stores.
2. `routine_lower_owner.pgy` dispatches a mutable owned builder in a loop.
   Let lowering changes CFG siblings; destructure lowering changes the demanded
   `cfg.instructions.destructure_facts.element_types|bindings` leaves.
3. `destructure_cfg_owner.pgy` and `routine_cfg_append_owner.pgy` detach named
   parents, invoke the leaf owner, and restore each parent before publication.
   `ArrayPushOwnedString` copies new elements; it does not promote borrowed or
   unknown pre-existing elements into uniform ownership.
4. `artifact_lower_owner.pgy` and `intent_routine_owner.pgy` accumulate target
   CFGs while the source remains readable through its final ABI receipt read.
   `routine_build_storage_lifetime_owner.pgy` then retires only that source.
5. Canonical IDs, validation and JSON publication consume the current program
   target generation. A native replacement driver is not an allowed fallback.

Two additional facts prevent a same-nominal/unchanged-cfg shortcut:

- Block -> Tracked -> If/While/For/Match -> Block is mutually recursive.
- `SelfMirRoutineExpressionRuntimeAbiAttach(own cfg)` returns a different
  nominal result whose declared `.cfg` field carries the current CFG. The
  builder restores that field. Type equality alone cannot express this edge.

## Missing owned-result contract

The backwards trace already retains the full physical path in
`path_field_ids/path_parent_rows`; generation and field transport currently
consume only its first field. The required local view is a conditional relation
from an exact input formal/path to an exact output return-path:

> Given a live, exclusive leaf with empty or uniformly owned elements, admitted
> changes and exact parent restoration preserve that authority at every normal
> return, continuing branch and backedge.

Sibling changes require declared-path non-overlap. Demanded-leaf changes need
the existing entry, owned-push and restoration facts. A zero-iteration loop
keeps its entry generation. Recursive components need all local preservation
obligations sealed under that same conditional invariant and a concrete caller
storage/origin anchor; a visited node is not ownership evidence. Constructor
wrappers must relate declared input/output field identities, not nominal names.
Uniqueness/reservation, alias/stale-view, restoration and publication still seal
the final plan. This is not a general C2 copy policy or a new query/cache track.

The native member-move deep-drop boundary explicitly preserves UNKNOWN for a
current parameter (`collection_ownership_fact.c`). Its admission and a native
positive execution are not substitutes for the missing self-host fact.

## Executable falsifier

`aggregate_owned_cfg_update_loop_probe.pgy` plus its functions file combines
zero/populated loops, mutual recursive calls, a different return wrapper,
owned-copy into a target, the source's last read/retirement, target reads and
terminal target cleanup. Identified native C/LLVM executions both produce:
`0, 3, 4, 4, first, even` with zero compiler errors/warnings. This is positive
behavior, not an independently instrumented allocation/retirement proof.

Current identified C/LLVM source analyzers both refuse at
`UpdateCfgAppend -> UpdateRowsAppend`, `aggregate_release_plan_unproved`, node
49. The paired shallow-copy mutation is analyzed only, never emitted/run. It
currently hits the same earlier missing contract, so it is not yet evidence
that shallow-copy rejection after owned-result admission works.

The graph2f source matrix separately passes sixteen positives and thirty-nine
falsifiers plus identity guards per C/LLVM, receipt
`.tmp/self_hosted/aggregate-release-source.mWZe8P`. This is bounded matrix
evidence, not complete sibling independence or the continuation contract.
Root subsequently reproduced intermediate aggregate aliases; their repair
passes nineteen positives/forty-four falsifiers and six physical-occurrence
owner controls per backend on graph b5e1380f, receipt
`.tmp/self_hosted/aggregate-release-source.p1JkX9`. A further direct leaf alias
(`LeafSiblingPair(names, names)`) is still accepted by those exact C/LLVM
analyzers: a sibling borrowed indexed write precedes demanded deep cleanup.
Nothing is emitted/run. Release uniqueness currently compares demanded rows
only; the reached repair must compare the demanded concrete storage generation
with every physical constructor placement, including undemanded fields, while
retaining readonly no-demand duplication and independent fresh definitions.
The b5e1380f seed was deliberately cancelled (inner 143), not completed green.
The actual whole-driver source observer timed out at its unchanged 300-second
budget without a verdict/output. Production seed/driver evidence remains a
separate executable boundary. No registry status, installed driver, GUI, CI,
commit or push claim follows from this report.

## Reconciled full-chain implementation plan

Latest verified graph is
`055515e1811a313a400d147c758d77bfdfba381d0664bb6179ef71f39c070286`.
Source receipt `.tmp/self_hosted/aggregate-release-source.RXxQBl` passes
twenty positives/forty-six falsifiers plus identity and six physical-occurrence
controls per C/LLVM. All four hash manifests revalidate. The direct leaf alias
is now refused; independent fresh Clone generations remain allowed. This
does not implement the owned-result relation: those exact analyzers still
refuse the compound positive at node 49, `UpdateCfgAppend -> UpdateRowsAppend`.
The following is a source-rechecked implementation plan, not admitted facts.

### Existing authority and missing facts

`SemanticAstBodyFlowOfBlock/OfStatement/OfIf/OfMatch/OfLoop` owns typed child
order, reachable arms, missing else/default, zero iteration and terminal forms.
Do not invent another control topology from `StatementFacts.body_scope_node_ids`:
that column is not populated for every control form. Statement types and
exhaustive-match facts already exist before collection admission in
`ast_body_type_bundle_assembly_owner.pgy`. Pass those facts to the conditional
relation producer without moving the later source-only BodyFlow verdict,
changing its diagnostic order, or guessing a lost MIR-rebuilt Never fact.

Keep one demanded-path relation family, scoped to the reached executable rung.
Its premise is concrete Live/exclusive/EmptyOrUniformOwned input storage;
its result preserves that predicate and lineage at the exact output path.
An executed owned-push gives UniformOwned, but an unchanged/zero-iteration
empty path remains Empty: do not manufacture an OWNED state merely from a
preservation relation. Own-return transport and inout restoration are distinct
boundary shapes; neither mode creates ownership permission.

| Required local view | Identity and obligation | Existing authority consumed |
| --- | --- | --- |
| Conditional relation | artifact digest/count, callable/body/formal, complete declared input/output paths, effect row | signatures, AggregateTrace path rows, constructor fields |
| Generation/location | symbolic input leaf, exact binding/definition/physical placement, live/consumed/retired state, outstanding parent restoration | assignment/place/definition facts; no new storage source |
| Control outcome | exact control/statement/occurrence, normal/return/break/continue/backedge/terminal outcome, predecessor/successor location map | BodyFlow control facts and typed expression surfaces |
| Dependency seal | physical call/argument edge, callee relation, path substitution, side-argument/alias obligations, all local verdicts, component seal | call inventory, borrowed-text/retention facts and existing transfer fences |

Use `owned_push_leaf_formals` and `AggregateOwnedPushLeafAtCompletion` only as
conditional content-preserving effects. They neither own pre-existing elements
nor prove an aggregate recursive component. Existing formal effect fixed-point
flags are not an owned-result SCC certificate. Pending/visited records may
schedule local obligations, never discharge a concrete caller requirement.
Seal every local transition and external dependency in the reached recursive
component under one invariant before its relation can be consumed. This is
safety of finite-returning executions, not a termination theorem. Concrete
caller lineage, placement uniqueness and ordered reservation still anchor it.

### Complete location and lifetime obligations

- Preserve the full declared path, not only the first CFG field. The probe
  carries `build.cfg.rows.names`; production carries
  `build.cfg.instructions.destructure_facts.element_types|bindings`.
  Typed sibling-path non-overlap is not permission to mutate a prefix or leaf.
- Relate different output wrappers explicitly: `UpdateCfgAppendResult` maps
  input `cfg.rows.names` to return `.cfg.rows.names`; production runtime ABI
  attachment maps its cfg input to `.cfg` of the declared result. Every normal
  success/error return must carry the current leaf. An error Bool/status is
  not a terminal ownership exemption. Consume only the selected physical
  constructor edge; a shared argument node is not another move receipt.
- The caller restores `build.cfg = result.cfg` before reading the sibling
  result status. That scalar read is valid, but a later reuse of result.cfg or
  a captured old descriptor is not. Transfer, alias and occurrence owners
  remain mandatory consumers of the admitted generation/path facts.
- If/Match joins only continuing arms. A missing else/default contributes the
  identity continuation when the existing control owner admits it. Return,
  break, continue and terminal exit carry distinct obligations.
- Compare loop entry, normal backedge, continue and break-exit location maps.
  Do not demand blanket parent restoration at every backedge: UpdateCfgCopy
  and actual destructure attach/append detach a carrier before the loop and
  restore it afterwards. The head invariant keeps that outer-scope carrier
  Live/exclusive/owned while the parent field is unavailable. Iteration-local
  detaches must reach the destination's expected location map; publication of
  a still-detached parent fails.
- Keep carried program target and iteration-local routine source distinct.
  The source factory at one SyntaxId can execute again on the next iteration;
  that is a fresh scoped invocation, not revival of the retired prior source.
  Existing FreshLocalGenerationAtUse/RepeatedLocalGenerationPrecedesUse are
  scope/order evidence only. Preserve exact source definition, physical call
  edge and loop scope rather than merging these tokens by SyntaxId alone.
- Copying a readonly source element adds an independently owned target element
  through the existing clone/borrow-only contracts; it does not move the source
  token. Keep source readable through its last ABI/ownership receipt consumer,
  then retire that source only. Program target continues to canonicalization,
  validation and publication. Terminal release is not an owned-result return.

### Consumer migration and forbidden paths

The complete chain is typed control/effect facts -> sealed conditional full-path
relation -> generation/return-path mapping -> concrete constructor lineage and
PlanUnique -> ordered reservation/push/drop -> restoration/alias/use/publication
finalization -> actual source's last MIR consumer and retirement.

Generation and lineage consumers must receive the same admitted relation.
Replace first-field-only mutable reconstruction, latest-textual-assignment
selection, single lexical restoration-count standing in for branch outcomes,
and repeated consumer-local body proof once the typed facts own those answers.
The existing exact unchanged-field certificate remains a valid narrow claim;
it cannot be a `new ? old` fallback for a missing mutable/full-path relation.
Do not delete storage reservation or physical exclusivity/publication guards.
No caller/type whitelist, trace-bound increase, UNKNOWN promotion, growing-CFG
clone, new general query/cache engine or unrelated SoT track is authorized.

Before production admission changes, retain the fixed compound positive and
its shallow-copy mutation, and add specifically reaching missing restoration,
cross-wired wrapper, continuing-arm/backedge, stale view, unsealed recursion
and iteration-source/target controls. A negative that still hits the earlier
legitimate leaf refusal is not proof of its intended later boundary. Missing,
duplicate or cross-generation relation/control/dependency identities must
refuse before permission is consumed. Run that whole focused C/LLVM slice,
then fresh source-bound official seed/actual DRV-2 and ordinary/intent/destructure
parity plus installed cleanup. No row/status/count replaces those observations.

### Implemented source-control dependency, not relation admission

The subsequent control-selection slice is verified on graph
`c3e34496f98ec589f24f0bb600798c6813ce41d1753a83fff43dad097765dee3`.
BodyFlow now consumes one source-derived arm/loop selection, including missing
else/default, unreachable-arm first-return checking and zero iteration; the
old inline selection is deleted. C/LLVM source receipt
`.tmp/self_hosted/aggregate-release-source.g1A1cD` passes the unchanged 20/46
release matrix, identity/occurrence guards, 17 source selection snapshots with
malformed-input guards and 14 body-return controls. Four hash manifests
independently revalidate; builds retain nine warnings each.

The selection API requires one admitted analysis generation. Raw count/shape
checks and a projected digest are not a sealed relation receipt; the review's
same-count cross-artifact mixing concern is not executed by the current units.
Bind generation and typed transition dependencies at the one-time owner
boundary before result permission is consumed. Never reconstruct the source
per control or accept raw caller-assembled Preserve rows as source evidence.
Both identified analyzers still refuse the full compound positive at node 49,
UpdateCfgAppend -> UpdateRowsAppend. The conditional preservation relation,
caller instantiation and final guards remain required, not implemented here.

The preceding graph 055515e1 official seed passes with an independently
validated output receipt and byte-equal gen1/gen2 C. Its actual DRV-2 attempt
refuses at node 123287 (SelfMirCfgAttachLastDestructure ->
SelfMirDestructureFactRowsAttach), inner 1, unchanged pre/post source graph.
This is not current c3e34496 installed-driver evidence. No closure, substitution,
CI-green, GUI, commit or push claim follows.
