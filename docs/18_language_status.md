# Pergyra 언어 상태 평가

마지막 업데이트: 2026-04-04

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
- `relation`, `effect` declaration은 C backend에서 struct + method wrapper로 codegen됨
- `relation`, `effect` constructor는 positional nominal constructor로 type-check되며, runtime instance를 직접 만들 수 있음
- `relation`, `effect`, `zone`은 `subject slot` / `object slot` / `dto slot` 최소 표면까지 parser/semantic에 반영됨
- `relation`, `effect`, `zone`의 domain slot은 optional initializer를 받아 projection/resulting object wiring을 직접 표현할 수 있음
- `relation`, `effect`는 optional `for ...` header로 subject endpoint/target을 고정하는 최소 표면까지 반영됨
- `zone`은 `relation slot` / `effect slot`, `world`는 `zone` slot 최소 조립 표면까지 parser/semantic에 반영됨
- `zone`은 `apply effectSlot to targetSlot`, `detach effectSlot from targetSlot` 최소 attachment 표면까지 parser/semantic에 반영됨
- `zone`은 `link relationSlot between left, right`, `unlink relationSlot between left, right` 최소 relation wiring 표면까지 parser/semantic에 반영됨
- `zone`은 `refresh objectSlot from subjectSlot`으로 projection 갱신을 명시할 수 있음
- `zone`은 `publish dtoSlot from subjectSlot`으로 dto projection 갱신을 명시할 수 있음
- `relation` / `effect`도 `refresh objectSlot from subjectSlot`, `publish dtoSlot from subjectSlot` projection sync를 직접 가질 수 있음
- `zone`은 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right`로 지속 lifecycle rule을 둘 수 있음
- `zone`은 `authority subjectSlot`으로 승인 주체를 선언할 수 있음
- `zone` authority는 `requires Ability[, Ability]`를 붙여 subject type의 role impl ability까지 검사할 수 있음
- `zone`은 `state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias를 둘 수 있음
- `zone` lifecycle/projection 문장은 optional `by subjectSlot`을 받아 authority와 연결됨
- `zone` lifecycle 문장은 `apply/link/detach/unlink/maintain <stateName>` shorthand를 지원함
- `HasLayer(layerSlot)` builtin이 zone method 안에서 선언된 relation/effect layer slot의 활성 여부를 Bool로 읽을 수 있음
- `HasState(stateName)` builtin이 zone method 안에서 선언된 state alias를 Bool query로 읽을 수 있음
- `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)`로 state-slot 정합성까지 질의할 수 있음
- `world`는 `state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState` lifecycle surface를 가짐
- `HasZone(zoneOrState)` builtin이 world method 안에서 선언된 zone slot / world state alias를 Bool query로 읽을 수 있음
- `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)` builtin이 world method 안에서 embedded zone의 projection/layer/state flag를 직접 읽을 수 있음
- C backend는 zone layer를 `__layer_active_*` flag로, zone state를 `__state_*` flag와 `Zone_sync(self)` helper로, world zone lifecycle을 `__zone_active_*` / `__zone_state_*` flag와 `World_sync(self)` helper로 낮춤
- relation/effect method는 C/LLVM 양쪽에서 sync helper를 전후로 감싸 `refresh`/`publish` projection을 incremental하게 갱신함
- C backend는 relation/effect constructor를 runtime compound literal instance로 lowering하고, instance method call을 pointer-self로 호출함
- LLVM backend도 relation/effect constructor type path와 runtime instance method call parity까지 올라와 있음
- zone/world lifecycle sync와 `HasLayer` / `HasState` / `HasZone` query lowering도 이제 LLVM까지 올라와 C/LLVM parity를 가진다
- zone layer slot은 이제 C/LLVM에서 실제 typed `relation` / `effect` runtime instance로 유지되고, zone sync가 subject slot을 overlay endpoint/target에 바인딩한 뒤 projection sync까지 밀어준다
- zone method 안에서 `self.poison.view.hp`, `self.trust.packet.name` 같은 embedded overlay projection read가 LLVM runtime smoke로 검증된다
- `apply/detach`는 `effect`의 subject target arity/type와 기본 정합성을 검사함
- `link/unlink`는 `relation`의 subject endpoint arity/type와 기본 정합성을 검사함
- `refresh`/`publish`는 object/dto slot / subject slot kind와 projection field 정합성을 검사함
- `maintain`은 duplicate/conflicting lifecycle rule을 warning으로 보고함
- `authority`는 선언된 subject slot만 받을 수 있고, authority가 선언된 zone에서 mutable rule이 `by`를 생략하면 warning을 냄
- `state` shorthand는 effect/relation kind mismatch를 semantic error로 보고함
- `zone`은 subject-heavy shape에 대해 권장 기반 warning을 냄
- 장기 목표 계층 `ability -> role -> party -> relation -> effect -> zone -> world`가 문서상 고정됨
- `actor`가 semantic에서 subject execution profile로 취급되어 `role`, `subject slot`, `ToObject` / `ToDto`, subject copy restriction 경로에 실제로 참여함
- `subject`와 `class`는 parser AST에서 서로 다른 nominal declaration flavor로 기록되며, semantic도 둘을 구분함
- `role`은 non-subject nominal declaration에 바인딩될 수 없고, `party`는 subject-bound role impl이 없는 ability를 협력 슬롯에 둘 수 없음
- `subject`는 plain copy / plain value parameter / plain value return이 금지되고, `class`는 값 타입처럼 parameter/return/copy가 가능함
- C/LLVM lowering 모두에서 `subject` method는 pointer-self, `class` method는 value-self로 분기됨
- `object` keyword alias가 parser/LSP surface에 반영되어 `object`와 `struct`가 같은 declaration으로 파싱됨
- `dto` keyword alias가 parser/LSP surface에 반영되어 `dto`와 `struct`가 같은 declaration으로 파싱됨
- `object` / `dto` declaration은 passive projection form으로 제한되며 method를 가질 수 없음
- `ToObject(TargetObject, subjectBinding)` built-in이 local passive object projection surface로 C/LLVM에 반영됨
- `ToDto(TargetDto, subjectBinding)` built-in이 동명 필드 projection 기준의 최소 dto surface로 C/LLVM에 반영됨
- relation/effect/zone/world 문맥 밖의 direct `ToObject` / `ToDto`는 warning 대상이며, 권장되는 투영 흐름은 domain-local `object slot` / `dto slot`과 `refresh` / `publish`임
- `entity`는 코어 존재론 바깥의 프레임워크 어휘로 밀어두고, `object`는 subject의 수동 해석 모드로 정리됨
- 문서에 쓰던 `.Some/.None/.Ok/.Err` shorthand가 현재 파서에도 반영됨

## 현재 한계

- 문서/설계가 많아 표면이 커 보이지만, 실제로는 일부 영역이 “supported but evolving”
- `relation`, `effect`, `zone`은 declaration keyword와 lifecycle shorthand, C backend sync/codegen까지 올라왔지만 deeper runtime propagation semantics는 아직 얕음
- `relation`, `effect`, `zone`의 플래그/constructor/sync는 C/LLVM parity를 가지지만, cross-layer composition policy와 더 깊은 propagation model은 아직 남아 있다
- projection의 중심은 `dto` 자체가 아니라 `relation/effect/zone/world` 문맥과 `refresh` / `publish` 흐름이다
- `HasProjection(slotName)`는 relation/effect/zone 문맥에서 object/dto projection slot의 sync-ready 상태를 읽는 query surface로 들어갔고, semantic/C/LLVM runtime parity까지 닫혀 있다
- zone/world lifecycle은 C/LLVM 양쪽에서 flag + sync helper 기반 incremental semantics까지 올라왔지만 richer propagation model 자체는 아직 얕다
- `subject`와 `class`는 이제 parser/semantic뿐 아니라 C/LLVM method lowering, 저장/복사 규칙에서도 분기되기 시작했다
- `subject slot`과 `ToObject` / `ToDto` projection source는 subject host (`subject`, `actor`)에 허용되고 bare `class`는 제외된다
- `actor`는 subject-profile semantic에 편입됐고 `subject Name actor { ... }` subject-first surface도 추가됐다
- standalone `actor Name { ... }`는 아직 허용되지만, semantic warning과 함께 transitional syntax로 취급된다
- plain/secure `Slot<subject>`와 `Slot<actor>`는 local object-cell anchor로 동작한다
- `own/ref Slot<subject-host>`와 `own/ref SecureSlot<subject-host>`는 semantic + C/LLVM backend에서 함수 경계 전달이 가능하다
- secure boundary slot은 함수 body 안에서 paired `s_token` 심볼을 자동 제공받는다
- 남은 공백은 deeper handle/runtime propagation model이다
- 클래식 OOP 계층(상속, super)은 미지원
- 패키지 매니저, WASM, 디버거 등 생태계 영역은 미완성
- backpressure는 관측 surface와 send result surface까지는 올라왔지만, bounded policy/backpressure protocol 자체는 아직 미완성
- cancellation은 cooperative + descendant propagation 수준, fairness는 round-robin 시작 인덱스 수준까지 올라온 상태

## 2026-04-04 기준 확인된 상태

- `make test-semantic` 통과 (`406 passed`)
- `make test-transpile` 통과 (`296 passed`)
- `make llvm-test-smoke` 통과

## 다음 기준

1. 문서와 구현 표면의 1:1 정렬
2. orchestration/slot 의미론 고정
3. stable stdlib surface 확정
4. toolchain 품질 개선 (LSP/formatter/debugger)

## 단계 결론

Pergyra는 “돌아가는 철학 실험”을 넘어, **자원 의미론을 가진 실행 가능한 언어 프로토타입**이다.  
다만 아직 베타 수준의 안정성과 생태계 준비가 필요하다.
