# Pergyra 컴파일러 파이프라인 가이드

마지막 업데이트: 2026-07-10

이 문서는 "현재 저장소가 실제로 어떻게 동작하는가"를 설명한다. 이상적인 미래 설계가 아니라, 다음 기여자가 바로 코드를 따라 들어갈 수 있게 만드는 contributor guide다.

논리적 owner/handle/gate의 현재-목표 구분은
[`180_compiler_logical_spine_handles_gates.md`](180_compiler_logical_spine_handles_gates.md)가
소유한다. 이 문서의 파일 순서를 목표 SoT 순서로 오해하면 안 된다.

## 1. 한눈에 보는 파이프라인

```text
.pgy source
  -> read_file()                        [src/pgy_driver.c]
  -> Lexer                              [src/lexer/*]
  -> Parser -> AST_PROGRAM              [src/parser/*]
  -> import inline merge                [src/pgy_driver.c]
  -> semantic_analyze()                 [src/semantic/*]
  -> annotated AST
  -> annotated AST fan-out
       |-> hir_lower()                   [src/compiler/hir.c]
       |-> dir_lower() + dir_validate()  [src/compiler/dir.c]
       `-> rir_lower() + rir_validate()  [src/compiler/rir.c]
           + HIR flow enrichment
  -> mir_lower(HIR, RIR) + mir_validate()[src/compiler/mir.c]
  -> CompilerIRBundle
  -> backend dispatch                   [src/pgy_driver.c]
       -> LLVM backend (default if enabled)
          -> object file
          -> gcc link + runtime
       -> C backend (fallback/reference)
          -> generated .c
          -> gcc compile + runtime
  -> native binary
```

중요한 점은 세 가지다.

- 프론트엔드의 기준 자료구조는 여전히 AST다.
- driver는 backend 진입 전에 항상 `HIR`, `DIR`, `RIR`, `MIR`를 모두 만든다.
- backend runner는 현재 `CompilerIRBundle`을 받는다.
- **양쪽 백엔드 모두 MIR을 수신한다.** compiler MIR entry에서는 C 백엔드가 `transpile_from_mir(bundle->mir, ...)`, LLVM 백엔드가 `llvm_codegen_from_mir(bundle->mir, ...)` 또는 `llvm_codegen_to_object_from_mir(bundle->mir, ...)`를 호출한다.
- C 백엔드: MIR CFG/SSA 기반 함수 본문 emit + intent cleanup/rollback/invalidation block 직접 emit. MIR path에서는 ordinary function / class method / enum method / intent step carrier 누락이 hard error이며, intent run-body도 MIR step/check/eval/meta carrier를 우선 source로 읽는다. 도메인 선언(zone/world/relation/effect)은 아직 AST-carried declaration inventory를 소비한다.
- LLVM 백엔드: MIR 기반 함수/intent/method emit (`llvm_emit_func_from_mir` + intent carrier lookup)이며, MIR path에서는 ordinary function / nominal method / intent carrier 누락이 hard error다. Domain struct 타입 등록은 MIR emit 전에 MIR-carried declaration inventory에서 seed한다.
- backend 정책상 LLVM/native가 primary path이고, C backend는 reference/bootstrap/debug path로 취급한다. 자세한 역할 정의는 [51_c_backend_reference_policy.md](/mnt/e/PergyraLang/docs/51_c_backend_reference_policy.md), 전환 계획은 [52_llvm_native_first_roadmap.md](/mnt/e/PergyraLang/docs/52_llvm_native_first_roadmap.md)에 둔다.
- `driver_run_pipeline_timed()`는 같은 파이프라인을 phase별로 계측한다. ABI benchmark harness는 이 timing을 읽어 CI에서는 hard upper bound, 로컬에서는 comparative metric으로 사용한다. backend timing은 다시 `codegen`, `native_compile`, `link`로 분해된다.
- HIR는 아직 SSA 같은 깊은 IR은 아니지만, 더 이상 단순 top-level 분류 버킷만도 아니다.
- compiler 구현 파일은 점진적으로 역할 분리 중이다. 최근에는 `mir.c`가 `mir_lower(...)`와 block construction만 소유하고, instruction insertion primitive는 [`mir_base_helpers.c`](../src/compiler/mir_base_helpers.c), RIR resource/fallback population은 [`mir_lower_population.c`](../src/compiler/mir_lower_population.c), program validation은 [`mir_program_validate.c`](../src/compiler/mir_program_validate.c), public MIR query/surface refresh는 [`mir_public_surface.c`](../src/compiler/mir_public_surface.c)로 분리했다. `mir_lower_public_api.h`는 declaration-only wrapper로 남고, pass-through `mir_public.inc` shim은 제거됐다. `rir.c`도 named owner header로 flow 분석, scope 수집, dump/validation 표면을 갈라놓았다.
- parser domain constructors also follow the same owner rule: base domain constructors stay in `ast_domain_constructors.c`, while world / intent / zone constructors live in `ast_world_constructors.c`, `ast_intent_constructors.c`, and `ast_zone_constructors.c`.

## 2. 어디서 시작하나

실제 진입점은 `src/pgy_driver.c`의 `main()`과 `run_pipeline()`이다.

이 파일이 하는 일:

- 소스 파일 읽기
- 토큰 dump / AST dump / HIR dump 같은 CLI 모드 처리
- 렉서와 파서 생성
- import 해석과 AST 병합
- 시맨틱 분석 호출
- HIR lowering 호출
- DIR / RIR / MIR lowering + structural validation
- 선택적으로 `driver_run_pipeline_timed()`를 통한 phase timing 수집
- `CompilerIRBundle` 생성
- LLVM 또는 C 백엔드 선택
- 네이티브 바이너리 링크 및 `--run` 실행

즉, "언어 파이프라인의 연결 지점"은 대부분 이 파일에서 보인다.

## 3. 단계별 설명

### 3.1 Lexer

관련 디렉터리:

- `src/lexer/lexer.h`
- `src/lexer/lexer.c`

역할:

- 소스 문자열을 `Token` 스트림으로 변환
- 키워드, 식별자, 리터럴, 연산자 인식
- 행/열 위치 추적
- 파서가 pull 방식으로 `lexer_next_token()`을 호출하게 지원

드라이버에서 `--tokens`를 주면 이 단계만 실행해서 토큰을 출력한다.

### 3.2 Parser

관련 파일:

- `src/parser/parser.h`
- `src/parser/parser.c`
- `src/parser/ast.h`
- `src/parser/ast.c`

주 진입점:

- `parser_create(Lexer *lexer)`
- `parser_parse_program(Parser *parser)`

파서는 재귀 하향 방식이다. `Parser` 구조체에는 단순 토큰 상태 외에도 문맥 플래그가 들어 있다.

- `in_parallel_block`
- `in_with_statement`
- `in_async_context`
- `in_select_statement`
- `in_extern_block`
- `scope_depth`

이 플래그들은 "현재 어떤 문맥에서 이 문법을 허용할지"를 관리할 때 쓰인다. 새 문법을 넣을 때 파싱 가능 위치가 문맥 의존적이면 이 구조체를 먼저 봐야 한다.

파서 결과는 `AST_PROGRAM` 루트를 가진 AST다. 실제 노드 종류는 `src/parser/ast.h`의 `ASTNodeType`에 거의 전부 모여 있다.

현재 AST가 담는 범위:

- 함수, 클래스, extern block
- `let`, `if`, `for`, `while`, `return`
- `with`, `parallel`
- async 관련 구문
- role / ability / party / world
- event 관련 구문
- unsafe / defer / bind
- 호출, 멤버 접근, 배열 접근, 대입, 람다 등 표현식

### 3.3 Import 해석

이 프로젝트에서 import 해석은 별도 모듈 로더가 아니라 드라이버 안에서 처리된다.

관련 위치:

- `src/pgy_driver.c`

흐름:

1. 메인 파일을 파싱해 `AST_PROGRAM` 생성
2. top-level에서 `AST_IMPORT_DECL`을 찾음
3. 가져온 파일을 다시 lex/parse
4. 가져온 AST의 statements를 원래 AST에 inline splice

즉 현재 import는 "semantic 이전의 AST 병합"이다. 따라서 다음 단계들은 import가 이미 펼쳐진 단일 프로그램처럼 본다.

이 구조의 장점은 단순하다는 것이고, 단점은 import 시스템이 아직 드라이버 레벨 구현이라는 점이다.

### 3.4 Semantic

관련 파일:

- `src/semantic/semantic.h`
- `src/semantic/semantic.c`
- `src/semantic/type_checker.*`
- `src/semantic/slot_analyzer.*`

주 진입점:

- `SemanticResult *semantic_analyze(ASTNode *ast)`

`semantic_analyze()`는 현재 두 덩어리로 동작한다.

1. `type_check_program(ast, ctx)`
2. 에러가 없을 때 `slot_analyze_program(ast, sa)`

출력은 `SemanticResult`다.

- `success`
- `annotated_ast`
- `diagnostics`
- `error_count`
- `warning_count`

중요한 점은 `annotated_ast`가 새로운 트리를 만드는 게 아니라, 기존 AST에 타입 정보와 검증 결과를 붙인 같은 트리라는 것이다. 이후 HIR lowering은 이 annotated AST를 입력으로 받는다.

### 3.5 DIR Lowering

관련 파일:

- `src/compiler/dir.h`
- `src/compiler/dir.c`

주 진입점:

- `DIRProgram *dir_lower(ASTNode *annotated_ast, char **error_message)`

현재 DIR의 역할은 "도메인 선언과 관계를 독립 그래프로 정리하는 시작점"이다. backend codegen의 직접 입력은 아니지만, driver는 backend dispatch 전에 항상 `dir_lower()`와 `dir_validate()`를 수행한다.

현재 DIR가 하는 일:

- `ability`, `role`, `party`, `roster`, `world`, `relation`, `effect`, `zone`, `intent`를 node로 수집
- `role -> for type`, `role -> impl ability`, `party -> ability slot`, `world -> zone`, `zone -> effect/relation slot` 같은 declaration edge를 수집
- owner-qualified slot contract node를 직접 만든다
  - `party-slot`
  - `zone-slot`
  - `projection-slot`
  - `authority-slot`
- slot contract edge도 직접 수집한다
  - `party -> party-slot -> ability`
  - `zone -> zone-slot -> nominal type`
  - `owner(relation/effect/zone) -> projection-slot -> nominal type`
  - `projection-slot -> source slot`
  - `zone -> authority-slot -> subject-slot/ability`
- `intent`는 별도 `participant/step` 메타로 정리
  - `where`
  - `using`
  - `who`
  - `requires`
  - `authorized by`
  - `causes`
  - `transfer`
  - step predecessor dependency
- intent 관련 edge도 직접 수집
  - `participant -> type`
  - `step -> zone`
  - `step -> who alias`
  - `step -> required ability`
  - `step -> authorized-by alias`
  - `step -> causes effect`
  - `step(n) -> step(n+1)` dependency

즉 지금 DIR는 "도메인 관계 그래프"다. 아직 flow-sensitive IR은 아니다.

### 3.6 RIR Lowering

관련 파일:

- `src/compiler/rir.h`
- `src/compiler/rir.c`
- `src/compiler/rir_flow.inc`
- `src/compiler/rir_builder.inc`
- `src/compiler/rir_public_surface.h`

주 진입점:

- `RIRProgram *rir_lower(ASTNode *annotated_ast, char **error_message)`
- `bool rir_validate_against_dir(const RIRProgram *rir, const DIRProgram *dir, char **error_message)`

현재 RIR의 역할은 "explicit resource op와 static fact를 실제 코드 계층으로 여는 시작점"이다. 아직 DIR를 직접 입력으로 삼는 완성형은 아니고, annotated AST에서 직접 수집한다. backend codegen의 직접 입력은 아니지만, driver는 backend dispatch 전에 항상 `rir_lower()`, `rir_enrich_with_hir_flow()`, `rir_validate()`, `rir_validate_against_dir()`를 수행한다.

현재 RIR가 하는 일:

- scope 수집
  - `function`
  - `method`
  - `intent`
  - `zone`
  - `relation`
  - `effect`
  - `world`
- fact 수집
  - `resource`
  - `projection`
  - `authority`
  - `capability`
  - `intent-policy`
  - nominal `relation/effect/zone/world` handle
- op 수집
  - `Claim`
  - `Read`
  - `Write`
  - `Release`
  - `Move`
  - `ProjectRefresh`
  - `ProjectPublish`
  - `AttachEffect`
  - `DetachEffect`
  - `LinkRelation`
  - `UnlinkRelation`
  - `Authorize`
  - `AwaitLocal`
  - `AwaitRemote`
  - `CommitIntent`
  - `AbortIntent`
  - `CompensateIntentStep`
  - `using:` / `transfer:`에서 파생되는 conservative handle op
- `DIR` slot-contract graph와의 교차 검증
  - `zone-slot`은 matching `RIR` resource fact/state를 가져야 한다
  - `projection-slot`은 matching resource fact/state를 가져야 한다
  - `projection-slot -> source` 선언 edge가 있으면 matching projection fact도 가져야 한다
  - `authority-slot`은 matching authority fact와 capability fact를 가져야 한다

추가로, 현재 RIR는 scope별 normalized state summary를 만든다.

- tracked resource/projection마다 `initial_state`
- linear scan 이후 `final_state`
- 마지막 관련 op
- transition error 여부

즉 지금 RIR는 "resource graph + transfer ops + static ownership facts"를 넘어서 **CFG 비의존 선형 경로의 normalized state summary**와 **HIR-enriched `flow-block[...]` branch/join lattice summary**까지 가진다. 각 flow fact는 최소한:

- `entry_state`
- `exit_state`
- `merged_from_join`
- `widened_by_loop`
- `entry_conflict`
- `exit-conflict`

를 덤프한다. 완전한 CFG 기반 cleanup/exception merge는 여전히 MIR로 이월한다.

추가로, `RIR-flow`는 resource state가 없는 block도 버리지 않는다. projection op나 intent/world handoff처럼 "상태 lattice로는 안 보이지만 의미론적으로 중요한 것"은 scope-level `semantics=`와 block-level `sem-entry=` / `sem-exit=` conservative flag로 남긴다.

현재 merge는 resource kind를 함께 본다.

- `zone/world handle`은 ownership/borrow 중심으로 병합
- `relation/effect handle`은 detach/sync/dirty lifecycle 중심으로 병합
- move/release와 live state가 섞이면 보수적으로 conflict + invalid로 간다
- `authority` / `projection` / `world-handoff`는 별도 conservative semantic flag로 OR-merge한다

### 3.7 MIR Lowering

관련 파일:

- `src/compiler/mir.h`
- `src/compiler/mir_program.h`
- `src/compiler/mir_types.h`
- `src/compiler/mir_decl.h`
- `src/compiler/mir.c`

MIR 헤더 책임은 다음처럼 분리한다. `mir_types.h`는 instruction, block,
routine 및 CFG/SSA 표현 fact를 소유하고, `mir_decl.h`는 구조화된 declaration
metadata를 소유한다. `mir_program.h`는 이 fact들을 한 lowered program
inventory로 묶는 `MIRProgram`만 소유한다. `mir.h`는 이 소유자들을 노출하는
public operation API/umbrella이며 저장 구조를 다시 선언하지 않는다.

주 진입점:

- `MIRProgram *mir_lower(const MIRLowerRequest *request, char **error_message)`;
  callers initialize the versioned `pergyra.compiler-lowering-api` request
  through `mir_lower_request_init`.

현재 MIR의 역할은 "RIR와 HIR CFG를 붙여 실행-지향 블록/명령 스켈레톤을 만드는 시작점"이다.

현재 MIR가 하는 일:

- routine 수집
  - `function`
  - `method`
  - `intent`
- block 수집
  - HIR CFG가 있으면 block/predecessor/reachability를 그대로 가져옴
  - 없으면 단일 entry block 생성
- instruction 수집
  - RIR op를 `resource-op` instruction으로 entry block에 배치
  - block-local SSA def는 `def` instruction으로 materialize
  - intent의 `CompensateIntentStep` / `AbortIntent`는 rollback block에 배치
  - intent의 `using/transfer` cleanup은 invalidation block에 `DetachInvalidation` marker로 배치
  - HIR phi skeleton은 실제 `phi` instruction으로 반영
- SSA-prep 보강
  - HIR `local_defs`를 block-local renamed value로 materialize
  - HIR `phi` incoming predecessor를 MIR `phi` incoming value 목록으로 materialize
  - versioned name (`score.1`, `score.2`, ...)를 통해 최소 SSA rename skeleton을 유지
  - block별 `ssa_entry_versions` / `ssa_exit_versions`를 유지
- cleanup edge 보강
  - cleanup block이 있는 intent routine은 reachable normal block마다 explicit cleanup successor edge를 가짐
  - richer exceptional CFG:
    - normal block -> cleanup root
    - cleanup root -> rollback block
    - cleanup root -> invalidation block
    - rollback block -> invalidation block
    - block별 `cleanup` / `rollback` / `invalidation` successor 여부를 dump
- terminator 반영
  - HIR branch/return terminator를 MIR `branch` / `return` instruction으로 기록
- use-def 보강
  - phi incoming values는 versioned use로 materialize
  - branch/return은 terminator expression에서 identifier use를 수집
  - resource-op / cleanup instruction도 AST 기반 identifier use를 수집
  - block별 `entry:` / `exit:` version dump를 유지
  - routine-level value summary를 만들어 `def_block`, `def_inst`, `use_count`, `live_in/out block count`, `reaches_cleanup`, `slot_anchor`를 후속 pass가 직접 읽을 수 있게 유지
- slot anchor 보강
  - `resource-op` / `cleanup` instruction은 matching `RIR` op의 `slot_anchor`를 직접 유지
  - `def` / `phi`와 value summary는 base local 이름을 `slot_anchor`로 유지
  - validator는 slot-aware cleanup/resource instruction과 value summary가 slot anchor를 잃는 것을 허용하지 않음
- 실제 pass
  - lowering 중 `liveness` 재계산 수행
  - dead `def` / dead `phi` 제거 DCE pass 수행

즉 지금 MIR는 "실행 구조 스켈레톤 + instruction-level SSA/use-def 시작점 + routine-level value summary + slot-aware exceptional CFG 시작점"이다. full optimizer나 backend 전체를 아직 대체하진 않지만, phi/result/use와 cleanup-root/rollback/invalidation block이 이미 들어갔고, lowering 안에서 실제 liveness/DCE pass가 돌며, C backend에는 branch/return top-level function subset, MIR block 안의 non-SSA statement fallback, intent cleanup CFG를 MIR block/terminator에서 직접 emit하는 vertical slice가 들어갔기 때문에 더 이상 순수 dump 전용 계층은 아니다. 추가로 intent exceptional CFG에 있는 cleanup/resource op는 현재 `pgy_mir_cleanup_op_export(...)` no-op runtime hook 호출로 직접 emit되어, 분석용 op가 codegen 경로에도 명시적으로 남는다.

### 3.8 HIR Lowering

관련 파일:

- `src/compiler/hir.h`
- `src/compiler/hir.c`

주 진입점:

- `HIRProgram *hir_lower(ASTNode *annotated_ast, char **error_message)`
- `void hir_dump_mode(const HIRProgram *hir, FILE *out, HIRDumpMode mode)`
- `bool hir_run_block_pass(HIRProgram *hir, HIRBlockPass *pass, char **error_message)`

현재 HIR의 역할은 아직 "깊은 중간표현 생성"보다는 "top-level program 분류와 백엔드 입력 정규화"에 가깝다. 다만 최근 구조화로 `decl index`와 `routine summary`가 들어와, 단순 버킷 분류기에서 "백엔드/최적화 패스가 읽을 수 있는 indexed view"로 한 단계 올라왔다.

`HIRProgram`이 갖는 핵심 버킷:

- `externs`
- `types`
- `abilities`
- `roles`
- `parties`
- `rosters`
- `worlds`
- `subjects`
- `events`
- `functions`
- `executables`
- `items`

여기서 `items`는 선언 순서를 보존하는 ordered top-level 목록이고, 나머지 배열들은 종류별 빠른 접근용 버킷이다.

추가로, 지금 HIR는 아래 인덱스를 제공한다.

- `decls`
  - 모든 top-level declaration/executable에 대해 stable `id`, `kind`, `phase`, `name`, `ast`를 기록한다.
- `routines`
  - `func`와 `intent`를 별도 summary로 모은다.
  - `direct_calls`
  - `signature_type_refs`
  - `has_control_flow`
  - `is_hosted`
  - `is_action_like`
  같은 최소 분석 결과를 담는다.
  - `func`는 추가로 `cfg`를 가진다.
    - `basic block`
    - `branch/goto/return/unreachable terminator`
    - `loop header` 표식
    - natural loop depth
    - predecessor edge
    - reachability bit
    - dead block count
    - reverse-post-order index
    - immediate dominator
    - dominance frontier
    - dominator tree children
    - block-local def set
    - phi-candidate placement skeleton
    - phi-node skeleton (incoming predecessor list only)
- `callee_routine_ids`
  - direct call name을 실제 routine index로 연결한 call-edge 목록
- `is_entry_reachable`
  - `Main`, exported function, top-level intent를 root로 잡아 계산한 보수적 reachability bit

또한 `hir_run_routine_pass(...)`와 `hir_run_block_pass(...)`가 있어서, 새 패스는 AST 전체를 다시 재귀 순회하는 대신 HIR routine/block summary를 기준으로 "어떤 루틴/블록을 볼지" 먼저 고를 수 있다. 현재 이 pass-friendly 표면 위에 `func` body는 CFG v0로 정규화되고, predecessor/call-edge/reachability/immediate-dominator/dominance-frontier/dominator-tree/natural-loop-depth/local-def/phi-candidate/phi-node-skeleton까지 계산된다.

중요한 구현 특성:

- HIR는 AST 노드를 복사하지 않는다.
- 각 버킷 원소는 여전히 `ASTNode *`다.
- 따라서 현재 HIR는 owning IR이 아니라 annotated AST를 빌려 쓰는 indexed view다.
- lifetime 규칙은 `HIR -> SemanticResult/annotated AST` 순서가 아니라, 반드시 `HIR를 먼저 파기하고 그 다음 annotated AST를 파기`하는 쪽이다.
- `Main` 함수 존재 여부는 lowering 단계에서 `has_main_function`으로 기록된다.
- `AST_IMPORT_DECL`은 여기 오기 전에 드라이버에서 이미 해소되어 skip된다.

즉, 현재 HIR는 "백엔드가 AST 전체를 다시 뒤지지 않도록 top-level 구조를 정리한 뷰"이면서, 동시에 "초기 최적화 패스와 분석 패스가 읽을 수 있는 indexed program view"라고 보는 편이 맞다. 아직 SSA rename 단계는 아니지만, 최소한 routine graph, inter-routine edge, block predecessor, immediate dominator, dominance frontier, dominator tree, natural loop depth, block-local defs, phi candidate, phi-node skeleton은 HIR에서 직접 읽을 수 있다.

### 3.9 Backend: LLVM과 C

관련 파일:

- `src/compiler/compiler.h`
- `src/compiler/compiler.c`
- `src/codegen/llvm_backend.h`
- `src/codegen/llvm_backend.c`
- `src/codegen/transpiler.h`
- `src/codegen/transpiler.c`

드라이버는 HIR 이후에 백엔드를 선택한다.

#### LLVM 백엔드

기본 경로다. `PGY_LLVM_ENABLED`로 빌드된 경우 `pgy`는 LLVM을 기본 백엔드로 사용한다.

주 API:

- `compiler_emit_llvm_ir()`
- `compiler_emit_llvm_ir_to_file()`
- `compiler_build_native_llvm()`

LLVM 경로는 대략 다음 순서다.

1. `llvm_codegen()` 또는 `llvm_codegen_to_object()`
2. `.o` 생성
3. `gcc`로 런타임과 함께 링크
4. 실행 파일 생성

즉 "순수 LLVM 툴체인만 사용"은 아니고, 최종 링크는 현재도 `gcc`와 런타임 C 파일을 사용한다.

#### C 백엔드

참조 구현이자 fallback 경로다.

주 API:

- `compiler_emit_c()`
- `compiler_build_native()`
- `transpile()`

흐름:

1. HIR를 C 코드로 변환
2. `.c` 파일 생성
3. `gcc`로 컴파일
4. 실행 파일 생성

중요한 점은 C 백엔드도 이제 AST 루트를 직접 받지 않고 `HIRProgram`을 입력으로 받는다는 것이다.

### 3.10 Runtime / Link

관련 파일:

- `src/runtime/pgy_runtime.h`
- `src/runtime/pgy_runtime_lib.c`

백엔드가 무엇이든 최종 바이너리는 현재 런타임 심볼에 의존한다. 특히 LLVM 경로도 object만 LLVM이 만들고, 링크 시에는 런타임 C 구현이 같이 들어간다.

따라서 새 기능이 런타임 내장 함수나 ABI를 요구하면 백엔드만 고치면 끝나지 않고 런타임도 같이 수정해야 한다.

## 4. 디렉터리별 책임

```text
src/
  lexer/       토큰화
  parser/      AST 생성
  semantic/    타입 검사, 슬롯 분석, 진단
  compiler/    HIR + 빌드 파사드
  codegen/     LLVM / C 백엔드
  runtime/     런타임 심볼과 헬퍼
  lsp/         LSP 서버
  pgy_driver.c CLI와 전체 파이프라인 연결

examples/      예제 입력
tests/         회귀 테스트, 백엔드 비교 스크립트
docs/          설계 문서와 상태 문서
```

## 5. 새 문법이나 기능을 넣을 때 체크리스트

새 기능이 들어오면 보통 아래 순서로 본다.

1. 렉서
   새 키워드나 토큰 종류가 필요하면 `src/lexer/*` 수정.

2. AST
   새 노드 타입이나 필드가 필요하면 `src/parser/ast.h`와 생성/파괴 로직 수정.

3. 파서
   `parser_parse_*` 계열 함수 추가 또는 기존 분기 확장.
   문맥 제약이 있으면 `Parser` 플래그도 같이 검토.

4. 시맨틱
   타입 검사 규칙, 심볼 등록, 진단 메시지 추가.
   슬롯 자원 경계 상태와 관련 있으면 `slot_analyzer`까지 같이 수정.

5. HIR
   top-level 선언이면 `hir_lower()`의 분류 규칙에 추가.
   expression/statement 레벨 기능이면 HIR 수정이 필요 없을 수도 있다.

6. C 백엔드
   `src/codegen/transpiler.c`에 해당 노드 emit 추가.

7. LLVM 백엔드
   `src/codegen/llvm_backend.c`에 동일 기능 추가.

8. 런타임
   새 builtin, 메모리 모델, ABI가 필요하면 `src/runtime/*` 수정.

9. 테스트
   최소한 단위 테스트와 예제를 추가.
   가능하면 `tests/compare_backends.sh` 대상에도 넣어서 C/LLVM 결과를 맞춘다.

이 프로젝트에서는 "파서만 되고 LLVM은 안 됨" 상태가 금방 쌓이기 쉽다. 새 기능은 가능하면 C와 LLVM을 같이 닫는 편이 맞다.

## 6. 자주 쓰는 로컬 워크플로

빌드:

```bash
make LLVM_ENABLED=1 bin/pgy
make LLVM_ENABLED=1 all
```

예제 실행:

```bash
./bin/pgy examples/hello.pgy --run -v
./bin/pgy examples/slots.pgy --run -v
```

IR 출력:

```bash
./bin/pgy examples/hello.pgy --emit-llvm -o hello.ll
```

테스트:

```bash
make llvm-test-all
make llvm-test-backend-compare
```

파이프라인 중간 상태 확인:

```bash
./bin/pgy examples/hello.pgy --tokens
./bin/pgy examples/hello.pgy --ast
./bin/pgy examples/hello.pgy --hir
./bin/pgy examples/hello.pgy --hir-cfg
./bin/pgy examples/hello.pgy --hir-dom
./bin/pgy examples/hello.pgy --hir-ssa
```

## 7. 현재 구조에서 꼭 알아야 할 제약

### 7.1 HIR는 아직 얕다

이름은 HIR지만, 여전히 expression/statement 수준이 전면 SSA 값 그래프로 재구성되지는 않는다. 다만 이제는 단순 버킷 분류만 있는 것이 아니라 `decl index` / `routine summary` / `signature_type_refs` / direct-call 스냅샷 / routine call-edge / entry reachability / `hir_run_routine_pass(...)` / `hir_run_block_pass(...)` / function CFG v0 / immediate dominator / dominance frontier / dominator tree / natural loop depth / block-local def / phi-candidate placement skeleton / phi-node skeleton이 있으므로, 이후 pass는 최소한 "무엇을 최적화할지"와 "어떤 routine/block이 연결되는지"를 AST 전체 재탐색 없이 고를 수 있다.

### 7.2 import는 드라이버 책임이다

모듈 시스템이나 패키지 로더가 따로 있는 게 아니다. import 확장은 `src/pgy_driver.c`에 있다.

### 7.3 annotated AST가 프론트엔드의 실질 기준 구조다

semantic 단계 이후에도 많은 정보는 AST에 달려 있다. 따라서 AST 구조를 바꾸면 semantic, HIR, 백엔드가 함께 영향을 받는다.

HIR도 여전히 이 annotated AST를 참조한다. 즉 현재의 HIR는 "AST와 분리된 독립 IR"이 아니라 "AST를 빌려 쓰는 pass-friendly view"에 가깝다.

### 7.4 LLVM가 기본이지만 런타임 의존은 남아 있다

LLVM이 object를 만들어도 최종 실행 파일은 런타임 C 구현과 링크된다. 따라서 "완전 독립 LLVM 세계"라고 생각하면 구조를 잘못 읽게 된다.

### 7.5 C 백엔드는 아직 중요한 참조 구현이다

기본 백엔드는 LLVM이지만, 기능 확인과 회귀 비교에서는 C 백엔드가 여전히 기준점 역할을 한다.

## 8. 다음 주자가 처음 읽으면 좋은 파일 순서

추천 순서:

1. `src/pgy_driver.c`
2. `src/parser/parser.h`
3. `src/parser/ast.h`
4. `src/semantic/semantic.h`
5. `src/semantic/type_checker.*`
6. `src/compiler/hir.h`
7. `src/codegen/llvm_backend.c`
8. `src/codegen/transpiler.c`
9. `src/compiler/compiler.c`
10. `tests/compare_backends.sh`

이 순서로 보면 "입력 -> 프론트엔드 -> lowering -> 백엔드 -> 검증" 흐름이 가장 빨리 잡힌다.
