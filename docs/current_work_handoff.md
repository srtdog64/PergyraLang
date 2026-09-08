# Current Work Handoff

Updated: 2026-09-09 (Asia/Seoul), verified CI repair packet; navigation only.
Compiler owners, registries and executable gates override this snapshot.

## Active self-host context — CI integration of source-admission snapshot

Pre-publication HEAD/origin/main: `e8fa15a24b73318466033df793462ac3e57ce6b9`.
The user authorized commit/push to inspect CI and reopened failed run
`34251704201`. The latest published run `34266995720` is RED, not green.
Windows/macOS, TSAN and Rocq passed. Linux/driver/codegen failures share the
nominal-array declaration error and downstream absence of the self-host driver.
Logs: `.tmp/ci-e8fa-failed.log`, `.tmp/ci-e8fa-codegen-job.log`.

This snapshot accompanies the repair; Git owns its publication identity. Primary-only
edit lease: [collaboration](current_work_collaboration.md);
[objective and owner boundaries](agent_work_directives/source_admission_parity_2026-09-07.md).
No shared compiler installation, skipped jobs, raised caps or native-built
replacement driver. CI integration blocks new Intent/GraphPlan implementation.

The reached seam is semantic nominal-array identity -> early C descriptor ->
completed element layout -> runtime bodies. One usage fact feeds declaration
and runtime consumers. The existing ABI owner remains authoritative; direct
by-value cycles still refuse. Native C uses the same declaration/body boundary.
Driver routing consumes a local cursor instead of mutating an immutable receipt.
MIR array-member writes retain the lexical root LocalRef and the carried
projection graph. Bootstrap tool parity now preserves nonzero status and fresh
diagnostic stdout instead of reusing a success-only output file.
Two reached admission gaps are sealed: generic template return ABI agrees with
its unspecialized header before substitution, and discarded call statements
carry exact ABI absence. Role override consumes wire kind `class` together
with nominal kind `subject`; crossed kinds refuse, not a value-type fallback.

Observed local evidence:
- Native semantic 2930/0 from the previous packet; current array repair:
  C transpile 978/0, memory layout 82/0. Latest MIR run: 194/0
  (`.tmp/ci-final-mir-test.log`).
- Fresh codegen bootstrap gen2 == gen3, 83,240 emitted-C lines
  (`.tmp/ci-final-seed-fixpoint.log`). Argument graph: 2 executions and
  12 refusals; nominal-array/MIR-root gate: 4 executions and 8 refusals.
  Subsequent whitespace and admission edits are not a new seed receipt claim.
- Earlier isolated Pergyra-built driver passed grammar examples (17), callable
  vocabulary (18), callable parameter identity C/LLVM, match binding, release
  output hygiene and the full Zone-sync gate including its real codegen build.
  Logs: `.tmp/ci-array-*.log`, `.tmp/ci-zone-sync-mir-root.log`.
- Header/backend size gates, shared test/production owner-size gate and UTF-8
  gate passed (`.tmp/ci-final-{header-size,backend-size,test-size,utf8}.log`).
  Final structural component gate passed: 2,412 cap requests, 1,017 function
  extractions, 690 reuses (`.tmp/ci-final-component-publication.log`).
- Latest actual Pergyra-built driver construction and source/C/manifest smoke
  passed (`.tmp/ci-final-driver-role-kind.log`). It was copied beside the
  isolated native launcher, not into shared `bin`, for installed-route checks.
  All six push replacement-frontier gates passed with that driver
  (`.tmp/ci-final-frontier-*.log`). Role override includes 8 C/LLVM runtime
  legs, 3 receiver runtimes, 14 source refusals and 16 MIR refusals.
- Final inferred-generic C/LLVM gate passed: exact 41, 14 metamorphics,
  2 value variants, concrete generic return control, 46 C refusals and
  3 LLVM sentinels. Case-math and logical-record C/LLVM gates passed too
  (`.tmp/ci-publication-*.log`). Both native LLVM push guards passed
  (`.tmp/ci-final-llvm-{option-context,intent-abi}.log`).
- Makefile source inventory passed (`.tmp/ci-final-source-inventory.log`).
  The initial Git Bash attempt lacked `make`; the MSYS2 rerun used the
  repository directory explicitly. This was not a compiler-gate skip.

Harness corrections preserve real claims: GraphPlan symbols replace removed
shape-specific names; nine provenance/display mutations now require byte-equal
C/LLVM. Struct member/arithmetic semantic changes execute as 0 and 5, while
missing-member/wrong-type mutations keep 15 actual refusals. Native comparison
erases only the exact synthetic effect-free Void exit (13 non-erasable controls).
The inferred-generic gate now honors `PGY_SELF_DRIVER_BIN`, avoiding the stale
shared driver accidentally selected during one final rerun.

Current native: `.tmp/ci-34251704201-native/pgy.exe`.
Verified bootstrap producer: `.tmp/ci-34251704201-seed-final/gen2.exe`.
Final driver: `.tmp/ci-34251704201-driver-final/pgy-self-driver.exe`;
SHA-256: `C5C53FE4530761A05F4B82E3F8B7E4612DE7C5950D191DB0D960F0134185352B`.
The sibling `.tmp/ci-34251704201-native/pgy-self-driver.exe` has the same hash.

Next falsifier: the first failing job on this packet's push CI. The complete
Linux/sanitizer/platform matrix was not rerun locally on this Windows host;
remote success must be observed on the new commit. Local gates do not prove it.
Physical owner moves preserve bodies and identities, delete former definitions
and migrate imports/source pins; they are not self-host substitution progress.
Existing unreachable-statement bootstrap and LLVM target-triple warnings remain
visible. This repair does not close the parked source-admission feature gaps.

### Parked source-admission evidence — lookup, not a parallel work queue

Pre-publication HEAD/local origin/main: `5b97f2e10ffa7ecf9cfe932829a83ffffaa3ba12`.
On 2026-09-09 the user explicitly requested commit/push now to inspect CI.
This supersedes the earlier publication hold, not the remaining language,
bootstrap or integration obligations. This snapshot accompanies the pending
source changes; use Git for its eventual commit identity. No shared install.
Read the [edit lease](current_work_collaboration.md) and
[objective card](agent_work_directives/source_admission_parity_2026-09-07.md).
Primary only; no new syntax, research lane or parallel implementation lane.

One active rung: driver source-LLVM purpose -> admitted MIR ->
existing Intent/GraphPlan owners -> ordinary Main composition. Do not replace
this with another independent SoT cleanup queue. The active contract is
[self-host Intent execution](self_hosted/19_intent_execution_transition_contract.md).

### Newly verified reached boundary

`mir_lower/intent_routine_plan_owner.pgy` now admits the existing routine
mode, priority, binding/phase inventory and cleanup contracts independently of
AST tree reconstruction. The tree consumes that plan. Step admission remains
in `intent_routine_step_plan_owner.pgy`; the binding owner resolves aliases
and placement reuses the action's already selected receiver row.

Each placed step now carries the Zone/participant declaration rows and exact
slot SyntaxNodeId. `semantic/intent_subject_slot_policy_owner.pgy` owns the
existing exact-compatible-alias / unique-compatible-slot rule. The source-C
environment adapter and MIR declaration-index consumer share that rule; a C
field absent from the declared field inventory is no longer silently selected.
Names choose a slot only at this semantic owner. A direct target consumer must
join its carried identity to cell layout, not choose the slot again.

Both producer MIR inputs execute the exact-name control with two same-type
slots and the differently named unique-type control: `true, 1, 50, 1`.
Native C/LLVM and public C independently produce those observations too.
Public LLVM still refuses both, as it did the existing nine controls. The
expanded four-source-leg gate is 49 passed / 11 failed; the former nine-case
gate on this same candidate was 43 / 9. The extra two failures are additional
coverage of the open execution route, not a green reclassification.

MIR-to-C completion/placement/nested regression has 88 passing checks. Typed
transition/compensation has 64 observations and 19 no-artifact refusals.
The native-compiled binding contract separately checks owner-scoped selection,
ambiguity, incomplete inventory and unlisted fields. The wrong-slot-kind MIR
is refused by the earlier nominal declaration owner, before placement.

The prior nested `on`/`intent` repair, exact callable SyntaxNodeId cross-seal,
indexed typed topology and retirement of legacy executable mirrors remain
gated. None of these admissions establish general Intent GraphPlan execution
or new hard self-host substitution. Typed mirror graph verification still
uses `MirIntentExecutionCoverLegacyGraphMirrors` with expression-order
coverage; a future typed direct consumer must not silently skip it.

### Next falsifying case — still open

The shared target transfer kernel is now implemented in
`compiler/direct_mir_intent_cell_placement_owner.pgy` and
`compiler/direct_mir_intent_cell_transfer_projection_owner.pgy`. It joins the
admitted declaration/slot IDs to cell layout and copies Subject state into the
existing slot storage or back to the participant. It never replaces the slot
pointer. `tests/self_hosted/parity/intent_cell_transfer.py` passed 42 checks:
12 C/LLVM executions across both producer inputs and 30 pre-emission refusals.
The three valid layouts cover the sole slot, exact second same-type slot and
unique differently named slot. Slot storage, participant storage and other
slots remain independent. Evidence: `.tmp/self_hosted/intent_cell_transfer/run.wz4rdfy8/`;
native-compiled validator SHA-256
`A85420347780BD9BBF2DC5F4924259595F8349A996DC9BB20A609D4C00125315`.
This kernel is not connected to production GraphPlan yet. It neither performs
nor proves synchronization, phase execution, compensation or general Intent
substitution. Its new source files are not in the older candidate's source
checksum manifest. The unchanged final driver only produced its valid MIR
inputs. The probe used explicit C emission and the existing O0 test profile
after the default native compile exceeded 60 seconds; that failed candidate
was not executed and all handles are terminal.

The reached common-path blocker is lowering the admitted step plan into actual
Zone placement, phase execution and cleanup in ordinary GraphPlan. The new
step plan is not that runtime/CFG protocol. The exact next materializer must
join `placement_binding.slot_source_syntax_id` to the existing
`logical_record.identity_cells` field identity, copy Subject state without
aliasing the Zone slot to the original participant, and preserve receiver
binding, synchronization, phase branches and cleanup. The common issuer is
`direct_mir_scalar_cfg_program_graph_admission_owner.pgy`; its ordinary
`DirectMirIdentityCellLifetimeReady` still forbids mutable calls through a
slot without that boundary plan. Do not relax this guard in isolation.
The existing
`tests/concept_semantics/intent_predicates/header_success_observed.pgy` is the
four-routine falsifier; do not remove the `signature.kind == "intent"` execution
guard until that owned plan is actually consumed. Parameter classification is
not sufficient, and a new role number or an ordinary pointer argument alone
would not supply the missing protocol.

The end-to-end regression also remains the existing
`tests/self_hosted/parity/fixture/direct_mir_legacy_intent_program_llvm.pgy`.
Its success condition is a field comparison, not literal true. The old bounded
emitter ignored it; its newly strict plan refuses with
`direct MIR legacy intent phase plan is invalid`. The formerly green legacy
LLVM gate is RED on the current pair: `intent-placement-final-legacy.log`,
scratch `self_hosted/direct_mir_legacy_intent_program_llvm/run.M29rGq/`.
Do not remove the completion guard, change the fixture to true, or count this
valid-source refusal as closure. Carry executable completion into the owned
Intent/GraphPlan path; do not add another fixed Main envelope alternative.

`tests/concept_semantics/word_deletion/cases/28_intent_header_policies/orig.pgy`
previously refused on public LLVM with
`direct MIR legacy intent Main instruction identity is invalid`.
That observation is preserved in
`.tmp/generic_execution_20260907/intent-call-spine-full-llvm.log`.
The preceding four-source-leg matrix refused every public LLVM Main shape.
Last consumers are `direct_mir_legacy_intent_program_plan_owner.pgy` and
`direct_mir_legacy_intent_program_graph_fact_owner.pgy`; the old emitter also
assumes inline Zone/Subject layout rather than the common identity-cell owner.
Do not splice that ABI into ordinary Main, silently omit instructions or retry
a failed claim through native/MIR-to-AST.

Both producers' observed-completion Main still stop at
`owner=callable-signature stage=signature-family routine=3 name=Complete`;
neither publishes an artifact: `intent-placement-final-{native,self}-direct.log`.
The nested direct-LLVM gate also stops at callable-signature for `InnerPriority`
(routine 2): `intent-placement-final-nested.log`, scratch
`self_hosted/direct_mir_nested_intent_program_llvm/run.q1VlEv/`. Earlier and new
drivers refused the same direct route. The now-green nested MIR-to-C execution
is not direct GraphPlan execution. Its direct LLVM/C and negative gate legs
remain unexecuted after that first failure. The gate now preserves each run
directory and reports both diagnostic streams instead of deleting old evidence
or hiding stdout errors. Do not restore fixed Main envelopes to green it.

The richer typed control previously refused at direct GraphPlan routine-instance
signature sealing on both producers; this direct-route observation is older:
`intent-call-spine-{native,self}-direct.log`; no C artifact. This is a distinct
boundary from the now-green explicit MIR-to-C projection. No universal signature
or target-execution claim follows from the binder/signature probes.

### Candidate and observed evidence

Directory: `.tmp/generic_execution_20260907/`.

| Artifact | SHA-256 |
| --- | --- |
| `pgy-intent-phase-single.exe` | `288ED56D8B331BC5342FDE1FAA0119A68257A4C44A69BBB88A898130E9227F57` |
| `driver.intent-placement-final.exe` | `1EFD357C8A5CD30F1CDD8250648D50CCFD03A91521ECDD4DA59D4ED09EE579D4` |
| `driver.intent-placement-final.c` | `69FE593FB59D78499E9CEFEFACFA0E9DC4F60FE1FC47753C0186AED2168A776D` |
| Machine companion | `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19` |

Native C emission, source freeze/recheck, isolated GCC and machine replay/cmp
pass: `intent-placement-final-driver-build.log`, `intent-placement-final-driver-inputs.sha256`
and `intent-placement-final-source-inputs.sha256`. The isolated native build is
unchanged from `intent-phase-single-native-build.log`. Emission has 0 errors /
4 existing warnings (three redundant Intent clauses and one unlocated unreachable
statement). C is 42,023,840 bytes. Final source/artifact checksum rechecks pass;
the frozen source list and all four candidate artifacts were rechecked on
2026-09-09 at the final handoff boundary.

| Observed gate on the final pair | Result | Evidence relative to .tmp/ |
| --- | --- | --- |
| Completion / step plan / nested MIR-to-C | 88 checks PASS; named/unique placement, nested observations, repeated calls and no-artifact refusals | self_hosted/intent_completion/run.la_tgmic/ |
| Typed role/compensation MIR-to-C | 64 observations + 19 no-artifact refusals PASS | self_hosted/intent_block_roles/run.ns2pfhkb/ |
| Phase carriage | Phase order and admitted MIR negatives PASS | generic_execution_20260907/intent-placement-final-phase.log |
| Source admission | 50/0 | self_hosted/concept_semantics_20260905/source_admission/run.dRqAo6/ |
| Four-source-leg predicates | 49 passed / 11 failed; all eleven public LLVM valid controls refuse | concept_semantics/intent_predicates/run.s8esme6i/ |
| Observed-completion common direct C route | Both producers REFUSED at callable-signature, no artifact | generic_execution_20260907/intent-placement-final-{native,self}-direct.log |
| Legacy / nested direct LLVM | RED: nonconstant completion / Intent body execution unsupported | generic_execution_20260907/intent-placement-final-{legacy,nested}.log |
| Intent compression source contract | PASS; retired mirrors, tree-owned admission, C-owned slot selection and repeated binding lookup absent | generic_execution_20260907/intent-placement-final-static.log |

Only valid controls and validator programs execute. Altered source/MIR is
admission/refusal-only; the plan validator never emits its altered owner facts.
These gates are not full CI, bootstrap or installed-driver proof. The final
native-compiled plan validator SHA is
`BDBA1ABC398235CF096B0834839DFD4F4417D202A8F0C6B225C6FD4B5DBB975E`.
It has one existing unlocated unreachable-statement warning and uses both
`run.la_tgmic/*-repeated.mir.json` controls.

The separate native binding-contract executable is
`intent-placement-binding-checked-probe.exe`, SHA
`E848E9232B6C4922D8D696B0F90F058DEA1A2A30B0A0636CFBC279D84D96FF73`.
Its build passed and execution prints `intent step binding contract: PASS`
with empty stderr; matching `*-build.log`, `*.out` and `*.err` record it.
The earlier probe had untyped empty-array constructor arguments; typed local
arrays repaired the fixture without weakening the checker.

The first placement build failed because `eval_text` was removed despite two
remaining uses. It is restored; the later TextBuilder errors were cascading
and do not recur on the final build. Do not reopen those as a runtime regression.

Native MIR/HIR units are the prior 191/0 and 25/0 observations
(`intent-phase-single-{mir,hir}-test.log`); native compiler code/binary did not
change in this slice. No new unit run is claimed. The earlier binder/participant
probe was not recompiled for this pair; its evidence remains
`intent-phase-owner-probe-{native,self}.log`.

The routine-index fixture had stale missing module provenance and declared method
identity. It proceeded from an invalid setup into an array bounds panic.
The repaired controls carry those facts and stop on invalid positive setup
before mutation. The earlier native C probe passed; it was not rerun on this
pair. Its full public C/LLVM smoke remains blocked by the size pin below.

### Retained completed prerequisites — not an active work queue

- Intent formal declaration IDs survive both producers and matching mirrors.
  The binding projection requires positive distinct IDs and exact purpose/order.
  Common callable signatures retain Intent kind, participant/value carriage and
  indirect/direct ABI, never Main-local IDs or invented ordinals.
- Public C executes pre/invariant phases after placement binding. Common source
  admission rejects non-Bool/duplicate predicates and unavailable current-step
  outcomes; native invariant checking was corrected before outcome scope.
  Explicit full rollback remains; current/none and typed predicate-failure
  execution are not complete.
- Ordinary GraphPlan constructs frame-owned Zone/Subject cells, preserves
  declaration kind/type/ID and explicit empty authority facts, initializes
  generation storage and supports readonly-ref borrowing/reborrow.
  Native signature/RIR owners preserve BorrowedRead and remove only invented
  owned cleanup. Explicit invalidation and Intent obligations remain.
- Passive class/object value records use the existing logical-record GraphPlan;
  mutable subject/vessel identity stays separate. Named field IDs select layout;
  initializer and eager operand effects retain source order. Four obsolete
  passive literal owners and the String-only control-flow route were deleted
  earlier and remain absence-gated.
- Local/SSA declaration identity, scalar inout copy-in/copy-out, finite loop CFG,
  named-enum match and unsafe effect/scope behavior have focused execution
  evidence. Missing facts fail before publication; no invalid execution or
  source-renaming workaround substitutes for those facts.
- Retry is rejected by common admission; timeout/backoff retain parser refusal.
  Their stable language-word rows have support mask 0. Action authority/binding,
  required role impl and Future lexical lifecycle checks remain in source gates.
- Checked Int division uses the existing runtime ABI at normalization and C/LLVM
  materialization; raw dynamic sdiv/literal-only guard are deleted together.
  Runtime manifest has 267 rows, preserving the previous 266.

### Earlier evidence — rerun at integration, not current-pair claims

The preceding 2BC23CA8/19922BCA participant-policy pair passed source admission
50/0 (`self_hosted/concept_semantics_20260905/source_admission/run.eHZqwT/`),
receiver/Zone 192/0 (`concept_semantics/identity_cell_receiver/run.vtjtacic/`) and
generic execution/constraints/cell lifetime 120/0
(`self_hosted/generic-instantiation.MRrUCq/`). These broader gates were not rerun
for the current Intent phase-occurrence slice and are not current-pair claims.

The prior 253898BE/83E504A7 binder/callable pair is superseded. Earlier focused
results remain attributable to their original artifacts/logs; no aliases or
retroactive green labels:

| Earlier gate | Observed result | Evidence relative to .tmp/ |
| --- | --- | --- |
| Independent binary/Intent order | 10/2; public LLVM refuses both Intent Main shapes | concept_semantics/binary_evaluation_order/run.heagdgq9/ |
| Intent pre/invariant | 27/5; 15 native C/LLVM/public C observations + 12 source negatives pass, five public LLVM legs refuse | concept_semantics/intent_predicates/run.2exzs9u_/ |
| Loop execution | 136/0 on 668AF768/A8974C89; three absent native mutations omitted | concept_semantics/loop_statement/run.rh0vxyj6/ |
| Unsafe execution / scalar inout identity | 146/0 and 38/0 on 668AF768/A8974C89 | concept_semantics/unsafe_block/run.zhml5ief/; concept_semantics/scalar_value_result/run.3d3cfxrc/ |
| Unsafe common owner / Future admission | 36/0 and 74/0; no invalid input execution | concept_semantics/unsafe_block/run.f3w_24n0/; concept_semantics/future_lifecycle/run.5u0q14fg/ |
| Action / enum / resilience admission | 52/0, 66/0, 26/0 | concept_semantics/action_authority/run.crbxos4d/; concept_semantics/named_enum_match/run.hDZEjF/; concept_semantics/resilience/run.goe002fj/ |
| Checked Int divide shared targets | PASS, valid execution + ABI/type refusal | self_hosted/direct_mir_scalar_int_divide/run.ViyUyr/ |
| Native semantic / C units | 2930/0 and 978/0 | generic_execution_20260907/intent-invariant-semantic-unit.log; generic_execution_20260907/binary-order-transpile-hygiene-unit.log |
| Native RIR / HIR units | 26/0 and 25/0 | generic_execution_20260907/zone-borrowed-native-rir-units.log; generic_execution_20260907/binding-world-arg-hir-unit.log |
| SoT authority edges | 89 authorities / 186 derived; CLOSED55 / BRIDGE32 / ACTIVE2 PASS at that checkpoint | generic_execution_20260907/unsafe-sot.log |

Earlier passive/mutable nominal gates, generic occurrence/instance/role
constraints, scalar/Array effects, typed Intent v3 publication/cross-seal and
word-registry gates also require integration reruns. The inferred-generic fixed
matrix failed on native/self Main instruction-count drift before target
projection (`intent-callable-owned-inferred-gate.log`).

### Remaining reported obligations

The source-only audit of 104 deletion fixtures on 60444203/7CCF05AD found six
native-positive public refusals and zero operational failures, with all 20
native refusals also refused publicly:
`generic_execution_20260907/intent-phase-owned-deletion-audit/report.json`.
This is not the historical 34/104 compile/runtime metric or runtime equality.

Queue after the active Intent rung, not parallel implementation tracks:

- Public positives for parallel joins, event/party/Slot/object projections.
- Future aggregate stored-type containment and Zone spawn callee ABI.
  Phantom generic arguments are not automatically stored Future handles.
- Ability-only generic field witnesses per instance; do not infer nominal
  equality from one call tuple or relabel valid controls as negatives.
- Nested callable ABI, public Now and broader field/record execution; native
  Option<subject> ABI and Option<Array<String>> C type spelling.
- World LLVM lifecycle compiles but crashes; do not rerun the unchanged crash.
  The world C control passes. LLVM compilation of the imported admission probe
  previously failed on ArrayPush(record.field); native C probe evidence is not
  public/LLVM compiler-source closure.
- Absolute Windows MIR paths currently produce silent unreadable/empty failures.
  Repository-relative arguments isolate language tests; CLI portability remains.
- Runtime progress/quiescence, Slot protected access and evidence-erasure
  measurements remain separate obligations, not proved by current compiler gates.

The user's boundary-closure proposal is reconciled in the existing
[review intake](audits/2026-09-06_architecture_review_5b97_reconciliation.md).
The plain collection-parameter mutation document now states the owned refusal
policy. The linked full proposal was not locally attached and all 28 claimed
acceptance cases were not supplied. No new syntax/IR layer or proof claim was
introduced. Module Build remains after self-host closure.

### Integration, workspace and publication

998 porcelain entries, zero staged before publication preparation; inherited work preserved.
Commit/push is now user-requested. No manual dispatch or shared installation.
The earlier no-publication condition is no longer active. This step-plan/phase work
deleted no material files. Earlier retired owners remain recoverable from Git.
D: free was 7.63 GiB at the earlier 01:06 observation. Publication preparation
reviewed the changed path/retirement inventory, build registrations and generated
owners; full semantic integration review is still owed, not implied by staging.
Full diff whitespace validation passes (six existing CRLF-to-LF warnings).
Changed shell syntax and this snapshot's five relative links pass. The Intent
compression contract was rerun successfully at this final handoff boundary.
Before the user-requested snapshot commit, the fixed builtin-effect registry,
language-word registry, SoT authority edges and Intent compression gates passed.
The language-word occurrence inventory initially drifted after fixture changes;
its owner generator refreshed it and the complete 146-row gate then passed.
No support flag, size limit or failed valid-source expectation was weakened.

New/moved owners are within their recorded caps: routine plan 215/240,
step plan 325/330, placement binding 54/100 and slot policy 50/70; the existing
binding-contract fixture is 173/180. No limit was raised.
Current selected exceeded caps: phase projection291/260 and expression-carrier
contract62/50. Routine-index owner remains 552/530. Its earlier selected smoke
is RED; these limits were not raised. A proposed projection-field ArrayPush
cleanup was withdrawn: current member-array inout gates prohibit it, and empty
array literals lack constructor-context type inference. Keep typed local
construction arrays; do not remove those guards to meet a line count.
The smoke exits 1 at that exact pin:
`generic_execution_20260907/intent-role-routine-index-smoke.log`.
Declaration index120/120, block170/180, routine facts600/600 and machine
facts431/440 retain their earlier in-cap inventory observations. The method
epoch owner is now 405 lines. No full component/performance gate was rerun.

Other observed size failures remain: stmt_emit816/800, assignment611/600,
expression_environment608/599, concrete_scalar_verdict641/599,
signature_fact613/599, statement_fact602/600 and diagnostic712/660.
`test_mir_lowering_part_b_2.cases.h` is 739/699 and the size script's first
failure still hides later sections. Component/perf gates previously timed out
at 60 seconds; find the repeated owned operation, do not extend the allowance.
String parity also stops at stale caps and retired two-Int paths.

Current full CI/platform/parity, bootstrap gen2/gen3, installed-driver and Rocq
checks remain unrun. The earlier published-HEAD push CI was green; Platform full
and weekly parity were red. The user-requested main push will trigger CI; no
new green result is claimed by this pre-publication snapshot.
Budgets remain static60 s / focused300 s / integration1,800 s.

## Historical lookup — never an active queue

Earlier long records remain in [audits/archive](audits/archive/). Evidence tables
above replace successive active narratives; do not append another execution log.
