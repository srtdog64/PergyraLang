# Direct-MIR Semantic-Dimension Map — 2026-08-26

Status: `AUDIT ONLY`; this report is not a semantic owner, a closure claim, or
self-host substitution progress.

Audit base: `9ca4a69517142a4c87eb47862afcd55a9a9f2011`.

## Scope and method

This is Track B from
`docs/agent_work_directives/semantic_hop_parallel_audit_2026-08-26.md`.
The census starts at
`src/self_hosted/compiler/driver_bootstrap_main.pgy`, follows only literal
Pergyra imports, and classifies the reachable `direct_mir_*.pgy` owners. No
build or test was run. "Live" below therefore means reachable from the
production self-host import closure; it does not mean that one fixture executes
every reachable route.

Observed import-closure census:

| Inventory | Reachable count | Counting rule |
| --- | ---: | --- |
| All Pergyra files | 1,546 | Literal import closure |
| `direct_mir_*.pgy` files | 887 | Basename filter inside that closure |
| Admission family | 66 | Basename contains `_admission_`; 65 end in `_admission_owner.pgy` |
| Route family | 19 | Basename contains `_route_`; 10 end in `_route_owner.pgy` |
| Plan family | 64 | Basename contains `_plan_`; 28 end in `_plan_owner.pgy` |
| Explicit C emission | 22 | Ends in `_c_emission_owner.pgy` |
| Explicit LLVM emission | 24 | Ends in `_llvm_emission_owner.pgy` |
| Target-neutral/mixed emission | 23 | Ends in `_emission_owner.pgy`, excluding explicit C/LLVM suffixes |

These sets overlap by responsibility. They are a navigation inventory, not a
completeness score. In particular, a filename count is not evidence of an
independent semantic decision.

## Live stage inventory

| Stage | Observed owners and responsibility | Boundary assessment |
| --- | --- | --- |
| Admission | The 66-file family includes program, routine, instruction, expression, literal, ABI, and call admission. Representative owners are `direct_mir_scalar_cfg_program_graph_admission_owner.pgy`, `direct_mir_scalar_program_call_expression_admission_owner.pgy`, `direct_mir_routine_parameter_set_admission_owner.pgy`, and the collection/nominal family admission owners. | Typed facts are produced from `MirMachineLayerAdmittedJsonInput`; this stage is allowed to reject missing or inconsistent identity. It is not a backend. |
| Route | The 19-file family includes claim facts and route admission, notably `direct_mir_scalar_program_route_admission_owner.pgy`, collection and intent route facts, nominal route facts, and scalar CFG subroutes. | Route owners make exclusive support-frontier decisions. `direct_mir_multi_routine_projection_owner.pgy` states that rejected shapes are not reinterpreted and orders collection, intent, scalar, nominal/generic, and terminal routes. |
| Plan | The 64-file family contains sealed receipts, plan projections, plan mutation/readiness owners, and local plan fragments. The 28 exact plan owners include scalar CFG, collection, struct/generic/option, callable-role, and three intent plans. | Plans bind admitted identity and supported topology before target text. Family plans are not interchangeable merely because their names end in `_plan_owner`. |
| C emission | All 22 explicit C-emission basenames have a reachable LLVM sibling. Examples include collection, struct value-flow, generic/option/nominal, scalar CFG, and nested intent. | These owners should materialize target spelling and ABI operations from a sealed plan. |
| LLVM emission | The 22 paired families plus `direct_mir_legacy_intent_program_llvm_emission_owner.pgy` and `direct_mir_composite_intent_program_llvm_emission_owner.pgy` are reachable. | The two additional owners mark LLVM-only admitted frontiers, not missing evidence that a common C/LLVM plan must exist. |

The backend root validates one `CompilerTargetProjectionFact`, selects an
exclusive route, and attaches admitted domain topology only after payload
projection in `CompileAdmittedDirectMirForTargetObserved`. The scalar program
path constructs one `DirectMirScalarCfgGraphPlan` before choosing
`DirectMirScalarCfgEmitC` or `DirectMirScalarCfgEmitLlvm`. Collection,
struct-value-flow, and nested-intent projection owners show the same one-plan,
two-target shape.

## Semantic-dimension map

The status column distinguishes observed sharing from an inference about
ownership. No row below proposes a universal plan or query layer.

| Semantic dimension | Existing fact/owner spine | Live consumers | Sharing and repeated-decision result |
| --- | --- | --- | --- |
| Callable identity | `direct_mir_routine_signature_fact_owner.pgy`, `direct_mir_routine_param_fact_owner.pgy`, `direct_mir_routine_parameter_set_fact_owner.pgy`, `direct_mir_scalar_program_callable_fact_owner.pgy`, callable inventory/admission, and `DirectMirScalarCfgRoutinePartitionFact` | Scalar GraphPlan readiness, routine signatures, direct-call projection, family emitters | **Observed shared.** Syntax identity, routine ordinal, ordered parameter type, and carriage are sealed before C/LLVM. Backend symbol spelling is target-only. |
| Call edge | `direct_mir_scalar_program_call_callee_identity_owner.pgy`, `direct_mir_scalar_program_call_expression_admission_owner.pgy`, `direct_mir_scalar_program_direct_call_readiness_owner.pgy`, and `direct_mir_collection_program_edge_fact_owner.pgy` | C/LLVM direct-call expression owners and collection emitters | **Shared identity, one deferred reconstruction.** `DirectMirScalarProgramDirectCallFact.argument_rows` owns ordered call arguments at admission. The normalized expression storage then uses `left/right` for up to two arguments and `nary_operands` above two; C and LLVM each repeat the empty-vector-to-`left/right` selection. This is real duplication, but removing it changes the current expression-storage convention and is not the smallest candidate selected below. |
| Type representation | Declaration/nominal identity facts, generic specialization and representation facts, logical-record/enum facts, collection/Option/Result ABI facts, and scalar target type owners | C type/signature owners and LLVM type/signature owners | **Logical type facts are shared.** C typedef spelling and LLVM structural type spelling are different target materializations. No second target-neutral type decision was proven from filename similarity. |
| Carriage/ownership | Routine parameter facts; callable parameter policy and role plan; owned-string, owned-array, readonly-ref, and value-result policy/target owners; GraphPlan `parameter_carriages` | Signature, binding, copy-in/out, call-argument, and cleanup materializers | **Mostly shared, with one exact repeated decision.** C and LLVM routine-signature owners both recompute the same target-indirect predicate from value-result, readonly logical-record, and readonly `Array<String>` facts. This is the sole consolidation candidate below. Backend-specific address formation and copy operations remain target-only. |
| Intent policy | Legacy, composite, and nested route/graph/plan owners; `direct_mir_intent_plan_projection_owner.pgy`; typed intent mode, priority, phase, action, and cleanup projections imported from `mir_lower` | Legacy/composite LLVM emitters and nested C/LLVM emitters | **Observed shared inside each admitted frontier.** Nested intent creates one `DirectMirNestedIntentProgramPlan` and passes it to both emitters. Legacy and composite are LLVM-only frontiers. Their exact cardinalities and policy topology do not justify a common `ProgramPlan`. |
| Authority | The direct-MIR filename inventory has no `direct_mir_*authority*` owner. Intent plans consume the existing intent action contract; the backend root delegates final domain topology to `domain_topology_graph_plan_consumer_owner.pgy`. | Intent plan validation and final C/LLVM domain-topology attachment | **Inference: transported, not re-owned.** Direct-MIR must not invent a second authority spine. No repeated C/LLVM authority decision was observed. |
| Cleanup | `direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy`, `direct_mir_scalar_program_runtime_value_lifecycle_owner.pgy`, intent cleanup contract projections, and family plan cleanup rows | C/LLVM array-string cleanup owners and intent emitters | **Observed shared.** Both array-string cleanup emitters call `DirectMirScalarProgramArrayStringCleanupDropSymbol`; runtime-value lifecycle is decided once from the GraphPlan. C/LLVM only spell calls and storage names. |
| ABI | Captured array/Option/Result ABI facts, scalar runtime ABI fact/projection owners, family ABI fact owners, `target_projection_fact_owner.pgy`, and runtime ABI registries | Plan readiness, selected-target ABI projection, C/LLVM declarations and calls | **Observed shared.** For example, collection constructs one `DirectMirArrayIntAbiProjection` from the sealed collection plan and selected target, then hands the same projection to one emitter. Target size/alignment/symbol spellings are projections, not new semantic owners. |
| Target-only projection | `direct_mir_backend_projection_owner.pgy`, family projection owners, 22 C emission owners, 24 LLVM emission owners, plus target-specific type, binding, expression, and cleanup owners | `CompilerEmissionArtifact` payload production | **Expected split.** C syntax, LLVM SSA, target symbols, and target runtime calls should remain separate. A target owner becomes suspect only when it reopens admitted/source/AST/JSON or re-decides a target-neutral fact. |

## Shared plan evidence across target pairs

| Production family | Shared receipt before target choice | Target boundary | Result |
| --- | --- | --- | --- |
| General direct CFG | `DirectMirCfgPlanFromAdmitted` | `DirectMirCfgEmitC` / `DirectMirCfgEmitLlvm` | One plan; no candidate. |
| Composable scalar program | `DirectMirScalarCfgProgramGraphPlanFromAdmitted` | `DirectMirScalarCfgEmitC` / `DirectMirScalarCfgEmitLlvm` | One GraphPlan. The parameter-indirection subdecision is the exception identified below. |
| Collection program | `DirectMirCollectionProgramPlanFromAdmitted`, then one selected-target array ABI projection | Collection C / LLVM emission owners | One plan and one ABI projection; no candidate. |
| Struct value flow and paired nominal/generic families | Family-specific sealed plan | Basename-paired C / LLVM emission owners | Target split is justified; inspected projection seams build the plan once. |
| Nested intent | Route fact, then `DirectMirNestedIntentProgramPlanFromAdmitted` | Nested-intent C / LLVM emission owners | Emitters read only the sealed plan; no admitted MIR/source/AST read was observed in either emitter. |
| Legacy and composite intent | Family-specific plan | LLVM emission only | Unsupported C frontier, not duplicated target policy. |

## Family plans that bound unsupported frontiers

Observed exact-shape checks are support boundaries, not evidence for a new plan
hierarchy:

- The backend root still selects bounded one-routine CFG shapes, including the
  explicit one-block and three-to-seven-block frontiers.
- The collection plan seals one producer/entrypoint/consumer program and exact
  operation inventories.
- The two-routine nominal/generic projection path classifies a bounded family
  before choosing its family plan.
- The legacy intent plan seals one action-bearing subject/zone topology and is
  LLVM-only.
- The composite intent plan currently admits an exact five-intent, seven-step
  topology and is LLVM-only.
- The nested intent plan seals exact outer/inner policy and expression
  occurrences, but unlike legacy/composite it already has a paired C/LLVM
  projection.

Inference: merging these receipts would weaken fail-closed unsupported-frontier
behavior. Similar fields such as names, priorities, or ABI rows are not enough
to define a universal `ProgramPlan`.

## Single consolidation candidate

### Candidate owner identity

**Proposal, not READY:** keep the existing
`DirectMirScalarCfgRoutinePartitionFact` identity, issued by
`DirectMirScalarCfgProgramRoutinePartitionFromOwners`, and seal one ordered
callee-signature target-indirection fact per parameter there. The issuer already
receives the parameter types/carriages and the logical-record fact. This is an
extension of the existing scalar GraphPlan receipt, not a new protocol species
or universal plan.

### Demonstrated duplicated decision

`DirectMirScalarCfgProgramCSignature` and
`DirectMirScalarCfgProgramLlvmRoutineSignature` independently calculate the
same three target-neutral conditions:

1. parameter carriage is `value-result`;
2. carriage is `readonly-ref` and the type is a logical record;
3. the parameter is a readonly `Array<String>` reference.

Both then choose indirect target passage when any condition is true. C adds a
qualified pointer spelling and LLVM emits `ptr`, but the decision to pass the
parameter indirectly is identical. This meets the directive's two-live-consumer
threshold.

### Consumers to migrate

- `direct_mir_scalar_cfg_program_c_signature_owner.pgy` /
  `DirectMirScalarCfgProgramCSignature`;
- `direct_mir_scalar_cfg_program_llvm_signature_owner.pgy` /
  `DirectMirScalarCfgProgramLlvmRoutineSignature`.

Caller-side address formation, value-result copy-in/out, and readonly binding
remain in their C/LLVM owners. They are not part of this smallest migration.

### Forbidden old read

After migration, neither signature owner may reconstruct target indirection by
combining raw `parameter_carriages`, logical-record type readiness, and
`DirectMirScalarProgramArrayStringReadonlyRefAt`. They must consume the sealed
ordered fact from `plan.routines`. A `new ? old` fallback is forbidden.

### Negative ratchet

Add a structural ratchet to the existing component inventory that requires both
signature owners to consume the sealed parameter-indirection row and forbids
their current local `value_result` / `readonly_record` /
`readonly_array_string` reconstruction. Keep the executable mutation legs in
`tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_owner.sh`:
carriage, pass-shape, resource, ABI-required, composed-record-pass, and
composed-copyout-carriage must fail for both C and LLVM and publish no artifact.
The readonly-`Array<String>` branch remains covered by
`direct_mir_scalar_array_string_readonly_ref_owner.sh` and its five negative
mutations. No new compiler build or CI job is required.

### Executable fixture

Primary falsifier:
`tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_owner.sh`
through the existing
`self-host-direct-mir-scalar-array-int-value-result-test-smoke` target. Its
single fixture contains both a readonly logical-record parameter and
value-result `Array<Int>` parameters, checks exact C and LLVM signatures,
compiles/runs both artifacts, and exercises fail-closed mutations. The existing
readonly-`Array<String>` target is the companion branch check.

### Production-rung decision

**Not the next production rung.** The candidate removes a demonstrated
duplicated decision inside an already reached scalar direct-MIR C/LLVM path,
but active Lease F owns the installed public `pgy --mir` diagnostic
substitution and its production bypass. Implement this candidate only after the
Lease F checkpoint and only if the primary integration decision selects it as
the single follow-up objective. Until then it is an audit proposal, not SoT
closure or substitution progress.
