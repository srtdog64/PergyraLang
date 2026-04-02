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

### 3. 함수 인수: own / ref

```pergyra
func Upload(own tex: Slot<Texture>) -> Void {
    // tex의 소유권을 가져옴 — 호출자의 원본 무효
}

func Render(ref tex: Slot<Texture>) -> Void {
    // tex를 빌려봄 — 호출자의 원본 유효
}

let t: Slot<Texture> = ClaimSlot<Texture>();

Render(t);      // ref — t 여전히 유효
Upload(t);      // own — t 이후 무효
Log(Read(t));   // 컴파일 에러
```

### 4. Release는 항상 명시적

```pergyra
Release(s);     // 자원 반환 — 항상 소유자가 직접 호출
```

---

## 시그니처가 계약서다

| 수식자 | 의미 | 호출 후 원본 |
|--------|------|-------------|
| `own` | 소유권 이전 (move) | **무효** |
| `ref` | 빌려봄 (borrow) | 유효 |
| (없음) | 값 타입은 복사, Slot 타입은 move | 타입에 따라 |

```pergyra
// GPU 텍스처
func Upload(own tex: Slot<Texture>) -> Void { ... }   // GPU로 이전
func Render(ref tex: Slot<Texture>) -> Void { ... }   // 읽기만

// 네트워크 세션
func Send(ref session: Slot<Session>, data: String) -> Void { ... }
func Close(own session: Slot<Session>) -> Void { ... }  // 세션 종료

// 양자 큐비트
func Measure(own q: QubitSlot) -> Int { ... }           // 측정 후 소멸
func Inspect(ref q: QubitSlot) -> Int { ... }           // 상태 확인만

// 보안 토큰
func Validate(ref key: SecureSlot<Key>) -> Bool { ... }
func Revoke(own key: SecureSlot<Key>) -> Void { ... }   // 토큰 폐기
```

GPU 개발자든 네트워크 개발자든 함수 시그니처만 보면
**"이 자원이 어떻게 되는가"**를 알 수 있다.
숨겨진 복사도, 모르는 사이의 이동도 없다.

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

`own`/`ref`는 View 시스템의 함수 경계 확장이다.
`ref`로 받으면 내부적으로 ReadView처럼 동작하고,
`own`으로 받으면 MoveToken처럼 소유권이 이전된다.

---

## 왜 이렇게 하는가

Pergyra는 도메인 파편화를 줄이기 위해 만들어졌다.
C++에서는 `&&`, `const&`, `*`, `unique_ptr`, `shared_ptr`이 난립하고,
Go에서는 값 전달인지 포인터 전달인지 암묵적이고,
Python에서는 모든 게 참조인데 뭐가 공유되는지 안 보인다.

Pergyra는 **두 단어로 끝낸다: `own`과 `ref`.**

모든 도메인의 모든 자원에 대해, 이 두 단어가 전달 규칙을 완전히 설명한다.
