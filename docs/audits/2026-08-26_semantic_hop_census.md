# Semantic-Hop Census — 2026-08-26

Status: `AUDIT ONLY`; this report is not a semantic owner, readiness claim, or
substitution checkpoint.

## Scope and method

This is Track A from
`docs/agent_work_directives/semantic_hop_parallel_audit_2026-08-26.md`.
The measured base is `9ca4a69517142a4c87eb47862afcd55a9a9f2011`; `HEAD`
still equals that revision. The current worktree contains the uncommitted Lease
F implementation and gates. None of the nested-intent route/plan/emitter files
measured below appears in the current dirty-file list. No build or test was run
for this audit.

The count uses responsibility boundaries, not function count or file count:

- `S` (semantic-decision hop): admits, derives, seals, or selects an identity
  whose meaning a downstream consumer must not reconstruct.
- `T` (transport/navigation hop): forwards an already typed request/fact or
  relays bytes/status without reopening its meaning.
- Several calls inside one responsibility-named owner count as one hop. A
  distinct carried fact responsibility counts separately even if implemented
  in the same file.
- Source compilation is treated as the existing source-MIR production owner,
  not expanded into every lexer/parser/semantic phase. Otherwise this census
  would measure the whole compiler rather than the named executable route.

Observed base/current boundary:

- At the base revision, explicit native selection ended at
  `src/pgy_driver.c:260-262`, while default `--mir` had no installed selector
  and reached the final `driver_run_pipeline` at base line 339. The three Lease
  F owners `self_host_mir_diagnostic_stdout_owner.c`,
  `driver_source_mir_stdout_execution_owner.pgy`, and
  `mir_diagnostic_projection_owner.pgy` did not exist at the base.
- In the current dirty tree, default `--mir` reaches the installed diagnostic
  relay at `src/pgy_driver.c:259-262`; explicit `--native-pipeline` remains
  earlier and separate.

## Route 1 — public installed `pgy --mir SOURCE`

### Observed ordered hops

| # | Kind | Owner and evidence | Input identity -> output identity | Owned responsibility | Executable falsifier |
|---:|:---:|---|---|---|---|
| 1 | S | `src/pgy_driver.c:234-262` | public argv -> admitted `DriverFlags` route | Keeps explicit native opt-out before the default installed `dump_mir` selection. | `tests/self_hosted/parity/public_mir_diagnostic_installed_self_host_owner.sh:202-207` rejects selector reordering; lines 103-111 keep the explicit native bytes distinct. |
| 2 | S | `src/compiler/self_host_mir_diagnostic_stdout_owner.c:19-73` | exact `dump_mir` flag envelope + source path -> canonical source identity + private child argv | Rejects incompatible public flags, resolves the one installed child, canonicalizes source identity, and maps the public mode to `--emit-mir-diagnostic-verified`. This is a process-boundary request admission, not MIR semantics. | Public gate lines 129-162 cover invalid source, missing child, unsupported flags, no native retry; lines 208-212 reject native/string-shell fallback in this owner. |
| 3 | T | `src/compiler/compiler_process.c:225-373` (Windows), `:444-567` (POSIX) | child argv -> bounded captured stdout bytes + child status | Transports bytes/status with output limit, timeout, and process-tree cleanup. It does not interpret MIR bytes. | Public gate lines 164-200 exercise silent success, descendant-held stdout, and child-closed stdout. |
| 4 | S | `src/self_hosted/compiler/driver_bootstrap_main.pgy:7-10`; `driver_rung2_cli_request_owner.pgy:53-59,124-129` | child argv -> `DriverCliSourceMirDiagnosticStdout(normalized_path)` | Sole child argv-to-request meaning assignment and path-shape admission. | Public gate lines 213-214 plus `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` can reject a missing or ambiguous variant. |
| 5 | T | `src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy:9-12,63-64` | typed diagnostic request -> same typed request at read-only executor | Chooses the installed read-capability executor; it does not reinterpret source or MIR identity. | `tests/self_hosted/parity/driver_source_mir_execution_action_gate.sh:135-136` and the public gate line 215 require the admitted read route. |
| 6 | T | `src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy:14-16,44-47` | typed request -> `(source_path, empty machine declaration)` call | Last CLI-mode dispatch and stdout call. The request variant already owns the mode meaning. | Public gate lines 215-217 requires the diagnostic consumer, compiler-world producer, and borrowed admission. |
| 7 | T | `src/self_hosted/compiler/driver_source_mir_stdout_execution_owner.pgy:9-16`; `compiler_world_direct_mir_owner.pgy:56-63` | source identity + `SourceMirVerified` -> compiler-world method call | Reuses the canonical source-MIR payload protocol and one executable world; creates no diagnostic-specific receipt species. | `driver_source_mir_execution_action_gate.sh:122,142-151`; public gate lines 216-219. |
| 8 | T | `src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy:6-23,61-62`; `world.pgy:277-284,310-315` | typed source-MIR request -> `DriverSourceMirExecution.ProduceSourceMir` action | Materializes declared executable zones and forwards the request to the identity-bearing subject/action. | `driver_source_mir_execution_action_gate.sh` is the focused world/action falsifier; the public byte-parity case at lines 67-90 reaches it through the installed child. |
| 9 | S | `src/self_hosted/compiler/driver_source_mir_execution_owner.pgy:9-30,33-98,101-117` | source identity + machine declaration + verified request -> `DriverSourceMirPayloadAdmission` | Seals subject/topology/request policy, compiles source once through the existing AST/semantic/MIR owners, serializes one canonical MIR payload, and returns the existing typed receipt/rejection. | `driver_source_mir_execution_action_gate.sh`; public gate lines 67-90 and 129-152 distinguish success from source rejection. |
| 10 | S | `src/self_hosted/compiler/driver_source_mir_stdout_execution_owner.pgy:17-35,47-54`; `driver_source_mir_protocol_owner.pgy:126-149` | payload admission -> receipt bound to source/request -> canonical payload | Last protocol admission. Schema/request/source/nonempty seals are checked before the payload is consumed. | `driver_source_mir_execution_action_gate.sh:147-151`; public gate lines 149-152 reject a failed producer with no stdout payload. |
| 11 | S | `src/self_hosted/mir_lower/mir_json_input_owner.pgy:27-94` | borrowed canonical JSON + same declaration -> `MirMachineLayerAdmittedJsonInput` | Performs the one consumer-side document/schema, parallel-capture, declaration/topology, machine-layer, and intent admission without a file reread. | Public gate lines 113-127 mutate the schema and require no diagnostic projection; lines 217,220-221 statically retain the borrowed-text seam. |
| 12 | S | `src/self_hosted/mir_lower/mir_diagnostic_projection_owner.pgy:35-70,94-184` | admitted document/routine/block/instruction facts -> `pgy.mir.diagnostic.v1` text | Owns the stable human view. It consumes admitted indexes and routine-local scalar bundles, labels rows as projection coordinates, and omits unowned native facts. | Public gate lines 67-101 check simple and CFG/local output; lines 86-88 reject guessed native facts; lines 222-224 reject source/file/native re-entry. |
| 13 | T | `driver_rung2_cli_read_execution_owner.pgy:44-47`; `self_host_mir_diagnostic_stdout_owner.c:74-121` | diagnostic String -> child stdout -> captured public stdout | `Log` and the native relay preserve child bytes and status. Native checks only capture protocol/empty success; it does not parse or own MIR semantics. | Public gate lines 67-90 require byte equality between direct installed child and public relay; lines 149-200 require no partial payload on pre-relay failures. |

Measured total: **7 semantic-decision hops** and **6
transport/navigation hops**.

### Observed work and repetition

- One compiler-scale source production occurs at hop 9. It produces the
  canonical serialized owner artifact once.
- One compiler-scale consumer admission occurs at hop 11. It is not the same
  decision as production: it establishes the trust/lifetime boundary required
  before any human rendering. Removing it would restore unchecked JSON as a
  second owner.
- Hop 12 builds one `MirRoutineInstructionFactBundle` per routine
  (`mir_diagnostic_projection_owner.pgy:58-70`). This reads exact admitted
  instruction spans for render-local scalar fields; it does not rescan source,
  AST, or the document root.
- Native flag-envelope validation, child argv admission, payload-receipt
  admission, and parent empty-success rejection look similar only at the text
  level. They seal different process/protocol lifetimes. The census found no
  duplicated compiler-semantic decision among them.

### Inference

The six transport hops make this route physically long, especially the
installed-executor/read-executor and compiler-world forwarding layers. They
separate public/native selection, child process authority, typed CLI admission,
world/action authority, payload protocol, MIR admission, and stdout relay.
Collapsing files or bypassing a forwarding method would reduce navigation but
would not remove a repeated semantic decision. Route 1 therefore has **no
sound consolidation candidate by itself** under the Track A objective.

## Route 2 — nested priority/observability source/direct C and LLVM

Fixture family:
`tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy`.
The measured core begins at its exclusive nested route claim, as required by
the directive. Origin-specific wrappers immediately before/after the core are
listed because the source-C wrapper contains the only observed duplicate.

### Observed origin-specific entry/exit

| Origin | Kind | Owner and evidence | Responsibility |
|---|:---:|---|---|
| Source/MIR-to-C | S (duplicate) | `src/self_hosted/compiler/driver_rung2_nested_intent_c_substitution_owner.pgy:9-22` | Before calling the canonical route owner, re-decides the family envelope as exactly four routines and two declarations (`:14-17`). |
| Source/MIR-to-C | T | `driver_rung2_owner.pgy:248-257`; substitution owner `:19-37` | Calls the common target-pair projection before MIR-to-AST, then attaches the already admitted topology plan and packages the C artifact. |
| Direct C/LLVM | T/navigation outside the measured core | `direct_mir_backend_projection_owner.pgy:81-102`; `direct_mir_multi_routine_projection_owner.pgy:22-40,63-68` | Carries the admitted document and target fact through the multi-routine dispatcher to the common nested projection. The dispatcher also evaluates earlier mutually exclusive family claims; those are not counted in the claim-to-emission core. |

### Observed common claim-to-emission hops

| # | Kind | Owner and evidence | Input identity -> output identity | Owned responsibility | Executable falsifier |
|---:|:---:|---|---|---|---|
| 1 | S | `direct_mir_nested_intent_program_route_fact_owner.pgy:11-20,61-138` | admitted declaration/routine inventories -> sealed route fact | Sole exclusive claim: one function, one method, two intents, one subject, one zone, with distinct rows and a digest. It scans admitted inventory arrays, not source/AST/JSON text. | `tests/self_hosted/parity/direct_mir_nested_intent_program_c_owner.sh:116-124` requires route before plan/emission and before scalar fallback. |
| 2 | S | `direct_mir_nested_intent_program_graph_fact_owner.pgy:14-69,454-538` | admitted spans + claimed route -> sealed graph fact | Seals callable/declaration syntax identity, field/zone identity, call edges, construction/method values, and log literals. | C/LLVM gate negatives `missing-inner-priority`, `priority-graph-drift`, `duplicate-source-identity`, `method-owner-crosswire`, and `action-name-crosswire` (`direct_mir_nested_intent_program_llvm_owner.sh:90-157`). |
| 3 | S | `direct_mir_nested_intent_program_plan_owner.pgy:13-51,99-199,559-598` | graph row identities -> four sealed routine header facts | Captures exact function/method/intent kind, owner, name, syntax ID, receiver, return, and parameter facts; cross-seals the four rows against the graph. Four bounded routine spans are consumed under one header-fact responsibility. | The same C/LLVM identity negatives plus the plan-order assertion at C gate lines 116-120. |
| 4 | S | `direct_mir_nested_intent_program_plan_owner.pgy:203-237,337-460,599-609` | admitted intent spans + graph/header facts -> outer/inner policy facts | Owns concurrent/exclusive mode, literal/parameter priority carrier, participant bindings, nested argument, expected result, and outer/inner policy relation. | `missing-inner-priority` and `priority-graph-drift` must fail at their owned diagnostic with no source/direct C or LLVM artifact (`C gate:64-100`; `LLVM gate:137-157`). |
| 5 | S | `direct_mir_nested_intent_program_plan_owner.pgy:466-530,559-628` | route + graph + headers + policies + target-capability identity -> sealed `DirectMirNestedIntentProgramPlan` | Cross-seals one backend-neutral plan schema, revision, digest, target capability, and all row relations. | C gate lines 116-120 asserts route -> plan -> both emitters; both emitter entrypoints reject an invalid plan. |
| 6 | S | `direct_mir_nested_intent_program_projection_owner.pgy:6-24` | sealed plan + `CompilerTargetProjectionFact` -> selected C or LLVM consumer | Exact target-pair boundary. A claimed route cannot fall through; an unknown target dies closed. | C gate lines 115-124 rejects LLVM-only gating and scalar re-entry; success requires source/direct C byte equality at lines 17-35. |
| 7C | S | `direct_mir_nested_intent_program_c_emission_owner.pgy:247-323` | sealed plan + runtime ABI/symbol owners -> C text | MIR-blind C-only materialization, ABI validation, C identifier projection, runtime header/zone sync, method/intents/Main. | C gate lines 17-62 requires byte-identical source/direct C, warning-clean compile, and exact nine-line execution. |
| 7L | S | `direct_mir_nested_intent_program_llvm_emission_owner.pgy:266-312` | same sealed plan + runtime ABI/symbol owners -> LLVM text | MIR-blind LLVM-only materialization from the same plan. | LLVM gate lines 26-88 requires exact LLVM anchors plus public/native nine-line runtime parity. |

Common measured total for either target: **7 semantic-decision hops**. The
core has no fact-free transport hop: each crossed responsibility derives,
checks, seals, selects, or materializes an owned fact. Source/MIR-to-C adds
**one duplicated semantic pre-claim** and two wrapper/navigation visits; direct
C/LLVM adds dispatcher navigation outside the stated claim-to-emission core.

### Observed work and repetition

- The common projection calls `DirectMirNestedIntentProgramRouteFactFromAdmitted`
  once and `DirectMirNestedIntentProgramPlanFromAdmitted` once
  (`direct_mir_nested_intent_program_projection_owner.pgy:10-14`). Both C and
  LLVM consume that same sealed plan and do not read admitted MIR, source, AST,
  or JSON.
- Route claim performs one complete pass over the admitted declaration and
  routine inventory arrays (`route_fact_owner.pgy:64-117`). Graph, header, and
  policy owners then consume exact rows/spans for different facts; no second
  whole-document parse was observed.
- The source-C wrapper's 4/2 check is a subset of the route owner's exact 4/2
  envelope. It therefore makes the same family-eligibility decision twice in
  one source-C execution.
- Both target emitters independently acquire `CompilerRuntimeCallAbiFormattedPrintFact`
  and the same four `IntentObservabilityAbiRowForSource` rows
  (`C emitter:253-260`; `LLVM emitter:272-278`). They also independently derive
  the shared subject/zone/method/outer/inner/sync symbols (`C:279-297`;
  `LLVM:279-292`). This is cross-consumer duplication, but only one target
  emitter runs per request. Moving it into the sealed plan would change plan
  payload/digest and is larger than the candidate below.

### Inference

The route/graph/header/policy/plan boundaries are not owner proliferation by
file aesthetics: they seal different semantic dimensions and collapse to one
backend-neutral plan before either target. The C and LLVM paths are already a
one-plan path. The only same-execution dual decision found in the measured
family is the source-C wrapper's count pre-claim.

## Smallest consolidation candidate

### Proposal (not implemented, not `READY`)

Remove the routine/declaration count decision from
`DriverRung2NestedIntentCSubstitutionIfClaimed` and let
`DirectMirNestedIntentProgramRouteFactFromAdmitted` remain the sole family
claim owner. To preserve the current cheap rejection for unrelated source-C
programs, the existing route owner may perform its own 4-routine/2-declaration
early check before scanning inventory rows and return its normal sealed
unclaimed fact. Owner identity, route schema/digest, plan identity, and all
consumers stay unchanged.

Expected measured effect:

- source/MIR-to-C semantic-decision hops from pre-claim through emission:
  **8 -> 7**;
- direct C and LLVM: unchanged;
- whole-program parsing: unchanged; unrelated source-C retains an O(1) reject
  inside the actual route owner rather than in an orchestration wrapper.

Consumers to retain/migrate:

- `driver_rung2_nested_intent_c_substitution_owner.pgy` continues calling
  `CompileAdmittedDirectMirNestedIntentProgramForTargetIfClaimed` and retains
  topology attachment/artifact responsibility only.
- `direct_mir_multi_routine_projection_owner.pgy` continues using the same
  common target-pair projection.
- C and LLVM emitters remain unchanged sealed-plan consumers.

Forbidden old read / negative ratchet:

- Add a structural negative to
  `tests/self_hosted/parity/direct_mir_nested_intent_program_c_owner.sh` that
  rejects `MirProgramRoutineIndexCount` and
  `MirProgramDeclarationIndexCount` in
  `driver_rung2_nested_intent_c_substitution_owner.pgy`.
- Retain the positive requirement that the common projection calls
  `DirectMirNestedIntentProgramRouteFactFromAdmitted` before plan and both
  emitters (`C gate:116-120`).

Executable falsifier:

- Reuse `tests/self_hosted/parity/direct_mir_nested_intent_program_llvm_owner.sh`,
  which sources `direct_mir_nested_intent_program_c_owner.sh`. Its exact
  success case must still produce the same nine-line C/LLVM result and
  byte-identical source/direct C, while all five owned mutations must publish
  no source/direct C or LLVM artifact and must not fall through to
  `scalar-program-route` (`LLVM:26-88,137-160`; `C:17-100`).

This candidate removes one demonstrated duplicated decision without merging
files, introducing a cache/query layer, changing the sealed plan, or treating
the audit report as authority. It is not a recommendation to open work before
Lease F is checkpointed.
