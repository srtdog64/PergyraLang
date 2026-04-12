# Language Completion Board

마지막 업데이트: 2026-04-12 (visibility/callable export boundary closure, name-token 분해, effect token family 정리, generic default type arg explicit reject, multiple ability-style bounds 회귀, transfer target inference diagnostics/예제 강화, using alias surface 1차 구현, lexical zone context 1차 구현, move transfer short surface 1차 구현, runtime authority validation 실교체, `refresh/publish/bind map { ... }` 연결, explicit `Clone(...)` surface 정식화, world zone embedding warning 추가)

이 문서는 아직 비어 있거나 부분 구현인 핵심 언어/컴파일러 축을 한 곳에서 추적한다.

병렬 실행 계층의 재정렬은 별도 보드로 추적한다.

- 정책: [53_parallel_core_policy.md](/mnt/e/PergyraLang/docs/53_parallel_core_policy.md)
- 보드: [54_parallel_execution_relayout_board.md](/mnt/e/PergyraLang/docs/54_parallel_execution_relayout_board.md)
- 경계/스냅샷 정책: [56_tobject_boundary_snapshot_policy.md](/mnt/e/PergyraLang/docs/56_tobject_boundary_snapshot_policy.md)

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
| Effect lattice | 부분 구현 | `effect_mask` closure, join/meet/compare/conflict API, 함수-level conflict warning, if/match branch-local effect join/conflict 경고, authority/resource helper 정리, `secure` vs `collapse`까지 resource-boundary conflict class 정렬 | authority/resource를 포함한 richer partial order로 확장 |
| Capability security | 부분 구현 | `SecureSlot` + `Token<T>` pairing, channel transport 차단, zone/intent authority 규칙, runtime file I/O/fingerprint policy 강화 | 토큰/권한/호출 계약을 capability/type rule로 더 일반화 |
| MIR -> LLVM | 진행 중 | routine body MIR 경로 고정, compiler MIR entry에서 원본 HIR direct dependency 제거, declaration/top-level inventory를 MIRProgram으로 이관, intent/class/subject MIR carrier 확장, intent step check/eval/meta MIR-only 강제 | dedicated declaration IR 없이 AST-carried inventory를 쓰는 구조 debt 축소 |
| Debugger / Formatter / LSP | 진행 중 | debugger breakpoint/backtrace 명령, formatter `--check`+parse guard+idempotent roundtrip guard, LSP completion/documentSymbol/definition/references/rename 추가 | formatter AST-aware layout, LSP semantic symbol/diagnostic 확장 |
| Stack slot / escape analysis | 진행 중 | return/call/channel-send 기반 slot escape 분류/경고 추가, LLVM AST path에서 non-escaping local slot을 entry-hoist 대신 current-block alloca로 sink | backend local sinking/elision 연결 확대 |
| Generic `where` validation | 진행 중 | func/class/role bound validation, function call-site check, class specialization enforcement 추가 | generic instantiation 전반과 richer diagnostics 확장 |
| Visibility / export boundary | 구현 정렬 | top-level nominal/domain/callable `public/private`, imported private `func/intent/event` call 차단, private `zone/effect` action-contract leakage 차단, syntax/contracts/status docs 동기화 | richer tooling presentation only |

## 2. 항목별 현재 진실

### 2.1 Effect system

현재:

- `Type.function.effect_mask`
- declared/inferred mismatch 진단
- `with effects ...` 계약
- `collapse -> nondeterministic` closure
- 최소 subeffect/subsumption check
- explicit partial-order compare API
- explicit meet/conflict API
- disjoint branch effect join 회귀
- 함수-level conflicting effect 조합 warning
- if/match branch-local effect delta를 join하고 conflicting branch 조합을 warning으로 표시
- authority/resource helper (`requires_authority`, `touches_resource_boundary`)
- resource-boundary conflict class는 `secure` vs `remote|collapse`까지 정렬됨

베타 stable subset:

- function-level declared/inferred effect contract
- join/meet/conflict API
- branch-local join/conflict warning
- `requires_authority` / `touches_resource_boundary` helper
- relation/effect/zone/world projection sync와의 기본 연결

부족한 것:

- richer partial order와 lattice 확장
- join/meet를 사용하는 정교한 제어흐름 병합 모델
- resource/authority/effect를 하나의 partial order로 검증하는 모델

현재 진입점:

- [type_system.h](/mnt/e/PergyraLang/src/semantic/type_system.h)
- [type_system.c](/mnt/e/PergyraLang/src/semantic/type_system.c)
- [type_checker_flow.c](/mnt/e/PergyraLang/src/semantic/type_checker_flow.c)

### 2.2 Capability security

현재:

- `SecureSlot<T>` + token 기반 최소 capability
- secure read/write/release builtin 규칙
- named paired token identifier와 `Token<T>` 타입 pairing 정적 검사
- `Token<T>` 시그니처 기반 secure effect 추론
- capability-bearing 값(`SecureSlot` / `Token<T>`)의 channel transport 차단
- authority가 선언된 zone의 boundary publish/bind에 explicit `by` 강제
- authority-bearing intent step의 `causes` / `transfer` / secure-effect helper call에 `authorized by` 강제
- runtime secure storage policy 일부
- zone authority runtime validation은 더 이상 stub이 아니라 실제 runtime check다. generated C는 inline validator를, LLVM은 exported runtime symbol을 호출한다
- runtime file I/O는 기본적으로 상대경로만 허용하고 `..` traversal을 금지한다
- `PGY_IO_ROOT`가 있으면 runtime file I/O는 root 아래로 고정되고, non-Windows에서는 canonical path로 symlink escape를 차단한다
- hardware fingerprint는 stable fallback identity를 채워 zeroed fingerprint 붕괴를 막는다
- `SecureMemoryLock/Unlock`는 unsupported platform에서 더 이상 조용히 성공하지 않고 explicit `SECURITY_ERROR_UNSUPPORTED_PLATFORM`를 반환한다

부족한 것:

- capability를 `SecureSlot` 밖으로 일반화한 타입 규칙
- authority / token / effect declaration 간 정적 연결
- zone/intent/domain 호출 규약과의 일관된 보안 계약
- capability 문서와 runtime policy를 하나의 계약으로 더 통합

현재 진입점:

- [type_checker_builtins.c](/mnt/e/PergyraLang/src/semantic/type_checker_builtins.c)
- [slot_security.h](/mnt/e/PergyraLang/src/runtime/slot_security.h)
- [slot_security.c](/mnt/e/PergyraLang/src/runtime/slot_security.c)
- [pgy_runtime.h](/mnt/e/PergyraLang/src/runtime/pgy_runtime.h)
- [pgy_runtime_lib.c](/mnt/e/PergyraLang/src/runtime/pgy_runtime_lib.c)

### 2.3 MIR -> LLVM 완전 전환

현재:

- LLVM backend는 MIR function emission을 이미 사용
- ordinary/async function은 MIR routine가 없으면 hard error로 실패한다
- async function도 MIR routine가 있으면 같은 경로로 직접 emit한다
- class method는 MIR routine가 있으면 MIR emission을 우선 사용한다
- subject method도 MIR direct path를 우선 사용한다
- top-level executable main-wrapper는 synthetic executable MIR routine을 통해 wrapper의 직접 AST loop를 줄였다
- intent는 cleanup/rollback/invalidation topology를 MIR에서 읽고, run-body step sequence도 MIR `STMT(intent step)` carrier를 읽는다
- intent step의 `pre/guard/post/expect/invariant/on/subintent/compensate` 및 `zone/who/transfer` metadata는 LLVM에서 MIR carrier를 필수로 요구한다
- 하지만 다음 범주에 decl-inventory debt가 남음
  - domain/world/zone/relation/effect declaration emission이 dedicated decl IR 없이 AST inventory를 소비함
  - intent/domain 내부의 일부 보조 해석이 AST inventory helper에 의존함

부족한 것:

- backend에서 AST inventory helper debt 제거
- MIR-only contract 강제
- `intent`를 expression-level MIR instruction contract로 더 세분화

현재 진입점:

- [llvm_backend.h](/mnt/e/PergyraLang/src/codegen/llvm_backend.h)
- [llvm_pipeline.c](/mnt/e/PergyraLang/src/codegen/llvm_pipeline.c)
- [llvm_mir_emit.c](/mnt/e/PergyraLang/src/codegen/llvm_mir_emit.c)

현재 검증/부채 요약은:

- [44_llvm_backend_coverage.md](/mnt/e/PergyraLang/docs/44_llvm_backend_coverage.md)
- [62_llvm_backend_debt_ledger.md](/mnt/e/PergyraLang/docs/62_llvm_backend_debt_ledger.md)

### 2.4 Debugger / Formatter / LSP

현재:

- `fmt`는 `--write` 외에 `--check`와 formatted output parse-guard를 가진다
- formatter는 formatted output을 한 번 더 포맷해 동일성을 확인하는 idempotent roundtrip guard를 가진다
- LSP는 diagnostics/hover에 더해 completion/documentSymbol/definition/references/rename을 제공한다
- debugger는 breakpoint set/clear/list와 single-frame backtrace를 지원한다

현재 진입점:

- [fmt.c](/mnt/e/PergyraLang/src/compiler/fmt.c)
- [pgy_lsp.c](/mnt/e/PergyraLang/src/lsp/pgy_lsp.c)
- [debugger.c](/mnt/e/PergyraLang/src/compiler/debugger.c)

### 2.5 Stack slot 최적화 / escape analysis

현재:

- backend에는 local alloca / temporary materialization이 많음
- semantic `slot_analyzer`는 이제 `return/call/channel-send` 기반 slot escape를 분류하고 conservative warning을 낸다
- LLVM AST emission path와 hosted method path는 non-escaping local slot에 대해 entry-hoist 대신 current-block alloca sinking을 시작했다
- MIR temporary/storage placement까지의 실제 local sinking/elision은 아직 미완료다

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
- foreign non-exported ability는 cross-module `role impl ability` / `action requires`에서 거부된다
- function/class where-clause constraint 실패는 이제 `Reason / Fix` 형식의 richer diagnostic을 제공한다

베타 stable subset:

- default type argument는 explicit reject
- exact bound (`where T: Int`) 동작
- ability-style bound (`where T: Comparable`) baseline 동작
- multi-bound (`where T: A + B`) baseline 동작
- `ability<T> where ...` bound는 reference/impl 경로에서 재검증

아직 부족한 것:

- `Comparable` 같은 ability-style constraint를 전 타입군으로 일반화
- generic class 이외의 instantiation 경로 전반에서 constraint enforcement 확장
- richer diagnostics (`expected constraint`, `actual type`, fix suggestion)

현재 진입점:

- [type_checker.c](/mnt/e/PergyraLang/src/semantic/type_checker.c)
- [type_checker_helpers.inc](/mnt/e/PergyraLang/src/semantic/type_checker_helpers.inc)
- [test_semantic_misc.inc](/mnt/e/PergyraLang/src/tests/semantic/test_semantic_misc.inc)

### 2.7 `requires` generic ability reference

현재:

- `ability<T> { ... }` generic declaration 지원
- `ability { fields field: Type; }`는 일반 type parser를 타므로 `List<Int>` 같은 generic field type 자체는 받을 수 있다
- `action requires Ability` / intent step `requires:` / zone authority `requires`는 이제 type-reference AST를 저장한다
- `requires SomeAbility<T>` 같은 generic ability reference를 parser/AST/semantic/DIR 경로에서 표현할 수 있다
- semantic은 이제 generic reference를 blanket reject하지 않고 declaration 존재 여부, arity, type argument 해석을 검증한다
- `impl ability Ability<T>`도 type-reference AST를 사용한다
- subject/role satisfaction은 이제 base name이 아니라 full generic ability ref 기준으로 판정된다
- foreign hidden ability / default-export ability 정책도 generic ability ref와 같은 규칙으로 정렬되었다
- `ability<T> where ...` declaration surface도 parser/AST/semantic에 연결되었다
- generic mismatch diagnostics는 이제 `required Ability<Int>` vs `actual Ability<String>`를 action/intent step/zone/party role slot 경로에서 직접 보여준다
- `ability<T> where ...` bound는 `requires Ability<...>` / `impl ability Ability<...>` / party role slot ability ref에서도 다시 검증된다

베타 stable subset:

- `ability<T>`
- `requires Ability<T>`
- `impl ability Ability<T>`
- zone authority generic ability ref
- action/intent step/zone/party role slot mismatch diagnostics
- hidden/default-export와 generic ref visibility alignment

부족한 것:

- generic ability declaration의 richer constraint validation
- 예제 전반의 generic ability authoring 확대

현재 진입점:

- [parser_decl.c](/mnt/e/PergyraLang/src/parser/parser_decl.c)
- [parser_intent.c](/mnt/e/PergyraLang/src/parser/parser_intent.c)
- [parser_domain.c](/mnt/e/PergyraLang/src/parser/parser_domain.c)
- [ast.h](/mnt/e/PergyraLang/src/parser/ast.h)

## 3. 이번 라운드에서 실제 시작한 것

### 3.1 Authoring surface compression

현재:

- function/action clause parser는 이미 table-driven이고 clause 순서는 고정이 아니다
- `transfer target -> using` inference 구현됨
- `transfer target -> where` inference 구현됨
- explicit `using` zone binding -> `where` inference 구현됨
- explicit `where` + unique matching participant -> `using` inference 구현됨
- intent transfer mismatch diagnostics는 이제 추론된 `using/where`를 직접 보여준다
- example smoke에 아래 예제가 올라가 있다
  - [function_clause_order_minimal.pgy](/mnt/e/PergyraLang/examples/function_clause_order_minimal.pgy)
  - [generic_ability_requires_minimal.pgy](/mnt/e/PergyraLang/examples/generic_ability_requires_minimal.pgy)
  - [transfer_move_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_minimal.pgy)
  - [intent_inference_minimal.pgy](/mnt/e/PergyraLang/examples/intent_inference_minimal.pgy)
  - [zone_context_minimal.pgy](/mnt/e/PergyraLang/examples/zone_context_minimal.pgy)
  - [six_item_alignment_demo.pgy](/mnt/e/PergyraLang/examples/six_item_alignment_demo.pgy)

현재 구현됨:

- top-level `within Zone { ... }` lexical zone context 1차
- `using self.route as route;` / `using self.seal as seal;` explicit alias
- `move <from-alias> to <to-alias>;` transfer short surface 1차
- `who`의 유일 subject participant + matching action 기반 추론
- `refresh/publish/bind ... map { target <- source; }` field-level projection remap
- `Clone(...)` builtin이 semantic/C backend/LLVM 경로까지 닫혔다
- world constructor에 direct zone binding을 넘기면 implicit-copy warning이 난다

아직 설계 단계:

- nested lexical zone context
- file-global `zone context`
- type-directed `move <value> to <ZoneType>;`

### 3.0 token split debt

이 debt는 현재 구현 기준으로 **닫혔다**.
아래 항목은 과거 문제 정의였고, 이제는 historical note로만 남는다.

- 과거에는 `subject -> TOKEN_CLASS`
- 과거에는 `tobject -> TOKEN_STRUCT`
- 과거에는 `object`가 contextual keyword였다

즉 코어 존재론이 lexer/parser 단계에서 aliasing되던 부채가 있었다.

현재:

- `subject/class/struct/object/tobject/vessel/intent/world/roster/relation/effect/zone/event`
  는 declaration-grade token으로 분리됐다
- relation `between` clause는
  - built-in endpoint kind
  - concrete named endpoint type-reference AST
  로 구조화됐다

남은 것:

1. clause word 일부의 contextual keyword 유지 여부 재판단
2. relation endpoint diagnostic 문자열 품질 개선
3. parser/doc 테스트 표현 추가 정리

작성성 압축 방향:

- parser raw ordering debt는 줄어들었다
  - function/action clause parser는 `where`, `with effects`, `requires`,
    `within`, `causes`, `authorized by`를 table-driven으로 처리한다
  - clause 순서는 현재 고정이 아니고, duplicate clause는 명시적으로 진단한다
- 남은 부채는 parser ordering보다 authoring compression 쪽이다
  - intent 계약 추론
  - nested/file-global lexical zone context
  - type-directed transfer 축약 표면
  - domain-first diagnostics
  - action/authority contract 중복 축약
  - domain-first diagnostics는
    - `subject` by-value return
    - authority-bearing intent step
    - transfer target / `using` mismatch
    부터 `Reason` / `Fix` 형식으로 올리는 중이다
  - 여기서 `action -> step`은 inheritance가 아니라 contract inference로 다룬다
  - lexical zone context / using-transfer 정렬 규칙은
    [60_zone_context_and_transfer_inference.md](/mnt/e/PergyraLang/docs/60_zone_context_and_transfer_inference.md)
    에 별도 설계로 분리했다

- [58_keyword_authorship_pain_points.md](/mnt/e/PergyraLang/docs/58_keyword_authorship_pain_points.md)
- [59_authoring_surface_compression_plan.md](/mnt/e/PergyraLang/docs/59_authoring_surface_compression_plan.md)

- [57_token_split_plan.md](/mnt/e/PergyraLang/docs/57_token_split_plan.md)
  기준으로
  - `TOKEN_SUBJECT`
  - `TOKEN_OBJECT`
  - `TOKEN_TOBJECT`
  를 추가하고 declaration dispatch를 token-kind 기반으로 정리한다.

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
2. capability security 계약 일반화
3. effect lattice richer partial order
4. escape analysis / stack slot 분류
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
- `intent` step check(`pre/guard/post/invariant/expect`)와 eval(`on:` / subintent / compensate), default dispatch alias, zone metadata(`where/using/from/who`)도 MIR carrier를 우선 읽는다.
- `intent` forward declaration/signature seed도 MIR `IntentParticipant` carrier를 우선 읽는다.
- 남은 직접 AST/HIR 의존:
  - MIR participant metadata가 없을 때의 compatibility fallback
  - domain/world/zone declaration emission 자체

- 이번 라운드에서 줄인 것:
  - zone slot resolution은 LLVM class registry metadata(`is_subject_slot`)를 우선 사용한다
  - default action dispatch는 function registry metadata(`is_action`, `action_self_only`)를 사용한다
  - intent participant alias/type seed는 MIR `IntentParticipant` carrier를 우선 사용한다
  - intent forward declaration도 MIR participant type seed를 우선 사용한다
  - domain sync helper 선택도 LLVM class registry metadata(`domain_kind`, `sync_function_name`)를 우선 사용한다
  - world sync의 zone class lookup 하나는 이제 AST `world_zone` declaration lookup 대신 class registry의 field LLVM type 역조회로 처리한다

즉, `intent`는 더 이상 "전체 body가 HIR fallback"인 상태는 아니다.
남은 부채는 "intent orchestration의 마지막 symbol/bootstrap 층"이다.

### 4.4 Class method fallback 현실

- HIR는 이제 class/subject method를 hidden routine로도 수집한다.
- MIR/RIR matching도 `owner_name`을 같이 보도록 강화됐다.
- empty method body도 이제 valid MIR routine로 취급한다.
- 현재 상태:
  - plain `class` method: MIR direct path 우선, routine 없으면 hard error
  - `subject` method: MIR direct path 우선, routine 없으면 hard error
  - `subject` method: MIR direct path 우선, 없으면 compatibility fallback 유지
  - direct MIR method emission을 모든 nominal family로 hard-require 하는 단계는 아직 남아 있다
- 확인된 현재 한계:
  - `subject` method는 MIR direct emission 우선 경로는 들어갔지만, 아직 hard-require fallback 제거까지는 안 갔다
- 즉 이 항목은 "plain class method는 전진, subject method는 다음 단계" 상태다.

### 4.4 Main-wrapper fallback

- main wrapper는 HIR executable/top-level 정보를 직접 참조한다.
- 최종 목표는 executable/entry metadata를 MIR 쪽으로 옮겨 wrapper도 MIR-only로 만드는 것이다.

### 4.5 제거 순서

1. function-level fallback subset 축소
2. intent step 내부 의미를 MIR instruction으로 세분화
3. domain/main-wrapper metadata를 MIR entry metadata로 이동
4. 마지막에 `llvm_codegen_with_mir(...)` 문구와 API 계약을 MIR-only로 갱신
