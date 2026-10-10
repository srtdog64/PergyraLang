# Collection formal key consumer survey

Status: **READ-ONLY SURVEY COMPLETE; candidate parity and P1 closure OPEN**.
Date: 2026-10-09 KST. Base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`.
Surveyed formal-use source SHA256:
`c4fda09dae20257734bc15fddebb515b4fa45608ce09eaec35108d4e593f0b21`.
Existing dirty graph/model/gate/document work and `gmon.out` were preserved.
This report follows `../agent_work_directives/ownership_formal_effect_execution_unblock_2026-10-09.md`.
It is navigation/evidence, not a semantic owner or a successor work track.
This initial survey binds the pre-edit producer. Main's later observed
candidate gates and remaining cost boundary are recorded separately in
`collection_formal_key_consumption_validation_2026-10-09.md`; they do not
retroactively turn these source-only findings into executable evidence.

## Decision

Deferring the two temporary string keys until their existing map consumers
need them is a sound bounded candidate, provided all identity checks and
the failed-state return remain unchanged. No ownership policy, map schema,
fixed point, formal-effect carrier or downstream permission needs to move.
This is a source-level equivalence argument, not observed candidate C/LLVM
parity, full-input execution or installed-driver evidence.

The claimed equivalence is exact returned semantic facts and diagnostics
when the relevant computation completes. It is not equality of allocation
traces, allocation-failure locations or OOM occurrence: reducing those
allocations is the stated objective.

## Reached ownership and consumer chain

Paths below are repository-relative; line numbers bind to the surveyed source.

| Boundary | Observed owner/consumer |
|---|---|
| Installed source request | `src/self_hosted/compiler/driver_bootstrap_main.pgy:7` admits argv and constructs the route world. `driver_source_mir_execution_owner.pgy:33` constructs the AST, calls typed semantic analysis, and hands the admitted analysis to DRV-2. |
| Body admission | `src/self_hosted/compiler/driver_rung2_owner.pgy:38` checks artifact/admission, creates one body bundle, and checks its receipt. `src/self_hosted/semantic/ast_body_type_bundle_assembly_owner.pgy:230` produces and retains formal effects once; line 249 passes that same result into collection ownership. |
| Identity and occurrence producers | `src/self_hosted/semantic/ast_collection_formal_effect_identity_owner.pgy:33` builds exact formal syntax-id/function/ordinal/mode/body rows from signatures. `ast_expression_graph_surface_order_owner.pgy:11` validates a disjoint occurrence partition, child order and explicitly carried synthetic roots. `ast_collection_formal_execution_context_owner.pgy:40` consumes `order.ok` and binds callable coverage to the artifact digest and node count. |
| Formal-use owner | `src/self_hosted/semantic/ast_collection_formal_use_owner.pgy:30` owns expression-use arrays and forwarding edges. The temporary key at line 126 only supports the formal-row lookup; the temporary key at line 366 only supports statement-target lookup. Neither key is retained in the result. |
| Fixed point and retained facts | `src/self_hosted/semantic/ast_collection_formal_effect_fixed_point_owner.pgy:5` checks array/edge shape and resolves acyclic then recursive effects. `ast_collection_formal_effect_fact_owner.pgy:5` carries `ok`, inventory, effects, owned-push leaves, execution contexts and borrowed-text formals. |
| Last effect/permission consumers | `ast_collection_formal_effect_readiness_owner.pgy:21` validates signature joins and effect ranges; lines 108/123 derive count/digest. `ast_body_type_bundle_admission_receipt_owner.pgy:6` records them and line 35 rechecks them. `ast_collection_ownership_verdict_owner.pgy:301` refuses failed effects, then passes existing rows/modes/effects into call effects and the ownership scan. `ast_collection_argument_permission_effect_owner.pgy:17` derives negative storage effects; `ast_collection_call_argument_verdict_owner.pgy:9` applies the selected formal permission. |
| MIR, JSON, backend projection | `src/self_hosted/compiler/driver_rung2_owner.pgy:111` rechecks the retained receipt before lowering the admitted collection verdict, not another effect table. `src/self_hosted/mir/artifact_lower_owner.pgy:210` appends ownership rows and line 226 appends receipts; line 424 refuses unconsumed ownership rows. `mir/json_projection_owner.pgy:385` and `mir/program_json_artifact_writer_owner.pgy:248` serialize the same rows. `mir_lower/collection_ownership_fact_owner.pgy:1` is the fail-closed JSON reader; `compiler/direct_mir_scalar_program_collection_ownership_transition_admission_owner.pgy:175` requires its validity and checks transition receipts/source identity. |

The source-C branch independently builds a body bundle through the same
owner (`src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy:32`)
and consumes its admitted body view. It does not make the source-MIR bundle's
storage-retirement boundary applicable to source C: the explicit restriction
is in `src/self_hosted/mir/body_type_bundle_storage_lifetime_owner.pgy:1`.

## Exact conditions for the two deferred keys

1. **Binding key.** Keep `SemanticExpressionGraphBindingIdentity` and its
   `!binding.ok` refusal outside any formal-only guard (use-owner lines
   121-124). Keep signature lookup, ordinal presence, parameter syntax-id,
   inventory ordinal and function identity checks (lines 129-150). Only
   `ToString(binding.syntax_id)` may move into the existing formal branch,
   immediately before `MapHas(formal_rows, key)`. A non-formal binding is not
   permission to skip validation of that expression node or its children.

2. **Statement-root key.** `ast_collection_formal_statement_effect_owner.pgy:149`
   records the original Atom `target_root`, not the peeled Index base, against
   the exact formal row. Duplicate target roots already fail closed. Its
   only `target_rows` reader is use-owner lines 365-370. The lookup result
   affects only the formal-root exception (lines 371-377) and the borrowed
   element-root exception (lines 379-388). Thus issue/read the key only when
   `ok` and either `node_rows[root] >= 0` or `element_rows[root] >= 0`. Keep
   `statement_mutation_row = -1` otherwise. Slice member/value root checks
   (lines 390-395) do not consume this exception and must still execute.

3. **Failed traversal must not index unfinished rows.** A malformed binding
   or call argument can set `ok=false` and break the inner node loop before
   arrays reach `root`. The new condition must guard *all* root-row indexing
   with `ok` first; a nested `if ok` is an unambiguous representation. Moving
   an unguarded `node_rows[root]` or `element_rows[root]` read before `ok`
   changes refusal into an out-of-bounds failure. Initially broken surface
   order makes execution contexts false and skips traversal; a later binding
   failure can leave partial arrays. Both cases need negative fixtures.

4. **Preserve partial failed results.** The old post-failure root conversion
   and map read cannot change any returned field: every consumer is guarded
   by `ok`. The fixed-point owner returns immediately when status is false
   (line 13); owned-push-leaf construction is also guarded. Existing partial
   opaque effects, inventory, contexts and borrowed-text facts must still be
   returned with `ok=false`. Do not replace them with an empty carrier or
   bypass later cleanup merely because admission failed.

## Bypasses, lifetime and unchanged obligations

- `SemanticAstCollectionFormalEffectsFromResolvedFacts` and the convenience
  verdict wrapper call the same formal-use owner; they are not independent
  effect policies. The reached body path calls `WithSurfaceOrder` and then
  `WithFormalEffects`, preserving synthetic-root coverage and avoiding that
  convenience reconstruction. The source search found no alternate in-`src`
  caller of those convenience wrapper definitions.
- Formal-effect keys remain decimal syntax-id strings. Replacing them with
  display names, a global cache, ordinal-only identity, or an independent
  integer-key table is outside this candidate.
- Missing effect, opaque execution and unproved element use remain negative
  evidence, not borrow/ownership grants. The permission owner at lines 50-63
  distinguishes unknown/retiring/descriptor-retiring; the call verdict at
  lines 37-47 refuses opaque or unproved element use. Own-mode and deferred
  descriptor checks must not be inferred away from a key-allocation saving.
- Scratch use arrays still terminate in use-owner lines 439-448. Retained
  facts survive through receipt validation and collection/MIR projection;
  MIR-only carrier retirement remains at its existing last consumer. This
  candidate does not repair or replace manual cleanup responsibility.

## Closing evidence still required

Main owns the fixed 2531-source measurement and sole production edit. Before
calling this obstruction removed, require fresh current-source C and LLVM
owner/analyzer parity for inventory, every effect, owned-push leaf, readiness,
receipt digest and collection verdict/diagnostic; compare partial failed
carriers too. Controls must include direct formal/borrowed-element statement
targets, non-formal bindings, wrong parameter ordinal/function/syntax id,
broken order and late traversal failure, deferred/opaque/own forwarding and
live-borrow refusal. A digest alone is not full content equality.

Then rerun the same full input at 3221225472 address-space bytes with exact
source/executable endpoint bindings. Measurement dominance, candidate memory
saving, successful census completion and installed-driver substitution are
not established by this read-only survey. No compiler/native test, source
edit, gate edit, registry change, install, commit or push was performed here.
P1 and the wider source/MIR/JSON/backend ownership chain remain OPEN.

## Follow-up: reached member-read execution-context lookup

Status: **READ-ONLY RE-SURVEY; measurement and any further edit belong to main**.
The Git base remains `85fff5aa339ae136ac0797c4a6a95a5285ada70e`.
The preceding producer edit is now present with formal-use source SHA256
`08c14ec40fee4f167b306f79a80a4ba7aa89f7de8d037635d142d6ce1c04ec1c`.
Its parity results were reported by main, not rerun in this survey. The
following newly reached sources were inspected without modification:

- `src/self_hosted/semantic/ast_collection_member_read_permission_owner.pgy`:
  `4d450aca490a14970dc622b7ddd977bc23fd96c279fee6182d4b93d20808cf80`.
- `src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy`:
  `064fb6936d7e5215c99492c14446fc137584a78012b214f50f8297ff326e9676`.
- `src/self_hosted/semantic/ast_collection_execution_context_fact_owner.pgy`:
  `7294c7ed895e8a2c84f180a7609b26c3bdf4afa7b8004b022abbcf9404814a10`.

### Observed obstruction, not dominance

Read evidence under
`.tmp/ownership-formal-candidate-2026-10-09-69e5d7be28024a839d8af0b5695a9d6b/`:
`candidate.stderr.log` reports `formal-exit`, then a failed 24-byte allocation,
`SemanticAstCollectionMemberRootReadOccurrenceFromGraph+0x221`, and termination
by signal 6 under `prlimit --as=3221225472`. The time footer's `Exit status: 0`
must not override the explicit abort/core evidence or be reported as success.

Independent symbol inspection found the function at `0x19bca0`; `addr2line
-f -i` for `0x19bec1` maps to generated `formal-candidate-profile.c:61121`.
That line is the **forwarded callee** context check using
`signatures.function_node_ids[target.signature_row]`, not the earlier current
function check. `CallableRowsObserved` formats its key at generated line
58619. Thus reducing only repeated current-function checks is not yet an
explanation or remedy for the observed failing allocation. Invocation counts,
bytes retained by this operation, and its share of reached memory remain
unmeasured here.

### Chain and distinct results that must survive

The retained formal-effects carrier supplies `execution_contexts` to the
collection verdict, which invokes `FirstInvalidMemberMoveUse`
(`ast_collection_ownership_verdict_owner.pgy:261`). Its occurrence loop calls
`MemberRootReadOccurrenceFromGraph` for each admitted subtree node
(`ast_collection_ownership_member_transition_owner.pgy:56`). The result is
not merely a boolean:

| Result | Meaning and downstream consequence |
|---|---|
| `(0, 0, false)` absent | The node is outside this direct readonly nominal-formal-root proof: source identity/mode, resolved-type bounds or nominal-constructor classification failed. No formal-root negative row is issued. |
| `(source, 0, true)` blocked | A relevant exact formal root was identified but its execution context, whole-root use or forwarding is unproved. The caller records `unproved_read_roots[source]` at line 60, disabling otherwise readable occurrences of that root. Replacing blocked with absent loses negative evidence. |
| `(source, 0, false)` direct member occurrence | Exact receiver/selector/member topology was observed under the current callable's admitted context. This is not standalone read permission: field identity, deferred-site checks and the final blocked-root filter still apply. |
| `(source, target, false)` forwarding | An exact physical argument edge joins source to the callee's exact formal syntax identity. The target must have an admitted readonly/default boundary, a body, non-async callable identity and its own current execution context. The edge carries negative obligations backwards through the bounded closure. It must not be reduced to direct-member success. |

The source join is `ast_collection_ownership_identity_owner.pgy:201`: formal
binding kind, syntax id, ordinal, leaf kind, signature row, flattened bounds,
parameter identity/type/mode are checked. The call-target join additionally
uses the existing receiver-offset-aware argument owner and checks mode/body/
async state (`ast_collection_member_read_call_target_owner.pgy:6`). Names or
the caller's permission cannot replace that target identity.

After the occurrence scan, line 199 of the transition owner validates the
artifact-bound contexts and closes forwarding, element-escape and nested-read
obligations. The verdict refuses `read_permission_ready=false` before applying
the result (verdict lines 266-275). Later, `MemberIndexedReadReady`
(`ast_collection_member_read_permission_owner.pgy:102`) requires both a read
candidate and an unblocked root; the collection scan consumes it at line 150.
Ordered field extraction/restoration and retired-local errors remain separate
state transitions in the same caller and must retain their diagnostic order.

### Generation, synchronous execution and early-guard constraints

`CallableReady` at `ast_collection_execution_context_fact_owner.pgy:26` owns
`contexts.ok`, positive artifact digest, matching digest/node count, function
bounds, **candidate membership** and absence from the unsafe set. Absence from
`unsafe_functions` alone is not permission. `CallableRowsObserved` is only a
row lookup; no reached caller bypasses `CallableReady` to invoke it directly.
The producer remains `ast_collection_formal_execution_context_owner.pgy:40`:
it checks admitted order and scans callable/AST/surface execution coverage,
including async/spawn/deferred or omitted execution. A locally ordinary
member expression does not cancel another unsafe occurrence in its callable.

The inspected member-transition traversal borrows the same retained contexts
and does not write either context map. Their validity is nevertheless bounded
by the same artifact and immutable admitted facts; this is not evidence for
cross-artifact reuse. Current-caller and forwarded-callee checks are distinct.
Nested place lending (transition line 76) and local-field lending
(`ast_collection_member_read_local_root_owner.pgy:100`) also consume this
permission owner and may not silently receive weaker rules.

Preserve existing early-guard classification. In the root-occurrence owner,
exact identity/mode and type bounds precede nominal classification, which
precedes context admission and direct/forwarding topology. Hoisting a context
failure ahead of irrelevant-node classification can change absent into blocked;
skipping it for a relevant alias can change blocked into an allowed occurrence.
Moving callee/signature accesses earlier can expose malformed inputs that were
previously short-circuited. Any reordering therefore needs exact tuple and
failure-path parity, not just a successful final read fixture.

### Other repeated work and falsifiers

The current occurrence function copies `resolved_leaf_types[node]` through
`Concat("", ...)` and canonicalizes it (permission owner lines 46-49).
`SemanticCanonicalTypeNameFactFrom` owns trimming/generic-label normalization
and nominal-base extraction (`ast_type_name_canonical_owner.pgy:96`). The
member-identity path also canonicalizes its root type (member-move owner
lines 41-45). These are observed repetitions, not measured dominant cost.
Replacing resolved types with declared formal text could change instantiated
nominal classification; deleting a copy also needs its actual borrowing/
storage-lifetime justification. Neither change nor a new cache is proposed.

Required falsifiers for any subsequently measured candidate: stale digest or
node count; missing candidate but empty unsafe map; current caller safe but
forwarded callee unsafe/absent/async/bodyless; wrong formal ordinal or hosted
receiver offset; inout/own versus readonly/default modes; non-nominal and
out-of-range resolved-type inputs; exact direct topology versus a whole-root
alias; transitive/cyclic forwarding into a blocked sink; nested lending;
deferred read, field extraction/restoration and retired-local use. Compare
absent/blocked/forwarding tuples, closed maps and diagnostic order as well as
the final verdict. The same full source/cap and identified executable remain
the integration boundary. This follow-up changes only this report and does
not establish a second optimization, ownership closure or installed result.

## Follow-up: binding-move empty-prefix consumer survey

Read-only source survey at main HEAD
`85fff5aa339ae136ac0797c4a6a95a5285ada70e`, retaining the existing dirty
formal-key change and unrelated graph work. No source or gate change is made
by this follow-up. SHA-256 of the inspected source bytes:

- `ast_collection_ownership_binding_move_use_owner.pgy`:
  `48791281141d6dbc477eb1453d4738691c75c66a43e957bf2ddb8f0a62a3e75d`.
- `ast_collection_definition_storage_authority_owner.pgy`:
  `cd6d96e909139c2e1b16211ba466da7eee441779ed8c51ea9f598f547ba6f4df`.
- `ast_collection_ownership_verdict_owner.pgy`:
  `a21265b06f63e84d94220c17f2c6d7f88607e3a477e7a969f509f07806013c8a`.

### Observed obstruction, not an optimization verdict

The existing fixed-input member measurement is
`.tmp/ownership-member-read-2026-10-09-75f10c9e05e145d698e1bc2eeadd6fe9/baseline.stderr.log`.
Lines 3 and 6 report allocated bytes 2,894,232,112 at formal exit and
2,911,080,432 at member entry: a 16,848,320-byte difference, not attribution
to this scanner alone. Line 9 reports 135,208 visited nodes, 11,306 eligible
formals, 4,090 nominal formals/current checks, 1,289 forwarding checks,
5,756 context-row calls, 66,046 integer-string conversions and 118,409
concatenations with 3,110,728 usable bytes. Allocation failure and signal 6
remain in this log. These observations do not establish that context keys,
canonical type copies, or binding-move prefix work dominate cost. The main
lane separately measures binding-move work on the same fixed semantic input;
this report neither proposes a cache nor approves an implementation candidate.

### Actual authority and scan state

The only source caller is
`ast_collection_ownership_verdict_owner.pgy:250`. Before calling, this owner
checks surface/local-row/carried-call and other input admission at lines 64-80,
constructs canonical definitions at line 94, applies the owned-result plan at
line 100, and constructs/checks storage authority at lines 107-110. None of
these checks may be replaced by an empty move-map observation.

`ast_collection_assignment_definition_owner.pgy` owns initial and assignment
definition rows and exact surface/target/source joins. The owned-result
application only attaches admitted fresh-result identity to initial rows;
assignment rows do not inherit stale earlier result facts
(`ast_collection_owned_result_definition_owner.pgy:7`). Storage authority
tracks borrowed status and producer lineage through completed definitions;
its borrowed query fails closed on invalid facts or row bounds
(`ast_collection_definition_storage_authority_owner.pgy:87`).

The scanner creates an empty `move_points` map and current-definition array
at lines 18-19. `InitialDefinitions` initializes each local to its initial
definition row (`ast_collection_definition_query_owner.pgy:13`). This scratch
lineage is not the verdict's active-storage state, which begins inactive and
is advanced by its transition owner. In the read loop (lines 27-58):

- Graph identity excludes callable leaves and member-name selectors; the
  exact assignment target is excluded through `DefinitionWriteTarget`.
- Scoped local resolution retains function/scope/declaration identity and
  chooses declared or inferred type. Its copies at
  `ast_local_binding_identity_owner.pgy:98` participate in an identity whose
  empty/Unknown type is rejected. That predicate matters once a move exists.
- A successful identity indexes `current[binding.row]`; the decimal key is
  the current **definition syntax ID**, not the local spelling, graph node
  number, runtime slot or generation.
- Only a map hit with `syntax_id > move_site` returns a found use. This loop
  writes neither `current` nor `move_points` and emits no invalid-input
  diagnostic for `binding.ok == false`.

The only insertion is line 64, after the root's read loop. `move_points` is
never erased. Consequently, for admitted inputs on normally completing
execution, a root entered with an empty map cannot produce a found use from
its read loop. This is a source-level result about the returned fact, not a
runtime measurement or an arbitrary malformed-array panic-equivalence claim.

### Transitions and failure ordering that remain necessary

Even during that prefix, lines 60-66 must retain the existing completion
semantics: identify the Value-lane completed definition, consult its storage
borrow status, record the source's **previous current definition** for a
nonborrowed local-source move, then advance the destination's current row.
`DefinitionCompletedAtSlot` is owned by
`ast_collection_definition_query_owner.pgy:33`; assignment targets are a
different Atom-lane query at line 24. Preserve the existing `has_roots`
boundary, including completion after a missing subtree-start result. A root
without `has_roots` currently performs neither read nor completion here.

Skipping destination advancement before the first move can retire an old
definition after a reassignment. Recording after destination advancement can
likewise change source selection. Borrowed transfers can leave the map empty
but must still advance lineage. Empty means no earlier recorded move in this
whole scan, not a per-function reset condition. After the first insertion,
preserve root-slot order, node order, exact target exclusions and the strict
syntax comparison (including other lanes in the same statement).

The caller returns `move_from_released` / `binding_use_after_move` at lines
253-259 before member permission/member-use checks, assignment-alias checks,
and `formal_effects.ok` at line 301. A returned false triple still proceeds
to those checks; it is not successful ownership admission. Reordering the
scan after them changes first-diagnostic precedence. Upstream rejection must
also remain intact for malformed facts. Direct forged calls to this scanner
are not a validated public boundary: raw array accesses exist, so equivalent
out-of-bounds/abort behavior is not established by the empty-map reasoning.

### Existing downstream/model boundaries and required falsifiers

The semantic error scan does not publish a new ownership certificate. The
existing MIR consumer separately admits exact direct declaration moves from
MIR def/use and a single-leaf expression graph, with exactly one matching
source/destination pair (`mir_lower/collection_ownership_binding_move_owner.pgy:40`
and `:111`). This report does not widen that check to arbitrary source moves,
prove source-to-MIR refinement, or replace downstream JSON/backend admission.

The existing F/F1 plan remains the contract, not a new work track:
`docs/agent_work_directives/ownership_cutover_plan_2026-10-08.md:181`
requires replacing `formal.mode != 2` as borrowed authority with inferred
summary consumption; lines 184-187 keep Slice's real lifetime/generation
certificate OPEN. F1 at line 195 still requires place/temporary admission,
multi-inout normalization and exactly-once recovery, expression evaluation
order, ABI-owned result/view origins, collection operation/glue behavior,
and post-normalization/DCE analysis-generation checking. Current definition
syntax IDs do not discharge those generation obligations; no source
annotation or new memoization policy is introduced here.

For any later measured change, preserve exact return triples and caller
diagnostics on: no moves; first move only in the last Value root; borrowed
handoffs followed by an exclusive move; reassignment before the first move
and after a move; same-statement Atom/Value/auxiliary lanes; shadowed names;
callable/member-selector exclusions; assignment LHS versus RHS reads; two
invalid reads with distinct root/node order; missing subtree starts; upstream
malformed facts rejected before scanning; and later member/formal failures
following a false binding-move result. In particular, fresh replacement must
not inherit the retired definition's key, and skipping reads must not skip
definition completion. Fixed-input cost attribution and executable parity
remain unverified by this read-only follow-up.
