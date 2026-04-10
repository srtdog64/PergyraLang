# Pergyra JavaScript Backend Policy

마지막 업데이트: 2026-04-04

## 목적

JavaScript 백엔드는 브라우저와 Node.js 실행을 위한 타겟이다.
하지만 JS 생태계의 `class` / `extends` / 부모 호출 의미론이
Pergyra 코어 존재론을 오염시키면 안 된다.

따라서 JS 백엔드는 **언어 의미론을 JS에 맞추는 작업**이 아니라,
**Pergyra 의미론을 JS 위에 보존해서 내리는 작업**이어야 한다.

또한 멀티플랫폼 전략의 우선순위는 개별 플랫폼 언어 백엔드보다
**공통 UI IR**이 먼저다.
즉 Kotlin/Android 전용 백엔드를 서둘러 늘리기보다,
native / web / mobile이 함께 소비할 수 있는
scene/projection UI IR을 먼저 고정해야 한다.

## 고정 원칙

- 코어 언어는 inheritance를 기본 의미론으로 채택하지 않는다
- 코어 언어는 부모 호출 표면을 기본 표면으로 채택하지 않는다
- 재사용은 `ability`, `role`, composition, delegation을 우선한다
- JS backend는 필요하면 내부 lowering에서 delegation/mixin/object composition을 사용한다
- JS interop은 별도 `extern js` 계층으로 분리한다

즉 JS 백엔드가 필요하다는 이유로
Pergyra 코어에 `extends` / 부모 호출 / prototype-chain 중심 사고를 들이지 않는다.

## 존재론 대응

### struct

- plain value object
- shallow immutable-ish record lowering을 우선
- 필요하면 simple object literal 또는 helper constructor로 생성

### subject

- identity-bearing active host
- hidden self-cell / state cell을 가진 object로 lowering
- plain structural copy를 허용하지 않음
- method는 value-copy receiver가 아니라 identity cell 기반 dispatch로 본다

가능한 lowering 방향:

- closure-backed cell object
- hidden state slot을 가진 plain object
- generated factory + method table

핵심은 JS `class` 문법을 쓰느냐가 아니라,
**subject가 passive value가 아니라 identity-bearing host로 유지되느냐**다.

### class

- passive nominal object/value surface
- 필요하면 JS `class` 또는 plain factory/object로 lowering 가능
- 코어 의미론상 `subject`와 동일시하지 않는다

### object / tobject

- projection / transfer representation
- field-only surface를 우선
- serialization-friendly shape 유지

### participant

- 독립 ontological kind가 아니라 `subject` execution profile
- JS lowering에서는 subject + mailbox/scheduler wrapper
- event loop / microtask / runtime queue를 사용하더라도 본질은 execution model이다

## ability / role lowering

- `ability`는 JS interface 문법으로 직접 매핑하지 않는다
- `ability`는 contract metadata + dispatch table shape로 본다
- `role`은 mixin, delegated method bundle, generated vtable object 중 하나로 lower할 수 있다

권장 방향:

1. semantic/HIR에서는 지금처럼 `ability` / `role`을 분리 유지
2. JS lowering에서 role impl을 method bundle로 생성
3. subject/party/zone/world가 그 bundle을 참조하게 한다

## relation / effect / zone / world

JS 백엔드도 이 계층을 inheritance로 풀지 않는다.

- `relation` / `effect`는 overlay state object
- `zone`은 subject/object/tobject projection과 lifecycle state를 가진 coordinator object
- `world`는 zone registry + lifecycle orchestrator

즉 deeper runtime semantics는 JS에서도
prototype chain이 아니라 explicit state object와 sync step으로 푸는 것이 맞다.

## interop 정책

외부 JS 코드와 붙을 때만 별도 interop surface를 고려한다.

예시 방향:

```pergyra
extern js class HTMLElement;
extern js func setTimeout(cb: JsFn, ms: Int) -> JsHandle;
```

여기서의 `js class`는 Pergyra 코어 `class/subject` 존재론이 아니라
외부 런타임 타입을 가리키는 interop 전용 어휘여야 한다.

## 구현 순서

0. 공통 UI IR 우선
   - `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
   - `subject`는 직접 UI node가 아니라 projection source
   - `object` / `tobject` / projection surface가 UI 소비 표면
   - `zone` / `world` dirty sync가 UI 갱신 contract

1. 코어 의미론 유지
   - `subject != class`
   - inheritance / 부모 호출 미도입 유지

2. JS backend IR shape 고정
   - record
   - cell object
   - method bundle
   - async task / mailbox wrapper

3. 최소 lowering
   - `struct`
   - `class`
   - `subject`
   - `tobject`
   - basic function / method / projection

4. UI IR consumer로서 web surface 연결
   - scene/projection UI IR -> browser runtime
   - DOM / canvas / WebGL 중 구체 lowering 선택

5. orchestration/lifecycle
   - participant runtime
   - channel/future shim
   - zone/world sync semantics

6. interop
   - `extern js`
   - DOM / Node surface

7. mobile 재평가
   - Android/iOS는 우선 공통 UI IR consumer 또는 shell bridge로 접근
   - Kotlin backend는 web/native 경로가 부족하다고 확인된 뒤 별도 검토

## 결론

JS 백엔드는 필요하다.
하지만 그 이유로 코어 언어에 inheritance나 부모 호출 표면을 넣을 필요는 없다.

Pergyra는 계속 subject-first 언어로 남고,
JS는 그 의미론을 구현하는 한 타겟일 뿐이다.
