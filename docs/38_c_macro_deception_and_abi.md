# C 매크로의 기만과 ABI의 본질

## 요약

현재 Pergyra 런타임에 존재하는 `PGY_CHANNEL_DEFINE`, `PGY_SLOT_DEFINE`, `PGY_RESULT_DEFINE`, `PGY_OPTION_DEFINE`와 같은 C 매크로 기반 제네릭 구현체들은 **진정한 ABI가 아니다**. 이들은 컴파일 타임에 C 컴파일러(GCC/Clang)에 의해 전개되는 텍스트 복사일 뿐이며, Pergyra 컴파일러가 메모리 레이아웃을 완전히 통제하지 못하게 만든다.

ABI를 통일한다는 것은, `Slot<T>`, `Option<T>`, `Result<T, E>`, `Channel<T>` 등의 모든 핵심 타입이 기계어 레벨에서 몇 바이트를 차지하고, 패딩(Padding)이 어떻게 들어가며, 레지스터에 어떻게 실려 전달되는지를 **Pergyra 컴파일러(MIR)가 100% 결정하고 예측할 수 있어야 함**을 의미한다.

---

## 1. 현재 상황: 매크로가 만들어내는 "가짜 ABI"

### 1.1 문제의 본질

`src/runtime/pgy_runtime.h`에서 다음과 같은 매크로를 정의한다:

```c
#define PGY_SLOT_DEFINE_DEBUG(SuffixName, CType) \
typedef struct { \
    CType   value; \
    bool    occupied; \
} PgySlot_##SuffixName;
```

```c
#define PGY_CHANNEL_DEFINE(SuffixName, CType) \
typedef struct { \
    CType           *buf; \
    size_t           cap; \
    size_t           head; \
    size_t           tail; \
    size_t           count; \
    bool             closed; \
    pthread_mutex_t  mutex; \
    pthread_cond_t   cond_not_full; \
    pthread_cond_t   cond_not_empty; \
} PgyChannel_##SuffixName;
```

```c
#define PGY_RESULT_DEFINE(SuffixName, CType, ErrType) \
typedef struct { \
    PgyResultTag tag; \
    union { \
        CType ok; \
        ErrType err; \
    }; \
} PgyResult_##SuffixName;
```

```c
#define PGY_OPTION_DEFINE(SuffixName, CType) \
typedef struct { \
    PgyOptionTag tag; \
    CType value; \
} PgyOption_##SuffixName;
```

이들은 다음과 같은 근본적인 문제를 안고 있다:

| 문제 | 설명 |
|------|------|
| **컴파일러 의존 레이아웃** | `sizeof(PgyChannel_Int)`는 C 컴파일러가 결정한다. GCC와 Clang이 다른 패딩을 넣을 수 있다. |
| **플랫폼 의존 크기** | `pthread_mutex_t`는 Linux에서 40바이트, Windows에서完全不同한 크기다. Pergyra는 이를 예측할 수 없다. |
| **인라인 함수 전개** | `static inline` 함수는 C 컴파일러의 인라이닝 결정에 달려 있다. Pergyra가 호출 규약(calling convention)을 통제하지 못한다. |
| **텍스트 복사일 뿐** | 매크로 전개는 단순 텍스트 치환이다. Pergyra 컴파일러가 이 구조체의 존재를 타입 시스템 안에서 인지하지 못한다. |

### 1.2 구체적인 예: Channel의 크기 비대칭

`PgyChannel<Int>`를 생각해보자:

- Linux x86_64 + GCC: `pthread_mutex_t`(40) + `pthread_cond_t`(48) × 2 + 포인터/정렬 패딩 ≈ **152~168바이트**
- Windows MSVC: `CRITICAL_SECTION` + `CONDITION_VARIABLE` 기반 구현 → 완전히 다른 크기
- Pergyra 컴파일러는 이 크기를 **컴파일 시간에 알 수 없다**. C 헤더를 파싱하거나, `sizeof`를 직접 호출하거나, 런타임에 쿼리해야 한다.

이는 ABI 통일의 정반대다.

---

## 2. 정보 비대칭성의 역전: 백엔드의 지위 강등

### 2.1 현재 백엔드의 잘못된 자율성

현재 C/LLVM 백엔드는 **HIR(고차원 IR)과 MIR(저차원 실행 IR)을 동시에 참조하며 스스로 판단을 내리고 있다**.

**`src/codegen/llvm_backend.c`의 예:**

```c
// 라인의 3189-3280: LLVM 백엔드가 Slot 타입의 LLVM 표현을 직접 생성
LLVMTypeRef slot_types[] = {
    { "Int",    ctx->slot_type_Int,    ctx->type_i32 },
    { "Long",   ctx->slot_type_Long,   ctx->type_i64 },
    ...
};

// 백엔드가 직접 struct 레이아웃을 정의
LLVMStructTypeInContext(ctx->context,
    (LLVMTypeRef[]){ ctx->type_i32, inner_ty }, 2, 0);  // Option<T> 레이아웃
```

**`src/codegen/llvm_expr_helpers_part_*.inc`의 예:**

```c
// 라인 230: LLVM 백엔드가 Option struct 레이아웃을 직접 결정
LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
    (LLVMTypeRef[]){ ctx->type_i32, inner_ty }, 2, 0);

// 라인 1005-1034: Result struct 레이아웃을 직접 결정  
LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context, result_fields, 3, 0);
```

**`src/codegen/transpiler.c`의 예:**

```c
// 라인 293, 424, 447: C 백엔드가 직접 PgySlot_<T> 타입 이름을 사용
codebuf_write(ctx->out, "PgySlot_%s %s = pgy_claim_%s();\n", ...);

// 라인 570: Channel 타입 직접 참조
codebuf_write(ctx->out, "PgyChannel_%s %s;\n", inner, name);
```

이는 컴파일러 아키텍처에서 **최악의 안티 패턴**이다:

1. **백엔드가 타입 레이아웃을 결정** → 프론트엔드가 백엔드의 구현 세부사항에 종속됨
2. **백엔드가 런타임 함수 시그니처를 추정** → MIR에 명시적인 Call 명령어가 없음
3. **양방향 의존** → 프론트엔드가 백엔드를 알 수 없고, 백엔드가 프론트엔드의 의도를 추측해야 함

### 2.2 이상적인 구조: 백엔드는 "번역기"일 뿐

```
┌─────────────────────────────────────────────────┐
│  Frontend (DIR/RIR)                              │
│  - 도메인 로직: "Slot<Int>를 Claim하라"           │
│  - "Option<String>을 반환하라"                   │
│  - "Channel<Int>로 Send하라"                     │
└─────────────────────┬───────────────────────────┘
                      │ HIR (고차원 의미)
                      ▼
┌─────────────────────────────────────────────────┐
│  MIR Lowering (중요!)                            │
│  - "Slot<Int> Claim" →                           │
│    MIR_INST_RESOURCE_OP {                        │
│      op: "SlotClaim",                            │
│      slot_type: { size: 8, align: 4 },          │
│      runtime_call: "pgy_claim_Int"               │
│    }                                             │
│  - "Option<String> Some" →                       │
│    MIR_INST_DEF {                                │
│      type: { size: 16, align: 8,               │
│              layout: [{tag: 0..3}, {val: 8..}] } │
│    }                                             │
│  - 모든 타입의 바이트 오프셋이 여기에 명시됨      │
└─────────────────────┬───────────────────────────┘
                      │ MIR (명시적 바이트 레이아웃 + Call 명령어)
                      ▼
┌─────────────────────────────────────────────────┐
│  Backend (C / LLVM) - "Dumb Emission"           │
│  - MIR의 layout 정보를 그대로 기계어/ C로 번역   │
│  - 스스로 struct를 만들지 않음                   │
│  - 스스로 sizeof를 계산하지 않음                 │
│  - MIR가 "pgy_claim_Int를 호출하라"고 하면       │
│    그대로 Call 명령어만 생성                      │
└─────────────────────────────────────────────────┘
```

---

## 3. ABI 통일을 위한 구체적 요구사항

### 3.1 MIR에 명시적 타입 레이아웃 추가

현재 `MIRInstruction` 구조체 (`src/compiler/mir.h` 라인 37-53):

```c
typedef struct
{
    size_t      id;
    MIRInstKind kind;
    const char *name;
    const char *slot_anchor;
    const char *arg0;
    const char *arg1;
    const char *result_name;
    ...
} MIRInstruction;
```

**필요한 변경:**

```c
typedef struct
{
    uint32_t    size_bytes;       // 이 타입의 전체 크기 (바이트)
    uint32_t    align_bytes;      // 정렬 요구사항
    uint16_t    field_count;
    struct {
        const char *field_name;
        uint32_t    offset;       // 구조체 시작부터 바이트 오프셋
        uint32_t    field_size;
        uint32_t    field_align;
    } fields[MAX_FIELDS];
} MIRTypeLayout;

typedef struct
{
    ...
    MIRTypeLayout *type_layout;   // 이 명령어가 조작하는 타입의 명시적 레이아웃
    const char    *runtime_fn;    // 호출할 런타임 함수 이름 (백엔드가 추정하지 않음)
    ...
} MIRInstruction;
```

### 3.2 런타임 함수를 MIR에 명시적 Call로 하강

현재: 백엔드가 `pgy_claim_Int`라는 함수 이름을 스스로 알고 있고, 직접 호출 코드를 생성한다.

변경 후:

```
MIR_INST_CALL {
    callee: "pgy_claim_Int",
    callee_abi: {
        return_type: { size: 8, align: 4, passed_via: "registers(rax)" },
        calling_convention: "cdecl"
    },
    result: "%slot_1"
}
```

백엔드는 이 명령어를 보고:
- **C 백엔드**: `PgySlot_Int %slot_1 = pgy_claim_Int();` 생성
- **LLVM 백엔드**: `LLVMBuildCall2(builder, claim_fn, NULL, 0, "")` 생성

둘 다 **동일한 MIR 명령어에서 기계적으로 번역**된다.

### 3.3 C 매크로를 명시적 런타임 라이브러리로 전환

현재 매크로 기반 제네릭:

```c
PGY_CHANNEL_DEFINE(Int, int32_t)
```

이를 다음과 같이 **명시적 C 함수**로 전환해야 한다:

```c
/* pgy_runtime_channels.c - 컴파일된 .o로 링크 */

/* Pergyra 컴파일러가 결정한 레이아웃:
 * size: 160 bytes (Linux x86_64 기준, MIR에 명시)
 * align: 8 bytes
 * 레이아웃 오프셋: buf(0), cap(8), head(16), tail(24), count(32), 
 *                  closed(40), [padding 40..48), mutex(48), 
 *                  cond_not_full(88), cond_not_empty(136)
 */

typedef struct {
    /* 레이아웃은 Pergyra MIR가 결정, C 컴파일러는 이를 따름 */
    int32_t         *buf;       /* offset 0  */
    size_t           cap;       /* offset 8  */
    size_t           head;      /* offset 16 */
    size_t           tail;      /* offset 24 */
    size_t           count;     /* offset 32 */
    bool             closed;    /* offset 40 */
    /* 7 bytes padding (align 8) */
    pgy_mutex_t      mutex;     /* offset 48 - 플랫폼별 typedef */
    pgy_condvar_t    cond_not_full;  /* offset 88 */
    pgy_condvar_t    cond_not_empty; /* offset 136 */
} PgyChannel_Int;

/* 이 함수들은 런타임 라이브러리(.a/.so)에 링크됨 */
PgyChannel_Int  pgy_channel_init_Int(size_t capacity);
bool            pgy_channel_send_Int(PgyChannel_Int *ch, int32_t value);
int32_t         pgy_channel_recv_Int(PgyChannel_Int *ch);
void            pgy_channel_close_Int(PgyChannel_Int *ch);
void            pgy_channel_destroy_Int(PgyChannel_Int *ch);
```

**핵심**: 이 함수들은 `.c` 파일에서 **컴파일되어 정적/동적 라이브러리로 제공**되어야 한다. 매크로가 각 컴파일 단위에서 재생산되는 것이 아니다.

### 3.4 플랫폼별 추상화 계층

`pthread_mutex_t` 같은 플랫폼 의존 타입을 Pergyra 런타임이 직접 통제해야 한다:

```c
/* pgy_runtime_abi.h - Pergyra가 정의하는 플랫폼 독립 타입 */

#if PGY_PLATFORM_LINUX
typedef pthread_mutex_t  pgy_mutex_t;
typedef pthread_cond_t   pgy_condvar_t;
#elif PGY_PLATFORM_WINDOWS
typedef CRITICAL_SECTION  pgy_mutex_t;
typedef CONDITION_VARIABLE pgy_condvar_t;
#endif

/* Pergyra 컴파일러는 pgy_mutex_t의 크기를 플랫폼별로 정확히 안다 */
```

---

## 4. 현재 백엔드의 문제점 구체적 분석

### 4.1 LLVM 백엔드 (`src/codegen/llvm_backend.c`)

| 라인 | 문제 |
|------|------|
| 330-350 | `LLVMStructCreateNamed`로 Slot 타입을 직접 생성. MIR가 레이아웃을 정의해야 함. |
| 3189-3280 | Slot 함수 시그니처를 백엔드가 직접 조립. MIR에 명시적 Call이 있어야 함. |
| 3741+ | main wrapper는 이제 active inventory를 읽는다. 남은 debt는 HIR direct read가 아니라 entry metadata가 별도 declaration IR이 아니라는 점이다. |

### 4.2 LLVM 표현식 헬퍼 (`src/codegen/llvm_expr_helpers_part_*.inc`)

| 라인 | 문제 |
|------|------|
| 230 | `LLVMStructTypeInContext`로 Option 레이아웃을 직접 정의 |
| 1005-1034 | Result 레이아웃을 직접 정의 (tag + ok/err union + error pointer) |
| 1080-1162 | Slot 인자 처리 시 백엔드가 직접 토큰 필드를 찾음 (`%s_token`) |

### 4.3 C 백엔드 (`src/codegen/transpiler.c`)

| 라인 | 문제 |
|------|------|
| 293, 424, 447 | `PgySlot_%s` 타입 이름을 직접 생성. MIR에서 타입 레이아웃을 받아야 함. |
| 570 | `PgyChannel_%s`를 직접 참조 |
| 590-599 | `PgyOption_%s` Some/None 생성자를 직접 호출 |

---

## 5. 실행 계획 (Roadmap)

### Phase 1: MIR에 타입 레이아웃 정보 추가
- [ ] `MIRTypeLayout` 구조체 정의 (`mir.h`)
- [ ] MIR lowering 시 Slot/Option/Result/Channel 레이아웃 계산
- [ ] 레이아웃 계산은 프론트엔드의 타입 정보 + 타겟 ABI 규칙 기반

### Phase 2: 런타임 함수 Call을 MIR에 명시화
- [ ] `MIRInstCall` 명령어 정의
- [ ] 기존 암묵적 런타임 호출을 명시적 Call로 전환
- [ ] 함수 ABI 정보(호출 규약, 레지스터 할당, 반환 방식) 포함

### Phase 3: C 매크로를 명시적 런타임 라이브러리로 전환
- [ ] `pgy_runtime.h`의 매크로를 `pgy_runtime_slots.c`, `pgy_runtime_channels.c` 등으로 분리
- [ ] 각 타입의 레이아웃을 명시적 struct로 정의 (매크로 사용 금지)
- [ ] 정적 라이브러리(`libpgyruntime.a`)로 빌드

### Phase 4: 백엔드를 "Dumb Emission"으로 강등
- [ ] LLVM 백엔드가 스스로 struct 레이아웃을 만들지 않도록 수정
- [ ] C 백엔드가 직접 타입 이름을 생성하지 않도록 수정
- [ ] 백엔드는 오직 MIR 명령어만 읽어서 기계적으로 번역

### Phase 5: 검증
- [ ] `sizeof(PgySlot_Int)`가 Linux/Windows에서 모두 MIR가 예측한 값과 일치함을 테스트
- [ ] ABI 호환성 테스트 스위트 작성
- [ ] 크로스 컴파일 검증

---

## 6. 참고 파일

| 파일 | 역할 |
|------|------|
| `src/runtime/pgy_runtime.h` | 현재 매크로 기반 제네릭 정의 (4117 라인) |
| `src/compiler/mir.h` | MIR 구조 정의 (161 라인) |
| `src/codegen/llvm_backend.c` | LLVM 백엔드 (5272 라인) |
| `src/codegen/transpiler.c` | C 백엔드 (5396 라인) |
| `src/codegen/llvm_expr_helpers_part_*.inc` | LLVM 표현식 헬퍼 split chunks |
| `docs/20_compiler_pipeline_guide.md` | 컴파일러 파이프라인 가이드 |
| `docs/36_ir_pipeline_architecture.md` | IR 아키텍처 설계 문서 |

---

## 7. 결론

**"ABI 통일"은 단순한 호환성 문제가 아니다. 이것은 컴파일러 아키텍처의 권력 구조 문제다.**

현재:
- C 컴파일러(GCC/Clang)가 매크로 전개를 통해 메모리 레이아웃을 결정
- Pergyra 백엔드가 HIR/MIR를 "해석"하며 스스로 판단
- 프론트엔드는 백엔드가 무엇을 하는지 완전히 통제하지 못함

목표:
- Pergyra MIR가 모든 타입의 바이트 레이아웃을 100% 결정
- 백엔드는 MIR 명령어를 기계적으로 번역만 하는 "Dumb Emitter"로 강등
- 런타임 함수는 명시적 라이브러리로 링크되며, 매크로 의존 제거

이것이 이루어질 때, Pergyra는 비로소 **"Slot = 자원 소유"**라는 철학을 컴파일러 레벨에서 완전히 실현할 수 있다.
