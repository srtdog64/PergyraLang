# Slot 관계 모델 — 포인터를 넘어서 (TODO)

> **상태**: 설계 구상 단계 (미구현)
> **선행 조건**: Slot 접근 모드 시스템 (`ReadView`, `WriteView`, `MoveToken`) 완료

---

## 문제 인식

현재 Slot은 **소유권과 접근 권한**을 타입에 각인하는 데 성공했다:

| 타입 | 의미 |
|------|------|
| `Slot<T>` | 소유 셀 — 모든 권한 |
| `ReadView<T>` | 읽기만 가능 |
| `WriteView<T>` | 쓰기만 가능 |
| `MoveToken<T>` | 소유권 이전 |

하지만 **셀 간의 관계**(연결 리스트의 "다음", 트리의 "자식")는 여전히 정수 인덱스로 표현된다:

```pergyra
// 현재 — 인덱스 기반 (사실상 포인터)
let nexts: Array<Int> = [];
cur = nexts[cur];           // *(base + offset) 의 설탕
```

이것은 `int *p`와 본질적으로 같다. 관계에 대한 타입 안전성이 없고,
댕글링(무효 인덱스)을 컴파일 타임에 방지할 수 없다.

---

## 목표

**관계(link) 자체를 일급 시민**으로 만들어, 포인터의 늪에서 벗어난다.

핵심 통찰: QubitSlot의 얽힘 모델이 이미 관계의 프로토타입이다.
`Entangle(a, b)` → 풀에 등록 → `Measure(a)` 시 `b`도 붕괴.
이것을 일반화한다.

---

## 설계 구상

### 1. 관계 선언 (Link Declaration)

```pergyra
link next: Node -> Node;            // "다음" 관계 — 단방향
link parent: Node -> Node;          // "부모" 관계
link children: Node -> [Node];      // "자식들" 관계 — 1:N
```

관계는 **타입 수준에서 선언**되며, 어떤 셀이 어떤 셀과 연결될 수 있는지 제한한다.

### 2. 관계 조작 (Link Operations)

```pergyra
let a: Node = ClaimNode();
let b: Node = ClaimNode();

Connect(a, next, b);                // a → b 연결 설정
let target: Node = Follow(a, next); // 관계를 따라 이동
Disconnect(a, next);                // 연결 해제
```

- `Connect`는 관계 그래프에 간선을 추가
- `Follow`는 관계를 따라 대상 셀을 반환
- `Disconnect`는 간선을 제거

### 3. 자동 무효화 (Cascading Invalidation)

```pergyra
Release(b);
// b를 가리키는 모든 link가 자동 무효화
// Follow(a, next)는 컴파일 타임 경고 또는 런타임 안전 반환
```

`Release(b)` 시 `b`를 참조하는 모든 관계가 자동으로 끊어진다.
이것은 QubitSlot의 `ReleaseQubit`이 풀에서 제거하는 것과 동일한 패턴이다.

### 4. 관계에 권한 부여 (Link Access Modes)

```pergyra
link next: Node -> Node [ReadOnly];   // 따라갈 수 있지만 변경 불가
link next: Node -> Node [OneWay];     // a→b는 가능, b→a는 불가
link next: Node -> Node [Exclusive];  // 하나의 incoming만 허용
```

### 5. 포인터와의 대비

| 포인터 | Slot 관계 모델 |
|--------|---------------|
| `int *p = &x` | `Connect(a, link, b)` |
| `*p` (역참조) | `Follow(a, link)` |
| `p = NULL` | `Disconnect(a, link)` |
| 댕글링 가능 | Release 시 자동 무효화 |
| 아무거나 가리킴 | 타입이 허용한 관계만 |
| 권한 없음 | ReadOnly / OneWay / Exclusive |

---

## 구현 로드맵

1. **컴파일 타임 관계 그래프** — 시맨틱 분석기에 link 추적 추가 (얽힘 풀 모델 확장)
2. **런타임 관계 레지스트리** — 노드 풀 + 관계 테이블 (현재 배열 인덱스의 체계화)
3. **자동 무효화** — Release 시 관계 테이블 스캔 + 제거
4. **파서** — `link` 키워드, `Connect`, `Follow`, `Disconnect` 구문 추가
5. **코드젠** — C/LLVM 백엔드에서 관계 테이블을 배열 기반으로 구현

---

## 기존 시스템과의 관계

| 기존 기능 | 관계 모델에서의 역할 |
|----------|-------------------|
| 얽힘 풀 (`PgyEntanglementPool`) | 관계 그래프의 프로토타입 |
| `Entangle(a, b)` | `Connect(a, entangled, b)`의 특수 사례 |
| `Measure()` 전파 | `Follow` + cascading state change |
| `Party { role A; role B }` | 다자 관계 컨테이너 |
| `SecureSlot + Token` | 관계에 접근 제어 부여의 선례 |

---

## 왜 이것이 필요한가

Slot의 사명은 "점유·권한·수명·보호 규칙이 부착된 자원 셀"이다.
소유권과 접근 모드는 달성했다. 하지만 **셀 간의 관계**가 정수 인덱스로 표현되는 한,
자료구조는 결국 `*(base + offset)`의 설탕에 불과하다.

관계를 타입으로 만들면, Pergyra는 "포인터 없는 시스템 언어"가 아니라
**"관계가 일급 시민인 자원 언어"**가 된다.

---

# Slot Relation Model — Beyond Pointers (TODO)

> **Status**: Design concept phase (not implemented)
> **Prerequisites**: Slot access mode system (`ReadView`, `WriteView`, `MoveToken`) completed

---

## Problem Statement

The current Slot system successfully encodes **ownership and access permissions** into the type:

| Type | Semantics |
|------|-----------|
| `Slot<T>` | Owning cell — full permissions |
| `ReadView<T>` | Read-only access |
| `WriteView<T>` | Write-only access |
| `MoveToken<T>` | Ownership transfer |

However, **relationships between cells** (linked list "next", tree "child") are still
expressed as integer indices:

```pergyra
// Current — index-based (effectively pointers)
let nexts: Array<Int> = [];
cur = nexts[cur];           // Sugar for *(base + offset)
```

This is fundamentally the same as `int *p`. There is no type safety over relationships,
and dangling references (invalid indices) cannot be prevented at compile time.

---

## Goal

Make **relationships (links) first-class citizens**, escaping the pointer trap.

Key insight: QubitSlot's entanglement model is already a prototype of relationships.
`Entangle(a, b)` → registers in pool → `Measure(a)` collapses `b` too.
Generalize this.

---

## Design Concept

### 1. Link Declaration

```pergyra
link next: Node -> Node;            // "next" — unidirectional
link parent: Node -> Node;          // "parent" relation
link children: Node -> [Node];      // "children" — 1:N
```

Links are **declared at the type level**, restricting which cells can connect to which.

### 2. Link Operations

```pergyra
let a: Node = ClaimNode();
let b: Node = ClaimNode();

Connect(a, next, b);                // Establish a → b
let target: Node = Follow(a, next); // Traverse the link
Disconnect(a, next);                // Remove the link
```

- `Connect` adds an edge to the relation graph
- `Follow` traverses the link to the target cell
- `Disconnect` removes the edge

### 3. Cascading Invalidation

```pergyra
Release(b);
// All links pointing to b are automatically invalidated
// Follow(a, next) → compile-time warning or safe runtime return
```

When `Release(b)` is called, all relationships referencing `b` are automatically severed.
This mirrors `ReleaseQubit` removing qubits from their entanglement pool.

### 4. Link Access Modes

```pergyra
link next: Node -> Node [ReadOnly];   // Can follow but not modify
link next: Node -> Node [OneWay];     // a→b allowed, b→a not
link next: Node -> Node [Exclusive];  // Only one incoming link allowed
```

### 5. Comparison with Pointers

| Pointer | Slot Relation Model |
|---------|---------------------|
| `int *p = &x` | `Connect(a, link, b)` |
| `*p` (dereference) | `Follow(a, link)` |
| `p = NULL` | `Disconnect(a, link)` |
| Dangling possible | Auto-invalidation on Release |
| Points to anything | Only type-permitted relations |
| No permissions | ReadOnly / OneWay / Exclusive |

---

## Implementation Roadmap

1. **Compile-time relation graph** — Extend semantic analyzer with link tracking (generalize entanglement pool model)
2. **Runtime relation registry** — Node pool + relation table (formalize current array-index pattern)
3. **Cascading invalidation** — On Release, scan relation table and sever links
4. **Parser** — Add `link` keyword, `Connect`, `Follow`, `Disconnect` syntax
5. **Codegen** — C/LLVM backends implement relation tables as array-based structures

---

## Relationship to Existing Systems

| Existing Feature | Role in Relation Model |
|------------------|----------------------|
| Entanglement pool (`PgyEntanglementPool`) | Prototype of the relation graph |
| `Entangle(a, b)` | Special case of `Connect(a, entangled, b)` |
| `Measure()` propagation | `Follow` + cascading state change |
| `Party { role A; role B }` | Multi-party relationship container |
| `SecureSlot + Token` | Precedent for access control on relations |

---

## Why This Matters

Slot's mission is "a resource cell with occupancy, permission, lifetime, and protection rules attached."
Ownership and access modes are achieved. But as long as **inter-cell relationships**
are expressed as integer indices, data structures remain sugar for `*(base + offset)`.

Making relationships a type turns Pergyra from "a systems language without pointers"
into **"a resource language where relationships are first-class citizens."**
