# Pergyra 개발 현황

마지막 업데이트: 2026-04-05

## 요약

- 컴파일러 파이프라인은 `Lexer → Parser → Semantic → HIR → Backend`로 고정됨.
- LLVM이 기본 백엔드이며, C 백엔드는 폴백/reference 경로로 유지됨.
- async/await는 coroutine runtime을 통해 동작하며, channel/select/parallel이 동작함.
- 최종 목표 계층은 `ability -> role -> party -> relation -> effect -> zone -> world`로 문서화됨.
- 최종 존재론은 `struct`와 `subject`를 분리하며, 현재 surface는 `subject`와 `class`를 별도 nominal declaration flavor로 기록하고 semantic/codegen도 점진적으로 분기함.
- `vessel` declaration이 parser/semantic/transpile에 반영됐고, subject는 `vessel name: Type;` 형태의 피동 수용체 필드를 가질 수 있음.
- `subject`는 `action` declaration을 직접 가질 수 있고, `requires` / `within` / `causes` / `authorized by` 최소 clause가 parser/semantic에 연결됨.
- `action` clause는 이제 존재 확인을 넘어서 `authorized by` subject-host 검증, `within` zone subject/authority 적합성 검증, `causes` effect target/zone layer 적합성 검증까지 포함함.
- `intent` declaration이 parser/AST/semantic/HIR/codegen에 반영되어 `intent Name(args...)`, legacy `involves`, `step`, `exclusive`/`concurrent`, `priority`, `where`, `who`, repeated `on`, `pre`, `post`, `requires`, `authorized by`, `causes`, `expect`, `success`, `failure`를 검증하고 executable generated function으로 lowering함
- `intent` runtime은 이제 same-subject conflict registry를 가져서 `exclusive` 차단, `concurrent` 병행, higher-`priority` nested override까지 수행함. step-level `guard` / `invariant`도 실행되며, reverse-order `compensate` rollback과 `IntentLastTrace()` / `IntentLastFailure()` / `IntentLastName()` / `IntentLastHandle()` / `IntentLastStepCount()` / `IntentLastFailed()` history도 동작하고, `IntentHistoryCount()` / `IntentHistoryStep*()`로 마지막 completed intent의 typed step history도 읽을 수 있으며, `using: zoneAlias;`와 `transfer: source -> target;`를 통해 live zone instance sync, actor-to-zone-slot materialization, cross-zone handoff materialization도 수행함
- hosted `func` / `action` body 안의 bare field access와 bare helper call은 이제 subject/class/relation/effect/zone/world 전반에서 implicit `self`로 해석되며, `self.`는 선택적 표기로 남음
- `object`는 현재 `struct` 호환 passive state-target declaration alias로 동작하며 helper `func`와 국소 상태를 가질 수 있고, `dto`는 더 좁은 transfer/projection declaration alias로 동작함.
- `ToObject(TargetObject, subjectBinding)` 최소 passive projection surface가 semantic/C/LLVM backend에 반영됨.
- `ToDto(TargetDto, subjectBinding)` 최소 dto projection surface가 semantic/C/LLVM backend에 반영됨.
- relation/effect/zone/world 바깥의 direct `ToObject` / `ToDto`는 여전히 허용되지만 semantic warning으로 낮춰졌고, 권장 경로는 domain-local projection wiring임.
- `entity`는 코어 언어 존재론에 넣지 않고, 필요하면 프레임워크/도메인 용어로만 취급함.
- 상위 레이어로 갈수록 더 덜 구속적인 문맥 계층이라는 원칙을 채택함.

## 구현된 컴포넌트

### 렉서 / 파서
- 파서는 파일 분할 구조: `parser.c`, `parser_expr.c`, `parser_stmt.c`, `parser_decl.c`, `parser_domain.c`, `parser_async.c`
- 문법 표면: `let`, `func`, `async`, `spawn/await`, `if/for/while/match/select`, `slot/view/move`, `subject/class`, `struct/object/dto`, `ability/role/party/relation/effect/zone/systemic/world`, `event`, `actor`, `import/export/namespace`
- `world`, `systemic`, `relation`, `effect`, `zone`은 declaration position에서만 키워드처럼 동작하고, local variable / expression position에서는 식별자로 그대로 쓸 수 있음
- `subject`, `class`, `struct`, `object`, `dto` declaration은 parser AST에서 서로 다른 nominal flavor로 보존됨
- 현재 domain 표면은 `ability/role/party/systemic/world`에 더해 `relation/effect/zone`의 최소 body surface까지 parser/semantic에 연결됨
- `relation`, `effect`, `zone`은 `subject slot` / `object slot` / `dto slot` / `shared` / `func`까지의 최소 표면이 parser/semantic에 연결됨
- `relation` / `effect`도 `refresh objectSlot from subjectSlot`, `publish dtoSlot from subjectSlot`, `bind slotName from sourceSlot` projection sync를 declaration body에서 직접 가질 수 있음
- `relation`, `effect`, `zone`의 domain slot은 optional initializer를 받아 `object slot view: PlayerView = ToObject(PlayerView, player)` 같은 local projection wiring을 직접 표현할 수 있음
- `relation`, `effect`는 optional `for ...` header로 subject endpoint/target을 declaration header에 고정할 수 있음
- `relation`, `effect`는 optional `for object ...` header로 object endpoint/target도 declaration header에 고정할 수 있음
- `relation` / `effect`는 positional nominal constructor call을 받아 local runtime instance를 만들 수 있고, constructor argument arity/type을 semantic에서 검사함
- `zone`은 `relation slot` / `effect slot` / fixed-capacity `effect pool`으로 overlay type을 참조할 수 있고, `world`는 `zone` slot으로 하위 지역 규칙을 참조할 수 있음
- `zone`은 `apply effectSlot to targetSlot`, `detach effectSlot from targetSlot`으로 local effect attachment/detachment를 최소 surface로 표현할 수 있음
- `zone`은 `link relationSlot between left, right`, `unlink relationSlot between left, right`로 local relation wiring을 최소 surface로 표현할 수 있음
- `zone`은 `refresh objectSlot from subjectSlot`으로 subject -> object projection 갱신을 명시할 수 있음
- `zone`은 `publish dtoSlot from subjectSlot`으로 subject -> dto projection 갱신을 명시할 수 있음
- `zone`은 `bind slotName from sourceSlot`으로 object/dto target kind를 slot declaration에서 추론하는 projection sync surface를 가짐
- `HasProjection(slotName)` builtin은 relation/effect/zone declaration / method 안에서 선언된 object/dto projection slot의 sync-ready 여부를 Bool query로 읽을 수 있음
- `zone`은 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right`로 지속 lifecycle rule을 선언할 수 있음
- `zone`은 `authority subjectSlot`으로 mutation/projection 승인 주체를 선언할 수 있음
- `zone` authority는 `authority subjectSlot requires Ability[, Ability]`로 승인 주체가 수행 가능한 ability 계약까지 명시할 수 있음
- `zone`은 `state name: effect ... on ...` / `state name: relation ... between ..., ...`로 lifecycle state alias를 선언할 수 있음
- `zone`의 `apply/link/detach/unlink/refresh/publish/bind/maintain`은 optional `by subjectSlot`을 받아 authority와 연결됨
- `zone`은 `apply stateName`, `link stateName`, `detach stateName`, `unlink stateName`, `maintain stateName` shorthand를 지원함
- `HasLayer(layerSlot)` builtin은 zone declaration / zone method 안에서 선언된 `relation slot` / `effect slot` / `effect pool` 활성 여부를 Bool query로 읽을 수 있음
- `HasState(stateName)` builtin은 zone declaration / zone method 안에서 선언된 state alias를 Bool query로 읽을 수 있음
- `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)`로 state가 어떤 slot 조합에 붙는지까지 명시적으로 질의할 수 있음
- `world`는 `state name: zone zoneSlot`, `state name: zone zoneSlot projection projectionSlot`, `state name: zone zoneSlot layer layerSlot`, `state name: zone zoneSlot state zoneStateName`, `state name: all zoneOrState[, ...]`, `state name: any zoneOrState[, ...]`, `activate/deactivate/maintain zoneOrState` 최소 lifecycle surface를 가짐
- `HasZone(zoneOrState)` builtin은 world declaration / world method 안에서 zone slot 또는 world state alias를 Bool query로 읽을 수 있음
- `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)` builtin은 world declaration / world method 안에서 embedded zone의 projection/layer/state flag를 직접 Bool query로 읽을 수 있음
- 파생 `world state`는 zone active flag와 embedded zone projection/layer/state flag를 자동 조합해 계산되며, 조합 state는 `all`/`any`로 앞선 zone/state alias를 다시 합성할 수 있음
- 조합 `world state`는 duplicate input과 direct zone slot + plain zone alias 중복을 semantic warning으로 보고함
- 조합 `world state`는 raw zone slot을 직접 입력으로 받으면 warning을 내고, plain world state alias를 통한 파생층 입력을 권장함
- direct `activate/deactivate/maintain` 대상은 plain `state name: zone zoneSlot` alias만 허용함
- `world` lifecycle도 이제 duplicate `activate` / `deactivate`, conflicting `activate` + `deactivate`, redundant `activate` + `maintain`를 semantic warning으로 보고함
- `activate/deactivate/maintain battle`처럼 direct zone slot 이름을 쓰는 표면도 C/LLVM world sync 경로에서 semantic과 동일하게 해석됨
- C/LLVM world sync는 이제 `command pass(reset/directives) -> zone sync pass -> derived pass` 순서로 고정됨
- world runtime은 `__zone_dirty_<slot>`와 `__world_derived_dirty`를 유지해 active flag가 바뀐 zone만 다시 sync하고, derived state는 dirty일 때만 다시 계산함
- world constructor는 embedded zone dirty와 derived dirty를 `true`로 시작시켜 첫 world sync에서 zone projection/layer/state를 빠뜨리지 않음
- world method는 post-sync 전에 embedded zone dirty를 보수적으로 다시 올려 world-owned zone 교체/갱신이 derived layer까지 전파되게 함
- world value에 embed된 zone은 바깥의 옛 zone 바인딩을 통해 다시 assignment/hosted `func`/`action`으로 mutate할 수 없고, semantic error로 차단됨
- C backend는 zone layer slot을 `bool __layer_active_<slot>` 필드로, zone state alias를 `bool __state_<name>` 필드로, world zone lifecycle을 `bool __zone_active_<slot>` / `bool __zone_state_<name>` 필드로 낮춤
- C backend는 relation/effect/zone/world마다 `<Type>_sync(self)` helper를 생성하고, method 실행 전후에 호출해 `refresh`/`publish` projection과 state/lifecycle flag를 incremental하게 동기화함
- C backend는 relation/effect/zone projection slot에 `__projection_ready_*` flag를 두고 `HasProjection(...)`를 현재 domain self field query로 lowering함
- C backend는 `relation` / `effect` constructor를 compound literal runtime instance로 lowering하고, instance method call도 pointer-self로 호출함
- LLVM backend도 relation/effect/zone/world declaration에 대해 `<Type>_sync(self)` helper와 method 전후 sync 호출 parity를 가지며, contextual `HasProjection(...)` / `HasLayer(...)` / `HasState(...)` / `HasZone(...)` lowering과 constructor/runtime instance path까지 연결됨
- `HasProjection(...)`는 현재 relation/effect/zone 문맥에서 semantic/C/LLVM runtime parity까지 닫혀 있음
- `zone` layer slot은 이제 C/LLVM 양쪽에서 `void*` placeholder가 아니라 typed `relation` / `effect` runtime instance로 유지되며, zone sync가 subject slot 값을 overlay endpoint/target에 바인딩한 뒤 `<Layer>_sync(&self->layer)`를 호출함
- LLVM backend의 `effect pool`도 이제 실제 `{items, active, count, cap}` storage로 lowering되어 `apply poolName to slotName`와 `HasLayer(poolName)`이 semantic-only가 아니라 concrete runtime path를 가진다
- direct `apply/link/detach/unlink`와 `maintain effect/relation/state` 모두 C/LLVM zone sync에서 실제 layer active/state/projection 전파로 연결됨
- zone method 안에서 `self.poison.view.hp`, `self.trust.packet.name` 같은 embedded overlay projection read가 LLVM smoke까지 닫혀 있음
- C backend에서 `HasLayer(...)`는 zone rdlock + generation stale-warning을 감싼 generated helper로 lowering되고, `HasState(...)` / `HasZone(...)`는 zone/world method 문맥 안에서 실제 `self->__state_*` / `self->__zone_*` 필드 질의로 lowering됨
- `zone`의 `apply/detach`는 `effect` declaration의 bindable target 수와 타입을 검사하며 object target도 허용함
- `zone`의 `link/unlink`는 `relation` declaration의 bindable endpoint 수와 타입을 검사하며 object endpoint도 허용함
- `zone` / `relation` / `effect`의 `refresh`/`publish`/`bind`는 object/dto slot kind와 projection field 정합성을 검사하고, source는 subject/object를 허용하되 dto source는 금지함
- `zone` subject slot은 이제 bare `class`가 아니라 subject host (`subject`, `actor`)만 허용함
- `ToObject` / `ToDto` source projection은 이제 bare `class`가 아니라 subject host binding만 허용함
- `role`은 이제 non-subject nominal declaration에 바인딩되면 semantic error를 냄
- `party` role slot은 이제 subject-bound role impl이 실제로 존재하는 ability만 협력 슬롯으로 받을 수 있음
- `zone`의 `maintain`은 `effect/relation` contract를 재사용하고 duplicate/conflicting lifecycle rule에 warning을 냄
- `zone` authority는 선언된 subject slot만 받을 수 있고, authority가 있을 때 mutable rule이 `by`를 생략하면 warning을 냄
- `zone` state shorthand는 effect/relation kind mismatch를 semantic error로 보고함
- `zone`은 현재 subject가 0개이거나 4개를 크게 넘는 형태에 대해 운영 lint를 냄
- `relation`, `effect` declaration은 C backend에서 struct + method wrapper로 codegen됨
- `relation/effect/zone`은 여전히 계층 간 구조적 의미론이 더 필요함
- `actor`는 semantic에서 subject execution profile로 취급되며, role binding, subject slot, `ToObject` / `ToDto` source, subject copy restriction에 참여함
- `object`는 intent를 시작하지 않는 passive state target으로 정리되며, 상태/반응/helper `func`를 가질 수 있음
- `dto`는 `object`보다 더 좁은 경계 전달/투영 형식으로 유지됨
- `subject`는 이제 일반 `func`와 공적 `action`을 모두 가질 수 있음
- `func`는 계산/보조 판단/국소 상태 갱신용 hosted func이고, `action`은 zone/authority/effect와 연결되는 공적 오케스트레이션 동사임
- example smoke는 backend-aware exact stdout goldens와 backend-aware exact `expected_results` goldens를 함께 지원함
- enum/result 패턴 shorthand: `Some(x)`와 `.Some(x)` 둘 다 파싱됨. `case .Ok(v):`, `return .None;` 같은 문서 표기도 현재 파서 기준으로 허용됨
- `Option<T>` 표면: `Some/None`, `IsSome/IsNone`, `UnwrapOption`, `match` destructuring이 semantic/C/LLVM 경로에 연결됨
- `match` 시맨틱: `Option/Result/tagged enum` destructuring 바인딩과 제한된 exhaustiveness check가 동작함
- `match` 품질 진단: duplicate variant case와 redundant default를 warning으로 보고함
- `with effects ...` / `/// @effects ...` 계약: 선언이 있으면 body inferred effect와 mismatch를 semantic error로 보고함
- `Box<T>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>`는 현재 object handle 경로로 허용되며, plain class value parameter/return 제한을 우회하는 명시적 저장/전달 표면으로 사용 가능
- `subject`는 plain copy / plain value parameter / plain value return이 금지되고, `class`는 값 복사/값 parameter/값 return을 허용함
- C backend와 LLVM backend 모두에서 `subject` method는 `self` pointer, `class` method는 `self` value로 lowering됨
- plain `Slot<subject>`와 `Slot<actor>`는 이제 local object-cell anchor로 허용됨
- 현재 회귀 수치: `semantic 474 passed`, `transpile 381 passed`, `llvm-test-smoke` 통과
- `SecureSlot<subject>`와 `SecureSlot<actor>`도 이제 local secure object-cell anchor로 허용됨
- `own/ref Slot<subject-host>` / `own/ref SecureSlot<subject-host>` 함수 경계 전달이 semantic + C/LLVM backend에 반영됨
- secure boundary slot은 paired token symbol을 함수 바디 안에 자동 노출해 `Write(s, ..., s_token)` / `Release(s, s_token)` 형태를 유지함
- secure boundary slot은 helper를 한 번 더 거치는 forwarding call에서도 paired token이 함께 전달됨
- LLVM backend는 `self.battle.player.hp = hp` 같은 deeper nested member assignment도 world/zone/object-cell 경로에서 실제로 갱신함
- 남은 공백은 richer handle/object-cell propagation semantics의 더 복잡한 경계 조합임
- 채널 convenience surface: `TryRecv -> Option<T>`, `RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`, `TrySendStatus/SendTimeoutStatus -> Option<Bool>`이 C/LLVM 경로에 연결됨
- 채널 backpressure observation surface: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`이 C/LLVM/runtime에 연결됨
- 현재 `TryRecv/RecvTimeout/TrySend/SendTimeout`은 plain-value channel 중심이며 movable resource channel은 의도적으로 제외됨

### 시맨틱
- 타입 시스템 + 슬롯 규칙 + move/consume 추적
- anchored handle(`Slot/SecureSlot/DeviceSlot`)와 movable handle(`QubitSlot`)의 경계 규칙 반영
- `RemoteFuture<T>`의 `await`가 `Result<T>`를 반환하도록 강제

### 백엔드
- C 백엔드: reference/fallback, 스모크와 트랜스파일 테스트로 유지
- LLVM 백엔드: 기본 실행 경로, backend-compare 및 llvm-smoke로 회귀 체크

### 런타임
- slot/secure slot/device slot/qubit slot 런타임
- coroutine runtime (POSIX ucontext + Windows Fiber)
- channel/parallel/select 지원
- channel non-blocking/timeout helper 지원
- `select`는 round-robin 시작 인덱스로 readiness를 검사해 단순 고정 순서 starvation을 줄임
- task cancellation surface: `Cancel(task)` / `IsCancelled()`가 C/LLVM/runtime에 연결됨
- spawned descendant는 부모 task의 cancellation chain을 상속함
- 현재 cancellation은 cooperative/best-effort이며, preemptive interruption은 아직 아님

### 도구
- LSP 서버 구현 존재 (`src/lsp/pgy_lsp.c`)

### 구현 메모

- 저장소의 `.inc` 파일은 Pergyra 언어 문법이 아니라, 큰 C 구현 파일을 안전하게 분할하기 위한 **textual include 조각**이다.
- 예: `type_checker.c`, `transpiler.c`, `llvm_expr.c`, 테스트 대형 파일은 일부 로직을 `.inc`로 나눠 `#include`한다.
- 목적은 빌드 구조를 바꾸지 않고 파일 길이와 책임 범위를 줄이는 것이다.

## 테스트 현황

2026-04-04 현재 직접 확인한 기준:

| 스위트 | 결과 |
|---|---|
| concurrency | 5 passed |
| semantic | 474 passed |
| transpile | 381 passed |
| llvm smoke | 통과 (`zone_action_effect_runtime`, `cancel_propagation`, `channel_pressure` 포함) |

추가 회귀:
- `make test-semantic` 통과
- `make test-parser` 통과
- `make test-transpile` 통과
- `make llvm-test-smoke` 통과 (async, select, tagged-union, RemoteFuture, device slot, generics, channel pressure 등)
- zone method 안의 subject `action` call은 현재 C/LLVM 모두에서 matching `effect slot` runtime activation과 embedded layer sync까지 연결됨
- `self.player.Attack()` 같은 nested nominal host method call도 이제 C/LLVM 모두에서 실제 method dispatch로 lowering됨

## 남은 주요 작업

### 단기
- orchestration 고도화 (`select` 공정성, timeout, cancellation, backpressure)
- partial 완료: channel timeout/non-blocking built-in surface 추가
- partial 완료: channel backpressure observation surface (`ChannelLength`, `ChannelCapacity`, `ChannelSpace`, `ChannelFull`, `ChannelClosed`)
- partial 완료: `select` round-robin fairness
- partial 완료: cooperative cancellation surface (`Cancel`, `IsCancelled`)
- partial 완료: spawned descendant cancellation propagation
- 완료: semantic O2 crash root-cause 정리 및 회귀 고정 (`Channel*` diagnostic format bug)
- partial 완료: source-level `with effects ...` signature surface
- partial 완료: `Box<class>` explicit handle surface (`BoxGet/BoxSet/BoxDrop/BoxIsValid`)
- partial 완료: `subject` vs `class` lowering/runtime split의 첫 단계 (`subject=self-cell`, `class=value self`)
- partial 완료: `actor`를 subject execution profile로 semantic 정렬 (`role`, `subject slot`, projection source, copy restriction)
- partial 완료: `subject Name actor { ... }` subject-first actor profile surface
- partial 완료: standalone `actor Name { ... }`를 transitional syntax로 경고
- partial 완료: plain/secure `Slot<subject>` / `Slot<actor>` local object-cell anchor
- partial 완료: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` 함수 경계 전달 (semantic + C backend)
- effect system 2단계 (더 정교한 effect lattice, call-site contract)
- relation/effect/zone declaration 이후의 구조적 의미론 고도화
- 안정화 문서 갱신 및 표면 문법 정리

### 중기
- stable stdlib surface 고정
- toolchain 강화 (formatter, LSP 진단 품질, debugger)

### 장기
- JavaScript backend policy 초안 정리 (`docs/23_js_backend_policy.md`)
- WebAssembly 타겟
- 패키지 매니저
- 성능 최적화 (LLVM 쪽 집중)
