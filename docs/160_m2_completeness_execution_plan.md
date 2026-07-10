# 160. M2 완전성 실행 계획 (Whole-Compiler Self-Host)

Status: `execution-plan`. 작성 2026-07-05. **BDFL 결정(2026-07-05): M2 완주 —
전체 self-eating bootstrap을 종착지로.** 이 문서는 docs/158 §3(완전성 임계 경로)을
**파일·rung 단위**로 마저 채운 것 — 나 없이 집행 가능한 정밀도. 상위: docs/158
(전략), docs/self_hosted/01(Stage 5 정의). 이 문서의 semantic 분해는 C 계층
51,283 LOC / 258 파일의 실측 매핑(2026-07-05).

---

## 0. 실측 현재 위치 (이 계획의 출발점)

- 전체 컴파일러 self-host: **~6.57%**. semantic 계층은 **6.4%**(3,276 Pergyra LOC /
  51,283 C LOC). 나머지 ~48k LOC가 M2의 대부분.
- **코드젠은 이미 self-host fixed-point**(gen2==gen3, docs/158 M1). 현
  fixpoint의 AST 입력은 self-parser AST producer가 만든다. `pgy --ast`는
  parser parity의 C oracle로만 남고, 진짜 M2는 self-semantic이 그 산출물을
  검사해야 한다.
- self-semantic 현 경계(rung-2): Layer 0–2 부분(타입 표현·심볼테이블·표현식
  추론·call arity·body skeleton·진단). **미포팅: flow/decl/ability/domain/
  ownership/lifecycle/capability/builtin/DAG/orchestration.**

M2 = 이 ~48k LOC를 **의존 순서대로 rung으로 포팅**하고, 각 rung을 C oracle과
parity로 검증하며, fixpoint를 그 pass까지 확장하는 것.

---

## 1. 완전성 하니스 (WO-SH-COMPLETE — 최우선, 진척 metric)

**문제:** 현 `codegen_bootstrap.sh`의 breadth 검사는 out-of-subset 컴포넌트를
**조용히 skip**한다(248행 `out of codegen subset (skip)`). 완전성 갭이 숨는다 —
"몇 %가 self-host를 통과하나"를 아무도 안 센다. LOC%(6.57%)는 무엇을 닫으면
되는지 안 준다.

**설계:** self-host 자기 소스 전수(`src/self_hosted/**/*.pgy`, ~18k LOC)를
각 stage에 먹여 파일별·구문별 통과/실패를 집계하고, 별도로 **누적 pipeline
교집합**을 잠근다:

```
각 self-host .pgy 파일 →
  [1] self-lexer 토큰화 통과?
  [2] self-parser 파싱 통과?
  [3] self-semantic 검사 통과?
  [4] self-codegen C 방출 통과?     (self-parser AST 텍스트를 입력으로 사용)

누적 교집합 →
  [1+2] lexer와 parser를 둘 다 통과한 파일
  [1+2+3] lexer/parser/semantic을 모두 통과한 파일
  [1+2+3+4] 현 stage check를 모두 통과한 파일
```

산출 = **완전성 원장**: 파일 N개 중 각 단계 통과 수 + 실패 시 미지원 구문/검사의
집계(예: "self-semantic: ownership 검사 미구현으로 42파일 실패"). 이게 진짜
metric — rung을 하나 포팅할 때마다 이 원장의 [3] 통과 수가 오른다.

**구현:** codegen_bootstrap.sh의 `skip`을 `count-and-report`로. self-parser/
self-semantic 각각에 `--check <file>` 모드(pass/fail + 실패 사유) 추가.
게이트: `self-host-completeness-smoke`가 원장 수치를 **단조 비감소**로 잠금
(AIR erasure 래칫과 동형 — 통과 수는 오르기만).

**왜 최우선:** 이거 없이 semantic을 포팅하면 진척을 못 잰다(지난 수개월 LSP가
"활동은 많은데 진척 불명"이었던 이유). WO-SH-COMPLETE가 M2의 계기판이다.

**레드팀 보강(2026-07-05):** 단순 shell report는 완전성 원장이 아니다. 다음 네
조건을 동시에 만족해야 M2 계기판으로 인정한다.

1. **Source scope is owned.** 대상은 production self-host source다:
   `src/self_hosted/**/*.pgy` 중 `fixture/`와 `expected/` 아래를 제외한 파일.
   이 scope와 stage 이름은 `CompilerCompletenessLedger` owner가 내고, shell은
   그 manifest를 실행만 한다. 대상 파일 수 자체도 단조 baseline에 포함된다.
2. **Stage check is explicit.** lexer/parser/semantic/codegen은 `--check` 계약으로
   `Status: ok`를 내거나 nonzero/structured error로 실패한다. ledger가 일반
   stdout을 임의 grep해서 의미를 복원하면 self-host 내부 fallback이다.
3. **No silent skip.** codegen out-of-subset, parser 미지원 구문, semantic 미구현
   검사는 모두 fail count로 남는다. `skip`은 toolchain 부재 같은 환경 조건에만
   허용되고, M2 완전성 갭에는 허용되지 않는다. Per-file timeout은 completeness
   fail이 아니라 실행 인프라 실패로 즉시 red 처리한다.
4. **Monotone baseline + identity.** `source_min`, `lexer_pass_min`,
   `parser_pass_min`, `semantic_pass_min`, `codegen_pass_min`은 오르기만 한다. 또한
   누적 pipeline 수치 `lex_parse_pass_min`, `lex_parse_semantic_pass_min`,
   `full_pipeline_pass_min`도 함께 잠긴다. pipeline baseline manifest는 source
   inventory owner를 소비하므로, current production source가 빠지면 count를
   보존해도 실패한다. pass
   count를 올릴 때는 C/LLVM oracle parity 또는 stage fixture가 같이 있어야 하며,
   C oracle의 silent fallback을 그대로 따라간 결과는 pass 상승 근거가 아니다.

**착지된 M2 ledger baseline(2026-07-09, tightened):** `self-host-completeness-smoke`가
production self-host source 205개를 측정한다. locked minima:
`source_min=205`, `lexer_pass_min=205`, `parser_pass_min=205`,
`semantic_pass_min=205`, `codegen_pass_min=205`,
`lex_parse_pass_min=205`, `lex_parse_semantic_pass_min=205`,
`full_pipeline_pass_min=205`. 세 pipeline baseline manifest는 별도 복사본이 아니라
`CompilerCompletenessSourceInventory()`가 방출하는 source inventory를 소비한다.
따라서 새 production self-host source가 추가되면 source scope와 pipeline identity가
같은 owner에서 함께 확장된다. 이 숫자는 낮출 수 없고, source inventory가 바뀌는
커밋은 같은 게이트에서 새 source의 stage 통과도 증명해야 한다.

주의: `full_pipeline_pass_min=205`는 이제 lexer/parser/semantic/codegen stage가
같은 production source inventory 위에서 닫혔고, codegen stage는 self-parser가
방출한 AST 텍스트를 입력으로 사용한다. 다만 이것은 아직 최종 bootstrap
pipeline이 아니다. self-semantic의 typed facts가 codegen 입력으로 연결된 것이
아니라, stage별 통과 파일의 교집합을 정직하게 보여주는 계기판이다. 다음 승격은
AST 텍스트 브리지를 typed self-parser/self-semantic facts로 줄이는 것이다.

**Red-team 보정(2026-07-05): source identity != semantic check unit.** source
inventory는 계속 148개가 정본이다. 다만 parser expression/statement처럼 순환 문법
클러스터인 파일은 standalone semantic check 대상이 아니다. 그 파일의 source
identity는 유지하되, semantic stage는 `CompilerCompletenessLedger`가 선언한 owner
check target(`expr_owner.pgy`, `stmt_owner.pgy`, `expr_rewrite.pgy` 등)을 실행한다.
shell은 이 매핑을 소유하지 않는다. 이 보정 없이 각 split participant를 억지로
standalone 통과시키면 순환 import나 fake wrapper가 늘어 M2 원장이 더 푸르게 보여도
SoT는 후퇴한다.

**Red-team 보정 2(2026-07-05): import body 재검사 금지.** semantic `--check`는
root source identity의 body를 검사한다. imports는 transitive signature/nominal fact
stub로만 seed하고, imported body는 그 파일의 source identity에서 따로 검사한다. 이
규칙이 없으면 driver root가 parser+codegen 전체 body를 다시 검사해 timeout-shaped
fail을 만들고, 원장은 실제 미지원 semantic이 아니라 하니스 비용을 측정하게 된다.
timeout은 일반 fail count가 아니라 실행 인프라 red다.

---

## 2. ★ Semantic 포팅 사다리 (M2의 본체 — 실측 C 파일 매핑)

C semantic 51,283 LOC를 **의존 순서 10 레이어**로 분해(2026-07-05 실측). 각 rung은
**독립 검증 가능**(subset fixture를 self-semantic과 C oracle 양쪽에 돌려 진단
code/cause 일치). 아래 순서는 bottom-up(하위 없이 상위 포팅 불가).

| rung | 레이어 (C 파일 그룹) | C LOC | 검증 fixture | 현 상태 |
|---|---|---:|---|---|
| **SEM-3** | **Layer 0–1 기반**: type_system{,_compat,_slot,_tuple}.c · symbol_table.c · type_env.c · type_effects.c · type_infer.c · diag_codes/diagnostic_types + type_checker_diag.c | ~6,500 | 타입 표현·심볼·진단 단위 | **부분**(env/type skeleton, diagnostic_owner) |
| **SEM-4** | **Layer 2–3 표현식·흐름**: type_checker_expr{,_ops,_call,_names}.c · assignment{,_path}.c · flow{,_branch,_loops,_match,_parallel,_effects}.c · async_channel.c · bind_stmt.c | ~9,000 | expr-only + flow-only fixture | **~15%**(expr_type/call/body_check) |
| **SEM-5** | **Layer 4–5 선언·ability/generic**: class/enum/ability/role/party/roster/effect/event_decl.c · ability_{match,ref,where,fields}.c · generic_{contracts,support,validation,effective_args}.c · boundary_witness.c | ~3,400 | 시그니처 fixture + generic/ability 만족 fixture | **0%** |
| **SEM-6** | **Layer 6 도메인/intent**(최중 로직): intent_{decl,contract_summary,action_contract,on_inference,participants,transfer,bindings}.c · domain_{contracts,projection,projection_fields,role_lookup}.c · zone_*.c · world_*.c · decls_domain_helpers.c | ~3,600 | intent/zone/world 도메인 authority fixture | **0%** |
| **SEM-7** | **Layer 7 ownership/borrow**(최대·최복잡): ownership_{let,let_helpers,call,return,assign,destructure,boundaries,classify,diag*,let_slot_claim,let_view,param_summary,array_store}.c | ~4,800 | ownership escape/move/borrow fixture | **0%** |
| **SEM-8** | **Layer 8–9 lifecycle·capability**: lifecycle_{analyze,state}.c · slot_analyzer{,_access,_escape,_builtin,_lookup,_summary}.c · capability_analyze.c | ~1,700 | lifecycle N-state + declared⊇used fixture (CFG/MIR이 owns하면 축소 가능) | **0%** |
| **SEM-10** | **builtin/stdlib 타입 검사**: type_checker_builtins{,_stdlib_body,_stdlib_collections,_stdlib_scalar,_query_*,_slotops,...}.c | ~3,900 | stdlib 카테고리별 call fixture | **0%** |
| **SEM-11** | **Layer 10 해결 DAG·오케스트레이션**: resolution_graph_*.c(9) · resolution_stage_*.c(8) · resolution_metadata_*.c(7) · resolution_worklist.c · type_checker_program.c · semantic.c | ~9,000 | forward-ref/cross-file fixture + end-to-end 전체 프로그램 | **~5%**(program_check skeleton) |

**총 미포팅 ~48k LOC.** hot spot 3개(ownership 4.8k · builtin 3.9k · domain 3.6k ·
DAG+orch 9k)가 비용 지배.

**각 rung 집행 공통 절차:**
1. 해당 C 파일 그룹의 **검사 규칙**을 읽어 Pergyra owner로 재서술(typed 파라미터,
   Result/Option로 진단 반환 — string-munging 금지, §3 래칫 준수).
2. **subset fixture 세트** 작성(그 레이어 검사만 트리거하는 최소 프로그램).
3. self-semantic과 C oracle 양쪽 실행 → **진단 code/cause 일치** parity 게이트
   (LSP-0 O-LSP의 canonical event 비교 배관 재사용, docs/150).
4. 완전성 원장(§1)의 [3] 통과 수 상승 확인.
5. fixpoint 재실행(§5).

**오케스트레이션 shape(SEM-11이 미러할 것, 실측):** `semantic.c` →
`semantic_analyze_ex` → `type_check_program`(build_host_decl_index → precollect →
Pass1 statement tree-walk) → legacy slot/lifecycle. self-semantic은 이 pass
파이프라인을 그대로 재현(단일 tree-walk + on-demand DAG 해결).

**축소 기회(정직):** Layer 8(lifecycle/slot)은 **CFG/MIR가 이미 소유**(docs/125
SoT)라 self-host semantic에서 축소/생략 가능 — beta closure가 body 안전의 SoT를
CFG/MIR로 옮겼으므로(docs/self_hosted/01 Stage 0). SEM-8은 capability만 남을 수
있음. 착수 전 CFG/MIR 소유 범위 재확인.

---

## 3. Typed-AST 교체 (linchpin — SEM 포팅과 병행, 실측)

**착지된 typed owner:** `src/self_hosted/hir/typed_ast_arena_owner.pgy`.
`TypedAstArenaPayloadContractReady()`(래칫이 세는 유일 계약), schema
`pgy.selfhost.typed-ast-arena.v1`. **21 노드 kind**: Program·FuncDecl·Param·Block·
LetStmt·AssignStmt·ReturnStmt·IfStmt·WhileStmt·ForStmt·CallExpr·BinaryExpr·
UnaryExpr·IndexExpr·MemberExpr·IntLit·FloatLit·StringLit·BoolLit·Ident·TypeRef.

**설계(실측):** **flat arena** — `AstArena { nodes, children, atom_table: Array }`,
노드를 `NodeId: Int`로 인덱스. 재귀 enum이 현 self-host rung에서 lower 안 되므로
평면 arena 선택(부트스트랩 호환). **sentinel 없음** — 없는 edge는 `Option`(-1
아님). API: `ChildAt(arena, node, i) -> Option<Int>`, `AtomText(arena, node) ->
Option<String>`.

**★ 마이그레이션 순서 = strangler(consumer 먼저, parser 마지막):** 현재 parser는
**AST를 텍스트로 방출**(`ParseRootProgram -> String`, compact indented tree,
`pgy --ast`와 byte-equal). 데이터-흐름상 parser가 producer지만, **parser를 typed로
못 바꾼다 — 소비자(codegen/semantic)가 typed 노드를 못 읽는 동안은.** 그래서
표준 strangler:
1. **어댑터**: AST 텍스트 → `AstArena` 파서(기존 텍스트를 arena로 읽는 브리지).
   consumer가 parser를 안 건드리고 typed 노드 사용 시작 가능.
2. **소비자 typed화**(현재 core 밀도순 top-3): `codegen/emission/expr_rewrite.pgy`
   (11 sig) → `mir_lower/decl_lower.pgy`(10) → `mir_lower/routine_lower.pgy`(9).
   각각 `(String)->String`을 `(AstNode, AstArena)->...`로. 이게
   core_string_munge_sig를
   내린다.
3. **parser flip**: 모든 소비자가 typed면 parser를 `AstArena` 빌더로 전환,
   어댑터·텍스트 포맷 삭제. **이게 가장 큰 수술이고 마지막.**

**SEM 포팅과 짝짓기:** SEM-4(표현식) 포팅 시 expr 노드(CallExpr/BinaryExpr/…)를
먼저 arena화 → expr_type_owner가 typed expr 소비 → string_munge↓. rung ↔ 노드
family를 짝지어 전진하면 semantic 48k를 un-Pergyra로 안 쌓는다.

**★M2 STEP 0 — likeness 래칫 범위 재정의 착지(2026-07-05 실측):**
`self_host_pergyra_likeness_smoke.sh`는 이제 두 숫자를 분리한다.

- `core_string_munge_sig = 108 / max 108` — **blocking GREEN**. 이 값만
  compiler-core linchpin이다. `codegen/emission/expr_rewrite.pgy`(11),
  `mir_lower/decl_lower.pgy`(10), `mir_lower/routine_lower.pgy`(9) 같은 오래된
  core text transform은 계속 포함하고, §3 strangler의 typed-fix 대상이다.
- `total_string_munge_sig = 192` — **info only**. tools/LSP/fuzz/path/fixture/
  harness 텍스트 라우팅까지 포함한 broad surface다. 이 값은 숨기지 않고 출력하되,
  compiler-core idiom ratchet을 흔드는 blocking metric으로 쓰지 않는다.

이전 broad `string_munge_sig=166 > 156` red는 실제 drift라기보다 metric-scope
오염이었다. `lib/{json,json_emit,diagnostic}`만 제외하던 기준은 너무 좁아서
tools/·lsp/·path-harness의 정당한 텍스트 소유까지 core debt로 세었다. 지금 기준은
"코어가 idiomatic한가"만 압박하고, 주변 텍스트 도메인은 별도 owner/gate가 소유한다.
절대 166으로 baseline을 올려 통과시키지 않았고, core 기준을 108로 tighten했다.
앞으로 typed-AST 진척은 `core_string_munge_sig↓`, `typed_ast_contract↑`로 측정한다.

**결정점(BDFL, 로드맵 허용):** typed-AST를 fixpoint **전 완주** vs **후 끌어올림**.
권고: **rung과 짝지어 병행**(각 SEM rung이 그 노드 family만 typed) — un-Pergyra
누적 없이, 그러나 parser flip은 소비자 전부 typed된 뒤 한 번에.

---

## 4. Parser 완전성 (실측: 생각보다 완성됨 — 잔여 3종)

**정정(Agent 실측):** self-parser는 LOC로 52%지만 **기능 커버리지는 훨씬 높다.**
30 owner 파일, 120/121 예제·187 fixture byte-equal(1 skip은 secure_slots C-oracle
graceful, parser 실패 아님, self-parser exit 0/drift 0). **이미 처리**: generics
(중첩 `func<T,U>`/`impl T`/`any T`) · lambdas(`(p)=>body`) · Option destructure
(`if let Some`) · match(case/default) · array literal · postfix `?` try · closures.

**잔여(정확한 미지원 3종):**
1. **match brace body** — `case X: { ... }` 중괄호 블록 거절(현재 single-statement
   case만; `{`를 object-literal로 오해). self-host 소스가 match 중괄호를 쓰면
   벽. `stmt_match_owner.pgy`에 brace-body 규칙 추가.
2. **중첩 배열** — `Array<Array<Int>>` 명시 claim 없음(committed fixture에 없으면
   미지원 추정). self-host 소스가 쓰면 추가 필요.
3. **payload enum** — payload-free variant는 처리, payload tuple은 소비하나
   emit 안 함(C AST printer와 일치). 자기 소스가 payload enum을 쓰면 emit 확장.

**순서:** WO-SH-COMPLETE(§1) [2] 단계가 자기 소스에서 실제 걸리는 구문을 준다 —
위 3종 중 self-host 소스가 쓰는 것부터. 각 = owner 규칙 + fixture + `pgy --ast`
byte-equal 게이트(즉시 검증). **semantic(§2)보다 훨씬 작은 잔여** — 구문 3종은
mechanical, semantic은 48k LOC 설계. parser는 M2의 병목이 아니다.

---

## 5. Fixed-point 재실행 (각 rung의 검증 상시화)

현 fixpoint(`codegen_bootstrap.sh`): gen0(C-oracle) → gen1 → gen2 → gen3,
**gen2==gen3 byte-identical**. AST 입력은 C oracle이 빌드한 self-parser AST
producer에서 온다. `pgy --ast`는 parser parity oracle이지 bootstrap AST producer가
아니다. M2가 전진할수록 파이프라인을 확장:

1. **AST producer 확장:** DRV-0/DRV-1과 `codegen_bootstrap.sh`는 self-parser
   AST producer를 사용한다. parser parity는 계속 `pgy --ast`와 byte-equal을
   비교해 C parser oracle drift를 잡는다.
2. **semantic 삽입:** self-semantic을 파이프라인에 넣어 gen 각 단계가 self-check를
   통과하게. SEM rung이 오를수록 fixpoint가 그 검사까지 포함.
3. **최종 fixpoint:** self-lexer → self-parser → self-semantic → self-codegen이
   전체 컴파일러 소스를 처리, 결과 컴파일러 B==C byte-identical. = M2 완료.

**게이트 규율:** 매 rung 착지 시 fixpoint 재실행(gen2==gen3 유지). divergence =
hard fail(로드맵 Stage 5). C+LLVM parity를 dual oracle로 상시 유지(bootstrap을
blind 신뢰 안 함).

---

## 6. 시퀀싱 + WO 분해 + 정직한 비용

**권고 순서:**
1. **WO-SH-COMPLETE**(§1) — 완전성 계기판 먼저. 이거 없이 나머지는 진척 불명.
2. **WO-SH-PARSER**(§4) — parser 완전성. semantic보다 작고, self-parser가 전체
   소스를 읽어야 semantic 검사 대상이 생김.
3. **WO-SH-SEM-3..11**(§2) — semantic 포팅 사다리, 의존 순. typed-AST(§3) 병행.
   - 각 rung = 독립 WO, C oracle parity 게이트, 완전성 원장 상승.
   - 비용 지배: SEM-7(ownership 4.8k) · SEM-6(domain 3.6k) · SEM-10(builtin 3.9k) ·
     SEM-11(DAG+orch 9k). 이 넷이 M2 시간의 대부분.
4. **WO-SH-TYPEDAST**(§3) — rung과 짝지어 병행(별도 WO 아님, 각 SEM rung의 일부).
5. **fixpoint 확장**(§5) — 매 rung 상시.

**정직한 비용 규모:** ~48k LOC 포팅. 실측 hot spot 기준 **수개월**(1인). 로드맵
스스로 "M2는 substrate 성취, thesis 진전 아님, 최고 레버리지 아님"(docs/158 §4
분기에서 M2를 택한 건 이 비용을 받아들인 결정). **오버클레임 금지:** 각 rung은
"self-semantic이 이 레이어를 C oracle과 parity로 검사한다"까지만 주장 — "전체
self-host"는 fixpoint 최종 확장(§5-3) 전까지 불가.

**축소 레버(정직):** Layer 8(lifecycle/slot)은 CFG/MIR가 SoT를 이미 소유하므로
self-semantic에서 축소 가능(§2 주). SEM-8이 capability만 남으면 ~1.6k LOC 절약.

## WO 등록 (보드)

- **WO-SH-COMPLETE** — 완전성 원장 하니스(§1, skip→count, 단조 게이트). ★선행.
- **WO-SH-PARSER** — parser 완전성(§4, 빈도순 구문 닫기).
- **WO-SH-SEM-3..11** — semantic 포팅 8 rung(§2, 각 C oracle parity).
- **WO-SH-TYPEDAST** — typed-AST 교체(§3, SEM rung과 짝지어).
- **WO-SH-FIXPOINT** — fixpoint 파이프라인 확장(§5, 상시).

## Related

docs/158(self-bootstrap 전략 — §4 M2 분기 결정) · docs/self_hosted/01(Stage 5
정의·완전성 게이트) · docs/150(DRV rung — DRV-0 self-parser 삽입) ·
`tests/self_hosted/parity/codegen_bootstrap.sh`(fixpoint 하니스) ·
`self_host_pergyra_likeness_smoke.sh`(typed-AST 래칫) · docs/125(SoT — Layer 8
축소 근거) · PROGRESS.md(치환 원장)
