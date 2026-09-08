# Architecture review reconciliation at 5b97f2e1

Updated 2026-09-08. REVIEW INTAKE only: no semantic authority, progress counter
or parallel implementation queue. Observed HEAD/main/origin/main:
`5b97f2e10ffa7ecf9cfe932829a83ffffaa3ba12`.

Inputs are the user's preserved 2026-09-06 reviews:
`35448fb3-f5ac-4c0b-8197-c45023811ab9/pasted-text.txt` (SHA-256
`5F73783A55208B2C695825F124850E1813DBA65B8F854BF88FEAA63964F523F5`) and
`20893f63-97c3-401b-be5d-85ac3eeb3f5b/pasted-text.txt` (SHA-256
`FAC4C177B5DC4CFFBFCF03ECB0542E79C658B1194D3FD7597B86BFF42D9FC251`).
The 2026-09-08 follow-up is preserved in user attachment
`45f04918-7197-4311-ba4f-c0aeba0f10f2/pasted-text.txt`, SHA-256
`E402D91AFE5238D598BAA28C6322E3B0EF816A4EAB20484D9300F1C3358F444D`.
The later boundary-closure v0.1 proposal is attachment
`44a3e242-5d59-42fb-98be-b826e03eb6bb/pasted-text.txt`, SHA-256
`75F792141EB251DBD516B346C2D23E88CA96B632C0F3553541512C5989DCFCDE`.
Research in the earlier attachments was not independently verified at intake.
The bounded primary-source checks below do not authorize a new architecture.

## Exact-HEAD CI evidence

GitHub run and job logs were read directly on 2026-09-07. These runs test the
published HEAD, not the later uncommitted local source.

On 2026-09-08 the GitHub API again reports main at this SHA and exactly these
three completed runs for it. Job metadata confirms the same failed platform
jobs and weekly release-pair step. There is no newly green scheduled run.
Linux core job duration is 43m50s; macOS C-only is 41m18s. These are job wall
times, not compiler-stage CPU, retained-byte or hot-loop lock measurements.

| Workflow | Observed result and reached failure |
| --- | --- |
| [Push CI](https://github.com/srtdog64/PergyraLang/actions/runs/33998401657) | SUCCESS. Not full integration evidence. |
| [Platform full](https://github.com/srtdog64/PergyraLang/actions/runs/34048425312) | Linux/Windows core and parser/semantic/codegen shards pass. macOS C-only and Linux/Windows driver shards fail. |
| Same Platform full: common failed assertion | `driver_rung2_set_index_value_parity_owner.sh`: `c Set index diagnostic drifted`. All three logs show `array_index_type_mismatch`, expected Int / actual String. macOS records this as its sole failed step, 31 of 65. |
| [Weekly Self-host parity](https://github.com/srtdog64/PergyraLang/actions/runs/34056549349) | Fails at `callable-route-envelope`, stage `return-type`, routine 588 `SemanticAstEntrypointSelectionAccumulatorEmpty`, nominal accumulator result. This is a distinct integration failure. |

The Set fixture is `SetAdd(distinct, words["bad"])`: the invalid index belongs
to Array<String>, not to Set. The graph scalar-verdict owner already issues a
typed index diagnostic, while the gate expected the older structural
`ast_artifact_invalid / collection_value_type` result. The local gate is
corrected to require exit 1, the owned code and exact expected/actual types,
and to reject that structural substitute. The filtered existing harness owns
verification; its current outcome is in the [active snapshot](../current_work_handoff.md).
A local first-failure fix does not mean remote CI is now green.

The weekly return-type boundary has separate later local physical-record work,
but no exact-current scheduled rerun closes it. Historical local improvements
and old fixed-point results cannot be promoted into current full integration.

## Accepted direction and qualifications

| Review direction | Disposition |
| --- | --- |
| One language before self-host percentage | Adopt. Compare canonical source admission, diagnostics, carried facts and valid execution. Native is a comparator, not an infallible oracle: current controls also find native bugs. |
| 34/104 divergence | Historical installed-binary observation. The collector is not an expectation gate and has not remeasured all current dirty sources. Do not freeze incorrect native or installed outcomes as expected semantics. |
| Stable identity and conservative small analyses | Retain. Derive from existing owners, preserve identity, reject missing facts and delete reconstructed reads. Current capability/Intent work follows this boundary. |
| ResourceCarriage | After the active parity rung, map concrete resource/transfer/escape obligations to existing owners. The current same-name Int failure needs binding identity, not a resource calculus. Counterexamples justify repair, not automatically a new global owner or surface syntax. |
| Class mutation | Keep value semantics with the existing explicit `inout self` direction; verify copy-in/out and normal-exit obligations. Becoming identity-like is not the only alternative. |
| Evaluation order | Retain language-owned sequencing. Array/record and reached eager-binary obligations have separate executable controls; arbitrary call-argument and assignment-place ordering still need their own evidence. |
| Async, parallel and Slot | Follow docs/204's canon/rung distinctions. The nine-axis map already exists in [advanced_examples](../concurrency/advanced_examples.md), including explicit OPEN cells. Complete those contracts rather than create another map. No new scope keyword, AIR lowering or lane annotations. Resume revalidation is not implied by negative suspension tests. |
| Cancellation and scheduling | Cancelled must not mean retired: cancellation request leaves the Future join/transfer obligation. Proven overlap safety also does not justify run-to-completion serialization of communicating arms; retain progress/happens-before contracts. The existing lifecycle owner and ping-pong example own these distinctions. |
| Intent necessity | Keep the [purpose attribution binder](../01_intent_first_design.md), not an action-count rule. One action may have a typed outcome/terminal obligation; many actions without such a purpose-bound contract do not justify Intent automatically. |
| External MIR | Preserve strict grammar, identity and publication-boundary obligations. Reported acceptance is not proof of a sandbox escape; no invalid generated artifact is executed in this intake. |
| Erasure / synchronization / OperationSchema | Keep as bounded proposals until the active executable path reaches a measured missing fact or repeated operation. CI wall time is not a compiler-stage or live-byte measurement. |
| Module Build / query database | Deferred. No new primary implementation lane follows from the research table. |

## 2026-09-08 evidence qualifications

The review's same-source invariant is accepted with independent expected
semantics, not majority agreement or native output as ground truth. The current
nested-unsafe fixture requires `2,3,1`; at intake native C and LLVM both produced
`1,2,2`, while the updated public paths preserved the distinct declarations.
That native binding defect has since been repaired; exact regression evidence
is in the handoff. Thus two backends agreeing does not prove correctness. Compare admission, owned
diagnostic identity and execution separately; refusal for the wrong reason is
not exact diagnostic parity. The historical 34/104 full-outcome metric and the
newer admission-only corpus are different measurements. Current binary hashes,
counts and remaining failures live only in the [handoff](../current_work_handoff.md).

Bounded primary-source checks on 2026-09-08:

- The async paper's nine dimensions and their start/end/cancellation grouping
  are present in [the authors' paper](https://arxiv.org/html/2608.20677v1#S3).
  The review's nine questions are a useful checklist, not its exact axis labels.
  Pergyra's existing map names Eagerness, Suspension, Extent, Reference Strength,
  Destruction, Propagation, Awareness, Direction and Persistence.
- [Classifying Capabilities](https://arxiv.org/abs/2607.24504) describes classifier
  tags, Future/thread-local restrictions, a Scala implementation and Lean 4
  mechanization. This checks the authors' abstract, not their proof artifact or
  an adequacy mapping to Pergyra. ExecutionLane/authority/suspension owners do
  not become a new source capability vocabulary by analogy.
- The [CGO abstract](https://2026.cgo.org/details/cgo-2026-papers/27/The-Parallel-Semantics-Program-Dependence-Graph-for-Parallel-Optimization)
  states 15% average/46.6% maximum over eight NAS benchmarks on 56 cores relative
  to the original parallel plan. Those are reported study results, not reproduced
  Pergyra performance or a game-workload prediction.
- The [WASI 0.3 release page](https://wasi.dev/releases/wasi-p3) confirms Component
  Model async/future/stream support. The linked [roadmap](https://wasi.dev/roadmap)
  did not expose the review's threading-candidate assertion in the inspected
  text; keep that specific claim unverified rather than infer its status.
  WASI support does not prove Pergyra's external-MIR boundary is a sandbox.

Revocable capabilities, Free to Move, grouped lifetime research, benchmark
speedups and general resource-carriage adequacy were not revalidated here.
No proof port, runtime-lock redesign, hostile-artifact execution or new syntax
was started from the research table.

## Boundary-closure v0.1 disposition

This is a design proposal, not an implementation or proof report. Its linked
`sandbox:/mnt/data/Pergyra_Semantic_Boundary_Closure_v0.1_2026-09-08.md` was not
attached locally; the pasted body mentions 28 acceptance cases without supplying
their complete list. No claim is made that the linked file or all 28 cases were
reviewed or executed. No new external-source verification was performed for this
later attachment.

| Proposed boundary | Disposition and existing landing point |
| --- | --- |
| Plain collection parameter mutation | Confirmed documentation contradiction. Corrected [inout contract](../mut_borrow_parameters.md) to the existing native/public refusal policy; historical copy loss is not current permission. |
| Future aggregate containment | Retain recursive stored-type shape and conservative unresolved cases as obligations. Phantom generic arguments are not automatically stored handles. This is not a new general ResourceCarriage implementation or evidence that every aggregate is covered. |
| Zone spawn admission | Require one call identity to join parameter mode, source place, capture/lifetime and ABI obligations. Unsupported combinations must refuse before publication; no claim of repaired runtime layout from a source-only refusal. |
| Value receivers | Keep explicit value-result `inout self`. A normal `Result.Err` return is not panic or implicit rollback. Receiver overlap and exceptional-exit obligations need their own evidence; do not widen temporary/nested places by analogy. |
| Operand order | The reached repair and independent expected observations land in [abstraction loss contracts](../semantics/09_abstraction_loss_contracts.md#observation-order-across-expression-lowering) and `binary_evaluation_order.py`. Existing ordered children suffice; no SequencePlan. |
| External MIR | Retain strict parsing, local identity/type/ABI validation, target legalization and publication obligations. Foreign receipt/admitted fields cannot authorize trust; unsupported facts cannot be reconstructed after erasure. No permission bypass or execution of malformed artifacts. |
| Progress, Slot access, wrap and teardown | Keep check-plus-access protection, communicating-arm progress and quiescence-before-free as separate runtime obligations. Generation retirement is a proposal requiring an owner/gate, not a proved implementation. Cancel, timeout and sorting job IDs do not respectively prove join, quiescence or deterministic admission. |
| Result envelopes and erasure | Application revision/job IDs remain application facts. Compact receipts are conditional on a legitimate last consumer; they are not new mandatory IR layers. Measure live/retained bytes and invalidate changed facts before claiming erasure adequacy. |

Preserve the existing async/lifetime, ExecutionLane/capability, index-order
reduction and single-step-purpose Intent distinctions. Immediate contract
corrections and the reached Intent ordering falsifier are active; the other
rows are review constraints on their eventual owners, not parallel work queues.

## Active navigation, not another execution log

The [handoff](../current_work_handoff.md) is the single current candidate and
gate snapshot; the [objective card](../agent_work_directives/source_admission_parity_2026-09-07.md)
fixes the current edit boundary. Follow its single reached execution falsifier,
not a historical builtin, constructor-performance or research queue in this
intake. Local focused improvements do not close all backends, typed Intent,
full bootstrap, installed-driver evidence or scheduled CI.

The previous 175-line intake mixed review disposition with successive active
performance checkpoints. It is preserved byte-for-byte in the
[historical archive](archive/architecture_review_5b97_before_admission_refresh_2026-09-07.txt),
SHA-256 `64B33DB12C093381EA34A6B28D098846DE0A39ED17575AE03B346EAC5E822727`.
Its former “next” paragraphs are historical evidence, never a resume queue.
