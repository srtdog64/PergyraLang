# Language Completion Board

마지막 업데이트: 2026-04-09 (intent/MIR step carrier 반영)

이 문서는 아직 비어 있거나 부분 구현인 핵심 언어/컴파일러 축을 한 곳에서 추적한다.

이번 라운드에서 착수한 범위:

- 완전한 effect system (라티스 기반 체크)
- capability security (`SecureSlot` 중심)
- MIR -> LLVM 완전 전환
- debugger / formatter / LSP 완성
- stack slot 최적화 (escape analysis)
- generic `where` 제약 검증

## 1. 상태 요약

| 항목 | 현재 상태 | 이번 착수 내용 | 다음 구현 단위 |
|------|-----------|----------------|----------------|
| Effect lattice | 부분 구현 | 현재 `effect_mask` / mismatch 진입점 정리 | effect partial order와 join/check 모델 도입 |
| Capability security | 부분 구현 | `SecureSlot`이 capability의 첫 anchored family라는 점을 보드에 고정 | 토큰/권한/호출 계약을 type rule로 확장 |
| MIR -> LLVM | 진행 중 | 남은 HIR fallback 범주를 명시 | domain/intent/main-wrapper fallback 제거 |
| Debugger / Formatter / LSP | 초기 상태 | 현재 범위를 명시 | formatter AST roundtrip, LSP semantic symbol/diagnostic 확장 |
| Stack slot / escape analysis | 미구현 | 후보 위치를 고정 | alloca escape 분류 + non-escaping local sinking |
| Generic `where` validation | 진행 중 | func/class/role bound validation, function call-site check, class specialization enforcement 추가 | generic instantiation 전반과 richer diagnostics 확장 |

## 2. 항목별 현재 진실

### 2.1 Effect system

현재:

- `Type.function.effect_mask`
- declared/inferred mismatch 진단
- `with effects ...` 계약

부족한 것:

- 라티스 기반 subeffect check
- join/meet를 사용하는 제어흐름 병합
- resource/authority/effect를 하나의 partial order로 검증하는 모델

현재 진입점:

- [type_system.h](/mnt/e/PergyraLang/src/semantic/type_system.h)
- [type_system.c](/mnt/e/PergyraLang/src/semantic/type_system.c)
- [type_checker_flow.c](/mnt/e/PergyraLang/src/semantic/type_checker_flow.c)

### 2.2 Capability security

현재:

- `SecureSlot<T>` + token 기반 최소 capability
- secure read/write/release builtin 규칙
- runtime secure storage policy 일부

부족한 것:

- capability를 `SecureSlot` 밖으로 일반화한 타입 규칙
- authority / token / effect declaration 간 정적 연결
- zone/intent/domain 호출 규약과의 일관된 보안 계약

현재 진입점:

- [type_checker_builtins.c](/mnt/e/PergyraLang/src/semantic/type_checker_builtins.c)
- [slot_security.h](/mnt/e/PergyraLang/src/runtime/slot_security.h)
- [slot_security.c](/mnt/e/PergyraLang/src/runtime/slot_security.c)

### 2.3 MIR -> LLVM 완전 전환

현재:

- LLVM backend는 MIR function emission을 이미 사용
- ordinary non-async function은 MIR routine가 없으면 hard error로 실패한다
- class method는 MIR routine가 있으면 MIR emission을 우선 사용한다
- intent는 cleanup/rollback/invalidation topology를 MIR에서 읽고, run-body step sequence도 MIR `STMT(intent step)` carrier를 읽는다
- 하지만 다음 범주에 HIR fallback이 남음
  - `async func`
  - `intent` step 내부 표현식/행동의 직접 AST 해석
  - domain/world/zone/relation/effect declaration emission
  - main-wrapper / top-level executable orchestration

부족한 것:

- backend에서 HIR direct emission 제거
- MIR-only contract 강제
- `intent`를 expression-level MIR instruction contract로 더 세분화

현재 진입점:

- [llvm_backend.h](/mnt/e/PergyraLang/src/codegen/llvm_backend.h)
- [llvm_pipeline.c](/mnt/e/PergyraLang/src/codegen/llvm_pipeline.c)
- [llvm_mir_emit.c](/mnt/e/PergyraLang/src/codegen/llvm_mir_emit.c)

### 2.4 Debugger / Formatter / LSP

현재:

- `fmt` command는 존재하지만 완성된 formatter 수준은 아님
- LSP는 hover/기본 응답 위주
- debugger는 driver surface는 있으나 완성된 language debugging workflow는 아님

현재 진입점:

- [fmt.c](/mnt/e/PergyraLang/src/compiler/fmt.c)
- [pgy_lsp.c](/mnt/e/PergyraLang/src/lsp/pgy_lsp.c)
- [debugger.c](/mnt/e/PergyraLang/src/compiler/debugger.c)

### 2.5 Stack slot 최적화 / escape analysis

현재:

- backend에는 local alloca / temporary materialization이 많음
- MIR/LLVM 모두 “non-escaping local”을 아직 강하게 분류하지 않음

부족한 것:

- escape classification
- non-escaping stack slot elision
- borrowed/view local과 stack temporary의 통합 규칙

현재 진입점:

- [slot_analyzer.c](/mnt/e/PergyraLang/src/semantic/slot_analyzer.c)
- [llvm_mir_emit.c](/mnt/e/PergyraLang/src/codegen/llvm_mir_emit.c)
- [transpiler.c](/mnt/e/PergyraLang/src/codegen/transpiler.c)

### 2.6 Generic `where` 제약 검증

현재:

- parser는 `where T: Comparable`를 받음
- semantic은 function where-clause의 bound resolvability와 call-site exact-bound 검증을 수행
- class/role where-clause도 unknown bound를 semantic error로 보고한다
- generic class specialization annotation(`Box<Int>`)에서 class-level where constraint를 실제로 강제한다
- 첫 ability-style constraint(`where T: Comparable`)는 subject-bound role의 `impl ability`를 통해 일부 만족 판정을 한다

아직 부족한 것:

- `Comparable` 같은 ability/trait-style constraint를 전 타입군으로 일반화
- generic class 이외의 instantiation 경로 전반에서 constraint enforcement 확장
- richer diagnostics (`expected constraint`, `actual type`, fix suggestion)

현재 진입점:

- [type_checker.c](/mnt/e/PergyraLang/src/semantic/type_checker.c)
- [type_checker_helpers.inc](/mnt/e/PergyraLang/src/semantic/type_checker_helpers.inc)
- [test_semantic_misc.inc](/mnt/e/PergyraLang/src/tests/semantic/test_semantic_misc.inc)

## 3. 이번 라운드에서 실제 시작한 것

### 3.1 `where` 제약 검증

실제 코드 변경:

- function where-clause bound validation을 helper로 공통화
- class where-clause unknown bound validation 추가
- role where-clause unknown bound validation 추가
- generic function call-site exact-bound / ability-style pass/fail 회귀 테스트 추가
- generic class specialization(`let x: Box<Int> = ...`)의 class-level where enforcement 추가

현실적인 현재 해석:

- `where T: Int` 같은 exact-type bound는 일부 검증된다
- `where T: Comparable` 같은 ability/concept constraint는 subject-host + role `impl ability` 범위에서만 일부 검증된다

즉 문서 표면과 실제 구현 사이의 불균형을 숨기지 않고, “partial” 상태로 명시한다.

### 3.2 나머지 5개 항목

이번 라운드에서는 아직 대규모 구현을 넣지 않았다.
대신 각 항목의 실제 entrypoint와 결손 지점을 고정했다.

다음 구현 우선순위:

1. MIR -> LLVM fallback 제거
2. escape analysis / stack slot 분류
3. capability security 계약 일반화
4. effect lattice 기본 join/check
5. formatter/LSP/debugger 기능 확장

## 4. MIR -> LLVM fallback inventory

현재 코드 기준 분류:

### 4.1 Function-level fallback

- `llvm_func_requires_hir_fallback(...)`가 현재 아래 함수는 MIR-only로 내리지 못한다고 판단한다.
  - async function

즉 ordinary function 레벨의 남은 fallback은 현재 async subset으로 줄었다.

추가 메모:
- event-handler typed ordinary function fallback은 제거됐다.
- lambda-bearing ordinary function fallback도 제거됐다.
- smoke 기준 `lambda_expr`는 MIR LLVM 경로로 통과한다.

### 4.2 Program-level fallback

- `llvm_emit_program_from_mir(...)`는 MIR routine emission 전에 HIR을 읽어서
  - nominal/class/enum registration
  - domain declaration passes
  - generic template table population
  - intent forward declaration
  를 수행한다.

이건 registration-only use도 있지만, 현재는 여전히 HIR direct emission이 섞여 있다.

### 4.3 Intent fallback

- `intent`는 이제 MIR routine이 없거나 MIR step sequence가 없으면 LLVM MIR path에서 hard error로 실패한다.
- `intent` cleanup / rollback / invalidation은 MIR topology를 읽는다.
- `intent` run-body의 step 순서도 이제 MIR `STMT(intent step)` carrier를 읽는다.
- 아직 남은 직접 AST 해석:
  - step 내부 `pre/guard/post/invariant/expect`
  - `on:` / subintent expression
  - default action dispatch
  - compensation expression body

즉, `intent`는 더 이상 "전체 body가 HIR fallback"인 상태는 아니다.
남은 부채는 "step 내부 의미를 AST에서 직접 읽는 마지막 층"이다.

### 4.4 Main-wrapper fallback

- main wrapper는 HIR executable/top-level 정보를 직접 참조한다.
- 최종 목표는 executable/entry metadata를 MIR 쪽으로 옮겨 wrapper도 MIR-only로 만드는 것이다.

### 4.5 제거 순서

1. function-level fallback subset 축소
2. intent step 내부 의미를 MIR instruction으로 세분화
3. domain/main-wrapper metadata를 MIR entry metadata로 이동
4. 마지막에 `llvm_codegen_with_mir(...)` 문구와 API 계약을 MIR-only로 갱신
