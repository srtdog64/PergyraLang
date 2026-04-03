# Pergyra 언어 비전

## 한 문장 정의

**Pergyra는 포인터를 숨기기 위한 언어가 아니라, 추적하기 어려운 자원을 슬롯 단위로 통제하기 위한 언어다.**

---

## 핵심 통찰

### 메모리는 자원의 한 종류일 뿐이다

전통적 시스템 언어(C, Rust)는 "메모리 주소"를 프로그래밍의 기본 단위로 삼는다.
포인터를 역참조하고, 주소를 알고, 읽어도 상태가 변하지 않는다는 전제 위에 서 있다.

이 전제는 클래식 컴퓨팅에서는 성립하지만, 영원하지 않다.

- 큐비트: 관측하면 상태가 붕괴한다. "들여다보는" 메모리 모델이 성립하지 않는다.
- 분산 자원: 네트워크 너머의 상태는 로컬 주소로 참조할 수 없다.
- 하드웨어 가속기: GPU/TPU/FPGA의 메모리는 호스트와 주소 공간이 다르다.

공통점: 자원이 거기 있다는 건 알지만, 주소로 직접 접근하는 것은 불가능하거나 위험하다.

### 세마포어/뮤텍스의 교훈

Dijkstra의 세마포어는 자원을 이렇게 다뤘다:

```text
자원이 어디 있는지는 모른다.
자원이 존재한다는 것만 안다.
P(acquire) - 자원을 점유한다.
V(release) - 자원을 반환한다.
```

세마포어는 자원의 주소를 묻지 않는다. 자원의 점유 상태만 추적한다.
이것이 동시성 문제를 풀 수 있었던 이유다.

Pergyra의 Slot은 같은 철학을 메모리/상태 관리에 적용한다:

```text
Slot이 어디 있는지는 모른다.
Slot이 존재한다는 것만 안다.
Claim   - 자원을 점유한다.
Read    - 자원의 현재 값을 얻는다.
Write   - 자원의 값을 변경한다.
Release - 자원을 반환한다.
```

현재 구현에서는 이 추상화가 한 타입으로 완전히 수렴한 것은 아니다.

- `Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`는 로컬에 고정된 anchored resource handle이다.
- `QubitSlot`은 복사 불가 move-only resource handle이다.
- 원격/지연 계산은 슬롯 자체를 직접 노출하기보다 `RemoteFuture<T>`를 `await`해 `Result<T>`로 회수한다.

---

## Slot = 자원 점유권

`Slot<T>`는 "T를 저장하는 메모리 위치"가 아니라 "T 타입 자원에 대한 점유권"이다.
현재 구현에서는 이 규율이 특히 로컬 anchored handle 계열(`Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`)에 직접 적용된다.

| 개념 | 포인터 모델 | Slot 모델 |
|------|-------------|-----------|
| 정체 | 메모리 주소 | 자원 핸들 |
| 읽기 | 주소 역참조 (`*ptr`) | 점유 상태에서 값 요청 (`Read(slot)`) |
| 쓰기 | 주소에 값 저장 (`*ptr = v`) | 점유 상태에서 값 변경 (`Write(slot, v)`) |
| 해제 | 주소 무효화 (`free(ptr)`) | 점유권 반환 (`Release(slot)`) |
| 복사 | 주소 복사 (별칭 발생) | 금지, 단일 소유권 |
| 전제 | 주소를 알고, 언제든 접근 가능 | 점유했을 때만 접근 가능 |

### SecureSlot<T> = 권한 기반 자원 접근

`SecureSlot<T>`는 점유권에 토큰(권한)을 추가한다:

```text
올바른 토큰 없이는 Read도 Write도 불가능하다.
```

이것은 단순한 접근 제어가 아니라, 능력(capability) 기반 보안 모델이다.
자원에 대한 접근은 주소를 아는 것이 아니라, 권한을 가진 것으로 결정된다.

### RemoteFuture<T> = 원격 작업 결과 경계

분산 자원이나 디바이스 작업은 로컬 `Slot<T>`처럼 즉시 `Read/Write`하지 않는다.
현재 구현에서는 `SubmitDeviceRead(slot)` 같은 연산이 `RemoteFuture<T>`를 만들고,
`await` 결과는 항상 `Result<T>`가 된다.

즉:

```text
로컬 Future<T>       -> await -> T
원격 RemoteFuture<T> -> await -> Result<T>
```

---

## 타입 관계의 아름다운 표현

복잡한 자원 관계를 명시적으로 모델링하는 것이 Pergyra의 두 번째 축이다.

```text
ability = 자원이 할 수 있는 것 (인터페이스)
role    = 자원이 실제로 하는 것 (구현)
party   = 자원들의 협력 단위 (합성)
world   = 자원 시스템의 경계 (격리)
```

이 계층은 "자원이 무엇이고, 무엇을 할 수 있고, 누구와 협력하고, 어디까지가 경계인가"를 문법으로 표현한다.

---

## 미래 확장: 큐비트와 자원 추적

큐비트 대응까지 생각하면 Slot 위에 추가로 필요한 것들:

| 속성 | 현재 Slot | 미래 확장 |
|------|-----------|-----------|
| 복사 금지 | `Claim/Release` 중심 | Linear/Affine 타입으로 강제 |
| 이동 중심 | Release 후 재Claim | Move 시맨틱 내장 |
| 관측 후 상태 변화 | 미지원 | `Measure(slot)` 이후 상태 붕괴 |
| 얽힘 (관계 추적) | Party로 합성 가능 | `Entangle(slotA, slotB)` 같은 관계 등록 |
| 권한 기반 접근 | `SecureSlot` 토큰 | Capability 타입 시스템 확장 |

확장 방향:

```pergyra
// 미래 - 양자 자원 슬롯
let q: QubitSlot = ClaimQubit();
let result: Bool = Measure(q);    // 관측 -> 상태 붕괴 -> 이후 Read 불가
// Read(q);  // 컴파일 에러: 측정된 큐비트는 읽을 수 없음

// 미래 - 얽힘
let a: QubitSlot = ClaimQubit();
let b: QubitSlot = ClaimQubit();
Entangle(a, b);                   // 관계 등록
let ra: Bool = Measure(a);        // a 측정 -> b의 상태도 결정됨
```

현재 구현 메모:

- `QubitSlot`은 실제 타입으로 다루되, 복사는 금지한다.
- `Entangle(a, b)`는 아직 완전한 양자 얽힘 시뮬레이터가 아니다.
- 현재 런타임은 두 큐비트가 같은 값으로 붕괴하는 simple same-value pair 모델만 제공한다.
- 목적은 양자 물리의 완전한 재현이 아니라, "주소가 아닌 자원 점유권" 모델이 이런 자원에도 적용될 수 있음을 보여주는 것이다.

---

## 요약

```text
Pergyra는:
  복잡한 타입 관계는 아름답게 표현하되,
  메모리 모델은 인간에게 덜 적대적인 언어.

그 근거는:
  자원은 주소가 아니라 점유권과 전송 경계로 다룬다.
  세마포어가 P/V로 자원을 추상화했듯이,
  로컬 anchored handle은 Claim/Read/Write/Release로,
  원격 작업은 Submit/Await/Result로 자원을 추상화한다.

그 미래는:
  클래식 메모리든, 큐비트든, 분산 자원이든,
  추적하기 어려운 자원은 모두 Slot으로 통제할 수 있다.
  포인터는 클래식 컴퓨팅의 유물이고,
  Slot은 자원 추상화의 시작이다.
```
