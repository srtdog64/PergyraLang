# AIR (Abstraction Intent Representation)

마지막 업데이트: 2026-04-25

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

- `kind: enum { Zone, World, Parallel, IO, Channel }`
- `enters_at: HIRBlock*` — 경계 진입 block
- `exits_at: HIRBlock*[]` — 경계 이탈 block 들
- `authority_required: AuthorityToken?` — 경계 통과 권한
- `sync_class: enum { Sync, Async, Either }`

### 3.3 (Phase 2) Constraint Node, Effect Node, Drift Fact

베타 후 확장. Phase 1 에서는 위 두 노드만 구현한다.

## 4. AIR 의 단일 책임 — Drift Detection

AIR 가 존재하는 이유는 **딱 하나**, "intent ↔ implementation drift 검출" 이다.

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

### 4.2 Drift Check Pass

Phase 1 에서 구현하는 단 하나의 pass.

입력: AIR (Intent Node + Boundary Node)
검사:
1. 각 intent 의 `sync_class` constraint 와 그 intent step body 가 거치는 boundary 의 `sync_class` 가 일치하는가
2. Intent 의 `failure_class` 가 `Recoverable` 인데 boundary 가 `Fatal` 만 노출하는 경계를 거치는가
3. (Phase 2 이후 확장)

출력: drift 진단 (`PGY_SEM_INTENT_BOUNDARY_DRIFT`,
`PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`)

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

### Phase 1 (베타 closure 안)

- AIR 데이터 구조: Intent Node + Boundary Node 두 종류만
- Synthesis pass: HIR + DIR + RIR 에서 AIR 합성 (read-only)
- Drift check pass: sync/async constraint vs boundary 충돌
- Strict evidence pass: default strict AIR에서 missing RIR boundary/authority evidence hard-fail. `PGY_AIR_STRICT_EVIDENCE=0`은 development/debug opt-out이다.
- Execution boundary scan: Phase 1 synthesis now walks intent-step AST clauses
  (`using`, `intent`, `pre`, `guard`, `post`, `invariant`, `expect`, `on`,
  `compensate`) and promotes `spawn` / `async` / `parallel`, `channel` /
  `select`, and known IO calls (`ReadFile`, `WriteFile`, `ReadLine`) into AIR
  `Boundary Node`s before drift checking.
- Expression boundary evidence is source-specific: `spawn` / `async` /
  `parallel`, `channel` / `select`, and IO boundaries are not satisfied by a
  generic RIR scope with the same intent owner. They need matching
  boundary-source evidence, otherwise default strict AIR emits
  `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`.
- Transfer boundary synthesis is split: a step with both `where: ZoneType` and
  `transfer: from -> to` emits a `Zone` boundary for the `where` type and a
  separate `World` boundary for the handoff. The world boundary source is the
  transfer target alias when present, otherwise the source alias.
- World boundary evidence is transfer-op specific: a matching RIR intent/world
  scope is not enough. AIR accepts the boundary only when RIR exposes the
  corresponding `Move(from -> to)` or `Claim(to <- from)` evidence for the world
  boundary source.
- AIR owns synthesized names (`intent_owner`, `step_name`, boundary
  `owner_name`, `source_name`, and authority participants). AIR diagnostics and
  tests must not depend on DIR/AST string lifetime after parser teardown.
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

### Phase 2 (베타 후)

- Constraint Node, Effect Node 도입
- Drift fact 종류 확장 (failure class, transactional scope, persistence)
- DB / Network / FileSystem / External device 효과 노드

### Phase 3

- AIR 가 안정되면 일부 metadata 의 단일 source of truth 화
- 예: zone boundary 정보가 DIR 와 AIR 양쪽에 있던 것을 AIR 단일화

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
