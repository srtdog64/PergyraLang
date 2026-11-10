# Local / returned storage and Intent rebuild

Status and executable evidence: [private lane state](../truth/state/local_return_intent_repair.md).
This directive records the closure plan; it is not a semantic authority.

## Objective card

- Objective: carry allocation, transfer and retirement through the installed
  C/LLVM general MIR emitters; rebuild typed Intent from native and self MIR.
- Priority: sound lifetime and semantic identity, one fact owner, missing-fact
  refusal, public-entry consistency, physical release, whole compiler input.
- Fact owners: admitted GraphPlan for normalized storage/uses; nominal and enum
  owners for declaration identity; DIR Intent plan for transition/compensation.
- Last consumers: installed C/LLVM emitters and the selected public source-C
  route. A shared-driver rollout is a distinct boundary owned by main.
- Forbidden: guessed ownership, freeing borrowed or transferred storage,
  hidden compile retries, manual drops, program-name routes, native fallback,
  erased constructor IDs, safety-rule weakening or partial-input closure.
- Gates: official installed build with source/executable hashes; ASan/LSan
  allocations, returns and temporary DAGs; alias/double-free controls;
  constructor ID mutations; existing permit/refuse gates; full canonical input.

## Complete reached chain and dependency order

1. Source graph capture -> declared constructor index -> carried call IDs ->
   capability inventory / enum provenance / collection escape -> MIR ->
   exact declaration epoch join -> reconstructed semantic replay -> C/LLVM
   logical-record and enum consumers. Constructor identity is never a function
   leaf binding. Canonical namespace spelling comes from the callable owner.
   The index owns copies of String keys that outlive the source-fact read.
2. The identity epoch grows two owned ID columns. Its lookup consumes those
   columns by ref; it must not construct an apparent retaining aggregate just
   to query them. Declaration joins require kind, module and identity matches.
3. Typed source Intent plan captures the already admitted expression graph
   before the final graph join. Formal signature admission consumes only
   binders; complete reconstruction still requires execution-step evidence.
   A partial compensation list must refuse an uncovered surviving effect.
   Zero-compensation positive input removes both compensations; the partial
   list remains a separate native/self negative.
4. Normalized allocation roots -> unique local definition -> all reachable
   storage uses -> grounded fresh-return summaries -> caller ownership ->
   replacement/exit retirement in C and LLVM. Record storage must be disjoint
   before recursive retirement is granted. Aliases and mixed fresh/borrowed
   returns get no new retirement rights. No precision claim beyond these facts.
5. Root dependency traversal decides which LLVM DAG nodes execute. Array
   prefix nodes retain identity without allocating an unused buffer. String
   observer edges retire only unique fresh Concat children, after the result
   is materialized. Source order and short-circuit chunks stay intact.
6. Existing general C/LLVM paths share this retirement decision. The default
   public C selector still has legacy consumers; complete public dispatch
   migration cannot be claimed from explicit MIR-mode tests.

## Falsifiers and remaining obligations

- Borrowed literal/formal result, mixed return paths, live local alias,
  repeated record field origin, inout return, early return, loop replacement,
  nested fresh string observers, Array element side effects and short circuit.
- Missing/crossed constructor ID with otherwise valid topology; foreign action
  target, stale graph/plan, duplicate compensation and reachable empty scaffold.
- Keep full canonical source unchanged during each run. New source errors
  invalidate the current candidate; record the entire diagnostic set.
- Public C legacy emission, Array<String> returned storage, mixed/aliased
  result ownership and typed Intent general LLVM remain explicit obligations.
  No new per-program selector or broad fallback is part of this repair.

## Component-size review

The declaration epoch responsibilities were split into map and constructor
join owners. Existing retirement emitters and error owners retain their
responsibilities; narrowly increased caps cover physical cleanup, root
dependency execution, two diagnostic rows and behavioral negative cases.
The same executable component checker must pass after these additions.

## Constructor identity consumer closure (full-input falsifier)

Build 15 full source-MIR stopped at `aggregate_release_plan_unproved` in the
existing destructure-name cleanup. The exact input stayed unchanged. The
constructor producer now carries declaration IDs, but six reached consumers
still classified constructors by zero callable ID. The required repair is
one validated nominal call identity read, consumed by aggregate field
lineage, exclusive-value classification, copy landing, map payload backing,
constructor sharing and descriptor permission effects. Builtin zero-ID and
function-formal signature joins keep their existing contracts. Missing or
crossed declaration IDs grant no field provenance or release permission.
Run existing aggregate release/copy/constructor/map permit-refuse gates, then
repeat the unchanged full canonical input with the new fixed executable.

The native C build of the source-analysis gate exposed an additional call ABI
obligation: the new compensation admission received an artifact by ref while
the existing signature validator consumes the artifact value. Align this new
entry with the existing artifact-value contract; no wrapper, clone, fallback
or change to the signature owner is required. The original C/LLVM analyzer
builds are part of the verification boundary, not just MIR admission.

Two pre-existing artifact checks are refreshed only after examining emitted
artifacts: the Option<Bool> fixed MIR digest tracks the current producer, and
the LLVM copy-in assertion resolves a named type to its physical layout.
The exact pointer/local load/store identity, runtime output and all original
ABI/short-circuit mutation refusals remain mandatory. Native and installed
MIR use distinct syntax epochs/metadata here; byte equality is not claimed.


## Native publication replay of immutable member carriers

- Objective: admit the same formal-relative Array<String> field path at lexical
  capture and at sealed publication, without granting a new ownership right.
- Priority: stable declaration/field identity, fail-closed replay, preserved
  retaining/mutable negatives, then whole-input verification.
- Fact owner: FunctionParamPublicationStore's typed values and projections;
  collection_member_path_owner composes only those already-checked edges.
- Last consumer: collection_owned_required_member_reads_current, before owned
  element requirements are finalized. Other published member-path consumers
  retain their exact formal/path checks.
- Current mismatch: lexical capture follows immutable local aggregate aliases,
  but published replay stops at the first local binding. The member-read receipt
  also stores that immediate root while requiring the canonical formal root.
- Required change set: allow typed published prefix paths; replay immutable let
  initializer edges through the existing publication; compose field IDs in root
  order; save the canonical formal root in member-read receipts; preserve the
  existing proof of the immediate carrier and its birth when issuing a receipt.
- Forbidden fallback: names, mutable/alias declarations, unknown initializer
  facts, raw AST re-analysis, ignoring retaining reads, or a second owner.
- Gates: production parser bridge plus nested read-before-owned-push positives,
  mutable/retaining negatives, existing flow-summary suite, then the fixed full
  compiler input. Cycles/missing/current-identity disagreement fail closed.


## Grounded owned Array<String> literal results

- Objective: preserve the immutable element-ownership proof through the final
  fresh-result plan, including a nonempty returned literal.
- Owner: array_storage_result_owner consumes the finalized return-event and
  element-lifetime receipts; no new AST inference or ownership authority.
- Chain: return element capture -> sealed function return replay -> grounded
  result plan -> collection call-result birth -> MIR cleanup consumers.
- Current defect: the first replay proves every returned literal element, but
  the grounding pass unconditionally refuses every nonempty literal. Empty
  literals alone still cannot provide an owned-element ground.
- Required change: consume the completed element replay and count a nonempty
  literal as a ground, preserving every event/type/identity/cycle check.
- Forbidden fallback: treating nonempty as proof before element replay,
  promoting borrowed elements, bypassing missing return-event evidence.
- Gates: existing returned_owned_array_literal_independent_push_landed_valid;
  every existing flow-summary positive/negative, followed by the unchanged
  canonical compiler input. Retaining/borrowed element returns stay refused.


## Aggregate return retirement obligation

- Falsifier: an Array<String> descriptor placed in a returned record is freed
  by the callee; direct, named-record and immutable-alias returns all fail
  ASan on both installed backends. The caller currently has no field release.
- Owner: the existing GraphPlan lifetime owner must carry an allocation's
  return closure and its exact per-field retirement recipe together. The
  existing collection transition owner remains authoritative for String
  element ownership; record type alone cannot decide deep versus storage-only
  release. A raw syntax check of the final return is forbidden.
- Required chain: definition identity -> local/constructor return closure ->
  grounded, disjoint allocation result recipe -> exact caller local recipe ->
  C/LLVM field retirement. Array cleanup consumes the same return closure and
  must not free transferred descriptors. Local replacement uses the same recipe.
- Reject recipe synthesis for missing facts, formal storage, incompatible
  branch recipes, retaining uses or duplicate field allocation origins. Keep
  borrowed elements separate from owned elements; never deep-free by type.
- Gates: direct/local/alias record return, owned and borrowed String elements,
  nested fields, caller repetition and live-alias/double-free controls, then
  existing lifetime/ABI/Intent gates and pinned full source compilation.
