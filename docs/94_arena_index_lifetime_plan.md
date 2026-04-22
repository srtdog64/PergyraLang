# Arena + Index Reference Lifetime Plan

마지막 업데이트: 2026-04-22 (arena scratch slice 확장)

## 결론

Pergyra는 arena를 **명시적으로 도입**한다.

그리고 arena 도입 방식은 다음 3개를 같이 묶어서 고정한다.

1. `Arena`
2. `Index / stable handle` 기반 교차 참조
3. `타입/역할별 arena 분리`

이 결정이 맞다.

이유는 현재 코드베이스의 문제는 단순한 allocation 비용이 아니라, 다음 3개가 동시에 섞여 있기 때문이다.

- pass-local scratch string
- result-owned long-lived data
- cache / metadata / AST-backed long-lived reference

이 구조에서 raw pointer 공유를 계속 늘리면 dangling, stale cache, cleanup drift가 반복된다.
반대로 `Arena + Index 참조 + 역할별 분리`는 가장 보수적이고 디버깅 가능한 방식이다.

## 현재 문제

다음 경로에 임시 allocation churn이 많다.

- transpiler expression/statement emission
- semantic diagnostics formatting
- type rendering / generic signature formatting
- projection/consumer path formatting

현재 상태의 위험은 다음과 같다.

- `malloc/free`와 context-lifetime scratch allocation이 혼재
- early-return / fail path가 많아 소유권이 산발적
- cache가 short-lived string과 섞일 가능성 존재
- helper가 늘수록 "누가 free해야 하는가"가 흐려짐

즉, 지금 필요한 것은 “더 많은 free”가 아니라 “더 명확한 lifetime 경계”다.

## 설계 원칙

### 1. arena는 수명 기준으로 나눈다

arena는 “자료형”보다 먼저 “언제 reset되는가”로 나눈다.

최소 분할:

- `transpiler scratch arena`
- `semantic scratch arena`
- `semantic result arena`
- `type/render scratch arena` 또는 기존 scratch에 흡수된 render lane

필요하면 이후 추가:

- `llvm emit scratch arena`
- `diagnostic render arena`

### 2. arena 간 교차 참조는 pointer가 아니라 index로 한다

다른 arena의 데이터를 가리킬 때 기본 규칙은 raw pointer가 아니다.

기본 규칙:

- arena 내부 데이터는 자기 arena 내부에서만 pointer로 직접 접근 가능
- 다른 arena 또는 더 긴 수명의 구조가 그것을 참조할 때는 `index` 또는 stable handle 사용

예:

- diagnostic result entry가 scratch string을 직접 들고 있으면 안 된다
- cache가 transpiler scratch string pointer를 저장하면 안 된다
- AST/metadata가 scratch arena allocation을 장기 보관하면 안 된다

맞는 방식:

- string table index
- node index
- entry index
- stable handle struct

### 3. cache는 arena-owned pointer를 저장하지 않는다

강한 규칙:

- cache
- long-lived metadata
- AST-owned field
- global/static registry

이 네 군데에는 arena-owned pointer를 저장하지 않는다.

저장 가능한 것:

- copied stable string
- index/handle
- interned table entry
- owning heap/result-arena allocation

### 4. scratch와 result를 분리한다

semantic 쪽은 특히 이 분리가 중요하다.

- `scratch arena`: 분석 중간 계산, path formatting, temporary rendered type
- `result arena`: 최종 diagnostic payload, 외부에 노출되는 stable message/data

이 둘을 섞으면 result validity가 pass reset에 종속된다.

## 타입/역할별 arena 분리 원칙

여기서 “타입별”은 C struct type별 arena라는 뜻이 아니다.
의미는 다음에 가깝다.

- 역할별
- 수명별
- reset cadence별

권장 분할:

### Transpiler

- `ctx->arena_scratch`
  - expression temp strings
  - type render scratch
  - temporary declarator text

### Semantic

- `analysis scratch arena`
  - borrowed path string
  - projection path formatting
  - generic mismatch intermediate rendering
- `result arena`
  - final diagnostic payload fields
  - exported/stable rendered message fields

### LLVM/codegen

- `llvm scratch arena`
  - temporary symbol names
  - helper render text
  - temporary debug/path strings

## 왜 index 참조가 맞는가

Pergyra는 이미 다음 성격이 강하다.

- staged IR
- inventory lookup
- metadata reuse
- cache-heavy helper path

즉, “object graph를 raw pointer로 직접 엮는 스타일”보다 “inventory + index 참조” 스타일이 구조적으로 더 잘 맞는다.

장점:

- arena reset이 쉬움
- stale pointer 위험 축소
- cache invalidation 경계가 명확
- serialization / debug dump / diagnostics JSON와도 궁합이 좋음

단점:

- access 코드가 약간 장황해짐
- handle/index lookup helper가 필요

하지만 현재 Pergyra의 pain point는 verbosity가 아니라 lifetime drift다.
따라서 tradeoff는 index 쪽이 맞다.

## 도입 순서

### Phase 1. 규칙 고정

- scratch/result lifetime rule 문서화
- cache에 arena pointer 저장 금지 고정
- cross-arena reference는 index/handle 원칙 고정

### Phase 2. 첫 vertical slice

- transpiler temporary strings
- semantic diagnostic formatting scratch strings
- type render scratch helpers

현재 상태:

- transpiler scratch-only temporary는 첫 vertical slice가 시작됐고, 일부는 이미 전환됐다
  - zone authority temporary expression
  - intent priority default literal
  - projection refresh `source_expr`
  - event declaration `event_type`
- semantic diagnostics 쪽도 첫 result seam이 들어갔다
  - `DiagPayload` emit 경로는 이제 `Diagnostic`에 result-owned payload snapshot을 남긴다
  - JSON 출력은 payload field를 함께 노출할 수 있다
  - 즉, scratch formatting data와 result-visible structured data를 분리할 구조 훅이 생겼다
- semantic scratch arena도 첫 실제 사용처가 생겼다
  - `SemanticContext`가 scratch arena를 보유한다
  - ownership diagnostic의 assignment/member/array path string은 scratch arena에서 조립된다
  - result에 남겨야 하는 필드는 payload snapshot이 별도로 복사한다
- 추가로 scratch-safe slice를 3개 더 흡수했다 (2026-04-22)
  - `semantic_preload_stdlib_uses` 의 module path 조립은 function-local `PgyArena`로 이동
    - per-iteration `malloc/free`를 batch arena alloc 하나로 대체
    - path 문자열은 `import_resolver_load_program` 호출 후 재사용되지 않으므로 수명이 함수 내부에서 닫힌다
  - enum method name mangling (`type_checker.c`) 는 `ctx->scratch_arena` 로 이동
    - `symbol_create_function` 이 name을 내부에서 `pergyra_strdup` 하므로 mangled pointer는 함수 바깥으로 탈출하지 않는다
  - `slot_analyze_parallel_block` 의 outer task metadata 배열 3종 (`task_accesses`, `task_counts`, `task_caps`) 은 `sa->ctx->scratch_arena` 로 이동
    - per-task inner 배열은 `collect_slot_accesses` 가 여전히 heap-owned로 관리
    - outer pointer array / counter array 만 arena-owned
- 다시 scratch-safe slice 2개 추가 흡수 (2026-04-22, 2차)
  - `semantic_type_resolution_record_named_dependency` 의 cycle-detection 작업 배열 (`visited`, `path`) 을 `ctx->scratch_arena` 로 이동
    - 배열은 `type_resolution_find_path` 호출에만 사용되고 함수 밖으로 탈출하지 않는다
    - cycle diagnostic 포맷 경로의 `cycle_text` 는 return-contract helper (`type_resolution_format_cycle`) 가 반환하므로 기존 heap-owned 그대로 유지 (이번 slice 범위 밖)
  - `check_match_redundancy` 의 variant coverage tracker (`seen` bool 배열) 을 `ctx->scratch_arena` 로 이동
    - 배열은 case 순회 동안만 쓰이고 함수 밖으로 탈출하지 않는다
- HIR/MIR 쪽도 첫 slice 3개 흡수 (2026-04-22, 3차) — 이후 4차에서 routine-scope arena로 통합됨
  - HIR/MIR에는 아직 context-scope arena가 없어서 `semantic_preload_stdlib_uses` 선례대로 함수-로컬 arena를 사용했다
  - `hir.c:hir_compute_cfg_dominance` 의 `visited`/`postorder`/`idoms` 3배열 → function-local arena (이후 4차에서 `routine->scratch` 로 이동)
  - `hir.c:hir_mark_natural_loop` 의 `in_loop`/`stack` 2배열 → function-local arena (이후 4차에서 `routine->scratch` 로 이동)
  - `mir.c:mir_apply_ssa_rename` 의 outer `next_versions`/`root_versions`/`out_versions` 3배열 → function-local arena (이후 4차에서 `routine->scratch` 로 이동)

- HIR/MIR 4차: **routine-scope scratch arena 도입** (2026-04-22, 4차)
  - `hir.h` HIRRoutine 에 `PgyArena scratch` 필드 추가
  - `mir.h` MIRRoutine 에 `PgyArena scratch` 필드 추가
  - lifecycle 배선:
    - 생성: `hir_append_hidden_method_routine`, `hir_append_decl_and_routine`, `mir_lower` 루프 내 stack-init 에서 `memset` 직후 `pgy_arena_init(&routine.scratch, 0)`
    - 파괴: `hir_destroy()` / `mir_destroy()` 의 per-routine cleanup 블록에 `pgy_arena_destroy(&routine[i].scratch)`
    - OOM 경로: HIR `oom_free_calls` 라벨 + 직접 `goto oom` 분기, MIR ssa/use/stmt/append 실패 분기에 stack-local routine scratch destroy 추가 (배열에 안 들어간 stack routine은 mir_destroy/hir_destroy가 못 봄)
  - 기존 function-local arena 3개 (`dom_arena`, `loop_arena`, `ssa_arena`) 제거, 모두 `&routine->scratch` 로 통합
    - routine 하나당 init/destroy 한 번만 발생 (반복되던 per-pass init/destroy churn 제거)
    - 여러 HIR/MIR pass가 같은 routine 에서 scratch 를 공유 재사용
  - 의도된 비대칭 유지: MIR 패스는 MIRRoutine.scratch 만 씀. `routine->hir_routine->scratch` 는 HIR frozen 계약이라 read/write 모두 금지
  - struct copy 안전성: `pgy_arena_init` 직후 block_size=8192, current=NULL 상태에서 `realloc` 으로 배열에 복사됨. 이후 HIR pass가 array-resident routine 에 접근하면서 블록 할당이 stack routine 주소에서 일어나고 → realloc 이후에는 array-resident 주소로 이동. 첫 use 가 array 이후인 보통 경로는 안전. stack-routine 접근이 realloc 이전에 scratch 를 쓰는 HIR create 경로도 있으나, 이 경우 realloc 의 struct copy 가 `current` 포인터를 그대로 전달하므로 heap 블록은 유지됨 (append_routine 이 succeeded 경로).
- 반대로 반환 계약이 있는 expression string은 아직 전환하지 않는다
- `slot_ref_expr(...)` 같은 helper는 반환 ownership이 섞여 있으므로, scratch 전환을 먼저 시도했다가 되돌린 상태다
- 즉, 현재 원칙은 `scratch-only local temp 먼저, returned string 나중`이다

### Phase 3. result separation

- semantic result-owned payload를 result arena로 이동
- diagnostic renderer가 scratch/result 경계를 넘지 않도록 정리

### Phase 4. cache alignment

- cache가 stable string / index / handle만 잡도록 정리
- 기존 pointer cache는 lifetime audit 후 유지/치환 판단

## 금지 규칙

- scratch arena pointer를 cache에 저장 금지
- scratch arena pointer를 AST field에 저장 금지
- 다른 arena 데이터를 raw pointer로 장기 보관 금지
- result가 scratch arena reset 이후에도 살아야 하는데 scratch pointer를 들고 있게 두는 것 금지

## 첫 적용 후보

### 1. Transpiler

- `strdup_fmt` temporary result
- temporary rendered expression string
- temporary declarator/type render string

### 2. Semantic diagnostics

- ownership escape path string
- projection path / consumer path rendering
- generic mismatch assembled text

### 3. Type rendering

- temporary `render_type_name(...)` family
- generic subject/effective type list formatting helper

## acceptance line

arena 도입의 완료 기준은 “arena를 쓴다”가 아니다.

다음을 만족해야 한다.

- scratch/result boundary가 문서화되어 있다
- cache가 arena-owned pointer를 장기 저장하지 않는다
- cross-arena reference는 index/handle 기준으로 정렬된다
- 최소 1개 vertical slice가 malloc/free churn을 실제로 줄인다
- 기존 semantic/transpile 회귀를 깨지 않는다

## 현재 판정

`Arena + Index 참조 + 타입/역할별 arena 분리`는 채택한다.

이건 미래 최적화 아이디어가 아니라, 현재 구조 debt를 줄이기 위한 **정식 방향**이다.
