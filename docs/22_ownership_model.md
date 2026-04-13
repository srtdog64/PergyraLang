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

## 현재 구현에서 먼저 믿어도 되는 규칙

`own/ref`는 장기적으로는 더 넓은 ownership vocabulary를 지향하지만,
**현재 컴파일러에서 실사용 가능하게 닫힌 범위는 더 좁다.**

오늘 기준으로 안정적으로 믿어도 되는 표면은 다음이다.

- `ref Slot<subject-host>`
- `own SecureSlot<subject-host>`
- 이 둘의 helper forwarding / return-escape / channel-send escape 차단 규칙

즉 이 문서는 "장기 ownership 비전"보다 **현재 닫힌 boundary subset**을 우선 설명한다.

## 현재 closure 기준 판정

현재 stable surface에서 `own/ref`는 여전히 **일반 목적 ownership system**이 아니다.

현재 stable surface:

- `ref Slot<subject-host>`
- `own SecureSlot<subject-host>`

strict beta-quality closure track에서 다시 열리는 범위:

- 일반 값 타입 전반에 대한 ownership discipline
- `DeviceSlot<T>` / `QubitSlot` / arbitrary `Slot<T>` 전반에 대한 함수 경계 ownership
- region-based 또는 multi-level alias summary ownership model

즉 현재 문서 기준 `own/ref`는 아직 "완료된 범용 시스템"이 아니라
**anchored boundary ownership subset**으로 읽어야 한다.

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

### 3. 함수 인수: `own / ref`는 현재 anchored subject-slot boundary에 한정

다음 예시는 **현재 바로 믿어도 되는 구현 범위**로 바꿔 읽어야 한다.

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

현재 구현 상태는 이 규칙의 전체 일반화가 아니라 일부 닫힌 상태다.

- `QubitSlot` 같은 movable handle은 기존 move 규칙을 따른다 **(단, 전체 quantum resource semantics는 아직 v2 작업이고 현재는 partial surface/skeleton 상태다)**
- anchored handle 중에서는 현재 `Slot<subject-host>` / `SecureSlot<subject-host>`만 `own/ref` 함수 경계를 지원한다
- 여기서 `subject-host`는 `subject`를 뜻한다
- `Slot<Int>`, `SecureSlot<String>`, `DeviceSlot<T>` 같은 다른 anchored handle은 아직 local-only다
- 경계 전달 구현은 semantic + C backend + LLVM 경로까지 닫혔다
- 일반 anchored handle에 `own/ref`를 붙이면 컴파일러는 명시 오류를 낸다

### 4. Release는 항상 명시적

```pergyra
Release(s);     // 자원 반환 — 항상 소유자가 직접 호출
```

---

## 시그니처가 계약서다

| 수식자 | 의미 | 호출 후 원본 |
|--------|------|-------------|
| `own` | 현재는 `SecureSlot<subject-host>` 경계 이전에 대해 닫힘 | **무효** |
| `ref` | 현재는 `Slot<subject-host>` 경계 borrow에 대해 닫힘 | 유효 |
| (없음) | 일반 값/로컬 anchored handle 규칙 | 타입에 따라 |

```pergyra
// 현재 닫힌 boundary subset
func Borrow(ref session: Slot<Session>) -> Void { ... }
func Forward(own session: SecureSlot<Session>) -> Void { ... }
```

장기적으로는 더 넓은 자원 축으로 확장할 수 있지만,
**현재 문서/테스트/코드젠이 함께 닫힌 범위는 subject-host anchored slot boundary다.**

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

`own`/`ref`는 현재 View 시스템의 함수 경계 확장 중에서도
**subject-host anchored slot boundary에 대해 닫힌 subset**이다.

- `ref Slot<subject-host>`는 borrowed anchored handle처럼 동작한다
- `own SecureSlot<subject-host>`는 moved secure anchored handle처럼 동작한다
- return escape / channel send / rebinding / aliasing은 모두 보수적으로 차단된다
- 일반 `Slot<T>` 전반이나 `DeviceSlot<T>` / `QubitSlot` 전체에 대해 같은 규칙이 열린 것은 아니다

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

현재 구현 기준으로는 아직 그렇게 말하면 과장이다.

더 정확한 표현은 이렇다:

- Pergyra는 `own` / `ref`를 boundary vocabulary로 채택했다
- 하지만 현재 완전히 닫힌 규칙은 `Slot<subject-host>` / `SecureSlot<subject-host>` 경계 subset이다
- 나머지 자원 축은 동일 vocabulary로 넓혀 가는 중이다
