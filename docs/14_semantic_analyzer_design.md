# Pergyra Semantic Analyzer 설계

## 1. 전체 구조

Semantic Analyzer는 Parser가 생성한 AST를 받아 세 가지 작업을 수행한다.

```
AST (from Parser)
    │
    ▼
┌─────────────────────────────────────┐
│         Semantic Analyzer           │
│                                     │
│  Pass 1: Symbol Collection          │
│    → 모든 선언을 SymbolTable에 등록  │
│                                     │
│  Pass 2: Type Checking              │
│    → 표현식 타입 추론 및 검증        │
│    → Slot 타입 안전성 검사          │
│    → SecureSlot 토큰 추적           │
│                                     │
│  Pass 3: Lifetime Analysis          │
│    → Slot 생명주기 검증             │
│    → Release 누락 감지              │
└─────────────────────────────────────┘
    │
    ▼
Annotated AST  (타입 정보가 붙은 AST)
    │
    ▼
Codegen (다음 단계)
```

Effect System은 Phase 2에서 구현한다. 지금은 제외.

---

## 2. 파일 구조

```
src/semantic/
    symbol_table.h      ← 심볼 테이블 (스코프 관리)
    symbol_table.c
    type_checker.h      ← 타입 검사기
    type_checker.c
    slot_analyzer.h     ← Slot 생명주기 분석기 (Pergyra 고유)
    slot_analyzer.c
    semantic.h          ← 통합 인터페이스
    semantic.c
```

`type_system.h`는 이미 존재하므로 `type_system.c`만 구현하면 된다.

---

## 3. Symbol Table 설계

### 3.1 심볼 종류

```c
typedef enum
{
    SYMBOL_VARIABLE,    /* let x = ... */
    SYMBOL_FUNCTION,    /* func Foo() */
    SYMBOL_CLASS,       /* class Bar */
    SYMBOL_ACTOR,       /* actor Baz */
    SYMBOL_TYPE_PARAM,  /* T, U (제네릭) */
    SYMBOL_SLOT,        /* Slot<T> 변수 */
    SYMBOL_TOKEN        /* SecureSlot 토큰 */
} SymbolKind;
```

### 3.2 심볼 구조

```c
typedef struct Symbol
{
    char*       name;
    SymbolKind  kind;
    Type*       type;
    uint32_t    decl_line;
    uint32_t    decl_col;

    /* Slot 전용 */
    struct {
        bool     is_released;       /* Release() 호출됨 */
        bool     is_secure;         /* SecureSlot 여부 */
        char*    paired_token_name; /* SecureSlot의 경우 토큰 이름 */
        uint32_t scope_depth;       /* 선언된 스코프 깊이 */
    } slot_info;
} Symbol;
```

### 3.3 스코프 체인

```
Global Scope
    │
    ├── Function Scope (func ProcessData)
    │       │
    │       └── Block Scope (if / for / with 블록)
    │
    └── Class Scope (class Player)
            │
            └── Method Scope (func TakeDamage)
```

스코프는 링크드 리스트로 체인을 구성한다. 심볼 조회는 현재 스코프에서 시작해서 부모로 올라간다.

```c
typedef struct Scope
{
    struct Scope* parent;
    Symbol**      symbols;
    size_t        symbol_count;
    size_t        symbol_capacity;
    ScopeKind     kind;  /* SCOPE_GLOBAL, SCOPE_FUNCTION, SCOPE_BLOCK, SCOPE_CLASS */
} Scope;
```

---

## 4. Type Checker 설계

### 4.1 핵심 규칙 — Slot 타입 안전성

Pergyra에서 가장 중요한 검사다.

```
규칙 1: Slot<T>에는 T 타입 값만 Write 가능
    let slot = ClaimSlot<Int>()
    Write(slot, "hello")    ← ERROR: String을 Slot<Int>에 쓸 수 없음
    Write(slot, 42)         ← OK

규칙 2: SecureSlot<T>은 반드시 토큰과 함께 사용
    let (slot, token) = ClaimSecureSlot<Int>(SECURITY_LEVEL_BASIC)
    Write(slot, 42)          ← ERROR: 토큰 없음
    Write(slot, 42, token)   ← OK

규칙 3: 다른 SecureSlot의 토큰 혼용 불가
    let (slotA, tokenA) = ClaimSecureSlot<Int>(SECURITY_LEVEL_BASIC)
    let (slotB, tokenB) = ClaimSecureSlot<Int>(SECURITY_LEVEL_BASIC)
    Write(slotA, 42, tokenB) ← ERROR: tokenB는 slotA의 토큰이 아님

규칙 4: Released Slot 접근 불가
    Release(slot)
    let v = Read(slot)       ← ERROR: 이미 해제된 슬롯
```

### 4.2 타입 추론 흐름

```c
Type* type_check_expression(ASTNode* expr, Scope* scope, SemanticContext* ctx)
{
    switch (expr->type) {
    case AST_NUMBER:
        return TYPE_INT;   /* 기본값, 컨텍스트에 따라 Float 가능 */

    case AST_STRING:
        return TYPE_STRING;

    case AST_IDENTIFIER:
        /* 심볼 테이블에서 조회 */
        Symbol* sym = scope_lookup(scope, expr->data.identifier.name);
        if (sym == NULL) {
            semantic_error(ctx, expr, "Undefined symbol: %s", expr->data.identifier.name);
            return TYPE_UNKNOWN;
        }
        return sym->type;

    case AST_CALL:
        return type_check_call(expr, scope, ctx);

    case AST_BINARY:
        return type_check_binary(expr, scope, ctx);

    /* ... */
    }
}
```

### 4.3 내장 함수 타입 규칙

ClaimSlot, Write, Read, Release는 일반 함수가 아니라 컴파일러가 직접 처리한다.

```
ClaimSlot<T>()          → Slot<T>
ClaimSecureSlot<T>(level) → (SecureSlot<T>, SecurityToken)
Write(Slot<T>, T)       → Void
Write(SecureSlot<T>, T, SecurityToken) → Void
Read(Slot<T>)           → T
Read(SecureSlot<T>, SecurityToken) → T
Release(Slot<T>)        → Void
Release(SecureSlot<T>, SecurityToken) → Void
```

---

## 5. Slot Lifetime Analyzer 설계

### 5.1 Slot 상태 머신

```
UNCLAIMED
    │  ClaimSlot<T>()
    ▼
CLAIMED ──────────── Write() / Read() ──→ CLAIMED
    │
    │  Release()
    ▼
RELEASED  ← 이후 접근 시 ERROR
```

### 5.2 with 블록 처리

```pergyra
with slot<Int> as s {
    s.Write(42)
    s.Read()
}
/* 여기서 s는 자동으로 RELEASED 상태 */
s.Read()  ← ERROR
```

`with` 블록을 나올 때 Slot이 자동으로 RELEASED 처리된다.

### 5.3 분기에서의 Slot 상태

```pergyra
let slot = ClaimSlot<Int>()
if condition {
    Release(slot)
}
Read(slot)  ← WARNING: 조건부 Release 후 접근
```

양쪽 분기 모두에서 Release되지 않은 경우 경고를 발생시킨다. 두 분기 모두 Release하거나, 두 분기 모두 Release하지 않아야 한다.

---

## 6. 에러 리포팅

에러는 두 가지 심각도로 분류한다.

```c
typedef enum
{
    SEMANTIC_ERROR,     /* 컴파일 중단 */
    SEMANTIC_WARNING    /* 계속 진행, 경고만 출력 */
} DiagnosticLevel;

typedef struct Diagnostic
{
    DiagnosticLevel level;
    uint32_t        line;
    uint32_t        col;
    char*           message;
} Diagnostic;
```

에러 메시지 형식:
```
[ERROR] semantic_error.pgy:10:5 - Cannot write String to Slot<Int>
[ERROR] semantic_error.pgy:15:3 - SecureSlot requires token argument
[WARNING] semantic_error.pgy:22:1 - Slot may not be released on all paths
```

---

## 7. 구현 순서

총 4단계로 나눈다. 각 단계가 완료되면 독립적으로 테스트 가능하다.

### 단계 1: Symbol Table + type_system.c

```
목표: 선언을 등록하고 조회할 수 있는 스코프 시스템
구현:
    symbol_table.h / symbol_table.c
    type_system.c (헤더에 선언된 함수 구현)
테스트:
    func 선언 등록 → 호출 시 조회 성공
    중복 선언 → 에러
    스코프 벗어난 변수 → 에러
```

### 단계 2: 기본 Type Checker

```
목표: 기본 타입 추론 및 검사
구현:
    type_checker.h / type_checker.c
테스트:
    let x: Int = "hello"  → 타입 불일치 에러
    func 반환 타입 검증
    제네릭 없이 기본 타입 먼저
```

### 단계 3: Slot 타입 검사 (Pergyra 핵심)

```
목표: Slot 고유 규칙 검사
구현:
    type_checker.c에 Slot 처리 추가
테스트:
    Write 타입 불일치 에러
    SecureSlot 토큰 없음 에러
    토큰 혼용 에러
```

### 단계 4: Slot Lifetime Analyzer

```
목표: 생명주기 분석
구현:
    slot_analyzer.h / slot_analyzer.c
테스트:
    Release 후 접근 에러
    with 블록 자동 해제 확인
    조건부 Release 경고
```

---

## 8. semantic.h 통합 인터페이스

```c
typedef struct SemanticResult
{
    bool           success;
    Diagnostic**   diagnostics;
    size_t         diagnostic_count;
    ASTNode*       annotated_ast;  /* 타입 정보가 붙은 AST */
} SemanticResult;

SemanticResult* semantic_analyze(ASTNode* ast);
void            semantic_result_destroy(SemanticResult* result);
void            semantic_print_diagnostics(SemanticResult* result);
```

사용 흐름:
```c
/* main.c에서 */
ASTNode* ast = parser_parse(source);
SemanticResult* result = semantic_analyze(ast);

if (!result->success) {
    semantic_print_diagnostics(result);
    return 1;
}

/* 다음 단계: codegen_generate(result->annotated_ast) */
```

---

## 9. 이번 구현에서 제외하는 것

- Effect System (with effects IO) → Phase 2
- 제네릭 타입 추론 (unification) → 단계 2 이후
- Actor 메시지 타입 검사 → Phase 2
- 분산 Party/Roster/World 검사 → Phase 3
- NUMA/멀티소켓 관련 → Phase 3

---

## 10. 다음 단계: C Transpiler

Semantic 완료 후 Codegen은 C 트랜스파일러로 구현한다.

```
Pergyra Slot<Int>    → C: typedef struct { int value; bool occupied; } IntSlot;
Write(slot, 42)      → C: pgy_slot_write(&slot, 42);
Read(slot)           → C: pgy_slot_read(&slot)
Release(slot)        → C: pgy_slot_release(&slot)
with slot<Int> as s  → C: { IntSlot s = pgy_claim_slot_int(); ... pgy_slot_release(&s); }
parallel { A() B() } → C: #pragma omp parallel sections { #pragma omp section { A(); } ... }
```

이 방식으로 가면 GCC로 바로 컴파일 가능하고, LLVM 백엔드 없이도 Pergyra 코드가 실행된다.
