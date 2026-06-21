# AIR (Abstraction Intent Representation)

## 0. Inspection Path

AIR is a verification-only synthesis IR. It is not a codegen IR and it is not
carried in `CompilerIRBundle`.

Use `pgy --air <source.pgy>` to inspect the synthesized AIR state after
HIR/DIR/RIR evidence collection and before driver drift failure. The dump is
for review/debugging only; C and LLVM backends must not consume `AIRProgram`.

The expected dump shape starts with:

```text
AIRProgram intents=... boundaries=... evidence_nodes=... drifts=... strict_evidence=...
```

It then prints evidence counters, intent nodes, boundary nodes, legacy
per-boundary evidence flags, and first-class `AIREvidenceNode` provenance.
Current first-class evidence node kinds are `hir_routine`, `hir_cfg`,
`rir_boundary`, `rir_authority`, `mir_cleanup`, `mir_pin_cleanup`,
`mir_terminator`, `mir_select_receive`, `dag_metadata`, `dag_generic`,
`dag_ability`, `rir_effect_propagation`, `rir_relation_propagation`, and
`observability_schema`.

## 0a. Epsilon-Loss Isolation Contract

AIR does not pretend that abstraction lowering is lossless. When a domain,
business, or mathematical model is lowered into runtime, ABI, memory, backend,
or platform facts, some epsilon-loss is unavoidable: projection freshness can
become stale, authority evidence can be incomplete, a runtime boundary can
defer failure, ABI layout can force representation choices, and async/world
frontiers can reorder observable state.

AIR's job is to isolate that epsilon-loss at the boundary. It must not let the
loss leak back into business logic as ad-hoc defensive code, backend guesses, or
hidden semantic fallbacks. Every accepted loss must have explicit evidence:

- the boundary where the loss is introduced
- the source fact that made the loss unavoidable
- the owner that remains responsible for the original truth
- the decision: compile-time reject, conservative reject, runtime check, or
  accepted bounded approximation
- the user-facing diagnostic or trace schema that exposes the decision

This makes AIR an epsilon quarantine layer, not an epsilon elimination layer.
Semantic truth remains with the owning layer: CFG owns body-flow facts, DAG owns
type/generic/ability facts, RIR owns relation/effect/resource propagation facts,
MIR owns cleanup/pin/codegen-shape facts, and ABI/runtime owners define physical
representation and failure behavior. AIR verifies that those facts survive the
cross-layer boundary with explicit evidence or rejects the drift.

The general version of this rule is the abstraction loss contract in
`docs/semantics/09_abstraction_loss_contracts.md`: every boundary must say what
may be lost, what must be preserved, which owner keeps the original truth, which
downstream reads are forbidden, and which evidence proves the loss budget. AIR
is one consumer of that rule, not a special exception to it.

2026-05-02 debt status:

- AIR should be treated as the cross-layer verifier, not the owner of CFG, DAG,
  MIR cleanup, runtime propagation, or codegen.
- Covered evidence now has first-class inventory and consumers should use
  `air_boundary_has_evidence(...)` or the evidence-node inventory instead of
  reading legacy cached booleans directly.
- DAG metadata evidence is connected to AIR as provenance, but semantic
  judgement still belongs to the DAG owner. AIR must reject drift or missing
  evidence; it must not materialize generic/ability facts itself. DAG metadata
  reuse telemetry also stays owner-backed: AIR reads metadata hits through the
  `SemanticResult` accessor and rejects the impossible shape where hits are
  non-zero but the metadata inventory is empty. DAG evidence publication is a
  single AIR owner operation, not three separate local append/counter branches.
- MIR cleanup/pin evidence is connected to AIR as provenance, but cleanup
  generation and validation still belong to MIR. AIR must audit the evidence,
  not synthesize cleanup edges. A local `pin-unpin-cleanup-edge` fact is not
  enough by itself; AIR only accepts pin cleanup evidence when MIR also exposes
  a registered reachable cleanup root for the routine.
- Remaining AIR 1.0 debt is narrower consumer coverage: module/generic ability
  provenance edge cases and full runtime frontier scheduler coverage must become
  evidence-node-backed before AIR can be called the full abstraction-boundary
  verifier. RIR effect/relation propagation, trace/observability ABI evidence,
  and runtime frontier policy source-of-truth evidence are now present as global
  `AIREvidenceNode` entries and strict counter-only drift checks.
- Adjacent HIR/MIR source-of-truth cleanup continues below AIR: HIR CFG finish
  sequencing now lives in `hir_routine_cfg.c`, and MIR SSA use-edge population
  now lives in `mir_ssa_use_edges.c`. AIR should consume their evidence and
  summaries, not re-derive CFG shape or SSA use facts.
- Adjacent LLVM cleanup follows the same rule: `llvm_stmt_type_infer_nominal.c`
  owns nominal inference separately from expression type inference. AIR must
  treat backend type-inference results as backend evidence only; semantic type
  truth remains in the frontend/DAG pipeline.
- LLVM member-call support is similarly split into `llvm_member_call_support.c`
  so backend diagnostic/argument-storage mechanics do not become part of the
  member-call dispatch source of truth.
- LLVM scalar expression support is split into `llvm_expr_unary_core.c` for
  unary/try lowering and `llvm_expr_scalar_core.c` for callable signatures,
  coalesce lowering, and binary expressions. These remain backend lowering
  owners, not AIR semantic evidence owners.
- LLVM resource registry support is split into `llvm_registry_resources.c`
  for variable registry rows and `llvm_registry_resource_types.c` for resource
  and container type-shape construction. AIR must treat these as backend ABI
  evidence consumers only; resource ownership truth remains in MIR/ABI facts.
- LLVM domain struct registration is split into `llvm_domain_struct_register.c`
  for type-body construction and `llvm_domain_struct_register_fields.c` for
  generated class-field inventory. AIR must not infer domain boundary evidence
  from these backend field inventories; it should consume RIR/MIR evidence.
- LLVM let resource lowering is split into `llvm_stmt_let_resources.c` for
  view/move aliases and Slot/SecureSlot sugar. AIR must consume MIR cleanup and
  ownership evidence for these resources, not backend let-lowering branches.
- LLVM zone sync clause lowering is split into
  `llvm_domain_zone_sync_clauses.c` for action-cause and detach clauses, while
  `llvm_domain_zone_sync.c` keeps bounded frontier orchestration. AIR must
  consume RIR/MIR propagation evidence for zone behavior, not backend clause
  lowering branches.
- LLVM backend type rendering is split into `llvm_backend_type_render.c`, while
  `llvm_backend_type_map.c` keeps Pergyra-to-LLVM mapping policy. AIR must not
  infer semantic type truth from backend render strings; semantic type truth
  remains in DAG/frontend evidence.
- AIR summary counters are compatibility telemetry only. Reads and writes go
  through `air_validate_summary_counters.c`
  (`air_evidence_summary_count(...)`,
  `air_increment_evidence_summary_count(...)`,
  `air_increment_evidence_required_count(...)`). HIR, RIR, MIR, DAG, and
  runtime evidence collectors no longer mutate `air->*_evidence_count` or RIR
  propagation required counters directly, so counters cannot drift into a
  second evidence source of truth.
- AIR boundary evidence provider checks also go through a shared EvidenceNode
  accessor (`air_boundary_has_evidence_kind_provider(...)`). HIR collection,
  MIR pin cleanup collection, and boundary-evidence validation must not carry
  local provider-loop policy; they may only query whether the authoritative
  EvidenceNode inventory already contains a matching boundary/provider fact.
- Global provider checks follow the same rule through
  `air_has_global_evidence_provider(...)`. Boundary validation may require a
  matching global MIR cleanup provider, but it must ask the AIR evidence owner
  instead of reopening the global EvidenceNode inventory locally.
- Runtime singleton evidence uses
  `air_global_evidence_node_provider_subject(...)` for idempotence and count
  conflict checks. Runtime evidence collectors own which runtime facts are
  published; EvidenceNode identity lookup remains centralized with the AIR
  evidence query owner.
- AIR evidence-kind metadata is fail-closed. Every valid `AIR_EVIDENCE_*` kind
  must have an explicit `present` entry in `kEvidenceKindMeta`; otherwise
  `air_evidence_kind_is_known(...)` rejects it. This is intentionally stricter
  than relying on C zero-initialization, because a missing table entry must not
  become an accepted evidence kind with accidental default policy.
- AIR EvidenceNode identity matching is centralized inside the evidence
  inventory validator. Boundary/provider/subject checks must use the shared
  `air_evidence_node_matches_*` predicates through the public accessors rather
  than rewriting `kind + boundary + provider/subject` comparisons at each
  consumer.

마지막 업데이트: 2026-05-02

## 1. 포지셔닝: Verification IR, 별도 codegen 레이어 아님

Pergyra 의 컴파일러는 이미 다음 IR 스택을 가진다:

```
AST → HIR → DIR → RIR → MIR → C / LLVM
```

**AIR 는 이 codegen path 옆에 붙는 verification-only synthesis IR 다.** 기존 IR 들에서 단방향으로 합성되며, 어떤 IR 로도 lowering 되지 않는다.

```
AST → HIR → DIR → RIR → MIR → C / LLVM
              ↓     ↓
              ↓     ↓
              ↓ AIR (read-only synthesis)
              ↓     ↓
              drift / abstraction-safety check
```

이는 의도된 설계다:

- **Rust 의 MIR 은 codegen path 위의 IR** (HIR → MIR → LLVM). MIR 가 stale 되면 codegen 이 깨진다.
- **AIR 는 codegen path 옆의 IR**. AIR 가 stale 되어도 codegen 은 영향 없다.
- 결과적으로 6 번째 IR 의 유지비가 codegen IR 보다 훨씬 작다.

## 2. AIR 의 존재 이유 — CFG 사고에서 배운 패턴

Pergyra 는 한 번 같은 함정에 빠진 적이 있다.

| 단계 | CFG | AIR (예방하려는 것) |
|---|---|---|
| 사고 단계 | AST 트래버설 + 메타데이터 분산 + ad-hoc block tracking | HIR + DIR + RIR + semantic 에 metadata 분산, drift 검사가 cross-IR query |
| 사고 결과 | 소유권 분석이 분산된 metadata 위에서 일관성을 잃음 | (예방 없으면) 추상화 안전성 검사가 5 개 source 의 implicit 일관성에 의존 |
| 정정 단계 | HIR 에 명시적 CFG 도입 | 명시적 AIR (synthesis IR) 도입 |
| 정정 후 | semantic 이 HIR CFG 를 단일 source 로 read | drift check 가 AIR 를 단일 source 로 read |

핵심 교훈: **invariant 가 implicit 으로 분산되면 결국 깨진다.** 명시적 데이터 구조 + 명시적 invariant 로 단일 source of truth 를 만드는 것이 안전하다.

AIR 는 이 교훈을 의도(intent), 경계(boundary), 제약(constraint), 효과(effect), drift 의 도메인에 적용한 결과다.

## 3. AIR 가 표현하는 것 (Phase 1 scope)

AIR 그래프는 일반적인 CFG 위에 다음 도메인 노드를 합성한다.

### 3.1 Intent Node

이 코드가 해결하려는 의도와 보상/롤백 제약.

- `intent_owner: ASTNode*` — 이 routine 이 속한 intent step
- `step_index: size_t` — intent 안에서 step 순서
- `compensation_hook: ASTNode*` — rollback path
- `failure_class: enum { Recoverable, Fatal, Compensable }` — 실패 분류

### 3.2 Boundary Node

도메인/실행 경계.

- `kind: enum { Zone, World, Parallel, IO, Channel, Execution }`
- `enters_at: HIRBlock*` — 경계 진입 block
- `exits_at: HIRBlock*[]` — 경계 이탈 block 들
- `authority_required: AuthorityToken?` — 경계 통과 권한
- `sync_class: enum { Sync, Async, Either }`

### 3.3 (Phase 2) Constraint Node, Effect Node, Drift Fact

베타 후 확장. Phase 1 에서는 위 두 노드만 구현한다.

## 4. AIR 의 단일 책임 — Global Verification

AIR 가 존재하는 이유는 **딱 하나**, "intent ↔ implementation abstraction
safety 검증" 이다. Phase 1에서는 이것이 `air_verify(...)` 단일 entry point로
고정된다. 검증 순서는 AIR inventory shape, owner/source identity shape,
boundary sync-class shape, authority participant shape, authority evidence
participant membership, evidence provenance shape, drift/evidence failure
계산이다.
Drift Detection remains the primary Phase 1 diagnostic family inside this
global verification pass.

### 4.1 Drift 의 정의

Intent 가 선언한 제약과 실제 구현이 따라가는 boundary 가 충돌하는 상태.

예시:

```text
intent PlaceOrder
  requires:
    inventory must be reserved before order confirmation

constraint:
  inventory result must be synchronous

implementation:
  uses EventBus boundary (async)

→ AIR drift fact:
  "EventBus boundary conflicts with synchronous response requirement"
```

### 4.2 Verification Pass

Phase 1 에서 구현하는 단 하나의 pass. `air_check_drift(...)`는 오래된
테스트/스크립트를 위한 compatibility wrapper일 뿐이고, 새 compiler/docs
언어는 AIR를 verification layer로 부른다.

입력: AIR (Intent Node + Boundary Node)
검사:
1. 각 intent 의 `sync_class` constraint 와 그 intent step body 가 거치는 boundary 의 `sync_class` 가 일치하는가
2. Intent 의 `failure_class` 가 `Recoverable` 인데 boundary 가 `Fatal` 만 노출하는 경계를 거치는가
3. (Phase 2 이후 확장)

출력: drift 진단 (`PGY_SEM_INTENT_BOUNDARY_DRIFT`,
`PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`)

`AIRDriftKind` 는 위 in-pass 검사 외에 `AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH`
한 종류를 더 가진다. 이건 AIR 패스가 자기 안에서 채우지 않는다 — AIR 가 *선언*한
compression(A/B/C)과 백엔드 객체에서 *실측*한 물리 잔여물이 어긋날 때, AIR 가
보지 못하는 post-codegen `nm` 사실을 가진 out-of-band 계기판(`tests/air_erasure`)이
채운다. drift vocabulary 는 공유하되 ground truth 는 컴파일 바깥에 있다. 자세한 건
위 "Retain Cause and the Erasure Dashboard" 와 `docs/semantics/14`.

## 5. AIR 가 아닌 것 — 명시적 negative space

AIR philosophy 가 부풀려져서 6 번째 codegen IR 이 되는 것을 막기 위해 다음을 명시한다:

- **AIR 는 codegen IR 이 아니다** — C / LLVM 으로 lowering 되지 않는다.
- **AIR 는 ownership / borrow 검사의 home 이 아니다** — 그건 RIR + HIR CFG 의 책임.
- **AIR 는 type 검사의 home 이 아니다** — 그건 type-resolution DAG 의 책임.
- **AIR 는 effect propagation 자체의 home 이 아니다** — effect mask 는 여전히 semantic 에서 계산. AIR 는 effect 와 boundary 가 일관되는지만 본다.
- **AIR 는 새로운 keyword / syntax 를 추가하지 않는다** — 기존 intent / zone / world 선언에서 합성될 뿐.

## 6. 다른 언어 IR 과 비교

| 언어 | Killer IR | 책임 | Codegen path? |
|---|---|---|---|
| Rust | MIR | Memory safety (borrow check) | ✅ HIR → MIR → LLVM |
| Swift | SIL | Value semantics + ARC | ✅ AST → SIL → LLVM |
| **Pergyra** | **AIR** | **Abstraction safety (intent ↔ implementation drift)** | **❌ codegen path 옆의 verification IR** |

Pergyra 가 AIR 를 codegen path 위에 두지 **않는** 것은 의식적 선택이다. 이유:

- Pergyra 는 이미 codegen path 에 5 개 IR 이 있다 (AST/HIR/DIR/RIR/MIR). 6 번째를 codegen path 에 끼워넣는 비용은 너무 크다.
- AIR 의 책임은 codegen 이 아니라 verification 이다. 두 책임을 같은 IR 에 섞으면 둘 다 흐려진다.
- Verification IR 은 stale 되어도 codegen 출력에 영향이 없으므로, 점진적 도입 / 실험에 안전하다.

## 7. Phased Rollout

## 7.0 Final 1.0 Blueprint — Cross-Layer Abstraction Verifier

AIR is the 1.0 closure target for Pergyra's abstraction-safety story. This does
not mean AIR becomes the owner of the whole language. It means AIR becomes the
auditable ledger that checks whether facts proven by the lower layers agree
with the declared intent/zone/world/effect boundary contract.

The final AIR shape for 1.0 is:

- `IntentNode`: intent owner, step order, sync class, failure class, rollback /
  compensation policy, and source span.
- `BoundaryNode`: zone, world, parallel, channel, IO, execution, event, and pin
  boundaries with source identity and sync class.
- `EvidenceNode`: first-class references to proof facts from HIR CFG, DIR, RIR,
  MIR, and the type-resolution DAG.
- `DriftFact`: user-facing mismatch between the declared abstraction contract
  and the implementation evidence.

AIR 1.0 input evidence must come from the owning layer:

| Evidence source | Owner layer | AIR responsibility |
|---|---|---|
| Intent/zone/world declarations | DIR | Build `IntentNode` / declared `BoundaryNode` inventory |
| Body/control-flow reachability | HIR CFG | Prove implementation boundaries exist in lowered body flow |
| Authority/resource/effect/IO/channel/world ops | RIR | Prove boundary-specific resource and authority evidence |
| Cleanup/drop/pin/resource final facts | MIR | Prove implementation cleanup facts match boundary promises |
| Generic/ability/module/type facts | Type-resolution DAG | Prove declared contracts resolve to stable semantic facts |

AIR 1.0 must verify:

- declared intent sync/failure/authority constraints do not drift from actual
  implementation boundaries;
- zone/world handoff evidence is source-specific and does not pass through
  unrelated same-name scopes;
- IO/channel/parallel/event/execution boundaries have the required HIR and RIR
  evidence;
- pin/cleanup/resource boundaries have the required MIR cleanup facts when
  those facts are part of the declared contract;
- generic/ability/module facts referenced by abstraction contracts are resolved
  by the DAG, not by a recursive compatibility path;
- every drift diagnostic includes source span, reason, fix, and evidence
  provenance.

AIR 1.0 must not:

- construct CFG;
- perform type checking or generic resolution;
- perform ownership / borrow checking;
- run the runtime frontier scheduler;
- lower to C or LLVM;
- mutate HIR/DIR/RIR/MIR/DAG facts to make evidence fit.

The rule is: **each layer proves its own facts; AIR audits cross-layer
agreement.** If AIR starts replacing the owning layers, it becomes a duplicated
sixth compiler core and the architecture is wrong.

1.0 entry criteria for AIR:

- Phase 1 beta contract is frozen and green.
- `EvidenceNode` is explicit rather than a loose set of booleans.
- HIR, RIR, RIR effect/relation propagation, MIR cleanup/pin-cleanup, MIR
  terminator/select-receive, DAG generic/metadata/ability, runtime frontier, and
  observability evidence are represented as provenance-carrying nodes. The next
  closure target is deeper consumer coverage, not reintroducing boolean-only
  proof.
- `pgy --air` has a stable text dump and a stable machine-readable dump for CI /
  tooling.
- Positive and negative dogfood fixtures exercise intent, zone/world, event,
  IO/channel/parallel, pin/cleanup, and generic/ability contract evidence.
- C and LLVM backends remain non-consumers of `AIRProgram`.

### Phase 1 (베타 closure 안)

- AIR 데이터 구조: Intent Node + Boundary Node 두 종류만
- Synthesis pass: HIR + DIR + RIR 에서 AIR 합성 (read-only)
- Drift check pass: sync/async constraint vs boundary 충돌
- Strict evidence pass: default strict AIR에서 missing RIR boundary/authority evidence hard-fail. `PGY_AIR_STRICT_EVIDENCE=0`은 development/debug opt-out이다.
- Strict evidence policy update: default strict AIR hard-fails missing HIR CFG,
  RIR boundary, and RIR authority evidence; older wording that mentions only
  RIR boundary/authority evidence is stale.
- HIR provenance is now split into two facts: `has_hir_routine_evidence`
  records a matching lowered routine, while `has_hir_cfg_evidence` records that
  the matching routine also carries generated CFG for the same boundary AST when
  a boundary AST is available. This prevents routine-only intent summaries from
  being mistaken for CFG-backed body evidence.
- First-class `AIREvidenceNode` inventory is present for HIR routine, HIR CFG,
  RIR boundary, RIR authority, MIR cleanup/pin-cleanup, DAG generic/ability,
  RIR effect/relation propagation, and observability/trace schema evidence.
  Legacy
  per-boundary flags remain as the driver compatibility seam; new cross-layer
  checks should add evidence nodes instead of adding more boolean-only proof
  flags.
- Strict verification treats `AIREvidenceNode` as authoritative whenever an
  evidence inventory exists. The legacy per-boundary flags are cached summaries
  for dumps and compatibility fixtures; they do not independently satisfy
  strict evidence once inventory nodes are present.
- MIR summary counters follow the same rule. `mir_cleanup`,
  `mir_terminator`, and `mir_select_receive` counters are observability
  summaries only; strict AIR requires matching global `AIREvidenceNode`
  entries before those facts can prove abstraction-boundary safety. If MIR
  evidence nodes are present, their node count must match the corresponding
  summary counter; counter-only states are verifier drift, not validation
  invariants.
- Summary-counter consistency is owned by
  `src/compiler/air_validate_summary_counters.c`. The main evidence validator
  owns inventory shape; it delegates compatibility counter matching so the AIR
  validation layer stays split by responsibility rather than file size.
- AIR verification provenance formatting is owned by
  `src/compiler/air_verify_provenance.c`. `air_verify.c` makes drift decisions;
  the provenance owner renders authority lists and `source_provenance` /
  `who_provenance` strings for diagnostics.
- Consumers must use `air_boundary_has_evidence(...)` instead of reading those
  cached flags directly. This keeps driver diagnostics and AIR graph dumps on
  the same evidence-node source of truth as strict verification.
- Evidence nodes are shape-checked against their boundary class. Global evidence
  (`mir_cleanup`, `dag_*`, `rir_*_propagation`, `observability_schema`,
  `runtime_frontier_policy`) must not attach to a concrete boundary; `hir_cfg`
  evidence requires same-boundary `hir_routine` evidence; `rir_authority`
  evidence requires same-boundary `rir_boundary` evidence and a declared
  authority participant. Runtime frontier policy evidence is global and mirrors
  the runtime policy header: `pass_limit_fact_count=9`,
  `overflow_reason_fact_count=5`, and `fact_count=14` in
  `pgy.air.graph.v1`.
  authority participant; `mir_pin_cleanup` evidence can attach only to a `pin`
  execution boundary with the same source AST provenance.
- MIR cleanup and pin-cleanup evidence are collected through
  `air_collect_mir_evidence(...)` after MIR has produced cleanup facts. AIR does
  not create cleanup facts; it records that MIR-owned cleanup evidence exists,
  and strict verification requires matching `mir_pin_cleanup` evidence for AIR
  `pin` execution boundaries once MIR input has been attached.
  Global MIR cleanup evidence consumes CFG cleanup successors first and only
  then falls back to named cleanup-edge facts; boundary-specific pin cleanup
  evidence remains `AIR_EVIDENCE_MIR_PIN_CLEANUP` and is not collected for
  synthetic pin boundaries that lack exact source AST provenance.
- Parsed-source intent routines now have a minimal HIR CFG materializer:
  `hir_lower_intent_cfg(...)` builds ordered clause blocks for intent
  priority/success/failure expressions and each step's `where`, `using`,
  `intent`, contract, `on`, and `compensate` clauses. AIR consumes this as
  CFG-backed evidence for intent-step boundaries. It is not a replacement for
  the runtime propagation scheduler; it is the source-level CFG proof that the
  boundary AST exists in the lowered routine.
- Execution boundary scan: Phase 1 synthesis now walks intent-step AST clauses
  (`using`, `intent`, `pre`, `guard`, `post`, `invariant`, `expect`, `on`,
  `compensate`) and promotes `spawn` / `async` / `await` / `parallel` /
  `task-group`, `channel` /
  `select`, `with` / `unsafe` / `defer` / pin-block metadata, and stable
  resource IO/time calls into
  AIR `Boundary Node`s before drift checking. The current stable AIR boundary
  set is `FileOpen`, `FileExists`, `FileRead`, `FileWrite`, `FileClose`,
  `ReadFile`, `WriteFile`, `Input`, `ReadLine`, `Now`, and `Sleep` for IO; `spawn` /
  `async` / `await` / `parallel` / `task-group` for parallel; `channel-send` / `channel-recv` / `select`
  for channel; and `with` / `unsafe` / `defer` / `pin` / `event-subscribe` /
  `event-unsubscribe` for execution. `Print` / `Log*` are observability output
  calls, not AIR resource-boundary evidence in Phase 1. Execution boundaries
  are synchronous body/CFG boundaries; strict evidence checks HIR/CFG evidence
  for them, not RIR resource-boundary evidence. `with` and event-handler body
  traversal are part of the Phase 1 contract so nested IO/time boundaries are
  not hidden by execution containers.
- Expression boundary evidence is source-specific: `spawn` / `async` / `await` /
  `parallel` / `task-group`, `channel` / `select`, and IO boundaries are not satisfied by a
  generic RIR scope with the same intent owner. They need matching
  boundary-source evidence, otherwise default strict AIR emits
  `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`. They also need HIR CFG evidence:
  RIR evidence alone cannot satisfy a body-boundary proof.
- HIR CFG evidence is containment-aware. A boundary AST may be the direct CFG
  statement, the terminator condition/value, a pin-region marker, or a nested
  expression inside a CFG-carried statement such as `with { ReadFile(...) }`
  or `while ReadFile(...) { ... }`. The containment matcher covers the same
  core executable/expression forms as the AIR boundary walker: block, with,
  for/while, parallel, async, task group, spawn, call, assignment, arrays,
  tuples, await, channel ops, select, match, unsafe, defer, event invoke, and
  lambda body. AIR does not accept routine-name evidence alone for these
  implementation boundaries, and AST-less implementation boundaries can receive
  HIR routine evidence but not HIR CFG evidence. Manually constructed
  `AIR_EVIDENCE_HIR_CFG` nodes follow the same rule.
- AIR boundary walking and HIR containment also descend through executable
  payload carriers that are not boundaries by themselves: `let` and
  destructuring initializers, event subscribe/unsubscribe handler payloads,
  party-instance assignment values, party shared-field initializers, world
  roster/zone initializers, and domain-slot initializers. This prevents an IO /
  parallel / channel / execution boundary from being hidden behind a container
  node that only exists to carry a payload.
- `await` evidence is operation-specific: AIR accepts await boundary evidence
  only when RIR exposes the exact `AwaitLocal` or `AwaitRemote` operation for
  the same AST boundary. A generic intent scope named `await` or an unrelated
  await operation does not satisfy the proof.
- IO evidence is operation-specific for the beta-stable IO builtin set.
  RIR materializes `IO` operations for `FileOpen`, `FileExists`, `FileRead`,
  `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`, `Input`, `ReadLine`,
  `Now`, and `Sleep`; AIR accepts an IO boundary only when that operation matches the
  boundary source and AST provenance. If parser source spans fall back to the
  enclosing intent step, AIR accepts only an op contained in that same step.
  Parser-produced builtin call nodes now carry call source spans, so parsed
  IO boundaries normally match the exact `AST_CALL` rather than the enclosing
  step fallback.
- `channel-send`, `channel-recv`, and `select` evidence is operation-specific
  when a boundary AST is available. RIR now materializes `ChannelSend`,
  `ChannelRecv`, and `ChannelSelect` operations, and AIR accepts channel
  boundary evidence only from the matching same-AST operation. A same-owner RIR
  scope without the operation is not evidence. `make test-rir` includes a
  parsed-source fixture that lowers `ch <-`, `<- ch`, and `select` into those
  RIR operations before AIR consumes them.
- Parallel boundary evidence is operation-specific for the beta-stable parallel
  surface. RIR materializes `AwaitLocal`, `AwaitRemote`, `Spawn`, `Async`,
  `Parallel`, and `TaskGroup` operations; AIR accepts a parallel boundary only
  when the matching same-AST operation exists and HIR CFG evidence also reaches
  the boundary. `task-group` is no longer treated as HIR-only.
- `parallel`, IO, and channel boundaries do not fall back to scope-name RIR
  evidence when their AIR boundary lacks source AST provenance. These
  operation-specific boundaries remain unproven until the owning layer attaches
  a concrete boundary AST and RIR exposes the matching operation.
- Transfer boundary synthesis is split: a step with both `where: ZoneType` and
  `transfer: from -> to` emits a `Zone` boundary for the `where` type and a
  separate `World` boundary for the handoff. The world boundary source is the
  transfer target alias when present, otherwise the source alias.
- World boundary evidence is transfer-op specific: a matching RIR intent/world
  scope is not enough. AIR accepts the boundary only when RIR exposes the
  corresponding `Move(from -> to)` or `Claim(to <- from)` evidence for the world
  boundary source.
- World boundary evidence is also AST-specific when the boundary came from a
  source step. If the AIR boundary has an AST pointer, the matching RIR
  `Move`/`Claim` operation must point at the same AST. This prevents an
  unrelated transfer-like operation in the same scope from satisfying a parsed
  source handoff boundary by alias alone.
- Strict AIR also requires HIR CFG evidence for zone/world boundaries when HIR
  input exists. Standalone AIR unit graphs without HIR input remain valid, but
  the full compiler pipeline cannot validate a lowered zone/world boundary with
  RIR evidence alone.
- AIR owns synthesized names (`intent_owner`, `step_name`, boundary
  `owner_name`, `source_name`, and authority participants). AIR diagnostics and
  tests must not depend on DIR/AST string lifetime after parser teardown.
- AIR inventory invariants are hard validation failures, not user drift facts:
  intent owner/step names and boundary owner/source names must be non-empty,
  each boundary owner must match its referenced intent owner, each boundary step
  index must match its referenced intent step, and world / parallel / channel /
  IO boundaries must carry the frozen sync-class shape (`world`, `parallel`,
  and `channel` are async; IO is either-sync). Failures use
  `PGY_AIR_INVARIANT_INVALID`.
- Evidence provenance is also shape-checked. If HIR routine, RIR boundary, or
  RIR authority evidence flags are set, their provenance names must be
  non-empty. Boolean-only evidence and empty-string evidence are invalid AIR
  inventory, not weak proof.
- Existing drift inventory is validated before recomputation: persisted drift
  nodes must have a real drift kind, valid intent/boundary references, and a
  non-empty message. Placeholder `AIR_DRIFT_NONE` entries are invalid inventory,
  not silently cleared.
- Authority evidence match: `authorized by` participant 이름과 RIR authority
  fact / authorize op subject가 일치해야 한다. 같은 scope 안의 unrelated
  authority evidence는 boundary를 만족시키지 않는다.
- Authority evidence diagnostics: strict evidence 실패가 authority evidence
  누락이면 `Reason:`에 expected authority participant list를 포함한다.
- 진단 코드: `PGY_SEM_INTENT_BOUNDARY_DRIFT`, `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`
- 회귀: `make air-drift-test-smoke` 신설
- backend non-impact 회귀: `make air-backend-nonimpact-test-smoke` 는 relaxed AIR (`PGY_AIR_STRICT_EVIDENCE=0`)와 default strict AIR가 no-drift intent/zone, cross-world transfer, handoff frontier, world projection, relation/effect, authority-failure fixture set에서 동일한 C/LLVM 텍스트를 생성하는지 비교한다.
- full sweep: `make air-backend-nonimpact-full-test-smoke` 는 `tests/cases/backend_compare/*/main.pgy` 전체를 같은 방식으로 비교한다. 이 타겟은 Linux CI gate로 승격되어 default strict AIR가 backend output을 바꾸지 않는다는 full frozen fixture 증거를 제공한다.
- execution parity: `make air-strict-backend-compare-test-smoke` 는 strict evidence 상태에서 C/LLVM backend compare를 실행해, default strict AIR validation이 실제 실행 결과 parity를 깨지 않는지 확인한다.
- JSON schema gate: `make air-json-schema-test-smoke` 는 `pgy --air-json`
  출력이 `pgy.air.graph.v1` graph shape를 유지하는지 확인한다. Python이
  있으면 실제 JSON parser로 summary/boundary/evidence/observability schema를
  검증하고, Python이 없는 환경에서는 literal schema gate로 통과한다.

AIR keeps authority provenance explicit. The `authority_from_zone` JSON field is
retained for schema compatibility, but active beta semantics do not derive
`authorized by` from a local `who` clause. Approval must be explicit or
action-inherited, and drift diagnostics report active provenance as
`authority_provenance=action-inherited|explicit|none`; compatibility-only
zone authority fields are labeled `legacy-zone-field`.

### Compression Budget

AIR also owns the first stable proof-gated erasure vocabulary. Source-level
axes such as World, Zone, Intent, and Slot are semantic axes; they are not
automatically backend-level physical artifacts. AIR therefore exposes a
`compression_budget` and `compression_reason` on intent and boundary nodes in
`pgy.air.graph.v1`:

- `retain`: runtime-visible execution, authority, coordination, failure, IO,
  channel, parallel, world transfer, or execution-boundary behavior remains.
- `summarize`: the runtime carrier may disappear, but AIR keeps the
  evidence/provenance fact.
- `erase`: static evidence fully discharges the abstraction and ordinary
  call/value lowering is enough.
- `forbid`: erasure would change meaning until a stronger owner fact exists.

The initial implementation is conservative: runtime-visible boundaries retain,
authority-free static zone contracts summarize, and pure intent orchestration
can erase to the call sequence. Backends must not invent zone/world/intent
carriers, padding, barriers, or runtime authority checks from source syntax
alone; physical layout still belongs to MIR/ABI layout facts.

### Retain Cause (A/B/C) and the Erasure Dashboard

The disposition verb (`retain`/`summarize`/`erase`/`forbid`) says *what* AIR does;
it does not say *why a retain was unavoidable*. That "why" is the only thing that
distinguishes irreducible runtime from improvable static-analysis debt, so AIR
makes it machine-readable as a `retain_cause` on each node (the `reason` taxonomy,
structured — **not** a new disposition):

- `inherent` (**bucket A**): the boundary is a runtime fact — concurrency,
  transfer, authority — that no analysis can erase. Derived from a `retain`
  budget.
- `policy` (**bucket B**): kept by deliberate policy/traceability (a `summarize`
  digest, an always-on fail-closed tag). Removable by opt-out.
- `unproven` (**bucket C**): retained only because the static analysis could not
  discharge it; a stronger analysis could erase it. The **sole improvable
  bucket**; it must trend downward.

Three program-level counts in `pgy.air.graph.v1` source the buckets the per-node
`retain_cause` cannot (they are not intent-step boundaries):

- `unproven_retain_count` (bucket C) — lifecycle `CHECK` guards at ambiguous
  control-flow joins, collected from MIR lifecycle-guard facts. This is how the
  analyzer's "I could not prove it" verdict reaches AIR instead of staying
  invisible to it.
- `inherent_concurrency_count` (bucket A) — `parallel`/`async`/`spawn` blocks and
  channel send/receive, so a bare `parallel{}`/`channel` declares its irreducible
  concurrency residue even though it never becomes a boundary node.
- `slot_capability_retain_count` (bucket B) — SecureSlot/DeviceSlot resource
  operations declared from MIR type-layout facts, so token/capability checks
  that survive across method or ABI boundaries are declared instead of becoming
  an undeclared residue mismatch.

The honest thesis this encodes is **not** "loss = 0"; it is **bounded, measured,
attributed loss**. AIR *declares* the A/B/C decomposition; an out-of-band
instrument (`tests/air_erasure`, see `docs/semantics/14`) *measures* the physical
residue that survives `-O2` and **joins** the two. AIR is off-path and pre-codegen
— it cannot see `nm` facts — so a compiler self-grading its own erasure is not
evidence; the independence of the two instruments is the point.

Where the declaration and the measurement disagree —
AIR declares a program fully compressed yet physical residue survives, or vice
versa — that is `AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH`, a kind in the existing
drift vocabulary, populated by the out-of-band instrument (the ground truth is
post-codegen). The CI gate fails the build if bucket C grows past its baseline or
a provable fixture stops erasing; a *new* residue mismatch is a hard failure while
*documented* ones are recorded, not hidden.

**Newly-found, honestly recorded (surfaced by the drift/parity checks, in
`tests/air_erasure/baseline.json`):**

- Slot-capability (secure/device token) checks are modeled as AIR bucket-B
  retains via `slot_capability_retain_count`; the secure check that genuinely
  survives `-O2` across a method boundary (`08_secure_slot_method`) is now
  declared instead of listed as `expected_drift`.
- The LLVM backend rejects tuple-destructure `let (slot, token) = ClaimSecureSlot()`
  (the C backend accepts it) — a real surface-coverage divergence the C == LLVM
  parity check found, recorded as `llvm_unsupported`.
- A faithful LLVM *physical* residue pass (runtime-linked, `opt`/`llc`) is
  tooling-gated where those LLVM tools are absent; residue parity currently rests
  on shared-runtime + verified behavioral parity.

### Phase 2 (post-beta, toward 1.0)

- `EvidenceNode` is no longer a post-beta TODO. HIR/RIR/MIR/DAG provenance is
  already represented as first-class evidence inventory, while legacy
  per-boundary flags remain cached compatibility summaries.
- Constraint Node, Effect Node 도입.
- Drift fact 종류 확장: failure-class mismatch, transactional-scope mismatch,
  persistence mismatch.
- DB / Network / FileSystem / External device 효과 노드.
- MIR cleanup / pin evidence 연결.
- DAG generic / ability / module evidence 연결.

### Phase 3 (1.0 hardening)

- AIR graph dump의 Phase 1 stable JSON baseline은 `pgy.air.graph.v1`로 고정됐고
  `make air-json-schema-test-smoke`가 CI 소비 가능성을 gate한다. Phase 3는
  Constraint/Effect node를 추가하되 Phase 1 key를 깨지 않는 방향으로 확장한다.
- LSP/CI/tooling이 AIR JSON을 읽어 abstraction drift를 표시한다.
- AIR가 안정되면 일부 metadata의 단일 source of truth화를 검토한다.
  예: zone boundary 정보가 DIR와 AIR 양쪽에 있던 것을 AIR 단일화.
- 단, codegen backend는 계속 AIR를 consume하지 않는다.

## 8. 비용 분석

| 항목 | Phase 1 비용 |
|---|---|
| AIR 데이터 구조 | 작음 — Intent + Boundary 두 종류, ~150 LOC |
| Synthesis pass | 중 — HIR/DIR/RIR 의 기존 metadata 활용 |
| Drift check pass | 작음 — 한 종류의 drift 만 |
| AIR invariants 문서화 | 작음 — Phase 1 invariants 4-5 개 |
| **Codegen 영향** | **0** — codegen path 아님 |
| **기존 IR 변경** | **최소** — metadata accessor 추가만 |

## 9. AIR 가 제공하는 진단의 모양

```text
PGY_SEM_INTENT_BOUNDARY_DRIFT at PlaceOrder:step_2
  Reason:
    - intent constraint: sync (requires synchronous inventory result)
    - implementation boundary: EventBus (async)
    - drift class: sync_async_conflict
  Fix:
    - bind inventory check inline before EventBus dispatch
    - or relax intent constraint to allow async result
  Provenance:
    - intent: PlaceOrder, step 2 ("reserve inventory")
    - boundary: EventBus.publish, src/order/service.pgy:42
```

이런 진단은 **타입 시스템도, ownership 도, effect 도 잡지 못하는 종류의 버그** 를 잡는다. 이게 AIR 의 진짜 가치다.

## 10. 요약

- AIR 는 **추상화 안전성을 위한 verification-only synthesis IR**.
- Codegen path 옆에 위치하며, codegen 으로 lowering 되지 않는다.
- HIR + DIR + RIR 로부터 단방향 합성된다.
- Phase 1 책임: intent ↔ boundary drift 검출 단 하나.
- 존재 이유: CFG 사고에서 배운 "implicit 분산 metadata 는 깨진다" 교훈을 abstraction safety 도메인에 적용.

Rust 의 MIR 이 *"이 메모리 접근은 위험하다"* 를 말해준다면, Pergyra 의 AIR 는 *"이 추상화 경계는 의도한 intent 와 충돌한다"* 를 말해준다. 단, 이 책임을 **codegen IR 위에 얹는 대신 codegen 옆에 별도 verification IR 로 분리** 하는 것이 Pergyra 의 architectural choice 다.

## 11. Global Verification Entry Point

AIR Phase 1 is now treated as a compiler validation layer, not just a drift
helper. The public verification entry point is:

```c
bool air_verify(AIRProgram *air, char **error_message);
```

`air_verify(...)` owns the global AIR invariants:

- every `Intent Node` has a stable owner name, step name, sync class, and
  failure class;
- every `Boundary Node` has a known kind, owner name, source name, sync class,
  and valid intent reference;
- non-zero AIR node/drift counts must carry the matching inventory arrays;
- boundary owner must match the referenced intent owner;
- boundary `step_index` must match the referenced intent node's step index;
- world / parallel / channel / IO boundaries must carry the frozen sync-class
  shape (`world`, `parallel`, and `channel` are async; IO is either-sync);
- existing drift inventory must carry valid drift kind, intent/boundary
  references, and message before recomputation;
- authority-required boundaries must carry explicit authority participant names;
- RIR authority evidence must sit on an authority-required boundary and must
  have matching RIR boundary evidence first;
- first-class evidence nodes must match their boundary class: global evidence is
  global-only, HIR CFG evidence requires HIR routine evidence, authority evidence
  must name a declared participant, and MIR pin cleanup evidence must target a
  pin execution boundary with exact source AST provenance;
- implementation boundaries must carry matching HIR CFG evidence;
- evidence flags must carry provenance names, not boolean-only claims;
- strict evidence mode is split by owner layer: pre-MIR verification checks HIR
  CFG and RIR boundary/authority/effect evidence before MIR lowering, then the
  post-MIR verification pass checks MIR cleanup/pin/terminator evidence before
  backend codegen.

`air_check_drift(...)` remains as a compatibility wrapper over `air_verify(...)`
for older tests and scripts, but new compiler code and docs should describe AIR
as a verification layer. This keeps AIR from becoming another ad-hoc checker:
synthesis builds the read-only graph, verification validates the graph and
records drift facts, and codegen still consumes only HIR/DIR/RIR/MIR.

AIR inventory failures use `PGY_AIR_INVARIANT_INVALID` and route as
`air:invariant:invalid` with `report-compiler-bug`. They are compiler IR
contract failures, not user-facing intent drift. User-facing abstraction
failures remain under `PGY_SEM_INTENT_BOUNDARY_DRIFT` and
`PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`.
