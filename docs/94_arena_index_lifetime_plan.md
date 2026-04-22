# Arena + Index Reference Lifetime Plan

마지막 업데이트: 2026-04-22

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
