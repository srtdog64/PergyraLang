# Pergyra 소유권 모델 — own / ref / Clone

> 도메인 파편화를 막기 위해, 자원이 어떻게 전달되는지를 **항상 명시**한다.

---

## 원칙

Pergyra는 이종 자원(메모리, GPU, 네트워크, 양자)을 같은 Slot 모델로 다룬다.
서로 다른 자원의 "전달"이 숨겨지면 파편화가 시작된다.

따라서:

- **이동(move)과 복사(clone)는 모두 가능하지만, 어떤 것인지 항상 보여야 한다.**
- 함수 시그니처가 곧 자원 계약서다.

---

## 베타 stable ownership surface

`own/ref`는 이제 anchored-only 실험 표면이 아니다. 현재 컴파일러에서
닫힌 기준은 **ownership classifier 기반 stable subset**이다.

stable subset:

- copy-only 값: `ref Int`, `own Int`, enum 등은 legal surface이며 ordinary value passing과 같은 trivial semantics를 가진다
- boundary-visible aggregate: tuple / class / object / subject 계열 borrowed value는 escape/rebind/store/return/send/helper 경계에서 추적된다
- movable resource value: `QubitSlot`처럼 이미 move-only 의미가 있는 값은 `own` transfer와 `ref` borrow boundary를 가진다
- slot handle boundary: `ref Slot<subject-host>`, `own SecureSlot<subject-host>`, explicit `own` anchored-handle transfer boundary
- nested projection provenance: `holder.packet`, `cargo.wrapper.packet`, `items[0]` 같은 source path가 진단에 남는다
- consumer coverage: new binding, destructure binding, assignment/member rebind, constructor field store, container store, array literal/store, return, channel send, helper/function call, transitive helper chain

explicit reject:

- authority-bearing `Token<T>` escape/transport
- current classifier/summary model 밖의 arbitrary universal ownership lattice

beta-out-of-scope:

- region/lifetime solver
- arbitrary alias graph solver
- v2 quantum resource model

## 기본 규칙

### 1. 대입은 move

```pergyra
let a: Slot<Int> = 42;
let b: Slot<Int> = a;      // move — a는 이후 무효
Log(Read(a));               // 컴파일 에러: "a는 이동되었습니다"
```

Slot은 "점유"다. 하나의 자원에 소유자가 둘이면 점유가 아니다.

### 2. 복사는 명시적 Clone

```pergyra
let a: Slot<Int> = 42;
let b: Slot<Int> = Clone(a);  // 새 Slot에 값 복사, a 여전히 유효
```

### 3. 함수 인수: `own / ref`는 classifier 기준으로 해석된다

```pergyra
subject Session {
    let id: Int;
}

func Inspect(ref s: Slot<Session>) -> Void {
    Log(Read(s).id);
}

func Revoke(own s: SecureSlot<Session>) -> Void {
    Release(s);
}

let live: Slot<Session> = ClaimSlot<Session>();
let secured: SecureSlot<Session> = ClaimSecureSlot<Session>();

Inspect(live);       // ref boundary
Revoke(secured);     // own boundary
```

구현 규칙:

- copy-only type의 `own/ref`는 허용되며 borrow tracking을 만들지 않는다
- subject / class / object / tuple / boundary-visible aggregate는 borrowed escape를 추적한다
- movable resource value는 explicit `own` transfer 또는 `ref` borrow로 처리된다
- anchored slot handle은 subject-host boundary와 explicit transfer boundary에서 닫힌다
- `Token<T>`는 authority-bearing value이므로 transport/escape는 explicit reject다
- 경계 전달 구현은 semantic + C backend + LLVM 경로까지 닫혔다

### 4. Release는 항상 명시적

```pergyra
Release(s);     // 자원 반환 — 항상 소유자가 직접 호출
```

---

## 시그니처가 계약서다

| 수식자 | 의미 | 호출 후 원본 |
|--------|------|-------------|
| `own` | copy-only trivial pass 또는 ownership transfer boundary | copy-only는 유효, move/handle/subject identity는 **무효** |
| `ref` | non-owning borrow boundary | 유효. 단 borrow-tracked 값은 escape/rebind/store/send/return 차단 |
| (없음) | 일반 값/로컬 anchored handle 규칙 | 타입에 따라 |

```pergyra
// 현재 닫힌 boundary subset
func Borrow(ref session: Slot<Session>) -> Void { ... }
func Forward(own session: SecureSlot<Session>) -> Void { ... }
```

현재 문서/테스트/코드젠이 함께 닫힌 범위는 `copy-only`,
`borrow-tracked`, `move-only`, `subject identity`, `slot handle
(anchored)` classifier branch다.

---

## 기존 Slot 체계와의 관계

| 기능 | 역할 |
|------|------|
| `Slot<T>` | 소유 셀 |
| `ReadView<T>` | 읽기 전용 빌림 |
| `WriteView<T>` | 쓰기 전용 빌림 |
| `MoveToken<T>` | 소유권 이전 토큰 |
| **`own` 파라미터** | 함수 경계에서의 move |
| **`ref` 파라미터** | 함수 경계에서의 borrow |
| **`Clone()`** | 명시적 값 복사 |

`own`/`ref`는 View 시스템보다 넓은 함수 경계 ownership vocabulary다.

- `ref Slot<subject-host>`는 borrowed anchored handle처럼 동작한다
- `own SecureSlot<subject-host>`는 moved secure anchored handle처럼 동작한다
- return escape / channel send / rebinding / aliasing은 모두 보수적으로 차단된다
- copy-only 값은 legal no-op ownership surface다
- `QubitSlot`은 current move-only resource subset에 포함된다. 전체 quantum resource model은 v2/beta-out-of-scope다
- `Token<T>`는 authority-bearing explicit reject다

---

## World 생성과 Zone 값 복사

`World(...)`에 기존 zone binding을 바로 넣는 표면은 허용되지만,
현재 의미론은 참조 공유가 아니라 값 복사에 가깝다.

```pgy
let zone = ConnectionZone(...);
let world = PacketWorld(zone);
```

이 경우 `zone`과 `world` 내부 zone slot은 같은 핸들을 공유하는 것으로
자동 연결되지 않는다. 그래서 컴파일러는 direct binding world-embedding에
경고를 내고, 복사 의도를 surface에서 보이라고 요구한다.

권장 표면:

```pgy
let world = PacketWorld(Clone(zone));
```

또는:

```pgy
let world = PacketWorld(ConnectionZone(...));
```

핵심 규칙:

- `World(zone)`는 hidden alias가 아니다
- world 내부 zone 값과 바깥 binding은 자동 동기화되지 않는다
- 복사를 의도했다면 `Clone(...)`으로 드러내는 편이 맞다
- hidden control flow나 hidden reference semantics를 추가하지 않는다

---

## 왜 이렇게 하는가

Pergyra는 도메인 파편화를 줄이기 위해 만들어졌다.
C++에서는 `&&`, `const&`, `*`, `unique_ptr`, `shared_ptr`이 난립하고,
Go에서는 값 전달인지 포인터 전달인지 암묵적이고,
Python에서는 모든 게 참조인데 뭐가 공유되는지 안 보인다.

현재 베타 기준의 정확한 표현은 이렇다:

- Pergyra는 `own` / `ref`를 general boundary vocabulary로 채택했다
- compiler는 ownership classifier로 copy-only / borrow-tracked / move-only / subject identity / slot handle branch를 분류한다
- parser가 받는 current stable `own/ref` surface는 semantic diagnostics와 C/LLVM parity 기준으로 닫혀 있다
- region/lifetime solver와 arbitrary universal ownership lattice는 beta-out-of-scope다
