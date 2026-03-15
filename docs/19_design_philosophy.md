# Pergyra 설계 철학

## 1. 핵심 원칙: 컨테이너 격리 (Isolation by Container)

Pergyra의 정체성은 **"컨테이너로 상태를 격리하는 FP 사상을 시스템 프로그래밍에 도입하는 것"**이다.

전통적 동시성 프로그래밍에서 공유 상태 문제는 뮤텍스/세마포어라는 **저수준 동기화 프리미티브**에 의존한다. 이 접근은 다음 문제를 일으킨다:

- 데드락 (Deadlock)
- 레이스 컨디션 (Race condition)
- Lock 순서 역전 (Priority inversion)
- Lock granularity 실수 (Too coarse / too fine)

Pergyra는 이 문제를 **"공유하지 않음"**으로 해결한다. 뮤텍스를 더 잘 쓰는 게 아니라, **뮤텍스가 필요 없는 구조**를 문법 수준에서 강제한다.

## 2. 격리 계층 (Isolation Hierarchy)

```
┌─────────────────────────────────────────────┐
│  World                                       │
│  ┌─────────────────────────────────────────┐ │
│  │  Systemic                               │ │
│  │  ┌───────────────────────────────────┐  │ │
│  │  │  Party                            │  │ │
│  │  │  ┌─────────┐  ┌───────────────┐  │  │ │
│  │  │  │  Role    │  │  Shared Field │  │  │ │
│  │  │  │  (Slot)  │  │  (read-only)  │  │  │ │
│  │  │  └─────────┘  └───────────────┘  │  │ │
│  │  └───────────────────────────────────┘  │ │
│  └─────────────────────────────────────────┘ │
└─────────────────────────────────────────────┘
```

각 계층은 **합성 컨테이너(composition container)**이다:

| 계층 | 역할 | 격리 보장 |
|------|------|----------|
| `Slot<T>` | 단일 값 컨테이너 | 소유권 기반 — Claim/Read/Write/Release 생명주기 강제 |
| `SecureSlot<T>` | 보안 강화 슬롯 | 토큰 기반 접근 제어 — 올바른 토큰 없이 읽기/쓰기 불가 |
| `Channel<T>` | 통신 컨테이너 | CSP 스타일 — 송신자와 수신자가 메모리를 공유하지 않음 |
| `Actor` | 실행 컨테이너 | 메시지 패싱 — 내부 상태에 직접 접근 불가 |
| `Party` | 합성 컨테이너 | Role 슬롯 + 공유 필드를 하나의 단위로 캡슐화 |
| `World` | 최상위 컨테이너 | Systemic/Party를 묶어 전체 시스템의 경계를 정의 |

## 3. 키워드와 메커니즘의 관계

Pergyra의 키워드는 특정 도메인(RPG, 게임 등)의 용어가 아니라 **합성 패턴의 추상화**이다.

```
ability  = 인터페이스 정의 (vtable 타입)
           → Rust의 trait, Go의 interface

role     = 인터페이스 구현 (vtable 인스턴스)
           → Rust의 impl Trait for T

party    = 역할 합성 단위 (컴포넌트 컨테이너)
           → 여러 Role을 조합하여 하나의 엔티티를 구성

systemic = 시스템 수준 합성 (서비스 컨테이너)
           → Party들을 묶어 하나의 서브시스템을 구성

world    = 최상위 합성 (루트 컨테이너)
           → Systemic들을 묶어 전체 애플리케이션 경계를 정의
```

이 계층 구조의 핵심은: **각 수준에서 상태가 캡슐화되어 있으며, 상위 컨테이너는 하위 컨테이너의 내부 상태에 직접 접근하지 않는다.**

## 4. 뮤텍스/세마포어에 대한 입장

### 4.1 런타임 내부: 사용함

채널(Channel)과 태스크 풀(Task Pool)의 C 구현 내부에서는 `pthread_mutex`를 사용한다.
이것은 **구현 세부사항**이지 언어의 프로그래밍 모델이 아니다.

### 4.2 언어 문법: 의도적으로 노출하지 않음

`.pgy` 코드에서 `Mutex`나 `Semaphore` 키워드를 직접 사용할 수 없다.
이것은 미구현이 아니라 **설계적 결정**이다.

| 전통적 접근 | Pergyra 접근 |
|------------|-------------|
| `mutex.lock(); shared_data++; mutex.unlock();` | `Write(slot, Read(slot) + 1);` |
| `semaphore.acquire(); doWork(); semaphore.release();` | `channel.Send(task); result = channel.Recv();` |
| 공유 메모리 + 동기화 프리미티브 | 격리된 컨테이너 + 메시지 패싱 |

### 4.3 저수준 제어가 필요한 경우

시스템 프로그래밍에서 뮤텍스/세마포어가 필요한 상황은 존재한다.
Pergyra는 이를 `extern` 블록을 통한 C FFI로 해결한다:

```pergyra
extern "C" {
    func pthread_mutex_lock(mtx: Ptr) -> Int;
    func pthread_mutex_unlock(mtx: Ptr) -> Int;
}
```

이 접근은 **"안전한 기본값 + 명시적 탈출구"** 패턴이다:
- 기본: 컨테이너 격리 (안전)
- 탈출: extern C FFI (명시적으로 위험을 감수)

## 5. FP 사상의 도입 방식

Pergyra는 순수 함수형 언어가 아니다. **시스템 프로그래밍 언어에 FP의 핵심 통찰을 선택적으로 도입**한다.

도입한 FP 개념:

| FP 개념 | Pergyra 적용 |
|---------|-------------|
| 불변성 (Immutability) | `let` 바인딩 기본, `Slot`의 명시적 Write |
| 소유권 (Ownership) | `Slot`의 Claim/Release 생명주기 |
| 격리 (Isolation) | Actor 메시지 패싱, Party 캡슐화 |
| 합성 (Composition) | Role/Party/Systemic/World 계층 합성 |
| 명시적 부수효과 (Explicit Effects) | `Result<T,E>` 반환, 에러를 값으로 처리 |

도입하지 않은 FP 개념:

| FP 개념 | 미도입 이유 |
|---------|-----------|
| 모나드 (Monad) | 시스템 프로그래밍에서의 인지적 비용이 이점보다 큼 |
| 지연 평가 (Lazy Evaluation) | 예측 가능한 성능이 우선 |
| 패턴 매칭 (Pattern Matching) | 기본적인 match 문은 도입, 고급 패턴(guard, nested)은 제한적 |
| 고차 카인드 타입 (HKT) | 컴파일러 복잡도 대비 실용성 부족 |

## 6. 설계 결정 기록

### Q: 왜 Party인가?

"composite"나 "component"가 더 범용적인 이름이 아닌가?

**A:** Party는 "역할(Role)을 가진 참여자들의 모임"이라는 의미로, 합성의 **의도**를 드러낸다.
`composite`는 구조적 합성만을 암시하지만, `party`는 "각 구성원이 독립적 역할을 수행하며 협력한다"는 의미론적 뉘앙스를 가진다.
이는 단순한 데이터 합성이 아니라 **행위(behavior)의 합성**임을 강조한다.

### Q: 뮤텍스 없이 모든 동시성 문제를 해결할 수 있는가?

**A:** 아니다. 하지만 **대부분의 애플리케이션 수준 동시성 문제**는 컨테이너 격리로 해결 가능하다.
뮤텍스가 진정 필요한 저수준 시나리오(커널 드라이버, lock-free 자료구조 등)는
extern C FFI를 통해 접근할 수 있으며, 이는 의도적으로 "위험한 영역"임을 코드에서 명시하게 만든다.

### Q: 이 접근의 한계는?

- **성능 오버헤드**: 채널 기반 통신은 공유 메모리 + 뮤텍스보다 느릴 수 있다
- **학습 곡선**: 뮤텍스에 익숙한 개발자에게 패러다임 전환이 필요하다
- **표현력 제한**: 일부 복잡한 동기화 패턴(barrier, latch 등)을 직접 표현하기 어렵다

이러한 한계는 인지하고 있으며, extern C FFI를 통한 탈출구로 실용성을 보장한다.

## 7. 동적 디스패치: `dyn` 키워드

### 7.1 문제: 컴파일 타임 고정의 한계

기본 Party의 Role 슬롯은 컴파일 타임에 vtable이 고정된다.
이는 메모리 안전성과 성능 면에서 최적이지만, 런타임에 구현체를 교체해야 하는 시나리오에서는 제한적이다:

- 플러그인 아키텍처
- 전략 패턴 (Strategy Pattern)
- 동적 모듈 로딩
- 테스트에서의 모킹 (Mocking)

### 7.2 해결: `dyn` 수식어

`dyn` 키워드를 Role 슬롯에 붙이면 **런타임에 vtable을 교체할 수 있다.**

```pergyra
party Team {
    role slot tank: Combatable;           // 정적 — 컴파일 타임 고정
    dyn role slot fighter: Combatable;    // 동적 — 런타임 교체 가능
}
```

#### 코드젠 차이

```c
// 정적 슬롯: vtable이 한 번 설정되면 변경 불가
const Combatable_vtable *tank_Combatable_vt;

// dyn 슬롯: vtable 포인터가 교체 가능 + bind 헬퍼 생성
const Combatable_vtable *fighter_Combatable_vt; /* dyn */

// 자동 생성되는 bind 헬퍼
static inline void
Team_bind_fighter(Team *self, void *impl, const Combatable_vtable *vt) {
    self->fighter = impl;
    self->fighter_Combatable_vt = vt;
}
```

#### 사용 예시

```pergyra
// 런타임에 전략 교체
Team_bind_fighter(&team, &warrior_data, &Warrior_Combatable_vtable_instance);
// ... 나중에 ...
Team_bind_fighter(&team, &archer_data, &Archer_Combatable_vtable_instance);
```

### 7.3 설계 원칙

- **기본은 정적**: `dyn` 없이 선언하면 정적 디스패치 (성능 우선)
- **명시적 동적**: `dyn`을 붙여야만 동적 디스패치 (의도를 코드에 표현)
- **Ability 필수**: `dyn` 슬롯은 반드시 Ability 타입을 지정해야 한다 (vtable 없이 동적 디스패치 불가)

이 접근은 Rust의 `dyn Trait`과 동일한 철학이다:
정적 디스패치를 기본으로 하되, 필요할 때만 동적 디스패치의 비용을 감수한다.

## 8. 로드맵: `unsafe` 블록

### 8.1 현재 상태

저수준 메모리 제어가 필요한 경우 `extern "C"` FFI에 의존한다.
이 접근의 문제는: 언어 경계를 넘는 순간 Pergyra의 타입 시스템 보호가 완전히 사라진다.

### 8.2 계획: `unsafe` 블록 도입

```pergyra
// 미래 문법 (계획)
func fast_copy(dst: Ptr, src: Ptr, len: Int) -> Void {
    unsafe {
        // 이 블록 안에서만:
        // - 원시 포인터 역참조 허용
        // - 타입 캐스팅 허용
        // - 직접 메모리 조작 허용
        memcpy(dst, src, len);
    }
}
```

### 8.3 `unsafe`가 필요한 이유

| extern C FFI | unsafe 블록 |
|-------------|------------|
| 언어 경계를 넘음 — Pergyra 타입 시스템 무효화 | 같은 언어 안에서 동작 — 컴파일러가 위험 범위를 추적 |
| C 헤더 의존성 필요 | 자체 완결적 |
| 디버깅이 두 언어에 걸침 | 단일 디버거로 추적 가능 |
| 링킹 복잡도 증가 | 추가 링킹 불필요 |

### 8.4 우선순위

`unsafe`는 중요하지만 현 단계에서는 `dyn`과 핵심 기능 안정화가 우선이다.
도입 시점은 다음 조건이 충족된 이후:

1. 코어 타입 시스템 안정화 완료
2. 기본 최적화 패스 구현
3. 실제 프로젝트에서 extern C FFI의 한계가 반복적으로 보고될 때
