# Test-Driven ABI Generation & Explicit Lowering 전략

Anti-hype status note (2026-04-29):

- Older wording in this document may use intentionally sharp language about AI
  hallucination and ABI facts. Read it as a risk warning, not as a literal
  universal claim.
- The stable policy is concrete: ABI layout, padding, and calling-convention
  facts must come from tests, target toolchains, and explicit lowering rules.

## 서문: 왜 이 접근이 필요한가

이전 문서 (`38_c_macro_deception_and_abi.md`)에서 C 매크로가 "가짜 ABI"라는 문제를 규명했다. 이 문서는 **그 문제를 실제로 해결하기 위한 실행 전략**을 다룬다. 핵심은 세 가지:

1. **Test-Driven ABI Generation**: ABI 테스트 코드를 AI에게 먼저 짜게 하여 환각을 차단
2. **MCP 기반 컨텍스트 격리**: AI가 전체 파이프라인을 건드리지 못하도록 역할 극한 제한
3. **Explicit Lowering 룰셋**: "공식"을 먼저 정의하고, AI는 단지 그 공식에 맞는_visitor 코드_만 타이핑하게 함

---

## 1. Test-Driven ABI Generation

### 1.1 핵심 아이디어

AI에게 "컴파일러 로직을 짜라"고만 시키면 ABI 물리 사실을 높은 확률로 잘못 추정한다. 타입 크기, 필드 오프셋, 패딩, calling convention 같은 물리적 사실은 모델 추론이 아니라 **타겟 플랫폼(GCC/Clang on Linux/Windows)의 ABI 사양과 실제 toolchain 측정**으로 고정해야 한다.

따라서 순서를 **뒤집는다**:

```
기존:   AI → 컴파일러 로직 생성 → (운 좋으면) → ABI 일치
새로운: AI → ABI 테스트 코드 생성 → human 검증 → ABI 고정 → AI → 컴파일러 로직 생성
```

### 1.2 생성해야 할 ABI 헤더 파일

**파일 위치**: `src/runtime/pgy_abi_spec.h`

이 파일은 Pergyra의 모든 핵심 타입에 대해 다음을 명시한다:

```c
/* pgy_abi_spec.h — Pergyra ABI Specification
 *
 * 이 파일은 Pergyra 컴파일러가 타겟하는 메모리 레이아웃의
 * "유일한 진실(Single Source of Truth)"이다.
 * 모든 백엔드(C/LLVM)는 이 파일을 준수해야 한다.
 *
 * 빌드: 이 파일은 컴파일되지 않는다. static_assert 테스트 전용.
 * 실행: make test-abi  →  bin/test_abi_spec
 */

#ifndef PERGYRA_ABI_SPEC_H
#define PERGYRA_ABI_SPEC_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ----------------------------------------------------------------
 * 플랫폼 의존 타입 추상화
 * Pergyra 컴파일러가 직접 크기를 통제한다.
 * ---------------------------------------------------------------- */

#if defined(_WIN32) || defined(__CYGWIN__)
    /* Windows: SRWLOCK + CONDITION_VARIABLE */
    typedef struct { void* ptr; int reserved[5]; } pgy_mutex_t;
    typedef struct { void* ptr; int reserved[4]; } pgy_condvar_t;
#else
    /* Linux: pthread_mutex_t + pthread_cond_t */
    #include <pthread.h>
    typedef pthread_mutex_t pgy_mutex_t;
    typedef pthread_cond_t  pgy_condvar_t;
#endif

/* ----------------------------------------------------------------
 * 1. Slot<T> ABI
 * ----------------------------------------------------------------
 * Canonical checked ABI: { value: CType, occupied: bool } + padding
 * Raw opt-out:           { value: CType } only under whole-program PGY_RAW_SLOTS
 *
 * Pergyra MIR가 레이아웃을 결정. C 컴파일러는 이를 따른다.
 */

/* --- Canonical checked ABI --- */
typedef struct { int32_t  value; bool occupied; } ABI_Slot_Int;
typedef struct { int64_t  value; bool occupied; } ABI_Slot_Long;
typedef struct { float    value; bool occupied; } ABI_Slot_Float;
typedef struct { double   value; bool occupied; } ABI_Slot_Double;
typedef struct { bool     value; bool occupied; } ABI_Slot_Bool;
typedef struct { char*    value; bool occupied; } ABI_Slot_String;

/* ----------------------------------------------------------------
 * 2. SecureSlot<T> ABI
 * ----------------------------------------------------------------
 * { value: CType, occupied: bool, token: uint64_t } + padding
 */
typedef struct { int32_t value; bool occupied; uint64_t token; } ABI_SecureSlot_Int;
typedef struct { int64_t value; bool occupied; uint64_t token; } ABI_SecureSlot_Long;

/* ----------------------------------------------------------------
 * 3. Option<T> ABI
 * ----------------------------------------------------------------
 * { tag: int32_t (PgyOptionSome=0 / PgyOptionNone=1), value: CType } + padding
 *
 * 중요: tag는 항상 0..3 바이트, value는 align(CType)에 정렬.
 * 컴파일러가 패딩을 직접 계산한다.
 */
typedef struct { int32_t tag; int32_t  value; } ABI_Option_Int;
typedef struct { int32_t tag; int64_t  value; } ABI_Option_Long;
typedef struct { int32_t tag; bool     value; } ABI_Option_Bool;
typedef struct { int32_t tag; char*    value; } ABI_Option_String;

/* ----------------------------------------------------------------
 * 4. Result<T, E> ABI
 * ----------------------------------------------------------------
 * { tag: int32_t (PgyResultOk=0 / PgyResultErr=1), union { T ok; E err; } } + padding
 *
 * union의 크기는 max(sizeof(T), sizeof(E)).
 * 전체 크기는 tag(4) + union + padding(align).
 */
typedef const char* PgyError;
typedef struct {
    int32_t tag;
    union { int32_t ok; PgyError err; };
} ABI_Result_Int;
typedef struct {
    int32_t tag;
    union { bool ok; PgyError err; };
} ABI_Result_Bool;

/* ----------------------------------------------------------------
 * 5. Channel<T> ABI
 * ----------------------------------------------------------------
 * 이 구조체가 가장 문제다. pthread_mutex_t/condvar가 플랫폼마다 다르다.
 * Pergyra는 이 전체 크기를 "블랙박스 불투명 타입"으로 취급하고,
 * MIR에서는 포인터(Channel*)만 주고받도록 해야 한다.
 *
 * 스택 할당은 금지. 힙에서만.
 */
typedef struct {
    int32_t         *buf;
    size_t           cap;
    size_t           head;
    size_t           tail;
    size_t           count;
    bool             closed;
    /* padding here (platform-dependent) */
    pgy_mutex_t      mutex;
    pgy_condvar_t    cond_not_full;
    pgy_condvar_t    cond_not_empty;
} ABI_Channel_Int;

/* =================================================================
 * STATIC ASSERT 검증
 * 이 assert들이 모두 pass해야 ABI가 고정된 것이다.
 * ================================================================= */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define ABI_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define ABI_STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond) ? 1 : -1]
#endif

/* Slot<Int> canonical checked: value@0, occupied@4+, size >= 8 */
ABI_STATIC_ASSERT(offsetof(ABI_Slot_Int, value) == 0, slot_int_value_offset);
ABI_STATIC_ASSERT(offsetof(ABI_Slot_Int, occupied) >= 4, slot_int_occupied_offset);
ABI_STATIC_ASSERT(sizeof(ABI_Slot_Int) >= 8, slot_int_min_size);

/* Option<Int>: tag@0, value@4, size == 8 */
ABI_STATIC_ASSERT(offsetof(ABI_Option_Int, tag) == 0, option_int_tag_offset);
ABI_STATIC_ASSERT(offsetof(ABI_Option_Int, value) == 4, option_int_value_offset);
ABI_STATIC_ASSERT(sizeof(ABI_Option_Int) == 8, option_int_size);

/* Result<Int>: tag@0, union@4, size == 8 (LP64에서 char*==8) */
ABI_STATIC_ASSERT(offsetof(ABI_Result_Int, tag) == 0, result_int_tag_offset);
/* union offset은 컴파일러에 따라 4 또는 8 */

/* Channel<Int>: 최소 크기 검증 (플랫폼 의존) */
ABI_STATIC_ASSERT(sizeof(ABI_Channel_Int) >= 48, channel_int_min_size);

#endif /* PERGYRA_ABI_SPEC_H */
```

### 1.3 ABI 테스트 실행 파일

**파일 위치**: `src/test_abi_spec.c`

```c
/* test_abi_spec.c — Pergyra ABI Spec Validation
 *
 * 빌드: make test-abi
 * 실행: ./bin/test_abi_spec
 *
 * pgy_abi_spec.h의 static_assert를 런타임에서 재검증하고,
 * 실제 sizeof/offsetof 값을 출력하여 MIR 레이아웃 계산과 대조.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "runtime/pgy_abi_spec.h"

static int g_pass = 0, g_fail = 0;

#define ABI_TEST(name, cond) \
    do { \
        printf("  %-65s", name); \
        if (cond) { printf("PASS\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

int main(void) {
    printf("=== Pergyra ABI Spec Validation ===\n\n");

    printf("[Slot<Int> canonical checked]\n");
    printf("  sizeof = %zu, align = %zu\n",
           sizeof(ABI_Slot_Int), alignof(ABI_Slot_Int));
    printf("  value @ %zu, occupied @ %zu\n\n",
           offsetof(ABI_Slot_Int, value),
           offsetof(ABI_Slot_Int, occupied));

    ABI_TEST("value at offset 0", offsetof(ABI_Slot_Int, value) == 0);
    ABI_TEST("occupied >= 4", offsetof(ABI_Slot_Int, occupied) >= 4);
    ABI_TEST("size >= 8", sizeof(ABI_Slot_Int) >= 8);

    printf("[Option<Int>]\n");
    printf("  sizeof = %zu, align = %zu\n",
           sizeof(ABI_Option_Int), alignof(ABI_Option_Int));
    printf("  tag @ %zu, value @ %zu\n\n",
           offsetof(ABI_Option_Int, tag), offsetof(ABI_Option_Int, value));
    ABI_TEST("tag at 0", offsetof(ABI_Option_Int, tag) == 0);
    ABI_TEST("value at 4", offsetof(ABI_Option_Int, value) == 4);
    ABI_TEST("size == 8", sizeof(ABI_Option_Int) == 8);

    printf("[Channel<Int>]\n");
    printf("  sizeof = %zu (platform-dependent)\n\n",
           sizeof(ABI_Channel_Int));
    ABI_TEST("channel >= 48 bytes", sizeof(ABI_Channel_Int) >= 48);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    if (g_fail > 0) {
        fprintf(stderr, "\n*** ABI SPEC VIOLATION ***\n");
        fprintf(stderr, "MIR 레이아웃 계산이 실제 C struct와 일치하지 않는다.\n");
        fprintf(stderr, "백엔드를 수정하거나 pgy_abi_spec.h를 업데이트하라.\n");
    }

    return g_fail > 0 ? 1 : 0;
}
```

### 1.4 Makefile에 테스트 추가

```makefile
# ABI spec validation
test-abi: $(BINDIR)/test_abi_spec
	@echo "[RUN] ABI spec validation"
	@$(BINDIR)/test_abi_spec

$(BINDIR)/test_abi_spec: src/test_abi_spec.c src/runtime/pgy_abi_spec.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -Isrc -o $@ $<
```

---

## 2. MCP 기반 컨텍스트 격리

### 2.1 문제: AI가 전체 파이프라인을 한번에 건드리면 실패한다

현재 파이프라인은 이렇다:

```
AST → HIR → DIR → RIR → MIR → Backend(C/LLVM)
```

AI에게 "MIR에서 C 코드로 내려라"라고 하면, AI는 다음을 **동시에** 해야 한다:

1. MIR 구조 이해
2. RIR의 자원 상태 파악
3. C 런타임 함수 시그니처 파악
4. 타입 레이아웃 계산
5. C AST 방문자 패턴 구현

이것은 **5개의 독립적 인지 부하**를 한 컨텍스트에 넣는 것이다. LLM의 컨텍스트 윈도우는 이를 처리할 수 없다. 환각이 필연적이다.

### 2.2 해결책: RIR Facts를 JSON으로 출력, AI는 JSON→MIR 매핑만

```
┌──────────────────────────────────────────────────────────────┐
│  Phase 1: RIR Facts Extractor (Pergyra 네이티브 코드)         │
│                                                              │
│  RIRProgram → JSON 출력                                      │
│  {                                                           │
│    "scopes": [                                               │
│      {                                                       │
│        "name": "Main",                                       │
│        "kind": "function",                                   │
│        "facts": [                                            │
│          { "name": "s", "kind": "resource",                  │
│            "resource": "LOCAL_SLOT", "type": "Slot<Int>",   │
│            "state": "OWNED" }                                │
│        ],                                                    │
│        "ops": [                                              │
│          { "kind": "CLAIM", "subject": "s",                 │
│            "slot_anchor": "s", "type": "Slot<Int>" }        │
│        ]                                                     │
│      }                                                       │
│    ]                                                         │
│  }                                                           │
└──────────────────────┬───────────────────────────────────────┘
                       │ JSON (정적 자원 사실)
                       ▼
┌──────────────────────────────────────────────────────────────┐
│  Phase 2: JSON→MIR Mapping Rules (AI/MCP가 생성)              │
│                                                              │
│  입력: JSON RIR Facts                                        │
│  출력: MIR 명령어 목록                                       │
│                                                              │
│  규칙 예:                                                    │
│  IF fact.kind == "resource" AND fact.resource == "LOCAL_SLOT"│
│     AND op.kind == "CLAIM"                                   │
│  THEN emit:                                                  │
│     MIR_INST_RESOURCE_OP {                                   │
│       op: "SlotClaim",                                       │
│       slot_anchor: op.subject,                               │
│       type_layout: lookup_abi(op.type),  /* pgy_abi_spec.h */│
│       runtime_fn: "pgy_claim_" + strip_type(op.type)         │
│     }                                                        │
│                                                              │
│  AI는 오직 이 "IF-THEN 규칙 테이블"만 생성한다.               │
│  실제 MIR struct 생성 코드는 별도 Visitor가 처리.              │
└──────────────────────┬───────────────────────────────────────┘
                       │ MIR 명령어 목록
                       ▼
┌──────────────────────────────────────────────────────────────┐
│  Phase 3: MIR Emitter Visitor (순수 기계적 코드)              │
│                                                              │
│  입력: MIR 명령어 목록                                       │
│  출력: C 코드 / LLVM IR                                     │
│                                                              │
│  이 Visitor는 "규칙"을 모름. 단지 MIR struct를 읽고           │
│  해당하는 C/LLVM API를 호출한다.                              │
│                                                              │
│  예: MIRInst.type == MIR_INST_RESOURCE_OP                   │
│      → fprintf("pgy_claim_%s(...)\n", inst->runtime_fn)      │
│                                                              │
│  AI는 이 파일을 "수정"하지 않는다. 생성만 한다.               │
│  한번 생성되면 human이 리뷰 후 커밋.                          │
└──────────────────────────────────────────────────────────────┘
```

### 2.3 MCP 서버 구조

Pergyra 리포지토리에 MCP 설정 파일을 추가한다:

```json
// .mcp.json
{
  "mcpServers": {
    "pergyra-abi-mapper": {
      "command": "node",
      "args": ["scripts/abi_mapper_mcp.js"],
      "env": {
        "ABI_SPEC_PATH": "${workspaceFolder}/src/runtime/pgy_abi_spec.h",
        "RIR_FACTS_SCHEMA": "${workspaceFolder}/scripts/rir_facts_schema.json"
      },
      "capabilities": ["rir_to_mir_mapping"]
    }
  }
}
```

**MCP 서버의 제한된 역할**:

```
수행 가능:
  ✅ RIR Facts JSON을 받아 MIR 명령어 목록으로 매핑
  ✅ pgy_abi_spec.h를 참조하여 타입 레이아웃 lookup
  ✅ 매핑 규칙 테이블 생성 (IF-THEN)

수행 불가:
  ❌ MIR struct 정의 수정
  ❌ 백엔드 코드 수정
  ❌ AST 순회 로직 변경
  ❌ 런타임 함수 시그니처 변경
  ❌ 새로운 IR 계층 추가
```

---

## 3. 명시적 하강 (Explicit Lowering) 룰셋

### 3.1 핵심 원칙: "공식"을 먼저 정의한다

AI에게 **"Intent를 C 코드로 바꿔줘"**라고 말하는 것은 최악의 프롬프트다. AI가 임의로 판단하게 된다.

대신 CTO가 다음 형태의 **변환 공식(Mapping Rule)**을 설계해야 한다:

### 3.2 완전한 Lowering 룰 테이블

#### Rule 1: DIR `using` 노드 → RIR Claim Op

```
DIR 입력:
  DIRIntentStep {
    using_alias: "s",
    where_type_node_id: 5,  // "SubjectSlot<Int>"
  }

RIR 출력:
  RIRFact {
    name: "s",
    kind: RIR_FACT_RESOURCE,
    resource_kind: RIR_RESOURCE_LOCAL_SLOT,
    initial_state: RIR_STATE_UNINIT
  }
  RIROp {
    kind: RIR_OP_CLAIM,
    subject: "s",
    slot_anchor: "s",
    resource_kind: RIR_RESOURCE_LOCAL_SLOT
  }
  RIRFact {
    name: "s",
    state: RIR_STATE_OWNED
  }
```

#### Rule 2: RIR Claim Op → MIR Resource Op

```
RIR 입력:
  RIROp {
    kind: RIR_OP_CLAIM,
    subject: "s",
    slot_anchor: "s",
    resource_kind: RIR_RESOURCE_LOCAL_SLOT
  }
  + TypeEnv: "s" → Slot<Int>

MIR 출력:
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "claim_s",
    slot_anchor: "s",
    arg0: "Slot<Int>",
    result_name: "%slot_s_1",
    rir_op: <위 RIR OP 포인터>
  }
```

#### Rule 3: MIR Resource Op → C 코드

```
MIR 입력:
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "claim_s",
    slot_anchor: "s",
    arg0: "Slot<Int>",
    result_name: "%slot_s_1"
  }
  + ABI Spec: Slot<Int> canonical checked -> { int32_t value; bool occupied; } (size=8)

C 출력:
  PgySlot_Int s = pgy_claim_Int();
  /* MIR: claim_s → slot_anchor=s, type=Slot<Int> */
```

#### Rule 4: RIR Read Op → MIR Def → C 코드

```
RIR:  RIROp { kind: RIR_OP_READ, subject: "s", slot_anchor: "s" }
MIR:  MIRInstruction { kind: MIR_INST_DEF, name: "val", arg0: "pgy_read_Int", arg1: "&s" }
C:    int32_t val = pgy_read_Int(&s);
```

#### Rule 5: RIR Release Op → MIR Resource Op → C 코드

```
RIR:  RIROp { kind: RIR_OP_RELEASE, subject: "s", slot_anchor: "s" }
MIR:  MIRInstruction { kind: MIR_INST_RESOURCE_OP, name: "release_s", slot_anchor: "s", arg0: "Slot<Int>" }
C:    pgy_release_Int(&s);
```

#### Rule 6: Option<T> Some 생성

```
AST:  Some<Int>(42)
MIR:  MIRInstruction {
        kind: MIR_INST_DEF,
        name: "opt",
        arg0: "pgy_option_some_Int",
        arg1: "42",
        result_name: "%opt_1"
      }
  + type_layout: ABI_Option_Int { tag: int32_t@0, value: int32_t@4 } (size=8)
C:    PgyOption_Int opt = pgy_option_some_Int(42);
```

#### Rule 7: Result<T, E> Try-Propagate (?)

```
AST:  let x = maybe_fail()?
MIR:  MIRInstruction {
        kind: MIR_INST_BRANCH,
        arg0: "%result_1.tag",
        succ_true: "ok_block",    /* tag == Ok */
        succ_false: "err_block"   /* tag == Err → early return */
      }
C:    PgyResult_Int result_1 = some_func();
      if (result_1.tag != PgyResultOk) {
          return result_1;  /* early return */
      }
      int32_t x = result_1.ok;
```

#### Rule 8: Channel Send/Recv

```
AST:  ch |> send(42)
RIR:  (channel은 resource fact로 추적되지 않음 - 힙 할당)
MIR:  MIRInstruction {
        kind: MIR_INST_CALL,
        arg0: "pgy_channel_send_Int",
        arg1: "&ch",
        arg2: "42"
      }
C:    pgy_channel_send_Int(&ch, 42);

AST:  ch |> recv() → x
MIR:  MIRInstruction { kind: MIR_INST_CALL, arg0: "pgy_channel_recv_Int", arg1: "&ch", arg2: "&x" }
C:    int32_t x = pgy_channel_recv_Int(&ch);
```

#### Rule 9: Intent Commit → RIR Intent Op → MIR

```
DIR:  DIRIntentInfo { participants: [...], steps: [...] }
RIR:  RIROp { kind: RIR_OP_COMMIT_INTENT, subject: "intent_handle" }
MIR:  MIRInstruction {
        kind: MIR_INST_CALL,
        arg0: "pgy_intent_commit",
        arg1: "__intent_handle"
      }
C:    pgy_intent_commit(__intent_handle);
```

### 3.3 Lowering 룰 테이블 요약

| # | 입력 계층 | 입력 패턴 | 출력 계층 | 출력 패턴 | 복잡도 |
|---|----------|----------|----------|----------|--------|
| 1 | DIR `using` | `using s: Slot<T>` | RIR Fact+Op | `CLAIM` | 낮음 |
| 2 | RIR Claim | `RIR_OP_CLAIM` | MIR Inst | `MIR_INST_RESOURCE_OP` | 낮음 |
| 3 | MIR Claim | `MIR_INST_RESOURCE_OP(SlotClaim)` | C 코드 | `pgy_claim_T()` | 낮음 |
| 4 | RIR Read | `RIR_OP_READ` | MIR Inst | `MIR_INST_DEF(read)` | 낮음 |
| 5 | RIR Release | `RIR_OP_RELEASE` | MIR Inst | `MIR_INST_RESOURCE_OP(Release)` | 낮음 |
| 6 | AST Option | `Some<T>(v)` | MIR Inst | `MIR_INST_DEF(some)` | 낮음 |
| 7 | AST Result | `expr?` | MIR Inst | `MIR_INST_BRANCH` | 중간 |
| 8 | AST Channel | `ch |> send(v)` | MIR Inst | `MIR_INST_CALL` | 낮음 |
| 9 | DIR Intent | Intent steps | MIR Inst | `MIR_INST_CALL(commit)` | 높음 |
| 10 | RIR Secure | `RIR_OP_CLAIM(Secure)` | MIR Inst | `MIR_INST_RESOURCE_OP(SecureClaim)` | 중간 |
| 11 | RIR Projection | `RIR_OP_PROJECT_REFRESH` | MIR Inst | `MIR_INST_CALL(refresh)` | 높음 |
| 12 | RIR Authority | `RIR_OP_AUTHORIZE` | MIR Inst | `MIR_INST_CALL(authorize)` | 높음 |
| 13 | RIR Await | `RIR_OP_AWAIT_LOCAL` / `RIR_OP_AWAIT_REMOTE` | MIR Inst | `MIR_INST_CALL(await)` | 높음 |
| 14 | RIR Compensate | `RIR_OP_COMPENSATE_INTENT_STEP` | MIR Inst | `MIR_INST_CALL(compensate)` | 높음 |

---

## 4. AI 프롬프트 템플릿 (실제 사용 예)

### Phase 1: ABI 스펙 생성 프롬프트

```
당신은 C ABI 전문가입니다. 다음 정보를 기반으로 pgy_abi_spec.h를 생성하세요.

[제공 컨텍스트]
- 타겟 플랫폼: Linux x86_64 (GCC 11+), Windows x64 (MinGW-w64)
- 기존 런타임: src/runtime/pgy_runtime.h (PGY_SLOT_DEFINE, PGY_OPTION_DEFINE 등)
- 기존 테스트: src/test_memory_layout.c

[요청]
1. 모든 Slot, Option, Result, Channel, SecureSlot 타입의 struct 정의를 생성
2. 각 필드의 offset을 주석으로 명시
3. static_assert로 검증 가능한 조건 나열
4. Platform-dependent 타입(pthread_mutex_t 등)을 pgy_mutex_t로 추상화

[제한]
- 새 함수 구현 금지
- 로직 변경 금지
- 오직 struct 정의와 static_assert만
```

### Phase 2: RIR→MIR 매핑 규칙 생성 프롬프트

```
당신은 컴파일러 IR 변환 전문가입니다.
다음 JSON 스키마를 읽고, RIR Facts를 MIR Instructions로 변환하는 IF-THEN 규칙 테이블을 생성하세요.

[입력 스키마] scripts/rir_facts_schema.json
[출력 스키마] scripts/mir_instructions_schema.json
[참조] src/runtime/pgy_abi_spec.h (타입 레이아웃)
[참조] src/compiler/mir.h (MIR 구조)

[생성물]
매핑 규칙 테이블 (Markdown 표 형태).
각 규칙은 다음을 포함:
- Rule ID
- 입력 패턴 (JSONPath)
- 출력 패턴 (MIR struct 필드 매핑)
- 예외 조건

[제한]
- 실제 코드 생성 금지
- MIR struct 정의 변경 금지
- 오직 매핑 규칙 테이블만 생성
```

### Phase 3: C Emitter Visitor 생성 프롬프트

```
다음 Lowering Rule에 따라 MIR 명령어를 C 코드로 번역하는 Visitor 함수를 생성하세요.

[Rule 3]
입력: MIR_INST_RESOURCE_OP { name: "claim_s", slot_anchor: "s", arg0: "Slot<Int>" }
출력: "PgySlot_Int s = pgy_claim_Int();\n"

[Rule 4]
입력: MIR_INST_DEF { name: "val", arg0: "pgy_read_Int", arg1: "&s" }
출력: "int32_t val = pgy_read_Int(&s);\n"

[참조 파일]
- src/compiler/mir.h (MIRInstruction 구조체)
- src/codegen/transpiler.c (기존 C backend 패턴)
- src/runtime/pgy_abi_spec.h (타입 이름)

[제한]
- 기존 transpiler.c의 다른 함수 수정 금지
- 오직 새 Visitor 함수만 추가
- 함수 시그니처는 bool emit_mir_inst(CodeBuf* out, const MIRInstruction* inst, int indent);
```

---

## 5. 실행 순서와 체크포인트

```
Step 1: pgy_abi_spec.h 생성                          [AI 생성 → human 리뷰]
Step 2: test_abi_spec.c 생성 + make test-abi 통과     [컴파일 + 실행 검증]
Step 3: RIR Facts JSON exporter 구현 (rir_dump_json)   [네이티브 코드]
Step 4: RIR→MIR 매핑 규칙 테이블 생성                  [AI 생성 → human 리뷰]
Step 5: Lowering Rules 1-5 (Slot) 구현                [Visitor 코드 생성]
Step 6: make test-mir + 기존 테스트 통과 확인          [회귀 테스트]
Step 7: Lowering Rules 6-8 (Option/Result/Channel) 구현 [Visitor 코드 생성]
Step 8: 백엔드를 Dumb Emission으로 리팩토링            [human 주도, AI 보조]
Step 9: ABI 통합 테스트 (크로스 플랫폼)                [Linux + Windows]
Step 10: C 매크로 제거 및 명시적 런타임 라이브러리 전환 [최종 정리]
```

---

## 6. 현재 코드베이스와의 관계 매핑

| 현재 파일 | 문제 | 대상 Rule | 변경 방향 |
|-----------|------|-----------|-----------|
| `src/runtime/pgy_runtime.h` (매크로) | C 매크로 제네릭 | 전체 | pgy_abi_spec.h 분리, 명시적 함수로 전환 |
| `src/test_memory_layout.c` | 테스트 존재 but ABI 고정 아님 | Step 2 | test_abi_spec.c로 업그레이드 |
| `src/compiler/mir.h` | 타입 레이아웃 정보 없음 | Rule 2-3 | `MIRTypeLayout` 필드 추가 |
| `src/codegen/llvm_backend.c` (330-350) | 백엔드가 직접 struct 생성 | Rule 3 | MIR에서 레이아웃 수신으로 변경 |
| `src/codegen/llvm_expr_helpers_part_*.inc` | 백엔드가 Option 레이아웃 결정 | Rule 6 | MIR에서 레이아웃 수신 |
| `src/codegen/transpiler.c` (293,570) | 백엔드가 직접 타입 이름 생성 | Rule 3,8 | MIR에서 레이아웃 수신 |
| `src/runtime/pgy_channel.h` | _Generic 매크로 | Rule 8 | 명시적 함수 호출로 전환 |

---

## 7. 위험과 대응

| 위험 | 영향 | 대응 |
|------|------|------|
| pthread_mutex_t 크기 플랫폼 의존 | Channel ABI 불안정 | Channel은 MIR에서 "불투명 포인터"로 취급, 스택 할당 금지 |
| bool padding differs by compiler | Slot size drift | canonical checked Slot keeps `occupied`; raw value-only slots require explicit `PGY_RAW_SLOTS` |
| Union 패딩이 C 컴파일러마다 다름 | Result ABI 변동 | MIR가 union 레이아웃을 명시적으로 계산, static_assert로 검증 |
| 기존 매크로 의존 코드가 많음 | 마이그레이션 비용 | 점진적 전환: 새 코드는 ABI spec, 기존 코드는 매크로 유지 |
| AI가 규칙을 잘못 해석 | 잘못된 MIR 생성 | human 리뷰 필수, 테스트 기반 검증 |

---

## 8. 결론

이 접근법의 핵심은 **"AI에게 컴파일러를 짜게 하지 않는다"**는 것이다.

- **AI의 역할**: (1) ABI 스펙 초안 생성, (2) 매핑 규칙 테이블 생성, (3) 규칙에 맞는 Visitor 코드 타이핑
- **Human의 역할**: (1) ABI 스펙 검증, (2) 매핑 규칙 승인, (3) 아키텍처 결정, (4) 백엔드 리팩토링 주도
- **테스트의 역할**: AI의 환각을 차단하는 "물리적 방벽"

이것이 성공하려면 **각 Phase의 산출물이 독립적으로 검증 가능**해야 한다. ABI 스펙은 static_assert로, 매핑 규칙은 JSON 대조로, Visitor 코드는 단위 테스트로 각각 검증된다.

이 문서는 review 시 **"이 규칙 테이블에 따라 생성된 코드가 맞는가?"**를 확인하는 체크리스트로 사용된다.
