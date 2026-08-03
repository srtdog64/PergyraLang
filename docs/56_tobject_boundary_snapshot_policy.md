# `tobject` Boundary Contract and Snapshot Policy

## Relocation and cleanup non-goals

`tobject` is an immutable boundary-transfer snapshot whose lifecycle is
separate from its source. It is not a stable-address or immovable allocation,
not a pin lease, and not a guaranteed-finalizer handle. Crossing a boundary by
materializing a `tobject` may solve aliasing and lifetime coupling for that
payload, but it does not prove a Rust-style `!Move` or `!Forget` capability for
the source or destination type. Physical relocation and mandatory cleanup, if
added, require their own semantic capability facts under the ownership
authority; backend layout, `tobject` spelling, or transfer success cannot infer
them.

마지막 업데이트: 2026-07-29

## 2026-07-29 implementation audit

현재 구현은 `tobject`를 단순 `struct` 철자 별칭으로만 다루지는 않는다.

- native parser AST는 `NOMINAL_DECL_TOBJECT`를 별도 nominal identity로 보존한다.
- `ToTObject(Target, source)` semantic은 target이 exact `tobject` 선언인지,
  source가 `subject`인지, target field가 source에 유일하고 assignable하게
  존재하는지 검사한다.
- 직접 `tobject` member write는 immutable-transfer 진단으로 거부된다.
- typed intent semantic은 action outcome enum의 success/failure variant가 각각
  하나의 exact `tobject` payload를 가지는지 declaration/variant identity로
  봉인한다. MIR execution plan이 step, predecessor, completion, compensation,
  terminal control flow를 소유하고 `tobject`는 payload shape만 소유한다.
- self-host production code는 artifact commit receipt/failure와 direct-MIR
  execution receipt/rejection을 method-free data-only `tobject`로 실제 소비한다.

아직 완전히 닫히지 않은 구현 계약도 있다.

- parser/semantic은 호환 표면으로 `tobject` 안의 passive helper `func`를
  허용한다.
- declaration grammar는 bare field, `let mut`, default/destructure 표면을
  공통 value/projection parser에서 받는다. 단순 member assignment 거부만으로
  bare/nested/indexed mutation 전체가 fail-closed라고 주장할 수 없다.
- typed intent execution wire v2는 enum declaration뿐 아니라 success/failure와
  terminal의 payload `tobject` declaration stable ID도 운반하고, name/kind/ID를
  함께 cross-seal한다. v1의 name-only join은 호환 fallback 없이 제거했다.
- 따라서 현재 등급은 detached transfer payload로는 `REACHABLE`이지만,
  canonical method-free/fully immutable nominal 전체가 닫힌 것은 아니다.

이 debt를 해결할 때도 intent 실행 topology나 authority를 `tobject`로 옮기지
않는다. 별도 semantic closure에서 method-free declaration, 모든 write shape,
constructor/materialization 허용 범위를 함께 고정하고 negative gate를 둔다.

여기서 `intent`를 단순히 "여러 action을 호출하는 함수"로 축약하지 않는다.
`docs/01_intent_first_design.md`의 최상위 목적 축과
`docs/173_intent_axis_strengthening.md`의 정본에 따라, intent는 목적을 닫고 다른
언어 경계를 유도하는 source-level cross-axis binder다. Verification plane에서는
하나의 거대한 intent fact가 아니라 participant, coordination, boundary,
authority, effect, compensation, trace owner fact로 전개된다. `tobject`는 그중
boundary를 통과하는 값일 수 있지만 다른 fact의 owner가 아니다.

### 구현 closure 전략

`tobject` 강화는 parser, intent, backend가 각각 별도 규칙을 갖는 방식으로 하지
않는다. semantic nominal contract 한 곳이 다음 사실을 소유하고 나머지는 그
판정만 소비한다.

- declaration identity가 exact `tobject`인가;
- method/action/slot/authority를 갖지 않는 data-only nominal인가;
- 모든 field가 construction 뒤 immutable이며 nested/indexed/alias/inout write도
  같은 계약으로 거부되는가;
- field type이 경계를 넘길 수 있는 owned value인가. `subject`, `zone`, borrowed
  reference, authority handle, growable storage의 raw alias는 허용하지 않는다;
- 생성은 explicit literal/move 또는 `ToTObject(exact_target, subject_source)`
  materialization인가;
- enum payload가 이 declaration identity를 정확히 한 번 참조하는가.

마지막 소비자는 `ToTObject` materializer, publish/transport, action/intent enum
payload emitter다. 이 소비자들이 nominal spelling이나 `struct-like` shape로 같은
규칙을 재구성하면 안 된다. Backend는 semantic-admitted contract를 투영하고,
MIR은 stable declaration identity만 운반한다.

최소 negative gate는 method, `let mut`/bare mutable field, destructured/default
mutable storage, direct/nested/indexed/alias write, non-transfer-safe field type,
`object`/`struct` payload 대체, enum zero/multiple payload, 같은 이름의 foreign
declaration ID를 각각 거부해야 한다. Positive gate는
`subject -> ToTObject -> tobject slot -> publish`와
`action -> enum<tobject> -> intent terminal -> receipt/failure` 두 실행 경로를
같은 semantic contract로 통과시킨다.

## 한 줄 결론

- `tobject`는 경계를 넘는 **전송 계약**이다.
- zero-copy 관측은 `tobject`가 아니라 **snapshot ticket / generation / lease** 계층에서 다뤄야 한다.
- cross-domain plain borrow는 금지한다.

## 1. 왜 분리해야 하는가

설비 제어에서 다음 두 요구가 동시에 있다.

- 도메인 경계를 강하게 닫아야 한다.
- 큰 관측 데이터는 복사 비용을 줄여야 한다.

이 둘을 하나의 타입 개념으로 섞으면 의미론이 무너진다.

- `tobject`를 borrow처럼 쓰면 경계 계약이 시간적 결합으로 바뀐다.
- zero-copy 관측을 전송 객체에 얹으면 revoke, stale read, blocking policy가 불분명해진다.

따라서 Pergyra는 아래처럼 나눠야 한다.

## 2. 역할 분리

### `object`

- 동일 실행 경계 내부의 local projection/view
- read-mostly, borrow-first lowering 가능
- boundary transfer contract가 아니다

### `tobject`

- zone/world/API/transport 경계를 넘는 immutable transfer snapshot
- 기본 정책은 materialized transfer
- authority/action/slot ownership을 담지 않는다
- canonical 새 코드는 method-free다. 현재 parser/semantic이 허용하는 passive
  helper `func`는 호환 표면이며 transfer 의미나 authority를 추가하지 않는다

### telemetry snapshot

- 고성능 관측을 위한 별도 계약
- generation id, lease TTL, snapshot ticket 같은 메타를 가진다
- stale 감지와 timeout revoke를 지원한다
- `tobject`와 같은 층이 아니다

## 3. 기본 정책

### 3.1 `tobject`

- immutable
- authority-free
- canonical authoring은 method-free
- slot-free
- source lifecycle와 분리
- boundary crossing 가능

현재 구현은 `object`와 `tobject` 선언 안의 passive helper `func`를 받아들인다.
이 허용을 snapshot의 행위·identity·권한 소유로 해석하면 안 된다. 새 코드는 계산을
free `func` 또는 별도 수동 도구로 두고, `tobject`에는 전송할 immutable field만 둔다.
기존 helper 허용을 닫으려면 parser/semantic/codegen parity와 migration 진단을 같은
rung에서 바꾸며, 문서만 앞서서 현재 동작을 부정하지 않는다.

즉 `tobject`는 "borrowed read view"가 아니라 "전송용 스냅샷"이다.

### 3.2 zero-copy 관측

다음은 `tobject`가 아니라 telemetry/snapshot 정책이다.

- double buffering
- ring buffer + generation
- RCU/read-mostly snapshot
- lease + TTL

코어 제어 도메인은 외부 reader를 기다리지 않는다.

## 4. 금지하는 것

다음 모델은 설비 제어 코어에서 금지한다.

- plain lock-based cross-domain borrow
- reader 반환을 기다리는 write-side blocking
- dangling pointer를 만드는 강제 revoke
- `tobject`를 shared read lock처럼 해석하는 것

이 방식은 UI/모니터링/네트워크 지연이 코어 제어를 멈추게 만든다.

## 5. revoke 정책

revoke는 포인터 박탈이 아니라 **토큰 무효화**여야 한다.

권장 정책:

- generation mismatch면 read 포기
- lease TTL이 지나면 snapshot expired
- stale reader는 `Result<Failure>`로 격리
- 코어 writer는 다음 generation으로 진행

즉 원소유자는 reader를 기다리지 않고 계속 진행한다.

## 6. 데이터 종류별 권장 전략

### command

- 작고 치명적
- deep copy 또는 move
- 예: 정지, 승인, 취소, release

### transfer snapshot

- `tobject`
- immutable boundary DTO
- source와 lifecycle 분리

### telemetry stream

- large/read-heavy
- generation/lease/ticket 기반
- zero-copy 가능
- `tobject`가 아님

## 7. 권장 예시

### 7.1 command

```pergyra
tobject StopMotorCommand {
    motor_id: Int;
    reason: String;
}
```

- 작다
- 치명적이다
- move 또는 materialized transfer가 맞다

### 7.2 boundary transfer snapshot

```pergyra
object FurnaceView {
    temperature: Int;
    pressure: Int;
}

tobject FurnaceReport {
    temperature: Int;
    pressure: Int;
    tick: Int;
    emitted_at: Int;
}
```

- `FurnaceView`는 내부 조회용이다
- `FurnaceReport`는 외부 API/IPC/reporting용이다
- 둘은 같은 원본에서 나오더라도 같은 계약이 아니다

### 7.3 telemetry snapshot

개념적으로는 아래 계층이다.

```pergyra
struct SnapshotTicket {
    generation: Int;
    lease_ms: Int;
    buffer_index: Int;
}
```

- 큰 센서 배열
- 이미지 프레임
- read-heavy 상태 덤프

같은 데이터는 `tobject`로 통째로 복사하기보다
snapshot ticket으로 읽고,
generation mismatch면 다시 요청한다.

## 8. 금지 예시

### 8.1 잘못된 해석

```pergyra
tobject CameraFrame {
    pixels: Slice<Byte>;
}
```

이 타입을 "상대 도메인이 원본 프레임 버퍼를 borrow한다"는 뜻으로 해석하면 안 된다.

이건 두 가지를 섞는다.

- `tobject` = boundary transfer
- frame buffer borrow = telemetry snapshot

이런 표면은 reader 반환 지연이 writer를 막거나, revoke 시 dangling reference를 만들 위험이 있다.

### 8.2 올바른 분리

- 경계를 넘는 메타/리포트는 `tobject`
- 큰 프레임 버퍼 접근은 snapshot ticket
- stale read는 generation mismatch로 처리

## 9. 설계 체크리스트

새 표면을 만들 때는 먼저 이 셋을 확인한다.

1. 경계를 넘는가
2. source lifecycle과 분리돼야 하는가
3. reader를 기다리지 않고 writer가 계속 가야 하는가

판정:

- 1, 2가 예면 `tobject`
- 3이 예면 telemetry snapshot 계층
- 같은 경계 안의 조회면 `object`

## 10. 현재 언어 설계 기준

현재 Pergyra에서 고정해야 하는 의미론:

- `object`: local/internal passive projection
- `tobject`: boundary transfer/publish contract
- `slot`: ownership/capability/lifecycle
- `intent`: 현실 목적을 닫고 검증 평면의 cross-axis fact를 귀속하는 source-level binder
- `parallel`: execution contract

향후 필요하면 telemetry snapshot은 별도 표면으로 추가한다.
하지만 그것을 `tobject` 의미론 안에 섞지 않는다.

Self-host에서도 같은 계약은 `tobject` spelling 하나로 성립하지 않는다. 최소
실행 패턴은 `subject source -> tobject slot -> publish -> transfer/receipt`이며,
`TObjectSlot` identity와 `publish` operation, source/target member assignment가 typed
fact로 끝까지 운반돼야 한다. `HasProjection`/`HasZoneProjection`의 tobject 인자는
일반 값 변수가 아니라 이 선언 identity를 가리키는 symbolic name이다.

## 11. 설계 원칙

언어가 어려워져도 되는 조건은 하나다.

- 존재론과 계약이 섞이지 않을 것

즉:

- `tobject`는 transfer
- snapshot ticket은 telemetry
- `object`는 local view

이 경계만 유지되면 복잡성은 통제 가능하다.
