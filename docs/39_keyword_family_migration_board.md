# Keyword Family Migration Board

마지막 업데이트: 2026-04-12

이 문서는 [`37_compiler_contracts.md`](./37_compiler_contracts.md)의 키워드 taxonomy와 매트릭스를 실제 구현 작업으로 연결하는 보드다.

목표:

- 키워드 가족별 현재 상태를 한 눈에 본다
- parser / semantic / DIR / RIR / MIR / backend 중 어디가 아직 비어 있는지 추적한다
- migration 순서를 고정한다

상태 표기:

- `done`     : 계약과 구현이 대체로 일치
- `partial`  : 계약은 있으나 구현 비대칭 또는 fallback이 남아 있음
- `weak`     : 의미는 있으나 구현 근거가 약함
- `todo`     : 아직 본격 착수 전

## 1. Family Board

| 가족 | Lexer/Parser | Semantic | DIR | RIR | MIR | Backend | 상태 | 다음 작업 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `subject / class` | done | partial | partial | weak | weak | partial | `partial` | token split 이후 semantic/runtime contract와 진단 톤 정렬 |
| `object / tobject / struct` | done | partial | partial | partial | partial | partial | `partial` | nominal split 이후 projection/boundary contract 진단 강화 |
| `vessel` | partial | weak | weak | weak | weak | weak | `weak` | subject 내부 상태 수용체 계약 고정 |
| `ability / role` | done | partial | done | partial | n/a | partial | `partial` | role completion과 backend dispatch contract 정리 |
| `party` | done | partial | done | partial | partial | weak | `partial` | collaboration contract와 runtime/backend usage 정리 |
| `relation / effect` | partial | partial | done | partial | partial | partial | `partial` | projection/lifecycle MIR-only path 강화 |
| `zone / world` | partial | partial | done | partial | partial | partial | `partial` | top boundary metadata를 MIR 중심으로 이동 |
| `intent / step` | partial | partial | done | partial | partial | partial | `partial` | LLVM HIR fallback 제거 1순위 |
| `where / who / using / transfer` | partial | partial | done | partial | partial | partial | `partial` | clause metadata를 MIR routine contract로 승격 |
| `rollback / compensate / success / failure` | partial | partial | partial | partial | partial | partial | `partial` | exceptional path를 MIR-only로 고정 |
| `slot / authority / refresh / publish / bind` | partial | partial | done | done | partial | partial | `partial` | ABI + MIR metadata를 backend 실사용으로 연결 |

## 2. Recommended Order

1. `object / tobject / struct`
2. `intent / step`
3. `rollback / compensate / success / failure`
4. `zone / world`
5. `slot / authority / refresh / publish / bind`
6. `subject / class`
7. `ability / role / party`
8. `vessel`

## 3. Working Rules

- 새 분석이나 패치를 시작할 때는 반드시 어떤 "키워드 가족"을 다루는지 명시한다
- parser-only 정리는 backend migration 완료로 간주하지 않는다
- MIR-only backend 전환과 직접 연결되는 가족은 `intent`, `zone/world`, `slot/projection/authority`, `object/tobject`다
- `partial` 가족은 문서, parser, semantic, backend 중 최소 하나가 계약과 어긋난다는 뜻이다

## 4. Current Focus

현재 착수 주제:

- `surface trust / declaration-implementation alignment`

이유:

- 최근 실제 회귀는 "새 키워드 추가"보다 "이미 된다고 말한 표면을 끝까지 믿을 수 있게 만드는가"에서 더 자주 발생했다
- mixed `ability + zone` module export, `HashMap<Int, V>`, MIR cleanup topology, host method receiver typing처럼 구현 depth보다 표면 신뢰도가 더 중요한 항목이 드러났다
- 키워드 가족 작업도 이제는 개별 기능 확장보다 "문서/semantic/backend/examples가 같은 말을 하게 만드는 것"이 우선이다

이번 주기에서 닫힌 대표 항목:

- `HashMap<Int, V>` 표면을 semantic/runtime/test로 정렬
- default-export `ability`와 explicit export module 정책 충돌 제거
- MIR cleanup/rollback/invalidation topology 회귀 복구
- nominal host receiver type 오염으로 인한 C backend 오발행 복구
- `order_analytics`를 sketch가 아니라 compile-smoke covered example로 승격
- declaration name을 reserved keyword 재사용 없이 일반 식별자로 고정
- anchored-handle / `DeviceSlot` parameter diagnostic과 semantic test expectation 정렬

## 5. `object / tobject / struct` 상세 비대칭 표

### 5.1 구현 현황

| 계층 | 현재 구현 | 비대칭 지점 | 위험 | 고정 방향 |
| --- | --- | --- | --- | --- |
| Lexer | `object`, `tobject`, `struct`가 모두 별도 reserved token으로 분리됨 | 예전 alias/token-share 설명이 남아 있으면 문서가 더 틀리게 보인다 | 구현은 닫혔는데 문서가 불신을 만든다 | 모든 핵심 문서에서 nominal token split을 현재 기준으로 고정 |
| Parser | `object`는 `parse_object_declaration`, `tobject`는 `parse_tobject_declaration`, `struct`는 `parse_struct_declaration` | parser 진입은 분리됐지만 진단/설명에서 contract 차이가 약해질 수 있다 | "storage만 다르고 의미는 같다"는 오해 | parser split보다 semantic contract와 진단 차이를 계속 강조 |
| AST | `NOMINAL_DECL_OBJECT`, `NOMINAL_DECL_TOBJECT`, `NOMINAL_DECL_STRUCT`로 분리 | 표면 문법 공유에 비해 이 차이가 문서/진단에서 충분히 드러나지 않는다 | 사용자와 backend가 같은 것으로 오해할 수 있다 | AST nominal kind를 단일 semantic truth로 삼는다 |
| Semantic | `ToObject`/`ToTObject` builtin과 field mutability 규칙은 이미 분리됨 | 일부 진단은 아직 `class/object` 축 위주이며 `struct/tobject` 대비가 약하다 | projection/transfer contract가 흐려진다 | 모든 진단에서 `object=local projection`, `tobject=boundary transfer`, `struct=plain nominal data`를 명시 |
| DIR | `refresh`와 `publish` 경로가 분리돼 있음 | 키워드 family 관점의 설명과 이름이 아직 약하다 | projection lowering 이유가 추적되지 않는다 | DIR에서 projection kind를 로그/덤프에 드러낸다 |
| RIR | projection/resource 흐름이 있으나 `object/tobject` 이름으로는 잘 보이지 않음 | semantic 계약이 중간 IR에서 흐릿해진다 | backend가 왜 transfer인지 모르고 문자열 lowering에 기대기 쉽다 | projection/boundary contract를 RIR metadata로 유지 |
| MIR | `type_layout`와 resource/export 훅은 있으나 nominal contract가 backend에서 실사용되지 않는다 | MIR 메타데이터가 있으나 transpiler/LLVM이 충분히 소비하지 않는다 | ABI 전환이 반쪽으로 남는다 | backend는 nominal/type layout을 MIR metadata에서만 읽는다 |
| Backend | LLVM/C 모두 immutability와 일부 layout 판단에서 `OBJECT/TObject`를 함께 취급 | 의미 계약은 다르지만 codegen 분기에서 자주 묶인다 | `object`와 `tobject`가 같은 lowering class처럼 굳을 수 있다 | 공통 storage/layout은 공유해도 contract 분기는 유지하고, boundary path는 `tobject` 전용으로 고정 |

### 5.2 코드 근거

| 지점 | 근거 | 의미 |
| --- | --- | --- |
| Parser statement dispatch | `src/parser/parser.c` | `object / tobject / struct`는 모두 별도 nominal declaration entry를 가진다 |
| AST nominal split | `src/parser/ast.c` | `subject/class`, `object`, `tobject`, `struct`, `vessel`는 모두 별도 nominal kind로 내려간다 |
| Semantic projection builtin | `src/semantic/type_checker_builtins.c` | `ToObject`와 `ToTObject`는 이미 별도 nominal contract를 기대한다 |
| Semantic mutability | `src/semantic/type_checker.c` | `object`와 `tobject`는 둘 다 immutable 취급이지만 이유가 다르다 |
| LLVM nominal lowering | `src/codegen/llvm_backend.c` | backend는 `OBJECT/TObject`를 자주 같은 storage bucket으로 본다 |

### 5.3 시작 작업

1. parser/semantic/backend에서 `struct alias`처럼 읽히는 주석과 진단을 전부 제거한다
2. `object/tobject` 차이를 MIR metadata 이름으로 남긴다
3. backend 공통 layout 처리와 boundary-only transfer 처리를 분리한다

## 6. `intent / step` HIR fallback 제거 준비

### 6.1 현재 LLVM fallback map

| fallback 지점 | 현재 구현 | 문제 | 제거 조건 |
| --- | --- | --- | --- |
| ordinary function | MIR routine이 없거나 instruction이 비면 `llvm_emit_func_decl(stmt, ctx)` | backend가 MIR completeness를 강제하지 못한다 | routine lookup 실패 시 hard error로 전환 |
| intent declaration | `AST_INTENT_DECL`은 항상 `llvm_emit_intent_decl(stmt, ctx)` | intent가 MIR-only backend 바깥에 남아 있다 | intent routine과 clause metadata를 MIR에서 직접 emit |
| class methods | `ClassName_MethodName`로 AST 이름을 임시 변경한 뒤 HIR emission | backend가 AST mutation에 기대고 있다 | method routine lookup을 MIR name table로 전환 |
| main wrapper | `llvm_emit_mir_main_wrapper(ctx->hir, ctx)` | executable/main metadata가 HIR에 남아 있다 | MIR entry metadata로 wrapper 생성 |
| top-level executable flow | HIR executable/top-level statements를 wrapper가 참조 | backend가 top boundary를 HIR에서 읽는다 | MIR entry routine 또는 top-level schedule metadata 필요 |

### 6.2 정확한 코드 위치

| 위치 | 현재 상태 | 후속 작업 |
| --- | --- | --- |
| `src/codegen/llvm_backend.c` `llvm_emit_program_from_mir` | Pass 4 전체가 HIR fallback 블록 | Pass 4를 migration debt 블록으로 표시하고 항목별 제거 |
| `src/codegen/llvm_backend.c` `llvm_emit_intent_decl` | intent는 HIR lowering 전용 경로 | MIR intent routine schema를 먼저 확정 |
| `src/codegen/llvm_backend.c` `llvm_emit_mir_main_wrapper` | wrapper가 `hir->executable_count`, `hir->has_main_function`에 의존 | MIR program entry metadata 도입 |

### 6.3 intent MIR-only 조건

| 필요한 항목 | 이유 |
| --- | --- |
| routine kind가 `intent`임을 식별하는 metadata | ordinary function과 lowering 규칙이 다르다 |
| `where / who / using / transfer` clause metadata | orchestration/gating과 runtime hookup을 HIR 없이 재구성해야 한다 |
| cleanup / rollback / invalidation / success / failure edge 정보 | exceptional flow를 MIR에서 직접 그려야 한다 |
| subintent call contract | bool-gated orchestration을 backend가 HIR 없이 emit해야 한다 |
| entry/exit convention | intent 결과, failure policy, trace hook를 MIR contract로 고정해야 한다 |

### 6.4 시작 작업

1. `llvm_emit_program_from_mir`의 fallback 지점을 주석이 아니라 계약 표 기준으로 추적한다
2. intent MIR routine schema 초안을 문서에 추가한다
3. ordinary function fallback부터 hard error 전환 조건을 만든다
4. 그 다음 intent fallback을 제거한다

## 7. `subject / class` token/semantic 비대칭 정리

### 7.1 구현 현황

| 계층 | 현재 구현 | 비대칭 지점 | 고정 방향 |
| --- | --- | --- | --- |
| Lexer | `subject`는 `TOKEN_SUBJECT`, `class`는 `TOKEN_CLASS`로 분리됨 | 오래된 shared-token 설명이 남아 있으면 문서가 더 틀려진다 | token split 완료를 active docs 기준으로 고정 |
| Parser | `subject`와 `class`는 별도 declaration entry로 분기한다 | parser split은 닫혔고, 남은 건 semantic/runtime contract 차이 설명이다 | parser보다 semantic contract/diagnostic 차이를 계속 강화 |
| AST | `ast_create_class` 이후 `ast_create_subject`는 `NOMINAL_DECL_SUBJECT`로 덮어쓴다 | token 단계 정보는 사라지고 nominal kind가 truth가 된다 | AST nominal kind를 backend까지 유지 |
| Semantic | subject는 vessel field 제약과 authority/resource 흐름이 붙고, class는 일반 nominal type | 일부 진단은 여전히 `class` 중심 용어를 사용한다 | subject 전용 진단/규칙을 분리 |
| Backend | 대부분 class-like storage/emission 경로를 공유 | subject만의 orchestration/resource semantics가 codegen에서 약하다 | storage는 공유 가능하되 runtime contract는 subject 전용 metadata로 분리 |

### 7.2 코드 근거

| 지점 | 근거 | 의미 |
| --- | --- | --- |
| Parser dispatch | `src/parser/parser.c` | `subject`와 `class`는 lexer token 단계부터 분리돼 진입한다 |
| AST constructor | `src/parser/ast.c` | `subject`는 `NOMINAL_DECL_SUBJECT`를 가진 별도 declaration |
| Semantic vessel check | `src/semantic/type_checker.c` | subject field에는 vessel 타입 제약이 붙는다 |
| Backend naming/runtime | `src/codegen/llvm_backend.c`, `src/codegen/transpiler.c` | storage/emission은 class-like지만 semantic contract는 더 무겁다 |

### 7.3 시작 작업

1. semantic 진단에서 `class` 중심 표현을 `class/subject` 또는 subject 전용 문구로 정리한다
2. subject 전용 metadata가 DIR/RIR/MIR에서 유지되는지 점검한다
3. backend에서 subject를 class alias처럼 다루는 경로를 표로 뽑는다
