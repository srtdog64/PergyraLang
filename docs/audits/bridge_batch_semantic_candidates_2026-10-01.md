# Semantic bridge batch candidates

Status: PHASE-TWO C IMPLEMENTATION CANDIDATE; eight production files frozen.
No row closure or installed-driver behavioral acceptance claimed.
Revision inspected: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
Scope and integration owner: `bridge_batch_closure_2026-10-01.md`, root.
Phase one was read-only and wrote only this report. Its semantic candidates
and proposed gates below remain unimplemented. Phase two changed only the
seven initial C consumer files and the separately assigned Slice consumer
listed below, and used task-local native
development probes. No registry, tests, installed binaries, index, installation,
or Git history was changed by this reviewer. Root owns the shared driver rebuild
and integration acceptance.

## Result and selection advice

There is no observed one-line full-row closure among the eight semantic rows.
`semantic.nominal_field_kind` is the smallest useful follow-up candidate, but
the existing vocabulary smoke is not a proof that every field-kind consumer
or producer is closed. Its immediate executable delta is admission of exact
declaration-field kinds at the existing index, together with deletion of
consumer-local kind spellings. Full closure additionally requires preservation
of the currently erased vessel-field distinction. Do not change its row to
CLOSED after only the vocabulary/index delta.

The distinction matters: an explicit refusal of an unsupported operation is
not automatically dual semantic authority. Pool materialization, layer
scheduling, and resource cleanup need their own owners; the report does not
turn these features into field-kind closure requirements by association.

## Candidate 1: exact nominal field kind

### Owned facts and reached path

- Declared fact owner: `src/compiler/mir_decl_field_kind_vocabulary.def`,
  `PGY_MIR_DECL_FIELD_KIND_ROWS`; stable identity `SyntaxNodeIdFieldOrdinal`.
- Vocabulary projection: `src/self_hosted/lib/nominal_field_kind_owner.pgy` and
  generated `mir_decl_field_kind_vocabulary_projection_owner.pgy`.
- Source producer: `SemanticAstNominalConstructorFactsFromArtifact` in
  `src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy`; it consumes
  `TypedAstArenaAuxValueText`-carried kind, not the field type as kind.
- MIR projection: `src/self_hosted/mir/declaration_rows_owner.pgy`,
  `SelfMirDeclarationsFromAnalysis`, carries field kind and source ID from the
  semantic constructor owner. `SelfMirDeclarationRowsReady` rejects missing
  kinds and host-incompatible kinds for this producer.
- Untrusted document admission: `BuildMirProgramDeclarationFieldIdentityIndexFromDeclarationSpans`
  in `src/self_hosted/mir_lower/program_declaration_field_identity_index_owner.pgy`.
  This is reached by `BuildMirProgramDeclarationIndexFromTable`, domain/machine
  admission and the installed direct C/LLVM graph-plan consumers.
- Last legitimate kind consumer examples:
  `DirectMirScalarProgramLogicalRecordCandidateAt` in
  `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy`,
  exact topology/assignment joins and identity-cell placement.

### Actual remaining paths, not just the open-reason prose

1. The document field index requires a nonempty `field_kind`, but does not
   call `NominalFieldKindKnown` or `NominalFieldKindAllowedForDeclaration`.
   `MirDomainTopologyDeclarationIndexReady` checks `.valid` and row counts,
   not this contract. Thus a field which no topology operation happens to
   reference is not universally kind-admitted there. Later legacy
   `src/self_hosted/mir_lower/decl_lower.pgy:EmitDeclFields` makes the policy
   decision again. This is the smallest missing-fact/unknown-kind seam.
2. `src/self_hosted/parser/decl_nominal_owner.pgy:ParseNominalDecl` consumes
   the `vessel` field modifier without retaining a distinct flag/kind; it
   emits the same ordinary field row. `CodegenAstTextAuxPayloadFor` in
   `src/self_hosted/hir/ast_text_inventory_owner.pgy` has no VesselSlot case
   and defaults field rows to `NominalFieldKindField()`.
3. Native source has the distinction: `src/parser/parser_decl.c` sets
   `ClassField.is_vessel_field`; `pgy_class_decl_field_model_try_build` retains
   it. `src/compiler/mir_decl_header_fields.c:mir_decl_field_metadata_init_class`
   copies it into `MIRDeclField.is_subject_like`, but
   `src/compiler/mir_json_dump_decl.c:mir_json_decl_field_kind_name` emits
   ordinary `field` for every CLASS row. Native `ast_print.c` also does not
   print the class-field modifier. The registry contains fourteen identities
   and no vessel-slot identity. This is real information loss, not merely an
   unsupported unrelated runtime operation.
4. Native `MIRDeclField` stores coarse kinds plus flags. Both JSON projection
   and `mir_domain_topology_field_kind_matches` derive the precise domain/layer
   kind. These are observed projections of stored owner facts, not proof by
   themselves of illicit AST recovery. Nevertheless, a first-class exact kind
   or a single shared projection should own the classification, with
   contradictory flags rejected; do not call duplication closed solely
   because their string outputs happen to match.

### Consumer spelling residue outside the current six-file vocabulary gate

The following source files compare field-kind wire spellings directly instead
of consuming the generated vocabulary. These comparisons do not all choose
new meanings, but they escape the declared spelling owner and its ratchet.

- `src/self_hosted/mir/intent_resource_lifetime_owner.pgy`
- `src/self_hosted/mir/nominal_abi_layout_fact_owner.pgy`
- `src/self_hosted/mir_lower/program_declaration_zone_authority_index_owner.pgy`
- `src/self_hosted/compiler/direct_mir_composite_intent_program_graph_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_constructed_generic_member_declaration_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_identity_cell_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_inferred_generic_member_declaration_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_intent_cell_placement_owner.pgy`
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_graph_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_plan_owner.pgy`
- `src/self_hosted/compiler/direct_mir_nested_intent_program_graph_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_nominal_declaration_abi_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_nominal_literal_declaration_fact_owner.pgy`
- `src/self_hosted/compiler/direct_mir_role_override_program_identity_owner.pgy`
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy`

### Minimum implementation card

- Objective: one exact declaration-field identity/kind admission before C/LLVM
  plans; preserve recognized source distinctions rather than guess by names,
  type, ordinal, or legacy flags.
- Priority: stable kind IDs and field source IDs, one owner admission,
  information-loss removal, consumer migration, negative ratchet.
- Immediate delta: import the existing kind owner into the field-index owner;
  reject unknown and host-incompatible field kinds once; migrate the listed
  consumer literals to generated accessors; remove the late second policy
  choice in `EmitDeclFields` or make it consume the admitted index.
- Full-row delta: append, never renumber, the required vessel distinction or
  preserve it as an explicit typed semantic field fact; retain it from parser
  through arena, constructors, MIR, canonical field remap, and runtime
  consumers. A valid source feature cannot silently flatten to a different
  kind. A new refusal frontier is not a substitute for preserving established
  behavior without a repository-approved compatibility decision.
- Positive fixtures: existing effect/zone BindingSlot/tobject exact runtime
  fixture; scalar/passive record direct C/LLVM; nested vessel-field fixture
  `tests/cases/backend_compare/llvm_role_self_nested_field/main.pgy` as a
  type/field-identity falsifier before relying on its runtime result.
- Negative gate: delete kind; unknown nonempty kind on an unused declaration
  field; valid kind on wrong host; known wrong kind with unchanged source ID;
  foreign valid field ID; duplicate ID; retain sentinel artifacts after each
  direct C/LLVM refusal. Add a no-consumer-literal ratchet over production
  field-kind consumers, not just six legacy files.
- Existing gate foundations: `tests/mir_decl_field_kind_vocabulary_smoke.sh`,
  `tests/self_hosted/parity/domain_topology_admission_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_effect_declaration_parity_owner.sh`,
  `tests/self_hosted/parity/tobject_boundary_execution_owner.sh`.
- Effort: index/literal delta is small-to-medium, one shared driver rebuild;
  full preserved-vessel identity is medium-to-large and also reaches the
  native producer. Neither has been implemented or behaviorally verified here.
- ACTIVE collection dependency: no dependency for plain field-kind admission,
  ID/kind rejection or scalar/vessel identity. Deep-drop proof for a resource
  field is a separate ACTIVE obligation; a kind tag is not ownership proof.

Pool nuance: `mir_decl_header_fields.c` retains native pool capacity, but
`pgy.mir.v1` declaration field JSON omits it. `EmitDeclFields` explicitly
refuses effect/relation pools for missing capacity. That refusal is a visible
support limit, not evidence of guessed capacity or successful fallback. Do
not claim pool materialization from a closed kind vocabulary.

## Candidate 2: callable receiver carriage, full closure is larger

- Owner: `src/self_hosted/semantic/callable_receiver_carriage_policy_owner.pgy`.
- Reached self source-C path:
  `CodegenCallableReceiverFactsFromSemantic` in
  `src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy`
  already owns exact role target type, distinct target carriage, source
  parameter offset and erased-receiver shape at admission.
- Remaining producer gap:
  `src/self_hosted/mir/routine_receiver_carriage_owner.pgy:SelfMirRoutineReceiverForOwner`
  carries only owner/routine/carriage, derived from declaration
  `uses_pointer_self`; it does not carry the concrete role-target fields which
  the admitted general codegen view already has.
- Native parallel producer:
  `src/compiler/mir_signature_metadata.c:mir_routine_receiver_carriage_capture`
  and `src/compiler/mir_program_fact_validate.c:mir_validate_receiver_carriage_facts`
  also derive receiver from the declaration compatibility flag.
- Exact compatibility last consumers:
  `src/codegen/llvm_domain_lookup.c:llvm_type_name_uses_pointer_self`,
  `src/codegen/transpiler_host_self_policy.c:is_pointer_self_host_type_name`,
  general parameter consumers in LLVM decl/mir-param/boundary call emission,
  `src/compiler/mir_signature_metadata.c` parameter ABI capture, and their
  `src/codegen/host_decl_compat.c:pgy_host_decl_compat_uses_pointer_self` source.
- Minimum full delta: extend existing callable MIR fact/wire with concrete
  target carriage and source receiver offset from semantic owner; validate
  it at both native and self admissions; make receiver consumers read that
  exact row, general parameters read their distinct ABI facts; delete the
  compatibility classification from those reached consumers. Do not use
  pointer-self receiver facts to decide ordinary vessel parameter passing:
  `docs/grammar/01_syntax.md` explicitly separates those axes.
- Positive gate: existing `mir_receiver_carriage_admission_owner.sh`,
  `driver_rung2_callable_receiver_carriage_owner.sh`,
  `role_override_mir_replacement.sh`, native + installed C/LLVM mutable
  subject/vessel and builtin Int target controls.
- Negative gate: missing/foreign role target, wrong concrete target carriage,
  mutated 0/1 receiver source offset, temporary mutable receiver, and ordinary
  parameter ABI mutation; no target-name or late nominal-kind reconstruction.
- Effort: medium-to-large producer/wire/admission/native consumer migration;
  cannot be closed by moving one self C consumer. No implementation here.
- ACTIVE collection dependency: receiver-only scalar identity can be
  independent. Resource-bearing general formal transfer still requires the
  collection/aggregate-formal lifetime proof; do not infer it from this ABI.

## Other scoped rows: why not select for immediate census reduction

| Row | Current observed seam | Decision |
| --- | --- | --- |
| `semantic.domain_runtime_assignment` | `SelfMirProgramDomainFactsFromReadyArtifact` calls `SelfMirDomainRuntimeAssignmentsFromFacts` at the MIR boundary; semantic participant/path decisions remain in `src/self_hosted/mir/domain_runtime_assignment_fact_owner.pgy`. General C consumes the admitted plan. Full lifecycle/world-effect/shared native-self runtime plan is not closed. | Real stage-owner debt plus unsupported frontier; not a short full closure. Moving files alone does not replace the semantic path. |
| `parser.syntax_provenance` | Native HIR/DIR/RIR builders still take the annotated AST; `rir_builder.c` reads source AST while capturing stable IDs. | Stable IDs alone do not delete the AST fanout. Whole typed projection migration, not a vocabulary cleanup. |
| `semantic.symbol_type_graph` | `src/compiler/mir.c:mir_lower` requires `SemanticResult` for parallel capture; `src/compiler/air_evidence_dag.c:air_collect_dag_evidence` reads semantic counts/lifecycle/capability; MIR still captures source AST types. | Moving the remaining parallel facts behind HIR is a useful partial delta, not full-row closure while AIR/source type reads remain. |
| `semantic.resource_flow_universe` | Native routine-local rows already project HIR to MIR/RIR; `rir_flow.c:rir_attach_resource_flow_identity` matches stable IDs then finds summary by `fact->name`. Both self JSON writers emit `resource_flow_symbol_count:0` and `resource_flow_symbols:[]`. | Stable summary join is a small native delta, but self production and typed transition payload remain. Native-only deletion is not installed self-host substitution. |
| `semantic.loop_flow_summary` | `SelfMirLoopFlowRowsAppendRoutine` derives identity/kind but sets effect bases/deltas 0, flags 1 and empty entry/exit states. Production artifact writer carries these rows. | Nonempty effect/state and must-return need a semantic producer, not a consumer-side default. Shares resource-flow state obligations. |
| `resource.region_allocation_plan` | Native `verified_region_plan.c` consumes AIR and projected MIR escape facts; native C/LLVM use the plan. `RegionPlanProduce` is only used by its own witnesses and `region_plan_manifest.pgy`, not the installed driver route. | Self-host region owner is SURFACE, not SUBSTITUTING. General String-concat/retention region facts are absent from the self executable route. |

The resource/loop full semantics would intersect the ACTIVE field/formal
collection lifetime proof when the same resource is stored, borrowed,
transferred, merged or released. Start no parallel implementation of those
semantics while root is closing a different executable rung.

## Phase-one evidence limits

This review used current source and gate source, not registry prose alone.
It did not run a new compiler, rebuild a driver, execute gates, mutate MIR, or
execute rejected code. The installed paths above are statically traced reach
claims; fresh behavioral acceptance must be observed by root in the selected
shared integration gate. No BRIDGE count reduction is claimed by this report.

## Phase two: ArrayString ABI C consumer implementation candidate

### Objective and owner boundary

- Objective: make the reached C descriptor declarations and initializers
  consume the admitted `DirectMirArrayStorageAbiProjection`; remove local
  descriptor spelling/order reconstruction and duplicated program assertions.
- Priority: admitted identity/layout, required-fact refusal, last-consumer
  migration, negative evidence, then patch size.
- Fact owner: `direct_mir_array_storage_abi_projection_owner.pgy`, with the
  existing storage layout/target owners. The existing
  `direct_mir_array_storage_c_assertion_owner.pgy` owns the six C ABI assertions.
- Last consumers: CFG array/foreach/collection preambles and static slots,
  program ArrayString descriptor materialization, and empty literal emission.
- Forbidden fallback: successful empty output for a required invalid program
  ABI projection; consumer-owned four-field/order reconstruction; positional
  initializers which silently depend on that reconstructed order.
- Integration gate, owned and executed by root:
  `tests/self_hosted/parity/array_string_layout_consumer_closure_owner.sh`.
  Missing or inconsistent layout facts must refuse before replacing a sentinel
  artifact. This reviewer did not execute that gate or change registry status.

### Initial seven frozen production edits

All paths below are under `src/self_hosted/compiler/`:

| File | Owned change |
| --- | --- |
| `direct_mir_scalar_cfg_array_c_materialization_owner.pgy` | `DirectMirScalarCfgArrayCStruct(element_type, array_type, ref storage)` validates and emits fields from admitted order/names/types, then calls the existing six-assertion owner. `DirectMirScalarCfgArrayCInitializer` owns repeated named-field descriptor initialization and rejects invalid projection or missing expressions. |
| `direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy` | Both ABI preambles pass storage projection; Int/String static descriptors use the shared named-field initializer. |
| `direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy` | String ABI preamble and empty/nonempty static descriptors consume projected type/storage and the shared initializer. |
| `direct_mir_scalar_program_c_array_string_storage_materialization_owner.pgy` | Required invalid projection now calls `Die`; private `pgy_as` declaration, six assertions, and fresh/empty descriptor initialization reuse shared owners. Existing ownership, push, clone and drop algorithms remain unchanged. |
| `direct_mir_scalar_program_c_array_string_literal_expression_owner.pgy` | Empty compound literal uses shared named-field initialization; existing unsupported owned-literal frontier remains unchanged. |
| `direct_mir_scalar_cfg_array_int_c_emission_owner.pgy` | Int preamble passes admitted storage and projected descriptor type. |
| `direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy` | Both ABI preambles and static descriptor initializers consume projection. Unsupported element type now explicitly refuses instead of selecting the String branch. |

At final review, the seven-file diff was 94 insertions and 62 deletions.
`git diff --check` passed. Source was frozen before root's shared rebuild;
no additional production changes or builds will be made by this reviewer
without an explicit reopen for an observed integration failure.

### Exact caller requirements and preserved limits

The old `DirectMirScalarCfgArrayCStruct(element_type, length_type, array_type)`
signature is removed. All six existing CFG calls were migrated, and program
ArrayString materialization now also calls it. Callers select their family ABI
at its owner and pass `abi.storage`; they do not reconstruct storage facts.

Program String descriptors retain the existing private const-qualified read
view by using `Concat("const ", projection.c_element_type)`. This does not add
an ownership or lifetime contract or change the physical descriptor ABI.
The six assertion messages now use the existing shared `Array storage ...
receipt` wording rather than the deleted program-local `Array<String> ...`
strings. Integration checks must inspect the shared assertions accordingly.

Legitimate unused/absent plan preambles still produce no declaration. Literal
nonmatches and the existing unsupported multi-element owned-literal frontier
still return `None`. The change does not invent pool capacity, vessel semantics,
ACTIVE collection field/formal lifetime proof, or a native/schema revision.

### Observed development probes, not self-host acceptance

Task-local scratch: `.tmp/self_hosted/bridge_array_string_c_probe_semantic/`.
The installed native compiler compiled the changed shared materialization owner
into `probe.exe` and the invalid-projection probe into `invalid.exe`, each with
zero errors and warnings. This is native development evidence only: it is not a
rebuilt self-host driver, source-to-verified-MIR integration, or CI evidence.

- Positive: execute `probe.exe` directly to generate a descriptor and an empty
  named-field initializer, then compile/run its C with
  `gcc -std=c11 -Wall -Wextra -Werror`. Observed exit 0. Generated C included
  size 32, alignment 8, and offsets 0/8/16/24 for the admitted fields.
- Negative: pass an otherwise copied projection with `valid=false` to the
  shared initializer. `invalid.exe` returned nonzero, emitted
  `CODEGEN ERROR: direct MIR Array C initialization projection is invalid`
  on stdout, and emitted no `.data =` initializer. Corrected refusal harness
  observed exit 0. Diagnostic stream behavior was not changed.
- Harness correction: the first positive attempt redirected native CLI
  `--run` stdout, which also included its compilation status and contaminated
  generated C. The accepted probe ran the compiled executable directly.
  The first negative harness wrongly expected the `Die` diagnostic on stderr;
  the accepted harness checked stdout and preserved the external exit status.
- Native AST parsing of the changed program ArrayString materializer emitted
  no diagnostics; whole production typing and all integrated consumers remain
  root's pending single-driver verification responsibility.

No driver rebuild, installed-driver parity, full gate, commit, push, or BRIDGE
count reduction is claimed here. The source candidate and audit are ready for
root's independent integration review.

### Same-rung follow-up: supported String SliceCopy consumer

Root's final inventory found the supported C String SliceCopy path still using
`(pgy_as){NULL, 0, 0, NULL}` as its copied Array descriptor. Root independently
assigned only `direct_mir_scalar_program_c_slice_expression_owner.pgy` to this
reviewer and retained ownership of its general program C caller and tests.

Exact new signature:

```pergyra
func DirectMirScalarProgramCSlicePreamble(
    ref expressions: DirectMirScalarProgramExpressionSet,
    ref array_string_abi: DirectMirScalarProgramArrayStringAbiFact,
    array_string_projection: Option<DirectMirArrayStringAbiProjection>
) -> String
```

Root's existing call must pass `plan.program.array_string_abi` and the carried
`array_string_projection` after `plan.program.expressions`. When String slice
or clone runtime is reached, the consumer explicitly requires `Some` and calls
the existing private carrier owner, which validates `ReadyForFact` for the C
target. Its result supplies `fact.c_array_type`; the String SliceCopy empty
expression combines that admitted carrier and the shared named-field
`DirectMirScalarCfgArrayCInitializer(projection.storage, ...)`. The array length
field passed to the existing Slice runtime also comes from storage projection.

The borrowed Slice's distinct two-field physical representation is unchanged;
an Array descriptor is not substituted for a Slice. StringClone still uses the
already projected ArrayString `new()` path. The file's two remaining positional
Array initializers are confined to Int-only branches; no admitted Int target
storage was newly available in this scope, so they were deliberately unchanged.
No additional String descriptor-layout literal was found in this file.

Scoped `git diff --check` passed. No additional compile, driver rebuild,
executable probe, binary, test, registry, or Git-index change was made for this
follow-up. The eighth source file is frozen, and the initial seven remain
frozen. Root's shared integration gate owns positive String SliceCopy and
missing/drift projection refusal evidence for this final consumer.

### Reached integration dependency: canonical borrowed CFG admission

Root explicitly reopened a separate bounded admission dependency after both
the old installed driver and the frozen ABI candidate rejected current
producer-issued `for_each.pgy` MIR. No frozen C/LLVM layout materializer was
reopened. The blocker was the legacy scalar-CFG claimant's strict empty
collection table guard; seven-block input then reached the unrelated Option
envelope refusal. Terminal Void-return admission was a second dependency owned
and implemented separately by root.

The existing canonical reader is the fact owner:
`mir_lower/collection_ownership_fact_owner.pgy:BuildMirCollectionOwnershipFacts`
checks required fields/counts, canonical IDs, routine identity, unique source
binding joins, ArrayString binding type, origin/ownership consistency, duplicate
rows and retirement/transfer constraints. `BuildMirRoutineFactIndex` carries its
typed vectors and incorporates the reader's validity. It deliberately allows a
canonical empty ownership table and only requires positive borrowed-origin IDs;
those are not general completeness or origin-provenance proofs.

The current self producer supplies the narrower facts without new inference:
`semantic/ast_collection_ownership_verdict_owner.pgy` initializes origin ID from
`locals.node_ids` at line 84 and source binding ID to zero. Its borrowed-literal
branch changes only ownership/origin categories at lines 120-125. At lines
548-565 it emits one row for every inferred source StringArray local.
`mir/collection_ownership_fact_owner.pgy` preserves those semantic origin IDs
verbatim. Thus this bounded self-produced borrowed-literal subset has
`origin_syntax_id == binding_syntax_id`; the equality is not imposed on the
general MIR family or borrowed native output.

#### Frozen implementation boundary

All three files are under `src/self_hosted/compiler/`:

- `direct_mir_scalar_graph_admission_owner.pgy` now shares
  `DirectMirRoutineHasNoUnsupportedMetadataExceptLoop` for unchanged empty
  generics/params/resource/destructure and absent parameter-flow metadata.
  `DirectMirRoutineHasNoUnsupportedFactsExceptLoop` still requires collection
  count zero and an empty ownership table. The stricter no-loop function still
  additionally requires all loop/iteration arrays and counts to be empty/zero.
  Existing Option and bounded CFG-plan callers therefore keep their original
  zero-ownership meaning.
- `direct_mir_scalar_cfg_borrowed_collection_admission_owner.pgy` adds only
  `DirectMirScalarCfgBorrowedCollectionFactsReady(ref index) -> Bool`. It consumes
  a valid canonical typed index, requires every declared source ArrayString
  binding to join uniquely through `MirCollectionOwnershipLocalRow`, checks
  borrowed-literal/borrowed-elements/live/source0 and the existing producer's
  origin/binding equality, and requires the owned table to be complete for
  those bindings. The ArrayString type selects a required fact; it does not
  infer borrowed ownership from type. No names, raw AST or spelling recovery
  are used. Final source is 54 lines including comments and blank lines; SHA256
  `050afd3323770a77b2f6d59ea57f4d32e51893d56aba2492393dfd515794b372`.
- `direct_mir_scalar_cfg_graph_route_owner.pgy` consumes the base common program
  envelope, shared unsupported-metadata frontier, canonical routine index and
  the exact borrowed predicate. Existing boolean refusal behavior remains;
  this does not add a source/native retry. Root separately owns its terminal
  Void import/return branch and the final dispatcher attribution fix.

The last legitimate consumer is the scalar-CFG exact routine claimant, followed
by the existing graph admission/materialization owners. There is no new lifetime
schema, carrier, proof taxonomy, retirement algorithm, semantic producer rule,
pool/vessel contract or generic ownership-reader change. The eight C production
files remain frozen. This reviewer did not change tests, driver build/install,
registry, OWNERS, handoff, Git index or commits.

#### Observed native development evidence and limitations

Unique scratch:
`.tmp/self_hosted/bridge-borrowed-semantic-dabcd6524f444584a1b2f7fda1f72642/`.
`probe.pgy` consumes the canonical document/declaration/routine indexes and
`BuildMirRoutineFactIndex`; it does not reconstruct or compile program artifacts
from mutated MIR. `create_inputs.mjs` mechanically creates isolated copies;
`run_cases.mjs` executes only the admission probe and records `results.json`.

Native C emission and native C-backed probe compilation each observed exit 0,
zero errors and 15 unreachable-statement warnings. All 13 final
admission expectations passed:

- Original producer MIR: valid typed index, borrowed predicate true, shared
  metadata true, strict old Option/CFG collection guard false.
- Missing row with canonical count0/empty table: typed index remains valid;
  borrowed predicate false. This specifically falsifies completeness fallback.
- Positive origin-ID drift: typed index remains valid; borrowed predicate false.
  This specifically falsifies positive-number-only origin admission.
- Unknown, owned clone, retired, nonzero source binding, duplicate row, binding
  drift and foreign routine: borrowed predicate false through the established
  reader or bounded predicate; none is reinterpreted as borrowed.
- Added parameter-flow metadata, resource count drift and destructure count
  drift: shared metadata frontier false. Resource drift is also rejected by
  the canonical reader at `resource_flow`; no weaker expectation was retained.

The original producer JSON SHA256 remained unchanged. These are native
edit-loop checks, not installed-driver parity, ABI artifact publication,
negative sentinel preservation, fixed-point evidence or BRIDGE closure.
Root's shared integration gate owns those claims and broader non-regression.

Corrections were explicit: the first source probe rejected a local copy of
`index.collection_ownership_facts` as a borrowed-ref provenance escape. Root
authorized reopening that one file only to remove the copy and consume fields
directly; it was then re-frozen. The temporary harness needed `env` for Args,
and direct absolute Windows path arguments produced an empty ReadFile result;
the accepted scratch run used repository-relative forward-slash input paths.
An initial standalone gcc command lacked the runtime include path; the observed
successful native C build used the compiler's owned build path instead.

Native borrowed-literal output currently carries the literal AST origin ID
rather than this self producer's declaration origin ID. Its parameter-flow,
resource/loop and iteration-provenance differences remain independently
unsupported. Do not remove its rows, rewrite its origin or call this bounded
self-CFG admission a native/general lifetime proof. The row remains BRIDGE
until root independently observes the required installed executable gates.
