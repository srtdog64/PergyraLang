# LLVM 백엔드 커버리지 현황

> 작성일: 2026-04-07  
> 기준: AST 노드 88개, 키워드 77개

## 아키텍처 개요

```
[Pergyra 소스] → AST → HIR(CFG) → MIR(SSA) → LLVM IR → 네이티브 바이너리
                                                  ↘ 링크 ← pgy_runtime_lib.c
```

| 파일 | 역할 |
|------|------|
| `llvm_backend.c` | 프로그램 구조, 런타임 선언, MIR→LLVM 메인 루프 |
| `llvm_expr.c` | 표현식 코드 생성 (23개 AST 타입) |
| `llvm_stmt.c` | 문(statement) 코드 생성 (35+개 AST 타입) |
| `llvm_decl.c` | 함수 선언/정의 |
| `llvm_domain.c` | 도메인 구조체(Party/World/Zone 등) |

---

## 런타임 전략 (3계층)

| 계층 | 설명 | 대상 |
|------|------|------|
| **LLVM 인라인** | 백엔드가 직접 IR 생성 | 산술, 비교, 배열 인덱싱, 분기 |
| **C 런타임** | `pgy_runtime_lib.c` → 링크 | I/O, 스레드풀, 채널, Slot, 메모리 |
| **Pergyra stdlib** (미래) | `.pgy`로 작성 → 같은 파이프라인 | 고수준 유틸리티, 도메인 헬퍼 |

별도 백엔드 불필요 — C 런타임은 `declare + call` 패턴으로 자연 통합.

---

## 구현 완료 (Implemented)

### 표현식 (llvm_expr.c)

| AST 타입 | 설명 | 상태 |
|----------|------|------|
| `AST_NUMBER` | 숫자 리터럴 | OK |
| `AST_STRING` | 문자열 리터럴 | OK |
| `AST_BOOLEAN` | 불리언 리터럴 | OK |
| `AST_IDENTIFIER` | 변수 참조 | OK |
| `AST_BINARY` | 이항 연산 (+, -, *, /, %, 비교, 논리, 비트) | OK |
| `AST_UNARY` | 단항 연산 (-, !, ?) | OK |
| `AST_CALL` | 함수 호출 (일반 + 메서드 + 연산자 오버로드) | OK |
| `AST_ASSIGNMENT` | 대입 | OK |
| `AST_MEMBER_ACCESS` | 멤버 접근 (dot notation) | OK |
| `AST_ARRAY_LITERAL` | 배열 리터럴 `[...]` | OK |
| `AST_ARRAY_ACCESS` | 배열 인덱싱 `arr[i]` | OK |
| `AST_CONTEXT_ACCESS` | 컨텍스트 접근 (context.X) | OK |
| `AST_PARTY_INSTANCE` | 파티 인스턴스 생성 | OK |
| `AST_TASK_GROUP` | 태스크 그룹 | OK |
| `AST_CHANNEL_SEND` | 채널 송신 (`ch <- val`) | OK |
| `AST_CHANNEL_RECV` | 채널 수신 (`<- ch`) | OK |
| `AST_SPAWN_EXPR` | spawn 표현식 | OK |
| `AST_AWAIT_EXPR` | await 표현식 | OK |
| `AST_LAMBDA_EXPR` | 람다 표현식 | OK |
| `AST_EVENT_SUBSCRIBE` | 이벤트 구독 (`+=`) | OK |
| `AST_EVENT_UNSUBSCRIBE` | 이벤트 구독 해제 (`-=`) | OK |
| `AST_EVENT_INVOKE` | 이벤트 발화 (Emit) | OK |

### 문 (llvm_stmt.c)

| AST 타입 | 설명 | 상태 |
|----------|------|------|
| `AST_LET_DECL` | 변수 선언 | OK |
| `AST_RETURN` | 반환 | OK |
| `AST_BREAK` | 루프 탈출 | OK |
| `AST_CONTINUE` | 루프 계속 | OK |
| `AST_IF_STMT` | 조건문 | OK |
| `AST_WHILE_LOOP` | while 루프 | OK |
| `AST_FOR_LOOP` | for 루프 | OK |
| `AST_MATCH_STMT` | 패턴 매칭 | OK |
| `AST_SELECT_STMT` | 채널 셀렉트 | OK |
| `AST_WITH_STMT` | 리소스 관리 (with) | OK |
| `AST_BLOCK` | 일반 블록 | OK |
| `AST_ASYNC_BLOCK` | 비동기 블록 | OK |
| `AST_PARALLEL_BLOCK` | 병렬 블록 | OK |
| `AST_UNSAFE_BLOCK` | 안전하지 않은 블록 | OK |
| `AST_DEFER_STMT` | 지연 실행 | OK |
| `AST_BIND_STMT` | 동적 역할 바인딩 | OK |
| `AST_ENUM_DECL` | 열거형 선언 | OK |
| `AST_EXTERN_BLOCK` | 외부 함수 블록 | OK |

### 선언 (llvm_decl.c + llvm_domain.c)

| 대상 | 설명 | 상태 |
|------|------|------|
| `AST_FUNC_DECL` | 함수 선언/정의 | OK |
| `AST_CLASS_DECL` | 클래스(subject) 선언 | OK (도메인 패스) |
| `AST_PARTY_DECL` | 파티 선언 (구조체 + vtable) | OK |
| `AST_ABILITY_DECL` | 어빌리티(인터페이스) 선언 | OK |
| `AST_ROLE_DECL` | 역할 선언 (vtable 구현) | OK |
| `AST_ROSTER_DECL` | 시스테믹 선언 | OK |
| `AST_WORLD_DECL` | 월드 선언 (sync 메서드) | OK |
| `AST_ZONE_DECL` | 존 선언 (sync) | OK |
| `AST_RELATION_DECL` | 관계 선언 | OK |
| `AST_EFFECT_DECL` | 이펙트 선언 | OK |
| `AST_EVENT_DECL` | 이벤트 선언 (subscribe/invoke 함수) | OK |
| `AST_INTENT_DECL` | 인텐트 선언 | OK (런타임 추적) |
| `AST_ACTOR_DECL` | 액터 선언 | OK (도메인 패스) |

---

## 미구현 (Not Implemented)

### 1. 디스패치 불필요 (구조적/메타데이터 노드)

이 노드들은 statement/expression으로 직접 실행되지 않음:

| AST 타입 | 설명 | 이유 |
|----------|------|------|
| `AST_PROGRAM` | 최상위 프로그램 노드 | 프로그램 레벨에서 처리 |
| `AST_TYPE` | 타입 어노테이션 | 타입 정보만, 코드 생성 없음 |
| `AST_CHANNEL_TYPE` | 채널 타입 어노테이션 | 타입 정보만 |
| `AST_FUTURE_TYPE` | 퓨처 타입 어노테이션 | 타입 정보만 |
| `AST_EVENT_HANDLER_TYPE` | 이벤트 핸들러 타입 | 타입 정보만 |
| `AST_MATCH_CASE` | match 분기 | `llvm_emit_match_stmt` 내부 처리 |
| `AST_IMPORT_DECL` | import 선언 | 모듈 로더에서 처리 |
| `AST_NAMESPACE_DECL` | namespace 선언 | 모듈 로더에서 처리 |

### 2. 도메인 내부 노드 (부모 핸들러 내부에서 처리)

| AST 타입 | 설명 | 처리 위치 |
|----------|------|-----------|
| `AST_DOMAIN_SLOT` | 도메인 슬롯 정의 | `llvm_domain.c` 구조체 레이아웃 |
| `AST_ZONE_LAYER_SLOT` | 존 레이어 슬롯 | `llvm_domain.c` 구조체 레이아웃 |
| `AST_WORLD_ZONE` | 월드-존 참조 | `llvm_domain.c` 월드 핸들러 |
| `AST_WORLD_STATE` | 월드 상태 | `llvm_domain.c` 월드 핸들러 |

### 3. 미구현 기능 (default: warning 발생)

#### 우선순위 높음 (P1) — 핵심 기능 확장

| AST 타입 | 설명 | 영향도 |
|----------|------|--------|
| `AST_LET_DESTRUCTURE` | `let (a, b) = expr;` | 패턴 매칭 완성도 |
| `AST_TYPE_ALIAS` | `type Name = OtherType` | 타입 시스템 편의 |
| `AST_USE_DECL` | `use` 선언 | 모듈 시스템 |

#### 우선순위 중간 (P2) — Ability/Role 시스템

| AST 타입 | 설명 | 영향도 |
|----------|------|--------|
| `AST_INCLUDE_STMT` | role include | Role 합성 |
| `AST_REQUIRE_FIELD` | 필드 요구 선언 | Ability 계약 |
| `AST_IMPL_ABILITY` | Ability 구현 | 다형성 |
| `AST_OVERRIDE_FUNC` | 함수 오버라이드 | 상속 |
| `AST_ROLE_SLOT` | Role 슬롯 | 파티 내부 |
| `AST_PARTY_SHARED` | shared 필드 | 파티 공유 상태 |
| `AST_PARTY_METHOD` | 파티 메서드 | 파티 동작 |

#### 우선순위 낮음 (P3) — 고급 도메인 기능

| AST 타입 | 설명 | 영향도 |
|----------|------|--------|
| `AST_SYSTEMIC_SLOT` | 시스테믹 슬롯 | 시스테믹 내부 |
| `AST_WORLD_SYSTEMIC` | 월드-시스테믹 참조 | 월드 관리 |
| `AST_WORLD_ACTIVATE` | 월드 활성화 | 월드 라이프사이클 |
| `AST_WORLD_DEACTIVATE` | 월드 비활성화 | 월드 라이프사이클 |
| `AST_WORLD_MAINTAIN` | 월드 유지보수 | 월드 관리 |
| `AST_INTENT_INVOLVES` | 인텐트 involves | 인텐트 선언 |
| `AST_INTENT_STEP` | 인텐트 step | 인텐트 선언 |
| `AST_ZONE_APPLY` | 존 apply | 존 연산 |
| `AST_ZONE_LINK` | 존 link | 존 연산 |
| `AST_ZONE_DETACH` | 존 detach | 존 연산 |
| `AST_ZONE_UNLINK` | 존 unlink | 존 연산 |
| `AST_ZONE_REFRESH` | 존 refresh | 존 연산 |
| `AST_ZONE_MAINTAIN_EFFECT` | 존 이펙트 유지 | 존 관리 |
| `AST_ZONE_MAINTAIN_RELATION` | 존 관계 유지 | 존 관리 |
| `AST_ZONE_MAINTAIN_STATE` | 존 상태 유지 | 존 관리 |
| `AST_ZONE_AUTHORITY` | 존 권한 | 존 보안 |
| `AST_ZONE_STATE` | 존 상태 | 존 관리 |

---

## 수치 요약

| 범주 | 개수 |
|------|------|
| 전체 AST 노드 | 88 |
| LLVM 디스패치 구현 | 53 |
| 구조적 노드 (디스패치 불필요) | 8 |
| 부모 핸들러 내부 처리 | 4 |
| **미구현 (warning 발생)** | **23** |
| - P1 (핵심 확장) | 3 |
| - P2 (Role/Ability) | 7 |
| - P3 (고급 도메인) | 13 |

---

## 키워드 전체 목록 (77개)

### 제어 흐름
`let` `func` `return` `break` `continue` `if` `else` `while` `for` `in` `match` `case` `default`

### 타입/클래스
`class` `subject` `struct` `tobject` `enum` `type` `trait` `impl` `extends` `dyn` `where`

### 가시성/접근
`public` `private` `with` `as`

### 비동기/동시성
`async` `await` `actor` `channel` `select` `spawn` `parallel`

### 소유권
`own` `ref`

### 모듈
`export` `namespace` `import` `use` `extern`

### Role/Ability
`ability` `role` `include` `require` `override` `super` `secure`

### Party/상호작용
`party` `relation` `effect` `zone` `slot` `shared` `context`

### Roster/World
`roster` `world`

### 안전
`unsafe` `defer` `bind`

### 리터럴
`true` `false`

### 컨텍스트 키워드
`event` (TOKEN_IDENTIFIER로 파싱, 문맥 의존)
