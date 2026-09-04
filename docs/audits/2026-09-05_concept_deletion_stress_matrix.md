# Pergyra concept deletion stress matrix — 2026-09-05

Status: `READ-ONLY DESIGN AUDIT — NO SEMANTIC AUTHORITY`

Observed base revision: `01f280a3566fb581d0f65d7783e678eee6c987d9`.

This report applies the deletion half of
`docs/agent_work_directives/language_concept_stress_audit_2026-09-05.md`.
It does not change language semantics, a registry row, self-host progress, or
an implementation priority. Current source, owner documents, and executable
gates remain authoritative.

## Method and evidence labels

Deleting a spelling and deleting a semantic coordinate are different tests.
For every candidate this audit asks both questions:

1. If the surface spelling disappears but its fact is derived elsewhere, what
   is lost?
2. If the underlying fact and owner disappear, can the remaining Pergyra
   concepts distinguish the same good and bad programs without recreating the
   deleted concept or introducing a second authority?

A substitution fails when it must add the deleted concept's hidden state,
identity, graph, or admission rule under another name. That is a rename, not a
deletion. It also fails when two neighbors both start owning the same verdict.

Evidence is labelled as follows:

- **Observed**: present in source, an owner document, a fixture, or an observed
  gate run.
- **Inference**: the deletion verdict derived from those observations.
- **Proposal**: a future surface or admission policy; it is not current
  language truth.

The verdict vocabulary is:

- `KEEP-CORE`: removing the semantic coordinate loses an independently
  observable guarantee.
- `KEEP-FACT / NO NEW KEYWORD`: the compiler needs the coordinate, but a
  standalone spelling is not justified.
- `CONDITIONAL`: valid only when a stated bundle threshold is met; simpler uses
  should lower to existing concepts.

## Result

| Concept | Distinguishing fact lost on deletion | Can current neighbors substitute? | Verdict |
| --- | --- | --- | --- |
| `Slot` | Dynamic handle freshness, occupancy/release, token and pin state, plus the stable identity used by static escape/conflict checks | No. Plain values, `Scope`, and `Capability` can each model one projection, but not the same runtime identity across release/reuse and boundary checks | `KEEP-CORE` |
| `Zone` | Named residence/resource boundary and the exact identity joining `within`/`where`, `using`, authority slots, transfer, and frontier state | No. A struct has shape, a scope has lexical lifetime, and a capability has permission; their product still needs one zone identity and crossing rule | `KEEP-CORE` |
| `Capability` | Fine-grained, interprocedurally inferred `used` authority and the host-imposed grant that remains bounded across execution lanes | No. `Effect` is deliberately coarse, `Ability` says what can be implemented, `Authority` says who may exercise a grant, and `Zone` says where | `KEEP-CORE` |
| `Scope` | Containment/retirement boundary for tasks and leases | The semantic fact cannot be removed. The hypothetical `scope` keyword can: there is no canonical `scope` language-word row today, and existing lexical constructs instantiate the fact | `KEEP-FACT / NO NEW KEYWORD` |
| `Intent` | For qualifying workflows, one purpose identity cross-seals participant, coordination, boundary, authority/effect, compensation, and typed terminal facts | Sometimes. `func` + `action` + `Result` substitutes for simple sequencing; it does not substitute for an admitted multi-step spine without recreating a workflow owner | `CONDITIONAL` |

The concise deletion result is therefore:

```text
Slot       -> keep the temporal/resource handle
Zone       -> keep the named execution/authority/resource boundary
Capability -> keep the fine-grained grant/effect bound
Scope      -> keep the semantic boundary, do not add a generic keyword merely
              to name it
Intent     -> keep only as a cross-axis workflow binder; demote ornamental
              one-step/trace-only uses to action or func
```

This does **not** reintroduce a fixed universal lifecycle. `Slot` owns dynamic
temporal validity, `Scope` owns lexical containment, and `Zone` owns a resource
residence/boundary. Those three lifetimes may coincide in a program, but no
single global lifecycle is inferred from that coincidence.

## `Slot`: deletion loses temporal resource identity

### Observed owner facts

- `docs/semantics/08_slot_capability_calculus.md:22-49` defines `Slot` as the
  stable language resource boundary above a replaceable backend handle.
- The same contract deliberately limits the claim: runtime freshness/token/pin
  facts belong to Slot, while general static borrow safety requires the
  ownership/CFG and task/channel layers
  (`docs/semantics/08_slot_capability_calculus.md:51-99`).
- `src/runtime/slot_manager.c:57-76` joins a handle by slot id **and
  generation**; `src/runtime/slot_manager.c:108-125` refuses release while the
  entry is pinned.
- `src/semantic/slot_analyzer.c:384-439` classifies per-task Slot access and
  rejects write/write and read/write overlap in `parallel`.
- `src/semantic/type_checker_slot_view_boundary.c:28-65` rejects a live pinned
  view crossing an owned boundary, and
  `src/semantic/type_checker_slot_view_active.c:106-145` prevents the owner
  handle from escaping around that lease.

### Deletion attacks across distinct workloads

| Combination | Counterexample after deleting Slot identity/state | Why a neighbor cannot decide it |
| --- | --- | --- |
| `Slot + Release + Read` | Two executions have the same `Int` value, scope, and capabilities; one handle is occupied and one was released. `Read` must accept only the first. The durable negative is `tests/cases/slot_contract/reject/released_slot_read/main.pgy:1-6`. | A plain `Int` has no generation or tombstone. Adding them to a wrapper recreates Slot. Lexical scope cannot decide runtime reuse. |
| `SecureSlot + Token + Write` | Same slot and same zone, but an issued token and a foreign/stale token must produce different verdicts. `tests/slot_contract_smoke.sh:45-60` covers secure success, wrong-token write, and released secure read. | `Capability` can say `write` is permitted in general; it does not identify the token issued for this handle generation. |
| `Slot + pin + ReadView/WriteView + loop exits` | A view must be cleaned on normal exit, return, branch, `break`, and `continue`, while release/move or owner access conflicting with the live view is rejected. The positive control exercises loop `break`/`continue` in `tests/cases/slot_contract/positive/pin_read_write_cleanup/main.pgy:1-27`; the owner contract lists the explicit cleanup edges at `docs/semantics/08_slot_capability_calculus.md:115-151`. | `Scope` supplies the lexical interval but not which resource generation is pinned or whether the lease is read/write. `Capability` supplies mode but not cleanup identity. |
| `DeviceSlot + RemoteFuture + Result` | A device-backed read may complete after the dynamic resource was released and must become the owned failure rather than a valid value. The C/LLVM contract matrix includes `device_slot_async_read`, `device_released_read`, and `device_double_release` (`tests/slot_contract_smoke.sh:45-60`). | Future completion owns join and `Result` owns failure shape; neither owns device-handle freshness. |
| `Slot + parallel` | Same task structure and same declared permissions, but sibling read/read is admissible while read/write and write/write are not (`docs/113_memory_concurrency_model.md:70-82`). | `parallel` knows concurrency but needs Slot's stable resource identity to know that two accesses alias. |

**Observed run (2026-09-05):** `tests/slot_contract_smoke.sh` passed on C and
LLVM with four positive families and seven rejection families.

**Inference:** `Slot` passes the deletion test strongly. Removing only the word
while keeping a generational, token-bearing, pinnable resource handle would be
a rename. Removing the fact would make released/reused and live handles
indistinguishable. The verdict does not license the larger claim “Slot is a
complete borrow checker”; the owner contract explicitly rejects that claim.

## `Zone`: deletion loses named residence and crossing identity

### Observed owner facts

- The language orthogonality contract defines a zone as the immediate
  execution, authority, and resource boundary
  (`docs/42_keyword_orthogonality.md:103-120`, `198-202`).
- `src/self_hosted/semantic/ast_zone_authority_fact_owner.pgy:1-5` refuses to
  infer zone authority from a subject's type and binds the explicit authority
  row to one exact subject-slot identity.
- `src/self_hosted/semantic/ast_zone_value_carriage_verdict_owner.pgy:86-218`
  rejects unplanned copies and reassignment of resource-owning zone values;
  `ast_zone_parameter_boundary_verdict_owner.pgy:39-115` requires an admitted
  parameter boundary.
- The model-level boundary proof distinguishes residence from permission:
  crossing requires the destination's capability, crossing never grants new
  authority, and missing evidence fails closed
  (`docs/semantics/proofs/ZoneCrossingCore.v:59-68`, `73-117`).

### Deletion attacks across distinct workloads

| Combination | Counterexample after deleting Zone identity | Why a neighbor cannot decide it |
| --- | --- | --- |
| `Zone + subject slot + copy/ref` | `let copied: CounterZone = first` must not silently duplicate the lock/resource owner, while `func Observe(ref value: CounterZone)` is admitted. The negative and positive are `tests/self_hosted/fixtures/domain_runtime_zone_copy_threadsafe_rejected.pgy:1-12` and `domain_runtime_zone_parameter_ref.pgy:1-20`. | A struct type sees equal fields in both cases. `Scope` sees both values alive. Only a resource-boundary identity plus a carriage rule separates copy from borrow/transfer. |
| `Zone + Ability + Authority + Action + Intent` | `CockpitZone` binds authority to its `driver` slot; an intent step must keep `using`, actor, approval, postcondition, and projection in that exact zone (`tests/cases/backend_compare/intent_zone_binding/main.pgy:1-35`). The gate's negatives include where/using drift, undeclared authority slot, and ambiguous/missing subject slots (`tests/self_hosted/parity/intent_step_binding_contract_owner.sh:27-47`). | Ability states what the driver can do and authority states who approves. Neither names the residence in which the action and projection are valid. Encoding residence into every capability produces capability-per-zone products and still needs the zone topology owner. |
| `World + embedded Zone + effect/state + frontier` | Two worlds can carry equal visible values but different dirty/ready epochs and embedded-zone provenance. Bounded recompute must converge or fail with a distinct overflow class (`docs/semantics/01_intent_world_zone.md:77-97`; `tests/runtime_frontier_contract_smoke.sh:413-442`). | A lexical scope cannot represent a persistent world/zone frontier. Slot generations identify resources, not the dependency graph between zone state and world projections. |
| `Zone + spawn + own/ref` | A zone sent to outstanding work must preserve its ABI/ownership boundary. The current adversarial audit records an **open implementation hole**: native C emission can publish an invalid artifact for `own/ref CounterZone` spawn crossings (`docs/audits/2026-09-05_counterexample_attack_results.md:157-176`). | This is not evidence that Zone is removable; it is evidence that the Zone fact is not yet carried through every execution boundary. Treating ordinary task capture as a substitute loses the exact fact that the missing gate needs. |

**Observed run (2026-09-05):**
`tests/self_hosted/parity/domain_runtime_zone_sync_execution_owner.sh` passed,
including copy/reassignment rejection, default-parameter rejection, explicit
`ref` execution in single/thread-safe modes, and world-zone carriage.

**Inference:** `Zone` passes the deletion test. `struct + Scope + Capability`
can reproduce its fields, lexical duration, and entry permission separately,
but cannot join them to one residence/crossing identity without constructing a
Zone-equivalent owner. The open spawn-carriage counterexample prevents a claim
of implementation completeness.

## `Capability`: deletion loses an inspectable, transitive grant bound

### Observed owner facts

- The sandbox contract separates declaration, inferred manifest, runtime gate,
  and static proof (`docs/semantics/15_capability_sandbox.md:12-31`).
- Native semantic analysis checks transitive `used` against declared caps at
  `src/semantic/type_checker_func_decl.c:361-387`.
- The self-host owner computes the call-graph fixed point and fails closed on
  non-convergence or missing declarations
  (`src/self_hosted/semantic/ast_capability_fact_owner.pgy:377-460`).
- `src/runtime/pgy_runtime_context.h:36-38,146` keeps the capability masks in a
  bound context and snapshots them into a task context rather than consulting
  an executor-thread default.
- Capability is intentionally not an alias for coarse effect: the two masks
  classify different questions (`docs/semantics/15_capability_sandbox.md:60-79`,
  `110-114`).

### Deletion attacks across distinct workloads

| Combination | Counterexample after deleting Capability | Why a neighbor cannot decide it |
| --- | --- | --- |
| `func with caps + call graph + clock` | `entry` declares `io_read`, calls a helper, and only the helper calls `Now()`. The compiler must still reject `entry` for missing `clock` (`tests/capability/manifest_interproc.pgy:1-11`; `tests/capability/run_manifest.sh:2-14,53-70`). | A lexical scope sees no local clock call. An `Ability` bound on `entry` does not compute transitive ambient effects. A coarse nondeterministic effect cannot distinguish clock from random. |
| `file handle + host grant + no partial artifact` | The same compiled write operation is allowed under `io_write` and denied under `io_read`; denial must occur before the success marker and leave no file artifact (`tests/capability/run_runtime_enforce.sh:128-148`). | Zone says where the file operation occurs, not whether the host granted write. Authority may name an approver, but it does not replace the host's fine-grained operation mask. |
| `spawn/coroutine + capability context` | A child running on inline, blocking, worker, movable, C-extern, or LLVM lanes must inherit the parent's exact mask and restore the surrounding mask; executor TLS must not widen it (`docs/113_memory_concurrency_model.md:38-68`; `tests/runtime_spawn_context_propagation_smoke.sh:61-71,131-166`). | Scope contains the child but has no permission lattice. Zone residence can stay unchanged while the parent grant differs. |
| `Zone crossing + Capability + Authority` | Same origin/destination zone and actor, but a held and missing entry capability must yield different crossing judgments (`docs/semantics/proofs/ZoneCrossingCore.v:94-117`). | Folding the bit into Zone makes every host grant a different zone. Folding it into Authority loses the distinction between a capability set and the grant/delegation history; `AuthorityIrreducibility.v:72-97` separately proves that authority is not reducible to capability × zone. |

**Observed runs (2026-09-05):**

- `tests/capability/run_manifest.sh` passed after explicitly selecting
  `PGY_BIN=bin/pgy.exe`: two clean programs and eight under-declaration
  families, including the interprocedural clock case.
- `tests/runtime_spawn_context_propagation_smoke.sh` passed its inline,
  C-extern, and LLVM-runtime lane checks.
- The first Windows/Git-Bash manifest invocation used the script default
  `bin/pgy` and returned launch status 126. That launch failure is not counted
  as a semantic failure; the explicit `.exe` rerun is the observed result.

**Inference:** `Capability` passes the deletion test strongly. Replacing it
with finer `Effect` rows plus a host grant lattice, task-context carriage, and
runtime operation gates would recreate Capability under the Effect name and
would also erase the repository's deliberate coarse/fine distinction.

## `Scope`: delete the hypothetical word, not the containment fact

### Observed owner facts

- The canonical language-word registry has rows for `async`, `await`,
  `intent`, `parallel`, `pin`, `slot`, `spawn`, `unsafe`, `within`, `world`,
  and `zone`, but no `"scope"` row
  (`src/lexer/language_keyword_registry.def:63,78,343,473,483,608,613,698,738-748`).
- Named Future lifecycle is owned by
  `src/semantic/type_checker_future_lifecycle.c:173-227`; live handles are
  rejected at lexical or function exit, while `await`/explicit `own` transfer
  retires the obligation.
- `parallel` owns a join-before-continuation boundary
  (`docs/113_memory_concurrency_model.md:70-82`), and pin/view owners use lexical
  scope to delimit a resource lease.
- `AsyncScopeCore.v` makes the semantic need precise: a running task whose
  owning scope is closed is an orphan (`docs/semantics/proofs/AsyncScopeCore.v:81-100`),
  and the unstructured close-without-join rule reaches one
  (`docs/semantics/proofs/AsyncScopeCore.v:357-409`). That file is a bounded
  model, not evidence that a general user-visible scope tree is implemented.

### Deletion attacks across distinct workloads

| Combination | Counterexample after deleting the Scope fact | Why a neighbor cannot decide it |
| --- | --- | --- |
| `spawn + Future + nested if block` | A child Future created in the inner block and not awaited must fail when the block exits (`tests/cases/structured_spawn_lifecycle/negative_nested_scope.pgy:1-8`). The same code with `await` is valid. | `Future` identifies the obligation but needs an owner boundary at which liveness becomes an error. Zone may outlive the function and cannot be that boundary. |
| `parallel + Slot access + continuation` | Accepted sibling work must join before the following read, and overlapping Slot writes must be rejected. | Slot identifies the resource but not when sibling execution ends. Capability grants access but not happens-before. `parallel` therefore instantiates a scope/join fact. |
| `pin + WriteView + return/break/continue` | Cleanup must happen on every exit from the pin block and the view cannot survive the block. | Slot and view mode identify what is leased; only the lexical scope says how long. Turning every Slot into a globally pinned value would change semantics and cost. |
| `Cancel + Future + scope exit` | `Cancel` requests cancellation but does not retire or free the handle; an await or own transfer is still required before scope exit (`docs/113_memory_concurrency_model.md:232-275,277-282`). | Cancellation state and completion ownership are distinct. Treating cancel as scope closure would admit a still-running/orphaned task. |

**Observed run (2026-09-05):**
`tests/structured_spawn_lifecycle_smoke.sh` passed: its matrix includes nineteen
positive forms plus scope-exit, branch, loop, alias, transfer, and repeated-use
negatives (`tests/structured_spawn_lifecycle_smoke.sh:42-63,166-181,212-258`).

**Inference:** the semantic coordinate passes deletion; a generic surface
keyword does not. Current constructs already expose the meaningful boundary:
function/block exit for affine Future retirement, `parallel {}` for joined
work, and `pin {}` for lease lifetime. Adding `scope simulation {}` without a
new guarantee would duplicate ordinary block structure. A future explicit
scope spelling would need an independently observable policy such as a named
cancellation subtree or an authorized detach boundary before admission.

## `Intent`: keep the binder only where deletion loses the workflow proof

### Observed owner facts

- Intent is explicitly not a universal owner. It binds provenance while
  participant, zone, ability/capability, authority, effect, and resource facts
  retain their own owners (`docs/42_keyword_orthogonality.md:122-144`).
- The canonical formulation is a source binder elaborating to separate
  coordination, authority, effect, boundary, compensation, and trace facts
  (`docs/173_intent_axis_strengthening.md:50-100`). Purpose and trace by
  themselves are classified as library-expressible.
- `IntentSpine.v` forbids a hidden monolith: exact family facts sharing one
  spine identity determine exactly one intent, and an intent is nothing beyond
  them (`docs/semantics/proofs/IntentSpine.v:6-35`; the composition theorems are
  at lines 274-316).
- The current self-host execution owner carries exact step/predecessor,
  outcome-variant, compensation, terminal, and zone identities in one plan
  (`src/self_hosted/mir/intent_execution_fact_owner.pgy:1-9,29-63,162-270`).
- `selfhost.intent_declaration_rows` is recorded `CLOSED` with exact identity,
  typed outcome, phase, compensation, execution, and old-path ratchets
  (`docs/semantics/sot_owner_spine_registry.md:115`). Closure of that compiler
  fact family is implementation evidence, not proof that every source-level
  intent use is conceptually justified.

### Workload and negative-control matrix

| Combination | What deletion loses | Deletion verdict |
| --- | --- | --- |
| `Intent + two actions + typed enum outcomes + predecessor + compensation` | `RunWorkflow` binds step B to A's exact successful payload, maps each step failure to a typed terminal variant, and compensates only completed predecessors (`tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy:124-148`). The parity gate also attacks crossed identity and reverse compensation (`tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh:6-7,153-260`). | A normal function can hand-code this runtime behavior, but current concepts do not statically cross-seal all those rows under one identity. A replacement `Saga`/`Workflow` compiler owner would be an Intent rename. **Qualifies.** |
| `Intent + multi-phase guards/postconditions + ordered rollback` | The guard/expect/post fixture distinguishes failure before a step, failure after action evaluation, postcondition failure, and reverse ordered compensation (`tests/self_hosted/parity/fixture/intent_guard_post_compensation_execution.pgy:86-112`; gate summary at `intent_guard_post_compensation_execution_owner.sh:205-217`). | `action` owns each behavior contract, but not the cross-step completion log and compensation order. **Qualifies**, though it is the same workflow family as the typed-outcome case rather than a fully unrelated domain. |
| `Intent + nested exclusive/concurrent + priority + observability` | The nested fixture carries outer/inner mode and priority into runtime identity (`tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy:21-51`). | This is useful standardization, but mode/priority/trace can be recreated by a runtime workflow registry. The proof pack itself keeps trace in the library bucket. **Does not independently qualify.** |
| `Intent + Zone + Authority + one Action` | `SyncDrive` combines one action with using/authority/post/expect (`tests/cases/backend_compare/intent_zone_binding/main.pgy:21-35`). | Zone and action already own most static facts. With one step, no dependency or compensation proof is exercised. Unless the purpose identity is consumed by another admitted contract, this is a **borderline negative control**, not proof of Intent necessity. |
| `Intent + compiler World + one source-to-LLVM action` | `CompilePergyraProgram` is reached by the production compiler root and its direct host bypass is ratcheted (`src/self_hosted/compiler/world.pgy:578-600`; `tests/self_hosted/parity/compiler_root_intent_takeover_gate.sh:26-31,117-126`). | This proves real dogfood and substitution, not deletion irreducibility. It currently has one step and Bool terminals; a direct purpose-named action/function could express its local control flow. **Real workload, but concept-justification evidence remains partial.** |
| One-step readiness intents in the compiler world | `IntakeSource`, `LexSource`, `ParseTokens`, `CheckProgramSemantics`, `LowerProgramFacts`, `EmitProgramArtifact`, `PlanTargetProjection`, and `ProveSelfHostedParity` mostly wrap one action with `expect: true` and Bool terminals (`src/self_hosted/compiler/world.pgy:388-486,564-576`). | If no consumer uses an intent-specific compensation, typed terminal, cross-step dependency, conflict, or trace obligation, deleting the wrapper loses no static guarantee. These are **audit candidates for lowering**, not established defects; reachability and consumer inspection are required before any edit. |

**Inference:** `Intent` does not earn the unconditional `KEEP-CORE` verdict that
Slot, Zone, and Capability earn. The repository demonstrates one strong
transactional workflow family, plus real but mostly one-step compiler and
authority/projection uses. That is not yet three materially unrelated cases in
which the full binder is necessary. The honest verdict is `CONDITIONAL`:

```text
Use intent only when one purpose identity must own at least one genuinely
cross-step obligation (dependency, typed terminal routing, compensation,
cross-intent conflict, or purpose-bound trace) while binding the relevant
participant/boundary/authority/effect facts.

Otherwise use func or action.
```

One action is neither automatic rejection nor automatic acceptance; the test
is whether removing the binder loses a compiler-checked bundle. Purpose naming
or runtime trace alone does not pass that test. Before promoting Intent from
`CONDITIONAL`, collect three unrelated real workloads that each fail an exact
negative gate when the shared spine identity, rather than an individual action
fact, is removed or crossed.

## Cross-concept substitution attacks

The following combinations show why collapsing the strong concepts would
create dual authority rather than simplification:

| Proposed collapse | Distinguishing pair it cannot preserve | Result |
| --- | --- | --- |
| `Slot -> Scope` | same lexical scope, fresh vs stale generation | Recreates generation/state on Scope and conflates lexical and dynamic lifetime |
| `Slot -> Capability` | same read capability, issued token for generation A vs stale token for generation B | Recreates handle identity inside the capability grant |
| `Zone -> Scope` | same open lexical scope, valid residence vs unauthorized destination zone | Persistent domain residence is not block lifetime |
| `Zone -> Capability` | same destination, actor has/does not have entry capability | A boundary and permission to cross it are independent inputs |
| `Capability -> Ability` | same implementation contract, host grants/denies `io_write` | Ability satisfaction is not ambient-operation authority |
| `Capability -> Authority` | same capability mask and zone, different delegation history | `AuthorityIrreducibility.v:72-97` gives the model-level separating pair |
| `Scope -> Zone` | same zone, inner Future owner open vs exited | A task can outlive a lexical block without changing zone |
| `Intent -> Zone` | same zone and actions, different predecessor/terminal/compensation graph | Zone owns residence/resources, not workflow order |
| `Intent -> function` | same calls and values, crossed admitted step/terminal identities | A plain function has control flow but no current owner for the cross-axis spine; adding one is a workflow/Intent equivalent |

## Falsifiers and follow-up proposals

These findings should be overturned, not defended, if future evidence supplies
the following falsifiers:

- **Slot falsifier:** an encoding using only existing Pergyra value, scope, and
  capability facts rejects stale-generation, wrong-token, pin escape, and
  parallel alias cases without storing a Slot-equivalent identity/state.
- **Zone falsifier:** existing struct/scope/capability owners can express exact
  residence, authority-slot binding, transfer, and frontier provenance without
  a shared zone identity.
- **Capability falsifier:** existing effect/ability/authority facts can compute
  the same interprocedural used set and preserve an independent host grant
  across every execution lane without adding a capability-equivalent mask.
- **Scope falsifier:** Future retirement, parallel join, and pin/view cleanup can
  all be decided with no lexical/parent boundary fact. Conversely, a new
  `scope` keyword becomes justified only if it adds a named observable policy
  not already owned by those constructs.
- **Intent promotion falsifier:** three unrelated real workloads each require a
  shared intent-spine identity for a compiler rejection that action/function/
  zone/capability owners cannot make separately. Intent deletion is justified
  for any individual use whose exact negative disappears with no loss of a
  checked bundle.

## Verification record

Observed in this audit session:

- `tests/slot_contract_smoke.sh`: PASS, C and LLVM, four positive and seven
  rejection families.
- `PGY_BIN=bin/pgy.exe tests/capability/run_manifest.sh`: PASS, two clean and
  eight under-declaration families.
- `tests/structured_spawn_lifecycle_smoke.sh`: PASS.
- `tests/runtime_spawn_context_propagation_smoke.sh`: PASS on inline, C-extern,
  and LLVM-runtime lanes.
- `tests/self_hosted/parity/domain_runtime_zone_sync_execution_owner.sh`: PASS,
  including zone carriage and parameter admission sub-gates.
- `tests/self_hosted/parity/intent_step_binding_contract_owner.sh`: **inconclusive
  in the shared dirty worktree**. It stopped before its tested assertions with
  `ast_artifact_invalid`, owner `nominal_constructor_argument_type`, constructor
  `SemanticAstZoneAuthorityFacts`, argument index 9. This run is not used as a
  pass or as a semantic counterexample; the durable gate source is cited only
  as an existing negative matrix.

No full CI matrix, formal proof suite, or expensive self-host rebuild was run.
