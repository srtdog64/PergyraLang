# Reachable owned-result proof review

Date: 2026-09-30 (Asia/Seoul)
Status: CANDIDATE SOURCE REVIEW — build/projection acceptance pending
Base revision: `7a9fe09d8293170c4ce676de6464486cd25ca348`
Starting owner SHA-256: `B2DB3A6E2BB25BAB2CDE4DB65ECA70658CF5A4FB9D8CA143F0123A1A43D08A80`

This report supports the single implementation track coordinated by
`docs/agent_work_directives/owned_result_reachable_proof_work_2026-09-30.md`.
It does not issue ownership facts, change registry status, or claim whole-driver
or public-route verification. Existing dirty source and test work was preserved.

## Existing contract

The owner is
`src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy`.
Its current two admission consumers are collection transition plan readiness
and owned-String call-result argument admission. Both pass the existing third
argument as zero.

Observed source obligations that an iterative replacement must preserve:

- A callable row must be after the entrypoint ordinal and in the normalized
  routine return-type range; a String type alone grants nothing.
- Every terminal block without process exit must return a proven expression.
  At least one such return must exist. One accepted arm cannot hide another
  borrowed or invalid arm; routines with no returning terminal are rejected.
- Local references must resolve to the same routine and exactly one
  `ExpressionDefinition` operation. A duplicate/mutating definition is not
  a fresh-result proof.
- A runtime producer remains the exact `TextBuilderFinish` ABI with two
  parameters and an allocator local whose unique definition is the exact
  zero-parameter `AllocatorResult` ABI. Routine/local spelling is not proof.
- String concatenation still consumes the structured concat ABI identity.
- Direct calls consume normalized callable rows whose source identity has
  already passed the existing expression and receipt admission boundaries.
  The result owner must not invent a second name-based or type-based join.

These are source observations, not new runtime counterexamples.

## Iterative DFS review constraints

1. Mark a routine active when it is popped for actual body expansion, not when
   merely queued. Multiple terminals or sibling routines may queue the same
   unseen callee; that is sharing, not necessarily a cycle.
2. A negative exit marker may mark the routine completed only after all its
   dependencies have completed. Reaching an active row rejects an ancestor
   cycle. Reaching a completed row reuses the fact without re-expanding its
   body. A pending-but-unseen row must not be treated as active.
3. Validate a direct callee's normalized row before indexing a state array or
   encoding an exit marker. Preserve entrypoint/non-String rejection.
4. Expand every non-process-exit terminal before completing a body. Reject the
   whole query on a borrowed expression, invalid target, or invalid local
   definition; no partially admitted state escapes the query.
5. Local-initializer traversal needs its own finite expression-graph bound;
   it cannot borrow a program routine count or grow the native call stack.
6. States and pending storage belong only to this immutable GraphPlan query.
   Release them on accepted and rejected paths. No global cache or additional
   semantic authority is justified by this change.
7. Remove the obsolete depth API together with all its current production
   consumers, rather than silently reinterpreting depth as an unrelated result
   or instrumentation channel. The candidate does this for both consumers.

Necessary falsifiers include two-terminal sharing, sibling sharing/cross-edges,
long acyclic chains, direct and mutual cycles, local initializer cycles, a
fresh/borrowed return join, invalid/entrypoint callable targets, and no ordinary
return. A fabricated minimal typed owner probe proves owner behavior only;
whole GraphPlan admission needs the real projection path below.

## Actual candidate review

The first iterative candidate, SHA-256
`CD3FBA2C716A604A6477AEAB4B15B4A3E203264E2101CE96305A2EAD8B0109EE`,
had 172 source lines under the existing cap180. Its algorithm and two consumer
API changes were inspected. No source-level DFS/terminal/identity blocker was
found: routine state becomes active only on actual expansion, completion uses
the exit marker, all return arms are checked, direct targets are checked before
enqueue, local traversal is iterative and graph-bounded, and completed bodies
are not expanded again. The normalized partition owner separately requires
entrypoint ordinal zero and aligned routine ranges; the proof must consume that
existing admission boundary, not claim to validate arbitrary malformed plans.

That source review did **not** prove compilability. The independent probe then
rejected this exact candidate. This reviewer inspected the retained diagnostic
at `.tmp/self_hosted/owned_string_reachable_proof.toiYcy/emit.err`: five errors
and one unreachable-statement warning. The errors reject mutation of
`ref pending`, two derived/forwarded borrowed-pending helper escapes, and the
two undefined `ArrayDrop` calls. None of those diagnostics identifies
`ref plan` as the escaping parameter. The cleanup path existed syntactically,
but its original call spelling was not a valid executable cleanup API.

The revised source observed at SHA-256
`D9179E99A53001B0BBC8BD8572E202B1E6F850281EA6C8FF45DB84B0214EB974`
has 177 lines. It changes pending to `inout` and routes both local arrays through
`DirectMirScalarProgramOwnedStringProofStorageRetire(own rows: Array<Int>)`.
This is an exact resource-retirement boundary inside the existing owner, not
a new semantic result authority or generic helper bucket. It uses the existing
typed `CompilerRetireArrayStorage` operation. Both success and refusal after
allocation reach both retirement calls; the initial routine guard allocates
nothing. No proof state is read after retirement.

The retirement boundary additionally changes the canonical caller registry
and its generated self-host projection. Its complete path/name/own-parameter/
`Array<Int>`/`Void` identity must be registered once. The native admission
includes that registry, and the self-host admission delegates to its generated
projection; neither consumer hardcodes a five-row count. The generator itself
uses `len(rows)` and a count-bounded loop.

Source findings sent to the integration owner while that revision was being
prepared:

- Registry identities must be sorted. A newly appended `compiler/...` row
  belongs before existing `mir/...` rows, and the projection must be regenerated
  through its owner rather than hand-edited.
- The prior generated five-row projection has 99 lines under cap100. Adding
  the existing six-line row template yields 105; keep the existing cap through
  a justified bounded projection representation rather than hiding the size
  with a higher allowance.
- `routine_build_storage_lifetime_owner.sh` has an old exact three-path
  retirement call-site list and four-function approved-owner set. Those lists
  already omit the existing expression-environment retirement owner, although
  the current canonical registry has five rows. This is a static mismatch in
  the pre-existing gate, not a newly executed compiler failure. Integrating the
  proof retirement must reconcile the complete actual approved inventory while
  preserving wrong-path/own-boundary negative checks.

The revised source's native emission and real scalar C/LLVM acceptance remain
the root integration owner's gates. This reviewer has not run a heavy build.

Follow-up source snapshot: canonical registry SHA-256
`489E65FD83603753745E9DB038D5CF0924B5347828023E864EBA60E74C5AB421`
now places the compiler row first. The generated projection carries six rows
and still has 105 lines (SHA-256
`605E0614A9FDD249C3BFF2C5BE69EBBF0E2A89060B2D89E32350CD6796CEE73B`)
at that inspection. The routine lifetime gate now explicitly admits all six
registered owner function identities and derives the exact call-site module
set from the canonical rows, removing its old order-sensitive stale path list.
The original native and self-host no-copied-tuple ratchets remain present.
Root subsequently chose an explicit generated-data exception instead of
compacting generator formatting: the exact added registry row accounts for
99 -> 105 lines, and the structural inventory bound is now 105. That choice is
documented in the shared directive; the algorithm owner remains under its
unchanged cap180. The earlier projection-format suggestion was review advice,
not an imposed source change. It must not justify increasing proof depth,
runtime allowances, or other responsibility caps.

Observed read-only verification after that integration:

```text
C:/msys64/ucrt64/bin/python.exe scripts/render_compiler_internal_builtin_caller_registry.py src/common/compiler_internal_builtin_caller_registry.def src/self_hosted/semantic/compiler_internal_builtin_caller_registry_owner.pgy --check
exit=0
algorithm-owner=177 lines, SHA-256 D9179E99A53001B0BBC8BD8572E202B1E6F850281EA6C8FF45DB84B0214EB974
generated-projection=105 lines, structural bound=105
git diff --check: exit=0
```

Executable provenance acceptance still requires root evidence. This static
check proves the exact generated projection, not owner runtime behavior or
whole-driver admission.

## Smallest inspected real projection path

For the composed owned-String fixtures, the production dispatcher eventually
calls `CompileAdmittedDirectMirScalarProgramForTargetObserved` in
`direct_mir_scalar_program_projection_owner.pgy`. That function constructs and
seals the real graph, then calls the existing C or LLVM emitter. Graph sealing
checks collection readiness, which reaches the changed result owner.

A bounded test runner can therefore import these existing owners:

- `direct_mir_scalar_program_projection_owner.pgy`
- `direct_mir_scalar_program_route_admission_owner.pgy`
- `../mir_lower/mir_json_input_owner.pgy`
- `../mir_lower/generic_instance_plan_owner.pgy`

It must read the checked machine declaration, admit the MIR through
`MirJsonReadMachineAdmittedInput`, derive the generic occurrence/instance
owners, obtain and validate `DirectMirScalarProgramRouteAdmissionFromAdmitted`,
derive the target projection fact from its existing owner, and invoke the
scalar projection. It must not construct a route fact or GraphPlan by hand.

The following pre-candidate recursive-import inventory was observed with PowerShell reads;
counts include all transitively imported files and newline-split source rows.
They are not execution time, memory, reachable-call counts, or build success.

| Root | Imported files | Source rows |
| --- | ---: | ---: |
| Whole `direct_mir_backend_projection_owner` | 1,565 | 213,632 |
| Scalar program projection | 925 | 107,549 |
| Program graph admission | 660 | 87,428 |
| Changed result owner alone | 376 | 60,399 |
| Four-owner scalar runner union above | 931 | 108,346 |

All inventory imports resolved. The four-owner union contains the changed
owner and excludes `driver_pipeline_owner`. The whole direct-backend root
imports that pipeline owner and therefore brings the source parser, semantic
analysis and source codegen into its import closure. Removing only CLI code
does not establish a small build.

This runner would establish changed-source scalar C/LLVM projection behavior.
It would not establish public CLI dispatch priority, source-producer correctness,
installed-driver publication, fixed-point bootstrap, or P1 closure. No heavy
build was run by this reviewer.

## Remaining acceptance

The candidate algorithm, two admission consumers, and exact retirement policy
were inspected. The initial candidate's build failure is retained above. The
reviewer has since inspected the revised candidate's actual retained runtime
and provenance-negative logs; the focused acceptance update below supersedes
the earlier executable-evidence wait, not the broader completion limits.
Native forward-summary ordering and the
separate alias-depth-eight source support question are existing open evidence,
not bugs newly reproduced in this review. No performance completion claim is
made from the static import inventory or the proposed DFS shape.

## Integration follow-up: routine-build retirement omission

The root's scoped lifetime gate stopped before caller-inventory checks with:

```text
routine-build leaf coverage drift: missing=[('build.cfg.blocks.intent_roles', 'String'), ('build.cfg.instructions.binding_source_syntax_ids', 'Int'), ('build.cfg.local_refs.source_statement_syntax_ids', 'Int')] extra=[]
```

The retained diagnostic is
`.tmp/owned_result_reachable_20260930/lifetime-gate.err`. This reviewer diagnosed
the existing source read-only; no routine-build implementation was changed.
The existing retirement owner is clean at the inspected checkout, SHA-256
`A914487CCDD9428D3BFBB023A4E52A3E049E804E76E3B5F5061E4197032701D3`,
159 lines. This is a pre-existing integration finding, not a regression
attributed to the owned-result DFS patch.

The failure is not merely a stale path list. Each of these three fields is an
actual routine-local Array backing absent from the retirement owner, while its
elements are copied to the program backing before that routine owner retires:

| Routine-local backing | Copy/last-consumer evidence |
| --- | --- |
| `blocks.intent_roles` | `routine_cfg_append_owner.pgy:24` pushes each source role into the target role array |
| `instructions.binding_source_syntax_ids` | The same append owner at line79 pushes each source identity into the target identity array |
| `local_refs.source_statement_syntax_ids` | `local_ref_fact_owner.pgy:128` pushes each source statement identity into the target array |

The ordinary route consumes local instruction/ref identities while attaching
collection receipts (`artifact_lower_owner.pgy:231-232`) and then appends the
CFG (`:259`). The intent route appends the CFG in
`intent_routine_owner.pgy:476`. The orchestration then calls the existing
retirement owner on both success and failed-build paths for normal and intent
routines (`artifact_lower_owner.pgy:397`, `:410`, `:455`, `:463`). Later consumers
read the program's copied backings: JSON writers/verification read intent roles
and binding identities, and local-ref verification checks copied statement-ID
rows. No intentional borrowed-backing retention was found on those paths.
Shared String **elements** remain alive; only the routine-local Array carrier
storage should retire. No dynamic leak size or allocator count was measured.

Smallest proposed fix for the root integration owner: bind these three leaves
in `SelfMirRoutineBuildStorageRetireAfterLastConsumer`, shallow-retire each
exactly once through its existing approved builtin, and keep append-before-
retire ordering unchanged. Three bindings plus three retirement calls fit the
existing cap180 (159 -> 165 lines without header wrapping). Do not weaken the
leaf-reflection gate or deep-free String elements.

There is a second stale test census: the actual existing owner already has
94 bindings, Int43/String51, because it includes `match_facts.subject_families`
(last owner commit `2c196a77`), while the test still expects Int43/String50/93.
After adding the three missing leaves the source-derived census is
Int45/String52/97, comprising 92 CFG backings and five routine-state backings.
The old “Eighty-eight CFG backings” header is stale too.

Required negative/effect evidence: retain exact all-leaf/once-only-drop
reflection, forbid target-program backing retirement and String deep frees,
and preserve external/wrong-path compiler-internal caller refusal. A bounded
copy/retire probe should populate all three fields, append to a separate target,
retire the source, then inspect the target's exact role and IDs; a normal plus
intent MIR projection after retirement should preserve the existing metadata
and ownership receipts. These are proposed gates, not executions by this
reviewer, and do not open a parallel implementation rung.

## Next serial slice: body-bundle storage and typed Bool drop

After the root added the three routine-build leaves, the same integration
gate stopped at the next existing boundary. The retained diagnostic in
`.tmp/owned_result_reachable_20260930/lifetime-fixed.err` is:

```text
unsupported routine-build Array leaf body_types.capabilities.deferred_uses: Bool
```

This is not evidence that Bool is an invalid body fact. The reflection gate
currently accepts only Int/String leaves, and the self-host C retirement
emitter independently rejects Array<Bool>. Both must remain visible; changing
the test to skip Bool or casting Bool storage to Int would hide the gap.
The root is retaining this as the next serial slice, not implementing a
parallel body-bundle track during the owned-result integration.

The inspected body retirement owner is unchanged, 98 lines, SHA-256
`9475D37CFCF8556F4C3239261519240E1C5019579AF240F2232A30A91B2BE6B2`.
A separate read-only PowerShell reflection using the gate's reachable struct
and field patterns, permitting Bool only for inventory, observed 72 expected
leaves: Int47/String23/Bool2. The current owner binds 52: Int30/String22.
The omitted leaves are:

| Family | Missing typed leaves |
| --- | --- |
| `collection_ownership.receipts` | Int: `function_syntax_ids`, `owner_node_ids`, `lanes`, `local_call_ordinals`, `effect_kinds`, `receiver_binding_syntax_ids`, `source_binding_syntax_ids` |
| `zone_carriage` | Int: `fresh_local_node_ids`, `resource_path_starts`, `resource_path_counts`, `mutable_borrow_parameter_node_ids`; String: `resource_field_paths` |
| `capabilities` | Int: `callable_node_ids`, `declared_masks`, `used_masks`, `exported_masks`, `declared_effects`, `known_call_effects`; Bool: `deferred_uses`, `unknown_call_effects` |

The test's later body census is also stale: it still expects
Int20/String20/40 at `routine_build_storage_lifetime_owner.sh:180`.
The Bool rejection runs before either the exact all-leaf comparison or this
census. The inventory exposes 20 omissions; it does not by itself prove that
an arbitrary common admission boundary may retire them.

### Backing lineage and last legitimate consumers

All 18 omitted Int/String leaves are separately materialized carriers on the
successful semantic body-bundle path. The following source lineage is the
reason they can retire at the existing **MIR projection** boundary, rather
than a claim that every route has the same lifetime:

- Semantic receipts start with seven independent empty Arrays in
  `ast_collection_ownership_receipt_owner.pgy:28` and receive scalar pushes in
  its Append owner. The verdict seeds this fresh carrier at
  `ast_collection_ownership_verdict_owner.pgy:60-61`, passes it through
  statement transitions, then appends exact call/statement identity scalars.
  The resulting seven arrays do not borrow the analysis's input backings.
- `SelfMirCollectionOwnershipReceiptRowsEmpty` creates a different three-array
  carrier (`collection_ownership_receipt_fact_owner.pgy:12-17`). MIR projection
  extends that carrier (`collection_ownership_receipt_projection_owner.pgy:37-43`)
  and copies semantic effect/receiver/source scalars with ArraySet (`:99-104`).
  ProgramFacts retains these three MIR arrays, not the seven semantic arrays
  (`program_fact_owner.pgy:126`). The last semantic receipt consumer is this
  per-routine projection during `SelfMirAppendRoutine`
  (`artifact_lower_owner.pgy:226-232`), completed before ProgramFacts returns.
- Four zone arrays are fresh empty carriers at
  `ast_zone_value_carriage_verdict_owner.pgy:68-71`, populated by scalar IDs,
  offsets/counts and field names (`:89-120`). The fifth,
  `mutable_borrow_parameter_node_ids`, is not a signatures-array alias:
  `ast_zone_parameter_boundary_verdict_owner.pgy:45` creates a new Array,
  `:105-109` pushes each selected parameter-node scalar, and `:141-144`
  hands that carrier to the zone verdict. The local boundary verdict aliases
  that same fresh carrier only while constructing the returned body fact;
  it is not returned beside the bundle or persisted in ProgramFacts.
- The six capabilities Int arrays, plus the two Bool arrays, are independent
  empty carriers at `ast_capability_fact_owner.pgy:164-171`, populated with
  scalar pushes at `:174-181` and scalar updates thereafter. Their source
  capability state/instantiation arrays are not returned as these backings.

On the MIR path, full readiness reads zone/capability rows when the semantic
admission receipt is produced. At the later projection entry, the receipt
checks the zone counts once more
(`ast_body_type_bundle_admission_receipt_owner.pgy:80-85`); this is the last
zone-array read on this path. It does not reread capability arrays.
`DriverRung2MirProjectionFromVerifiedFactsObserved` passes the other semantic
families to MIR lowering (`driver_rung2_owner.pgy:124-130`), not the zone or
capability families. Once all routine receipt copies are finished, it calls
the existing body retirement owner at `:132-133` and returns MIR facts plus
analysis. Neither returned type retains these 18 backings.

The String carrier `zone_carriage.resource_field_paths` is fresh, but its
String elements come from `constructors.field_names` (`:110`) or the empty
literal. Only carrier storage may retire; deep-freeing String payloads would
invalidate the analysis's still-live constructor names.

There is a real **route-specific alias boundary** to preserve:
`CodegenSemanticBodyTypeFactsFromBundleOrDie` and its admitted-receipt variant
return the same five zone Array backings, not copies
(`semantic_body_type_codegen_view_owner.pgy:139-143`, `:168-172`). The C view
continues consuming resource paths and mutable parameter IDs during emission.
The current repository has only one call to the body retirement owner, in the
MIR projection above. The separate admitted-C-view path uses its own verified
bundle (`driver_rung2_owner.pgy:336-373`) and does not call that MIR projection.
The separate source-C admission adapter also constructs a bundle and transfers
its zone view to codegen (`program_admitted_semantic_owner.pgy:32-59`).
Moving body retirement into shared admission/view construction would therefore
introduce a use-after-retire hazard. Keep this retirement path-specific; do
not treat the absence of a ProgramFacts alias as an all-route lifetime proof.
`capability_manifest_owner.pgy` likewise builds and consumes a separate body
bundle; its lifecycle is not this MIR retirement boundary.

### Typed runtime support and minimum falsifiers

Native source contracts support typed shallow Bool carrier retirement:
`type_checker_builtins_stdlib_array.c:312-350` admits the exact approved caller's
named owned Array<T>; `transpiler_expr_stdlib_builtin.c:269-289` chooses the
concrete suffix; `pgy_runtime_builtin_storage_inline.h:45` instantiates
`PGY_ARRAY_DEFINE(Bool, bool)`, whose drop frees carrier storage and resets
length/capacity (`pgy_runtime_memory_array_slot_inline.h:269-278`). The native
LLVM path dispatches the concrete array element size through
`llvm_expr_array_raw_nominal_calls.c:88-105` into
`pgy_array_drop_storage_raw_export`, which shallow-frees and resets the carrier.
These are source checks, not executed Bool probes in this review.

In contrast, self-host C's `CollectionRuntimeCDropStorageFn`
(`collection_runtime_owner.pgy:177-185`) accepts Int/String only and explicitly
dies for the registered Bool kind. Its emitted prelude currently defines only
the Int/String storage-drop functions. The semantic retirement rewrite reaches
that exact mapping (`expr_semantic_call_emit_owner.pgy:259-285`), so ignoring
the reflection rejection would still not close self-host Bool emission.
No direct-MIR LLVM Bool retirement execution was inspected by this reviewer.

The next serial slice needs typed Bool support through the existing runtime
owner, exact once-only carrier retirement for the reached 72-leaf bundle at
the current MIR boundary, and a source-derived census. Preserve exact internal
caller provenance, borrowed/live-slice refusal, no deep String frees and no
post-retire body reads. A bounded false/true Bool storage probe should verify
the generated native C/LLVM and self-host C behavior; an external/wrong-path
caller remains a required negative. A semantic-to-MIR receipt probe should
materialize a real nonempty semantic receipt, copy it to the MIR carrier,
retire the bundle, and verify exact MIR effect and binding identities. A
zone-bearing fixture should distinguish the retained C-view lifetime from the
MIR-only terminal boundary and verify constructor String payloads remain
valid. These are proposed falsifiers, not completed test runs here. No heavy
build, runtime leak measurement or additional compiler edit was performed.

## Focused acceptance update and current typed-retirement candidate

The next serial directive is
`docs/agent_work_directives/body_bundle_typed_retirement_2026-09-30.md`.
Root remains the sole compiler editor. The earlier body omission and ABI gap
above describe the pre-candidate source, not the current mapping after this
patch. This reviewer read the implementation audit and the actual retained
scratch outputs rather than accepting its prose alone.

For the first repair, `.tmp/self_hosted/owned_string_reachable_proof.8Cli9u/`
contains five true and fifteen false owner results, each with query entry1
and scratch retire2. DAG6/10/14 terminal inspections are12/20/28, cross-edge14
is28 and chain4096 is4096. Wrong-path output retains the restricted-caller
diagnostic. Its before/after source receipt lists match ownerD9179E99,
fixture105D088D, wrong-path fixtureE8F39B8C and launcherF06E9EB7. The current
owner hash and retained C/executable hashes agree with the implementation
packet. The native emission's unreachable-statement warning remains visible.

For the composed projection, this reviewer directly compared all twelve
actual C/LLVM `run.out` files in `owned_string_scalar_projection.3sIw2w`
with `expected.run` after CR/newline normalization: all match, all run stderr
files are empty. The matching before/after receipt lists pin the owner,
launcher, source producer, machine declaration and canonical caller registry.
LLVM compile logs retain a target-triple override warning, and the runtime
compile log retains six ATOMIC_VAR_INIT deprecation warnings. This is focused
changed-owner projection evidence, not warning-free or whole-driver evidence.

For the routine repair, the plain and instrumented `run.out` files in
`routine_build_copy_retire.PzUGMt` both report readiness; the actual watched
stderr observes source frees1,1,1, target frees0,0,0 and shared-role frees0.
Before/after receipts pin fixture777D2F54, owner29EF2843, append ownerD0BE1C04
and launcherF06E9EB7. Both compile stderr files are empty; native emission
retains one unreachable-statement warning. These observations support the two
repairs in their stated owner/copy scopes. This reviewer did not rerun their
executables or build a new driver, and does not turn retained logs into
current-tree full-suite acceptance.

### Current candidate source review

- Body retirement owner SHA-256
  `9B1A4070A40F688AB1F1F2ED03725CD9583D5A4DD071C20F8ED4A9D9C983A23E`,
  129/cap140. It adds the exact20 omitted typed bindings and matching shallow
  calls at the existing MIR-only boundary; no alias-producing consumer or
  retirement call location was changed. The added two-calls-per-line retirement
  formatting matches the owner's existing inventory style.
- Self-host C collection-runtime owner SHA-256
  `1F9ADE545E5DBCD71BC320E99124093F965EC4435E9C7619D6C2653378377DFF`,
  589 lines. Kind4 maps to `pgy_ab_drop_storage`, and the prelude emits that
  function using `CompilerAbiLayoutArrayBoolCValueType()` and `bool *data`.
  It accepts a null carrier pointer, shallow-frees `data`, then resets
  data/len/cap. Kinds3/5/6/7 remain refused by this drop mapping. The semantic
  rewrite obtains the actual argument type fact before selecting the kind;
  there is no Bool-as-Int cast or guessed fallback.
- Canonical caller hash489E65FD and generated projection hash605E0614 remain
  unchanged. Bool support adds a representation to an already-approved body
  owner, not another caller authority. Existing borrowed/live-slice and exact
  module/function/type/mode checks are not bypassed by the changed mapping.
- A single data-only last-consumer inventory is cohesive. Receipts, zone and
  capabilities do not represent three independent lifetime transitions on this
  route. Three pass-through owners added only to meet a number would split
  the all-leaf/once-only obligation. The bounded98+20+20=138 justification for
  cap140 is confined to this inventory; proof and routine cap180 remain intact.

The reviewer executed the existing gate's first embedded Python structural
block, extracted unchanged from the live script without a new test file,
with `C:/msys64/ucrt64/bin/python.exe`. It exited0 and printed routine
Int45/String52 and body Int47/String23/Bool2 coverage. That block includes
all-leaf/once-only coverage, MIR retirement ordering, forbidden post-retire
reads, canonical caller consumers and the three concrete shallow-drop emitter
rows. This is an independently observed static PASS; it is not the full shell
gate, generated runtime execution, native LLVM probe or full component gate.

### Existing typed-AST cardinality drift exposed after72-leaf admission

Root's retained `body-lifetime-gate.err` first refused the old exact-six
post-early-retirement cardinality. The current unchanged artifact-lowering
source, SHA-256
`974CEF47D9BC3F6C4EC2FA8C51310012DC4476A71A518C21E6E4A5E5EABDCCD4`,
has seven such returns and seven preceding traversal-retirement calls:

| Return obligation | Cleanup line | Return line |
| --- | ---: | ---: |
| Invalid routine input | 366 | 369 |
| Failed normal routine build | 398 | 401 |
| Failed normal routine append | 412 | 415 |
| Unconsumed semantic collection row | 430 | 433 |
| Failed intent routine build | 456 | 459 |
| Failed intent routine append/plan | 465 | 468 |
| Successful all-routine completion | 476 | 479 |

Direct source review confirms each call is in the actual returning branch and
uses `traversal_artifact`. The extra collection-row refusal and its cleanup
date to `612cb85ac` (2026-09-21), not this candidate. Root corrected the stale
count to7 and explained that branch; the unchanged per-return lexical check
then passes in the independently executed structural block.

Scope limit: this gate starts after early non-traversal retirement. The
iteration-invalid return at308, generic-invalid return at324 and domain-invalid
return at334 occur before that point and have no explicit arena-retirement
calls in this function. Their full ownership/allocator effects were not
executed or closed by this slice. Likewise, the lexical per-return test is
not a general CFG proof that cleanup uses the correct object/branch; current
source review supplies that narrower evidence. Do not report whole-function
typed-AST failure cleanup closure from the fixed cardinality.

Current acceptance still requires the new typed body/free observation,
generated self-host Bool C runtime and exact-caller negative probe plus the
combined lifetime shell gate. Root/test agent own those executions. No new
compiler implementation or second track was added by this reviewer.

## Moving-tree review: shared comment-excluded source-size metric

The user separately requested exclusion of comments from source-size gates.
Root owns this integration; this reviewer changed no counter or cap consumer.
The compiler candidates above remain a separate fixed evidence packet.
Observed combined lifetime output in
`.tmp/owned_result_reachable_20260930/lifetime-final.out` reports both exact
censuses and PASS; its stderr is empty. This observation does not substitute
for the still-running typed body runtime probe.

The new `scripts/source_size_count.py` explicitly retains LF blank records,
inline code, quoted strings/docstrings, heredoc payloads and shebangs. Empty
input counts0, a nonempty final unterminated record counts1, CRLF is one record,
and NUL does not create another record. Unknown suffixes refuse rather than
guessing comment syntax; the listed plain-data suffixes have no comment
stripping. CLI cap rows preserve each duplicate limit and cache a path's
measurement only within one invocation. These are source-policy observations,
not a claim that every shell grammar is handled correctly.

### Independently reproduced lexical counterexamples

The first inspected counter hash was
`203057772FD071E390E8FC60138CB3441E36D6EF96B732338054673F5EB38DFC`.
Its single shell quote state counted the following valid shell input as2
instead of3 by stripping the inner quoted payload record:

```bash
printf '%s' "$(printf '%s' "
# payload
")"
# outside
```

`bash -n` exited0 with empty stderr. Subsequent source hashes changed during
root integration. The simple nested-quote case and a quoted heredoc inside
the quoted substitution now count3 and4 respectively and pass `bash -n`.
The old failure is retained as hash-scoped evidence, not a current-code claim.

A second actual failure was reproduced against hash
`4D2EA56D7D6F35ABD619092FFC67D573BEDD0A6A6AA3313ACA6678109E54220B`,
with that hash identical before and after the in-memory probe:

```bash
value="$(case x in
x) printf '%s' "
# payload
";;
esac
)"
# outside
```

`bash -n` again exited0 with empty stderr. The counter returns5, but six
records are code/string payload under the declared policy. The substitution
frame's generic closing-parenthesis branch at lines141-148 mistakes the case
pattern's `)` for the substitution end. Quote restoration then causes the
real String payload to be treated as an outside comment. This is an actual
under-count and can admit an over-cap shell source; passing the existing
19-method/46-CLI test packet does not falsify it. Root was notified immediately.
Do not claim shell lexical acceptance until this case is handled or explicitly
refused under an inspectable supported-dialect contract.

The C family additionally counts `/\\\n* comment-only\n*/ int x;\n` as3
instead of1. C preprocessing splices the first two records into a block-comment
opener, but the current scanner recognizes only same-record delimiters. This
is a conservative over-count, unlike the shell under-count. It was reported
without claiming a C runtime failure or implementing a second lexer. All
probes used the real imported counter and in-memory inputs; `bash -n` checked
syntax only and did not execute the shell payload.

### Remaining shell-written source cap consumers at review time

The following are actual source size ceilings, not output length, entry count,
progress metrics or arbitrary grep-match counts. Root is migrating this tree;
these locations record the inspected residual snapshot and may be fixed by
the time the integration finishes:

| Shell gate and location | Remaining physical counter / actual ceiling |
| --- | --- |
| `semantic_core_shape_smoke.sh:312-315` | `type_checker.c` uses wc; cap600, while the same gate's later owners already use the shared counter |
| `domain_topology_graph_plan_consumer_owner.sh:89-92` | Four real plan/build/schedule/consumer source owners use wc; each cap600 |
| `intent_observability_abi_registry_smoke.sh:130-132` | Generated Pergyra row projection uses wc; cap150 |
| `language_keyword_registry_smoke.sh:72-73`, `:81-82` | Generated Pergyra projection parts use wc with caps330/200/250; projection hub cap80 |
| `semantic_tagged_enum_payload_variant_provenance_owner.sh:75-78` | Real variant/identity source owners use awk NR; caps1450/160 |
| `self_hosted_component_contract_smoke.sh:24679-24695` | Eight nominal-owner source counts summed through wc into hard family cap900 |
| Same component gate `:24705-24716` | Four mutable-identity source counts summed through wc into hard family cap384 |
| `role_override_mir_replacement.sh:180-188` | Four real source-owner wc counts summed into hard family cap1250 |

The aggregate rows are important: individual `require_max_lines` calls have
moved to the shared metric, but their family ceilings still observe physical
counts. That is a second authority for the same source-size fact, not a
legitimate progress counter. By contrast, builtin-name inventories, producer
occurrence counts, public invocation COUNT_FILE checks, generated C fixpoint
size printed only as status and self-host progress metrics were excluded from
this residual migration report. Production backend/header size, semantic-TU,
beta checklist, world-contract and hard-contract source counts were observed
already migrated during this review.

No root compiler source, independent test fixture or shared scratch was edited.
Only this read-only findings document was updated. The acceptance status is
moving-tree and hash scoped; the independently reproduced shell under-count
is not waived by primary-metric unification or by a green smaller test packet.

### Follow-up: explicit CASE refusal and real shell-cap reachability

Counter hash
`1751B4DC5133A0135CF7C8C30CC6D71145FD71878EA6CA4F382B73E07CBAC8EF`
now explicitly refuses unsupported unquoted `case`/`esac` grammar inside a
command-substitution expansion. An independent in-memory import of the real
counter reproduced the exact second input above as
`ValueError: unsupported shell case grammar inside command substitution`.
The ordinary nested quoted payload still counts3 and the nested quoted
heredoc counts4. Counter hashes before/after this probe were identical. This
closes the reproduced under-count by fail-closed refusal; it is not a claim
that a full Bash grammar is now supported.

For actual gate impact, this reviewer extracted the unique existing literal
`.sh` targets of `require_max_lines` from the live component contract gate,
including multiline backslash syntax, and ran the counter's real `--rows`
CLI over all293 targets in one batch. The batch exited0 without a refusal or
`[source-size]` error, with the same counter hash before/after. This includes
the four literal cap600 shell targets: `codegen_reject_parity_leg.sh`,
`codegen_role_parity_leg.sh`, `codegen_tool_build_leg.sh` and
`codegen_bootstrap.sh`. No inspected actual shell-cap target is blocked by
the new refusal. Dynamic `$path` targets and all repository shell files were
not asserted covered; the component source and consumer migration are still
a moving tree. The historical CASE failure above remains hash-scoped rather
than a current implementation defect.
