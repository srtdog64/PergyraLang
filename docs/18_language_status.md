# Pergyra 언어 상태 평가

마지막 업데이트: 2026-04-11

## 요약

Pergyra는 **실행 가능한 실험 언어 알파** 단계다.  
문법과 런타임이 실제로 동작하며 C/LLVM 양쪽에서 테스트가 돌아간다.  
다만 “범용 언어”나 “상용 안정성”을 기대하기엔 아직 이르다.

## 현재 강점

- Slot/Resource/Orchestration 중심 의미론이 코드와 런타임에 연결됨
- `async/await`, `channel`, `select`, `parallel`의 실행 경로 존재
- `RemoteFuture<T> → await → Result<T>` 규칙이 시맨틱/코드젠에 반영됨
- `Option<T>` + `Some/None` + `match` destructuring이 시맨틱/C/LLVM에 반영됨
- 채널의 non-blocking/timeout convenience surface가 추가됨: `TryRecv`, `RecvTimeout`, `TrySend`, `SendTimeout`
- 채널의 결과형 send status surface가 추가됨: `TrySendStatus`, `SendTimeoutStatus`
- 채널의 backpressure observation surface가 추가됨: `ChannelLength`, `ChannelCapacity`, `ChannelSpace`, `ChannelFull`, `ChannelClosed`
- `select`가 fixed-order readiness check에서 round-robin 시작 인덱스 기반 검사로 올라옴
- `Cancel(task)` / `IsCancelled()` cooperative cancellation surface가 C/LLVM/runtime에 연결됨
- spawned descendant가 부모 cancellation chain을 상속함
- `match`의 제한된 exhaustiveness check가 `Option/Result/enum`에 반영됨
- `match`의 redundant branch 경고가 `Option/Result/enum`에 반영됨
- `with effects ...` / `/// @effects ...` 선언과 body inferred effect 사이의 mismatch 진단이 반영됨
- `Box<T>` explicit handle surface와 `Box<class>` object-handle 경로가 semantic/C backend에 반영됨
- Role/Party/World 문법과 코드젠이 C/LLVM 양쪽에 존재
- `relation`, `effect`, `zone` declaration keyword가 parser/semantic 표면에 반영됨
- `intent` declaration이 parser/semantic/HIR/codegen 표면에 반영되어 world/zone/action 계약을 참조하는 executable orchestration declaration으로 동작함
- `relation`, `effect` declaration은 C backend에서 struct + method wrapper로 codegen됨
- `relation`, `effect` constructor는 positional nominal constructor로 type-check되며, runtime instance를 직접 만들 수 있음
- `relation`, `effect`, `zone`은 `subject slot` / `object slot` / `tobject slot` 최소 표면까지 parser/semantic에 반영됨
- `relation`, `effect`, `zone`의 domain slot은 optional initializer를 받아 projection/resulting object wiring을 직접 표현할 수 있음
- `relation`, `effect`는 optional `for ...` header로 subject endpoint/target을 고정하는 최소 표면까지 반영됨
- `relation`, `effect`는 optional `for object ...` header로 object endpoint/target도 고정하는 최소 표면까지 반영됨
- `zone`은 `relation slot` / `effect slot` / fixed-capacity `effect pool`, `world`는 `zone` slot 최소 조립 표면까지 parser/semantic에 반영됨
- `zone`은 `apply effectSlot to targetSlot`, `detach effectSlot from targetSlot` 최소 attachment 표면까지 parser/semantic에 반영됨
- `zone`은 `link relationSlot between left, right`, `unlink relationSlot between left, right` 최소 relation wiring 표면까지 parser/semantic에 반영됨
- `zone`은 `refresh objectSlot from subjectSlot`으로 projection 갱신을 명시할 수 있음
- `zone`은 `publish dtoSlot from subjectSlot`으로 tobject projection 갱신을 명시할 수 있음
- `zone`은 `bind slotName from sourceSlot`으로 target slot kind 기반 projection sync를 명시할 수 있음
- `relation` / `effect`도 `refresh objectSlot from subjectSlot`, `publish dtoSlot from subjectSlot`, `bind slotName from sourceSlot` projection sync를 직접 가질 수 있음
- `zone`은 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right`로 지속 lifecycle rule을 둘 수 있음
- `zone`은 `authority subjectSlot`으로 승인 주체를 선언할 수 있음
- `zone` authority는 `requires Ability[, Ability]`를 붙여 subject type의 role impl ability까지 검사할 수 있음
- `zone`은 `state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias를 둘 수 있음
- `zone` lifecycle/projection 문장은 optional `by subjectSlot`을 받아 authority와 연결됨
- `zone` lifecycle 문장은 `apply/link/detach/unlink/maintain <stateName>` shorthand를 지원함
- `HasLayer(layerSlot)` builtin이 zone method 안에서 선언된 relation/effect layer slot의 활성 여부를 Bool로 읽을 수 있고, C backend는 rdlock + generation stale-warning helper로 lowering함
- `HasState(stateName)` builtin이 zone method 안에서 선언된 state alias를 Bool query로 읽을 수 있음
- `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)`로 state-slot 정합성까지 질의할 수 있음
- `world`는 `state name: zone zoneSlot`, `state name: zone zoneSlot projection projectionSlot`, `state name: zone zoneSlot layer layerSlot`, `state name: zone zoneSlot state zoneStateName`, `state name: all zoneOrState[, ...]`, `state name: any zoneOrState[, ...]`, `activate/deactivate/maintain zoneOrState` lifecycle surface를 가짐
- `HasZone(zoneOrState)` builtin이 world method 안에서 선언된 zone slot / world state alias를 Bool query로 읽을 수 있음
- `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)` builtin이 world method 안에서 embedded zone의 projection/layer/state flag를 직접 읽을 수 있음
- 파생 `world state`는 zone active flag와 embedded zone projection/layer/state flag를 자동 조합하는 읽기 전용 contract로 동작함
- intent observability는 `IntentLast*`, `IntentHistoryStep*`, `IntentActive*`에 더해 최근 완료 ring을 읽는 `IntentRecent*` (`Count/Name/Trace/Failure/StepCount/Failed`)까지 semantic/C/LLVM/runtime surface가 연결되어 있다
- `all` / `any` 조합 state는 앞서 선언된 zone slot 또는 world state alias를 다시 조합하는 최소 inter-layer composition policy로 동작함
- `all` / `any` 조합 state는 duplicate input과 direct zone slot + plain zone alias 중복을 warning으로 정리해 policy를 더 엄격히 가짐
- `all` / `any` 조합 state는 raw zone slot 직접 입력도 warning으로 낮춰, command layer보다 plain world state alias 중심의 derived layer 조합을 권장함
- `world` lifecycle도 duplicate `activate` / `deactivate`, conflicting `activate` + `deactivate`, redundant `activate` + `maintain`를 warning으로 정리해 zone lifecycle 쪽과 비슷한 정책 강도를 가짐
- direct `activate/deactivate/maintain <zoneSlot>`도 semantic/C/LLVM에서 동일하게 zone slot으로 해석됨
- C backend는 zone layer를 `__layer_active_*` flag로, zone state를 `__state_*` flag와 `Zone_sync(self)` helper로, world zone lifecycle을 `__zone_active_*` / `__zone_state_*` flag와 `World_sync(self)` helper로 낮춤
- relation/effect method는 C/LLVM 양쪽에서 sync helper를 전후로 감싸 projection sync를 incremental하게 갱신함
- C backend는 relation/effect constructor를 runtime compound literal instance로 lowering하고, instance method call을 pointer-self로 호출함
- LLVM backend도 relation/effect constructor type path와 runtime instance method call parity까지 올라와 있음
- zone/world lifecycle sync와 `HasLayer` / `HasState` / `HasZone` query lowering도 이제 LLVM까지 올라와 C/LLVM parity를 가진다
- zone layer slot은 이제 C/LLVM에서 실제 typed `relation` / `effect` runtime instance로 유지되고, zone sync가 subject slot을 overlay endpoint/target에 바인딩한 뒤 projection sync까지 밀어준다
- `effect pool`도 LLVM에서 concrete pool storage `{items, active, count, cap}`로 내려가며, `apply`와 `HasLayer(...)`가 pooled effect runtime 위에서 동작한다
- world sync는 이제 C/LLVM 양쪽에서 `command pass(reset/directives) -> zone sync pass -> derived pass` 두 층 모델로 계산된다
- world sync는 per-zone dirty flag와 world derived-dirty flag를 사용해 dirty zone만 다시 sync하고 derived layer를 필요한 경우에만 다시 계산한다
- world constructor는 zone dirty와 world derived-dirty를 `true`로 초기화해 첫 sync에서 embedded zone projection/runtime state를 놓치지 않는다
- world method는 post-sync 직전에 embedded zone dirty를 다시 세워 world-owned zone 교체가 projection/derived state까지 전파되게 한다
- zone method 안에서 `self.poison.view.hp`, `self.trust.packet.name` 같은 embedded overlay projection read가 LLVM runtime smoke로 검증된다
- `apply/detach`는 `effect`의 bindable target arity/type와 기본 정합성을 검사하며 object target도 허용함
- `link/unlink`는 `relation`의 bindable endpoint arity/type와 기본 정합성을 검사하며 object endpoint도 허용함
- `refresh`/`publish`/`bind`는 object/tobject slot kind와 projection field 정합성을 검사하고, source는 subject/object를 허용하되 tobject source는 금지함
- `maintain`은 duplicate/conflicting lifecycle rule을 warning으로 보고함
- `authority`는 선언된 subject slot만 받을 수 있고, authority가 선언된 zone에서 mutable rule이 `by`를 생략하면 warning을 냄
- `state` shorthand는 effect/relation kind mismatch를 semantic error로 보고함
- `zone`은 subject-heavy shape에 대해 권장 기반 warning을 냄
- 장기 목표 계층 `ability -> role -> party -> relation -> effect -> zone -> world`가 문서상 고정됨
- `subject`가 코어 identity-bearing host로 semantic에 고정되어 `role`, `subject slot`, `ToObject` / `ToTObject`, subject copy restriction 경로에 실제로 참여함
- `subject`와 `class`는 parser AST에서 서로 다른 nominal declaration flavor로 기록되며, semantic도 둘을 구분함
- `vessel` declaration이 parser/semantic/transpile에 반영됐고, subject는 `vessel name: Type;` 형태의 피동 수용체 필드를 가질 수 있음
- `subject`는 `action` declaration을 직접 가질 수 있고, `requires` / `within` / `causes` / `authorized by` 최소 clause가 parser/semantic/C/LLVM 경로에 반영됨
- `action` clause는 이제 `authorized by` subject-host 확인, `within` zone slot/authority 적합성 확인, `causes` effect target/zone layer 적합성 확인까지 semantic에 반영됨
- hosted `func` / `action` body 안의 bare field access와 bare helper call은 이제 subject/class/relation/effect/zone/world 전반에서 implicit `self`로 해석되고, C/LLVM 양쪽에서 동일하게 lowering됨
- zone method 안의 subject `action` call은 이제 C/LLVM 둘 다 matching `effect slot` runtime activation과 embedded effect sync로 이어짐
- `self.player.Attack()` 같은 nested nominal host method call도 C/LLVM 모두에서 실제 subject/class dispatch로 lowering됨
- `role`은 non-subject nominal declaration에 바인딩될 수 없고, `party`는 subject-bound role impl이 없는 ability를 협력 슬롯에 둘 수 없음
- `subject`는 plain copy / plain value parameter / plain value return이 금지되고, `class`는 값 타입처럼 parameter/return/copy가 가능함
- C/LLVM lowering 모두에서 `subject` method는 pointer-self, `class` method는 value-self로 분기됨
- `object`와 `tobject`는 parser AST에서 distinct nominal flavor로 보존되며, `object`는 local/internal projection, `tobject`는 boundary transfer/publish contract로 semantic과 codegen에 연결됨
- `object` declaration은 passive state target 형식이지만 helper `func`와 국소 상태를 가질 수 있고, `tobject`는 더 좁은 projection/value 형식임
- `subject`는 일반 `func`와 공적 `action`을 모두 가질 수 있음
- `func`는 계산/보조 판단/국소 상태 갱신용 hosted func이고, `action`은 zone/authority/effect와 연결되는 공적 오케스트레이션 동사임
- `Void`는 결과가 없음을 나타내는 반환 타입이고, `return`은 현재 실행을 종료하는 제어 문장으로 구분됨
- `return;`은 `Void` 경로의 조기 종료이고, `return expr;`은 non-`Void` 경로의 값 반환임
- example smoke는 backend-aware exact stdout goldens와 backend-aware exact `expected_results` goldens를 함께 지원함
- 현재 직접 확인된 회귀 범위: `test-transpile 464 passed`, `test-abi 56 passed`, `llvm-test-backend-compare` 통과, `example-test-smoke` 통과, `ir-pipeline-test-smoke` 통과, `fmt-test-smoke` 통과
- `ToObject(TargetObject, subjectBinding)` built-in이 local passive object projection surface로 C/LLVM에 반영됨
- `ToTObject(TargetDto, subjectBinding)` built-in이 동명 필드 projection 기준의 최소 tobject surface로 C/LLVM에 반영됨
- relation/effect/zone/world 문맥 밖의 direct `ToObject` / `ToTObject`는 warning 대상이며, 권장되는 투영 흐름은 domain-local `object slot` / `tobject slot`과 projection sync(`refresh` / `publish` / `bind`)임
- `entity`는 코어 존재론 바깥의 프레임워크 어휘로 밀어두고, `object`는 intent를 시작하지 않는 passive state target으로 정리됨
- `object`는 이제 문서 수준이 아니라 실제 semantic/codegen에서도 effect target, relation endpoint, projection source로 쓸 수 있음
- 문서에 쓰던 `.Some/.None/.Ok/.Err` shorthand가 현재 파서에도 반영됨
- 현재 `intent`는 `Intent(args...)` 호출이 generated runtime function으로 lowering되고, same-subject conflict registry를 통해 `exclusive` 차단, `concurrent` 병행, higher-`priority` nested override까지 수행한다. step-level `guard` / `invariant`도 실행되고, reverse-order `compensate` rollback과 `IntentLastTrace()` / `IntentLastFailure()` / `IntentLastName()` / `IntentLastHandle()` / `IntentLastStepCount()` / `IntentLastFailed()` history도 동작한다. `IntentHistoryCount()` / `IntentHistoryStep*()`는 마지막 completed intent의 step-level typed history를 읽는다. `using: zoneAlias;`는 live zone-instance sync와 participant-to-zone-slot materialization을, `transfer: source -> target;`는 cross-zone handoff materialization과 transfer trace를 제공한다.

## 현재 한계

- 문서/설계가 많아 표면이 커 보이지만, 실제로는 일부 영역이 “supported but evolving” 상태다
- `relation`, `effect`, `zone`은 declaration keyword와 lifecycle shorthand, C backend sync/codegen까지 올라왔지만 deeper runtime propagation semantics는 아직 얕음
- `relation`, `effect`, `zone`의 플래그/constructor/sync는 C/LLVM parity를 가지고, world 쪽도 `all` / `any` 조합 state까지 올라왔지만, 더 깊은 propagation model은 아직 남아 있다
- projection의 중심은 `tobject` 자체가 아니라 `relation/effect/zone/world` 문맥과 projection sync 흐름이다
- `HasProjection(slotName)`는 relation/effect/zone 문맥에서 object/tobject projection slot의 sync-ready 상태를 읽는 query surface로 들어갔고, semantic/C/LLVM runtime parity까지 닫혀 있다
- zone/world lifecycle은 C/LLVM 양쪽에서 flag + sync helper 기반 incremental semantics까지 올라왔지만 richer propagation model 자체는 아직 얕다
- 베타 기준에서 먼저 믿어도 되는 relation/effect/projection surface는 다음이다:
  - declaration + positional constructor
  - `subject slot` / `object slot` / `tobject slot`
  - `refresh` / `publish` / `bind`
  - `HasProjection` / `HasLayer` / `HasState`
  - zone/world sync helper 기반 incremental runtime parity
- 베타 범위 밖으로 남겨둔 것은 다음이다:
  - authority/resource/effect의 더 완전한 unified partial order
  - 더 깊은 propagation model
  - higher-order multi-layer runtime policy
- `subject`와 `class`는 이제 parser/semantic뿐 아니라 C/LLVM method lowering, 저장/복사 규칙에서도 분기되기 시작했다
- `subject slot`과 `ToObject` / `ToTObject` projection source는 subject host (`subject`, `subject`)에 허용되고 bare `class`는 제외된다
- `subject`는 코어 host 선언으로 고정됐고 `subject Name { ... }`가 기본 표면이다
- plain `Slot<subject-host>`와 secure `SecureSlot<subject-host>`는 local object-cell anchor로 동작한다
- `ref Slot<subject-host>`와 `own SecureSlot<subject-host>`는 semantic + C/LLVM backend에서 함수 경계 전달이 가능하다
- secure boundary slot은 함수 body 안에서 paired `s_token` 심볼을 자동 제공받는다
- secure boundary slot은 helper forwarding call에서도 paired token을 유지한다
- 현재 stable surface는 anchored subset까지다. strict beta-quality closure track에서는 일반 ownership system을 다시 연다
- LLVM backend는 nested member assignment와 world-owned zone mutation propagation까지 runtime smoke로 검증된다
- 남은 공백은 deeper handle/runtime propagation model이다
- `QubitSlot` / `ClaimQubit` / `Measure` / `Entangle` 표면은 남아 있지만, full quantum resource semantics는 베타 대상이 아니라 `v2 / experimental`이다
- 클래식 OOP 계층(상속, 부모 호출)은 미지원
- 패키지 매니저, WASM, product-grade debugger/LSP 같은 생태계 영역은 아직 미완성
- backpressure는 관측 surface와 send result surface까지는 올라왔지만, bounded policy/backpressure protocol 자체는 아직 미완성
- cancellation은 cooperative + descendant propagation 수준, fairness는 round-robin 시작 인덱스 수준까지 올라온 상태

## Stable subset / explicit reject / beta-out-of-scope

문서와 구현을 같은 기준으로 읽기 위해, subset surface는 아래처럼 분류한다.

- generics
  - current stable subset: exact/ability/multi-bound baseline
  - strict closure target: default type argument actual resolution across declaration/instantiation paths
  - beta-out-of-scope: broader generic generalization
- own/ref
  - stable subset: anchored slot-handle boundary subset
  - strict closure target: general own/ref on non-anchored/general movable value types
  - beta-out-of-scope: general ownership system
- collections
  - stable subset: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`
  - explicit reject: unsupported map key kinds
  - beta-out-of-scope: arbitrary key-universal collection contracts
- runtime observability
  - stable subset: `last / history / active / recent`
  - explicit reject: 없음
  - beta-out-of-scope: richer multi-instance timeline query와 deeper failure provenance query

## Failure handling status and policy boundary

현재 구현은 실패를 세 층으로 나눠 읽어야 한다.

- `recoverable failure`
  - 도메인 실행 중 예상 가능한 실패
  - 원칙: 값을 통해 반환하고, reason/state를 조회 가능하게 남긴다
- `contract violation`
  - semantic 단계에서 막는 것이 원칙
  - 런타임까지 도달하면 structured panic
- `internal bug / invariant break`
  - 즉시 중단

### 이미 recoverable path로 올라온 것

- `Result<T>` + `Ok/Err` + `UnwrapOr` + postfix `?`
- `RemoteFuture<T> -> await -> Result<T>`
- channel timed / non-blocking surface
  - `TryRecv`
  - `RecvTimeout`
  - `TrySend`
  - `SendTimeout`
  - `TrySendStatus`
  - `SendTimeoutStatus`
- channel closed/empty/full operational 상태는 warning + false/None/Some(false) 경로로 노출
- world roster wait timeout은 warning + false 경로를 가진다
- intent observability
  - `IntentLast*`
  - `IntentHistoryStep*`
  - `IntentActive*`
  - `IntentRecent*`

### 현재 hard-fail로 남아 있는 것

- slot/secure-slot invariant break
  - released slot read/write/release
  - invalid token
  - token permission mismatch
- explicit unwrap sharp tools
  - `Unwrap(result)` on `Err`
  - `UnwrapOption(option)` / option unwrap on `None`
- memory/runtime invariant break
  - allocator / box / rc / weak misuse
  - array/slice out of bounds
- current runtime zone authority guard
  - `pgy_zone_authority_check_export(...)`는 현재 real authority rejection이 아니라 `null self / null participant ptr` invariant guard이며, validation 실패 시 panic한다

### strict beta-quality 기준에서 다음으로 내려야 할 것

아래는 `panic`보다 `recoverable failure + observability`가 더 맞는 축이다.

- intent/zone/world authority rejection
  - authority 없음
  - wrong participant
  - boundary mismatch
- intent runtime failure provenance
  - failed step
  - failure reason
  - derived/inherited contract source

주의:
- 현재 runtime zone authority guard는 아직 위 축을 구현하지 않는다.
- 지금 guard는 internal/invariant check에 가깝고, future real authority rejection path가 추가되면 그때 recoverable failure로 내려야 한다.

반대로 아래는 panic을 유지하는 편이 맞다.

- released slot / invalid token / ownership invariant break
- internal compiler/runtime invariant corruption
- explicit `Unwrap(...)` misuse

## 2026-04-11 기준 확인된 상태

- `make test-transpile` 통과 (`464 passed`)
- `make test-abi` 통과 (`126 passed`)
- `make llvm-test-backend-compare` 통과
- `make example-test-smoke` 통과
- `make ir-pipeline-test-smoke` 통과
- `make fmt-test-smoke` 통과
- Windows CI에서 드러난 path/newline/list-warning 이식성 문제를 이번 정리에서 닫음

## 다음 기준

1. 문서와 구현 표면의 1:1 정렬
2. orchestration/slot 의미론 고정
3. stable stdlib surface 확정
4. toolchain 품질 개선 (LSP 정밀도, formatter style depth, debugger runtime integration)

## 단계 결론

Pergyra는 "돌아가는 철학 실험"을 넘어, **자원 의미론을 가진 실행 가능한 언어 프로토타입**이다.
다만 아직 베타 수준의 안정성과 생태계 준비가 필요하다.

## v2 계획: 양자 연산 (Qubit / Quantum Resource Model)

> ⚠️ **v1에는 양자 표면과 최소 런타임/시맨틱 스켈레톤이 있지만, 전체 양자 자원 모델은 아직 닫히지 않았다.**
>
> `PgyQubit`, `QubitSlot`, `ClaimQubit()`, `Measure()`, `Entangle()` 표면은 존재한다.
> 다만 이것이 곧 완전한 quantum resource semantics를 뜻하지는 않는다.
>
> **Linear/Affine 제약, 얽힘 관계 추적, 측정 후 붕괴 검증까지 포함한 전체 모델은 v2 작업이다.**

v2에서 구현 예정:

- Linear/Affine 타입 시스템 (복사 금지, Move 강제)
- `Measure(q)` 후 상태 붕괴 추적 (컴파일 타임 검증)
- `Entangle(a, b)` 얽힘 관계 추적
- 관측 후 읽기 금지 (컴파일 에러)
- 양자 자원 수명 주기 관리
## Visibility and module boundary status

The current implementation now closes the main visibility gap around callable surfaces.

- top-level `public` / `private` cover nominal declarations, core domain declarations, and callable declarations
- private `intent` and private `event` declarations are blocked across module boundaries
- imported action contracts cannot reference private foreign `zone` or `effect` declarations through `within` / `causes`

Remaining work is no longer basic visibility plumbing. The remaining surface work is mostly about richer diagnostics and additional convenience surface, not whether these declarations are actually hidden.
