# Pergyra TODO (배포 준비)

## 0-selfhost. Beta 이후 self-hosting 목표

**결정:** self-hosting은 beta blocker가 아니라 beta 이후의 검증 목표다.

Beta까지의 핵심 목표는 compiler core를 닫는 것이다: CFG body safety,
AIR evidence, DAG resolution, MIR/C/LLVM parity, ABI ownership, runtime
frontier, 그리고 dogfood path. 이 축이 닫힌 뒤에야 self-hosting을 시작한다.

**순서:**
1. beta closure를 끝내고 stable core surface를 freeze한다.
2. 첫 dogfood는 compiler 전체 rewrite가 아니라 compiler-adjacent tool부터 한다:
   diagnostic catalog checker, AIR graph JSON validator, MIR dump diff tool,
   C/LLVM backend output comparator, module/package resolver helper.
3. Pergyra로 작성한 도구는 기존 C compiler로 빌드하고, 기존 C 구현 결과와
   golden 비교한다.
4. beta+에서 parser/formatter/diagnostic 일부를 점진 이식한다.
5. full self-hosted compiler는 장기 proof target으로 둔다.

**Slot/ownership 기준:** Pergyra는 Rust-style lifetime 언어가 아니다. 모든
business-object lifetime을 정적으로 예측하려 하지 않는다. Slot은 포인터 주소가
아니라 resource boundary / ownership handle이며, static verifier는 unsafe
boundary transition을 거절하고 runtime은 generation/token/resource state를
검증한다. 이 선택은 borrow checker 누락이 아니라 의도적인 추상화 기준이다.

## ★ Core Goal — 진행 시퀀스 (2026-05-02 명시, 4-step)

**확정 순서 — BDFL 결정:**

1. **BETA closure** — 현재 (§0a 참조). 70% 기능 / 55-60% strict 신뢰도
   → 100% 신뢰도까지 닫기
2. **dogfood (compiler-adjacent first)** — §0-selfhost 의 첫 dogfood
   원칙: diagnostic catalog checker, AIR graph JSON validator, MIR dump
   diff tool, C/LLVM backend output comparator, module/package resolver
   helper 부터. dnd_tavern_campaign / 결제 saga mock / AI orchestration
   mock 등 도메인 워크로드도 양 백엔드 회귀. **§0c Intent-Compress
   추론 규칙 설계의 evidence source** (어느 clause가 과잉 required인지
   dogfood가 보여줌)
3. **§0c Intent-Compress sprint** — intent 장황함이 *제거 가능한 유일한
   비용* (§0c 상세). compressed-default + 4-clause 추론 (`who`/`where`/
   `requires`/`authorized by`). AI-assisted 3일 컷 추정. self-host
   진입 *직전* 자리. self-host가 *verbose intent를 Pergyra로 다시 쓰는
   비용*을 회피
4. **BETA+ self-host 시작** — §0-selfhost 의 점진 이식 경로
   (compiler-adjacent → parser/formatter/diagnostic → full long-term).
   docs/120 §4.4 참조. *aspiration이 아니라 committed 일정*.

**의도:**
- §0a (Strict Beta) → §0b (review/ 메타) → 코드 품질 sprint
  (review/compiler-quality-audit.md) → 이 모든 게 *self-host 진입 자격*
  을 만드는 작업. 1.0 닫기 전에 *컴파일러를 우리 언어로 다시 쓸 수 있는
  상태*가 목표
- 단, BDFL 의지에 *시기 강제*는 없음. trigger (사용자 demand 또는 C
  escape hatch 유지 비용 폭발 또는 Pergyra-only 표현이 필요한 feature)
  발생 시 진입. trigger 없으면 partial이 final form.

**Why slot was the right call (2026-05-02 reflection):**

> "러스트의 본질적 핵심 문제점인 라이프타임을 의도적으로 뺐어 그걸 slot
> 이라고 했지. 그게 좋은 선택이었던 거 같다." — BDFL

→ docs/118 §6 negative-space + memory project_lineage_synthesis.md 의
*substrate borrow* 결정 정합. Pergyra는 Rust의 lifetime annotation 학습
비용을 *의도적으로 회피*하고 generational refs (Vale-style
runtime-validated handles) 로 대체. 진입 비용 낮춘 자리, 자기인식 정합.

이 reflection은 *self-host 진입 시 가장 큰 자산*: lifetime annotation이
없으니 컴파일러 자체를 Pergyra로 다시 쓸 때 그 자리가 *일관되게 표현
가능*. Rust가 self-host 시 lifetime annotation으로 부닥친 자리를 우리는
회피.

## 0. 코어 규칙 — 600 LOC split-review threshold

**모든 production `.c` / `.h` owner는 600 LOC 이하로 유지한다.**
초과 시 *feature-owner split* 필수 — 주석 줄이기 / 함수 인라인이 아니라
*owner 분리*.

| Scope | Cap | Gate |
|---|---|---|
| `src/{codegen,runtime,compiler,semantic,parser,lsp}`의 `.h` | 600 LOC (hard) | `tests/production_header_size_smoke.sh` (env `PRODUCTION_HEADER_MAX_LINES` override 가능) |
| 같은 디렉터리의 `.c` | 600 LOC (split-review threshold) | 진행 노트에서 사람 검수 + 베타 closure sweep |
| `tests/` / generated / `.inc` 파편 | 면제 | — |

**Split 패턴:**
- `.inc` field-fragment 분리 — `docs/92_inc_split_roadmap.md`
- 별개 translation unit + 헬퍼 헤더 — `docs/101_semantic_split_template.md`
- 진행 상태 — `docs/115_inc_cleanup_status.md`

**Split application guide (2026-05-02):**
- This is not a rule change. 600 LOC remains the signal; it is not the
  prescription.
- Checklist when an owner reaches the signal:
  1. Does this owner carry two responsibilities? If yes, split by
     responsibility.
  2. Is it still one responsibility but large? If yes, keep one owner and
     improve internal structure with `static` helpers in the same `.c`.
  3. If split is needed, does the new owner name express the responsibility?
     If not, do not land the split.
- New `_helpers` owners are forbidden by default because `_helpers` does not
  name a responsibility. Exception: a genuinely cross-owner shared utility with
  a documented caller set.
- Prefer responsibility names such as `intent_types.c`,
  `resolution_graph_intent.c`, or `cleanup_fact.h`.
- If two helpers differ only by message/kind, prefer a data-driven helper with
  an enum or small table over adding another near-duplicate helper.
- Self-hosting is the planned large-scale structure recovery point: the
  Pergyra compiler should move toward feature-owned modules (`intent`,
  `zone`, `mir`, `air`, `dag`) rather than horizontal helper sprawl. Do not do
  a risky feature-folder migration before beta.

**의도:**
- 행 수 자체가 목표 아님. *행동(behavior)이 1개 owner로 응집*하는지 확인.
- 600 LOC를 넘었다는 신호 = "이 owner가 두 가지 책임을 지고 있다." split으로 응답.
- 진행 노트마다 owner 라인 수를 명시하는 컨벤션 유지 (예: `slot_manager.c는 564 LOC`).
- 현재 production scan: 0 `.c/.h` owners above 600 LOC (2026-04-29 기준).

## 0-meta. review/ 폴더 운영 프로세스 검토 (2026-05-01)

- review/ 폴더는 외부 리뷰 + 코드 감사 결과 누적 자리로 사용 중
  (`review/compiler-quality-audit.md` 추가됨 2026-05-01)
- **검토 필요 항목:**
  - review/ 문서가 TODO.md / docs/ 와 *어떻게 연동*되는지 명시적 룰 부재
  - audit 발견 → review/ 작성 → TODO.md sprint entry → 수정 → 검증의
    *closure 절차* 표준화 안 됨
  - review/README.md 가 단순 인덱스 — *프로세스 가이드* 부재
  - review/ 문서가 stale 됐을 때 detection 메커니즘 없음
- **결정 필요:** review/ 를 *living docs* 로 둘지 *snapshot 아카이브*로
  둘지. 현재 README는 "수정 작업의 근거" 표현 — living docs 의도로
  보이지만 운영 룰 부재
- *베타 closure 작업 아님*. 메타 프로세스 정합성 자리. 1.0 전 정리 권고

## 0a. Strict Beta Closure Order — 2026-05-01 재고정

**현재 판정:** 기능 구현률은 약 70%로 본다. 다만 strict beta 신뢰도는
약 55-60%다. 차이는 기능 수가 아니라 CFG/AIR/DAG/MIR/ABI가 실제
source-of-truth로 소비되는 깊이다.

**명시적 제외:** quantum full model, Rust급 lifetime/borrow checker, HKT/FP,
새 대형 언어 축은 beta 100% 계산에서 제외한다. WASM/WebGL은 실제 dogfooding
경로라 중요하지만, beta closure 자체를 흔드는 새 semantic surface가 아니라
LLVM-family target/extern bridge track으로 둔다.

**닫는 순서:**
1. **AIR evidence producer 정합성.** 빈 evidence node 금지, DAG/MIR/RIR/HIR
   evidence가 실제 fact 또는 explicit fallback debt가 있을 때만 생성되게 한다.
   Gate: `make test-air air-drift-test-smoke air-json-schema-test-smoke`.
   - 2026-05-02 slice: intent zone-authority compression now reaches AIR.
     DIR records `authorized_by_derived_from_zone`, AIR records
     `authority_from_zone`, AIR JSON exposes it, and drift diagnostics include
     `authority_provenance=zone-derived|explicit|none`.
   - 2026-05-02 slice: action-derived intent `causes` now reaches RIR
     propagation evidence. `rir_builder_intent.c` materializes inherited or
     explicit step causes as `RIR_RESOURCE_EFFECT_INSTANCE` +
     `RIR_OP_ATTACH_EFFECT`, preferring the unique zone effect-slot anchor over
     the effect type name when the current zone makes that anchor unambiguous.
     AIR verifies it through
     `AIR_EVIDENCE_RIR_EFFECT_PROPAGATION` instead of keeping
     `causes_from_action` as AIR-only provenance.
   - 2026-05-02 slice: action-derived `authorized by` is now pinned in the same
     parsed AIR fixture through real RIR authority evidence. The on-receiver
     action contract regression requires `authority_from_action`,
     `has_rir_authority_evidence`, `rir_authority_evidence_name == "healer"`,
     and an `AIR_EVIDENCE_RIR_AUTHORITY` node, so action-inherited authority is
     no longer only an AIR boundary flag.
   - 2026-05-02 cleanup evidence repair: AIR global MIR cleanup evidence now
     consumes MIR CFG cleanup successors before instruction-name fallbacks.
     Pin cleanup remains boundary-specific `AIR_EVIDENCE_MIR_PIN_CLEANUP`.
     Gate: `make test-air` (`62/0`).
   - 2026-05-02 MIR population ownership repair: statement reconstruction now
     preserves the rebuilt instruction-array capacity before later cleanup-edge
     materialization. This fixed a real pin-cleanup heap corruption in
     `mir_lower(...)` and restored `test-mir`, `cfg-body-dataflow-test-smoke`,
     `air-json-schema-test-smoke`, and C-only `air-backend-nonimpact` coverage.
   - 2026-05-02 codegen determinism repair: C MIR block mapping comments no
     longer emit raw AST pointer addresses. AIR strict/relaxed backend
     non-impact checks now compare deterministic C artifacts for all backend
     compare fixtures.
   - 2026-05-02 MIR CFG predecessor validation tightening: MIR now validates
     predecessor lists as bidirectional CFG facts. Forward successors must be
     reflected in predecessor lists, and every recorded predecessor must point
     back through a true/false/cleanup/rollback/invalidation edge. Gate:
     `make test-mir cfg-body-dataflow-test-smoke` (`28/0` MIR tests).
   - 2026-05-03 CFG/MIR direct-call fact tightening: direct statement calls now
     carry their callee name as `MIR_INST_STMT.arg0`, and direct initializer
     calls carry their callee name as `MIR_INST_DEF.arg1`. Intent observability
     no-trace detection consumes those MIR facts and HIR routine `direct_calls`
     before structural AST fallback. Gate: `make test-mir
     cfg-body-dataflow-test-smoke test-transpile perf-contract-test-smoke`
     (`32/0` MIR tests, `710/0` transpile tests).
   - 2026-05-03 MIR source-location materialization: C MIR block mapping
     comments now consume scalar `MIRBasicBlock.has_source_location` /
     `source_line` / `source_column` facts instead of reading
     `block->source_ast` in codegen. This keeps source AST pointers as MIR
     construction/debug provenance, not a backend consume path. Gate: manual
     native MinGW `test-mir` (`32/0`), `test-air` (`68/0`), and
     `test-transpile` (`710/0`).
   - 2026-05-03 MIR surface-usage fact materialization: MIR instructions now
     carry `has_surface_usage_facts` and `uses_thread_pool_surface`. C/LLVM
     thread-pool dependency checks consume that MIR fact first and only scan
     AST payloads for hand-built or legacy MIR without facts. The shared fact
     materializer is now called by base, cleanup, and intent MIR append paths.
     Gate: manual native MinGW `test-semantic` (`2500/0`), `test-mir` (`32/0`),
     `test-air` (`68/0`), and `test-transpile` (`710/0`).
   - 2026-05-03 MIR branch-shape materialization: branch and loop-init
     instructions now carry `MIRBranchShape` (`FOR_RANGE`, `FOR_IN`,
     `MATCH_CASE`, `SELECT_DISPATCH`). C and LLVM MIR control emitters consume
     that fact instead of classifying branch control by AST node type. AST
     payloads remain only for expression/condition emission. MIR validation and
     MIR lowering regressions also consume `branch_shape` for loop-branch
     completeness, so the fact is now part of the MIR contract, not just a
     backend convenience. Gate: manual native MinGW `test-semantic` (`2500/0`),
     `test-mir` (`32/0`), `test-air` (`68/0`), `test-transpile` (`710/0`), plus
     LLVM control owner compile smoke.
   - 2026-05-03 MIR dump source-location tightening: `mir_dump(...)` now prints
     source locations from `MIRBasicBlock.has_source_location` /
     `source_line` / `source_column` instead of rebuilding them from
     `source_statements[0]` or terminator AST pointers. This keeps public MIR
     dumps aligned with materialized MIR facts. Gate: manual native MinGW
     `test-mir` (`32/0`) and `test-transpile` (`710/0`).
   - 2026-05-03 MIR instruction source-location materialization:
     instructions now carry `has_source_location`, `source_line`,
     `source_column`, and `source_ast_type` facts. `mir_dump(...)` prints
     instruction `ast-type`/`line` from those facts instead of reading
     `inst->ast`. AST payloads remain available to expression emitters, but
     the public MIR dump path no longer consumes AST pointers for instruction
     provenance. Gate: manual native MinGW `test-mir` (`32/0`), `test-air`
     (`68/0`), and `test-transpile` (`710/0`).
   - 2026-05-03 MIR AST-type consumer tightening:
     C/LLVM codegen no longer branches on `inst->ast->type`. Instruction kind
     decisions now consume `source_ast_type` / `has_source_location`; AST
     payloads remain only where expression or statement emission still needs
     the original syntax tree. Gate: manual native MinGW `test-transpile`
     (`710/0`), `test-mir` (`32/0`), `test-air` (`68/0`), and
     `perf_contract_smoke`.
   - 2026-05-03 C backend source-array consumer tightening:
     `transpiler_mir_find_stmt_for_inst(...)` now trusts instruction-carried
     statement AST provenance first and falls back only to function-scope let
     lookup by name. Codegen no longer reads `block->source_statements`,
     `block->source_ast`, `source_terminator_*`, or `inst->ast->type` in the
     scanned C/LLVM backend owners; those block source arrays remain MIR
     construction input, not backend judgement input. Gate: manual native
     MinGW `test-transpile` (`710/0`) and `perf_contract_smoke`.
   - 2026-05-03 CFG loop-flow consumer tightening: `while` and static range
     `for` statements now return semantic CFG flow flags to their parent body
     instead of being flattened through the generic statement fallback. The
     accepted slice is conservative: `while true { return ... }` satisfies
     non-`Void` all-path return, and `for i in 0..1 { return ... }` satisfies it
     only when the range is statically non-empty and no `break` path exits the
     loop. `for-in`, empty ranges, dynamic ranges/conditions, possible `break`,
     and non-returning backedges remain fallthrough. Gate:
     `make test-semantic cfg-body-dataflow-test-smoke` (`2497/0` semantic
     tests).
   - 2026-05-03 DAG intent/action-contract seam tightening:
     `type_checker_intent_role_fields.c` no longer owns a second local
     materializing type-ref helper, and `type_checker_func_action_contract.c`
     consumes annotation metadata for action-contract domain-slot/parameter
     reads. This shrank the resolver inventory cap from `12` to `10`; the
     2026-05-03 generic/host/intent-type follow-up below shrinks it again to `5` while keeping
     fallback/materializer counters at zero. Negative probes confirmed
     `type_checker_ownership_let_helpers.c` still needs earlier collection
     shell/key-policy metadata before leaving the compatibility path. Gate:
     `make test-semantic type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke`.
   - 2026-05-03 DAG generic materializer seam removal:
     `type_checker_generic_effective_args.c`,
     `type_checker_generic_contracts.h`, and
     `type_checker_generic_validation.c` now consume annotation metadata and
     `semantic_type_resolution_lookup_metadata_type_ref(...)` only; they no
     longer call the materializing type-ref helper. `type_checker_host_helpers.c`
     now follows the same metadata-only path for host/domain slot type reads,
     `type_checker_func_decl.c` now uses metadata-only lookup for function
     parameter/return signatures, and `type_checker_expr.c` does the same for
     expression-local annotations. `type_checker_ownership_let_helpers.c` now
     consumes metadata type-ref facts plus the stable-shell arity,
     constructed-type, and unknown-bare-name diagnostic helpers; the rejected
     annotation-only probe caused broad semantic drift, so the accepted closure
     is metadata + diagnostics, not annotation-only. Semantic owners no longer
     call the materializing type-ref helper. The resolver inventory cap is now
     `2`, covering only the central API declaration/implementation, with
     fallback/materializer counters still at zero.
     Gate:
     `make test-semantic type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke` (`2500/0` semantic).
   - 2026-05-03: intent compression provenance tightened. A `using` binding
     derived from a unique `where`/zone type now records
     `derived_using_from_where`, so AST print and contract summaries no longer
     report it as a local clause. Gate:
     `make test-parser test-semantic cfg-body-dataflow-test-smoke` (`2498/0`).
   - 2026-05-03: intent transfer compression provenance tightened through DIR
     and AIR. Transfer-only steps can now derive both `where` and `using` from
     the transfer target; DIR records `where_derived_from_transfer` and
     `using_derived_from_transfer`, validates that provenance has concrete
     zone/binding facts, and AIR marks the corresponding zone boundary with
     `source_from_transfer`. Gate: `make test-semantic test-dir test-air
     test-transpile cfg-body-dataflow-test-smoke
     intent-compression-contract-test-smoke type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke perf-contract-test-smoke`
     (`2500/0` semantic, `9/0` DIR, `67/0` AIR, `710/0` transpile).
2. **CFG body safety source-of-truth 승격.** all-path return / definite assignment /
   move-use / pin cleanup 이후 ownership/drop/zone/effect transition 소비자가
   CFG/MIR fact를 직접 소비하게 만든다.
3. **DAG source-of-truth 마감.** fallback 수치 0 유지가 아니라 generic/ability/
   alias/module visibility 판단이 DAG metadata/API를 공식 경로로만 통과하게 한다.
4. **MIR/LLVM declaration inventory debt 제거.** frozen subset declaration/bootstrap
   inventory는 AST-carried metadata가 아니라 DIR/RIR/MIR inventory만 소비한다.
5. **Runtime propagation frontier scheduler.** world/zone/projection bounded
   recompute 다음 단계인 full transitive frontier scheduler를 마감한다.
6. **ABI ownership freeze.** Slot/Pin cleanup, Zone-bound handle, runtime-none,
   raw escape, ABI non-leakage를 코드 gate와 문서 계약으로 동시에 고정한다.
7. **WASM/WebGL dogfood feasibility.** beta semantic closure 이후 `C/LLVM family`
   경로로 `wasm32` target과 최소 WebGL bridge smoke를 만든다.

**운영 원칙:** 테스트 스위트를 무작정 넓히기 전에, 위 순서의 한 feature-owner를
먼저 닫고 그 feature의 gate를 초록으로 만든다. 새 키워드/새 축은 추가하지 않는다.

## 0b. Forward Plan — WASM/WebGL 경로 (post-beta 우선순위 2)

**입안일:** 2026-05-01. **상태:** 계획. **scope:** 사용자가 *이미 만들고 싶어하는*
도메인(웹 던전 크롤러)을 Pergyra 표면 안에서 가능하게 하는 최소 경로.

**우선순위 메모 (2026-05-01 재평가):** 직전엔 priority 1로 박았으나 §0b
analysis 후 priority 2로 강등. *언어 정체성 활용도*에서 server backend가
WebGL보다 강함 — `intent` saga / `authority` / `Result<T>`는 transactional
saga에서 *우회 없이* 작동하지만 WebGL은 DOM/JS shim 우회 필수. WebGL은
*개인 동기 + 시각적 마케팅 가치*로 여전히 유의미하나 ecosystem leverage
순서에서 §0b 뒤로.

### 결정 — JS 백엔드 ❌, WASM 백엔드 ✅
- **JS 백엔드 거부 사유:** GC + f64-only + reference-only emit 결과는
  `slot<T>` / zone / intent / authority *모두 흘림*. parity gate가 tri-way로
  분기 → 베타 closure 정합성 무너짐. docs/120 §4 vision territory 위반.
- **WASM 채택 사유:** linear memory + i32/i64/f32/f64 native + 명시적
  lifetime → slot/zone/intent 그대로 보존. LLVM target triple 한 줄 수준
  ("LLVM family"의 자연 연장이라 새 semantic 표면 없음). dual-emit
  parity gate에 영향 없음.

### Why WebGL — Pergyra가 *유난히 잘 표현*하는 자리
WebGL은 **불투명 리소스 핸들 API**. JS는 GC에 위임 → 텍스처 누수, 컨텍스트
loss 미스, 프레임 간 상태 누수가 사용자 책임. Pergyra slot/zone/intent가
바로 그 자리:

| WebGL 개념 | Pergyra 자연 표현 |
|---|---|
| `WebGLBuffer` opaque | `slot<Buffer>` |
| `WebGLTexture` opaque | `slot<Texture>` |
| `WebGLProgram` opaque | `slot<ShaderProgram>` |
| Vertex buffer ownership | `authority { ... }` |
| Render pass 경계 | `zone GPUFrame { ... }` |
| Frame 단위 의도 | `intent RenderFrame { precondition ... success ... }` |

memory: `project_killer_usecase_dungeon_crawler.md` 와 1:1 일치.

### 현재 인프라 — 70-80% 이미 있음
- ✅ `extern "ABI" { func ...; }` 파싱 — `src/parser/parser_decl.c:296`
  (`parse_extern_block`)
- ✅ `AST_EXTERN_BLOCK` LLVM 등록 — `src/codegen/llvm_register.c:322-328`
- ✅ LLVM 백엔드 자체 작동 (베타 inversion 완료)
- ❌ `wasm32-unknown-unknown` target triple 옵션 — `Makefile` /
  `src/codegen/llvm_pipeline.c` wiring 필요
- ❌ `extern "wasm-import"` ABI 인식 (현재 `extern "C"`만) — 한 줄 작업
- ❌ `Array<T>::as_raw_view()` stdlib (vertex buffer 데이터 패싱용)
- ❌ JS shim 자체 (수백 LOC, *언어 외부*)

### 진짜 막히는 자리 (3개)
1. **Linear memory pointer 노출** — `slot<Array<f32>>` → `(ptr, len)` 추출.
   JS shim이 zero-copy로 `new Float32Array(wasm.memory.buffer, ptr, len)`
   읽음. `as_raw_view()` 추가 필요.
2. **WASM module exports — frame loop** — JS의
   `requestAnimationFrame`이 wasm 함수를 매 프레임 호출. LLVM `extern "C"`
   symbol export 동작 검증 필요 (표준 LLVM이면 자동).
3. **Texture 업로드 path** — `gl.texImage2D`의 `HTMLImageElement` 입력은
   DOM-only. 우회: JS에서 디코드 → wasm 메모리에 raw pixel 쓰기 → wasm이
   raw 버전 `texImage2D` 호출. glue 30~50 LOC.

### Phase 분할

**Phase 0 (1주, 베타 내부) — Feasibility 경로 확보**
- [ ] `Makefile` / `src/codegen/llvm_pipeline.c`에 `--target=wasm32-unknown-unknown` 옵션
- [ ] `examples/wasm_hello/` — `extern "C" fn console_log(...)` 호출하는
  최소 .pgy + 손으로 쓴 wasm-loader HTML
- [ ] 산출물: `pgy --backend=llvm --target=wasm32 hello.pgy → hello.wasm`,
  브라우저에서 콘솔 출력
- [ ] *베타 scope 확장 아님* — 경로 확보. 베타 closure 후 즉시 시작 가능
  하도록.

**Phase 1 (2-3주, 베타 직후) — WebGL MVP**
- [ ] `extern "wasm-import"` ABI 토큰 인식
- [ ] `Array<T>::as_raw_view()` stdlib 추가
- [ ] `examples/webgl_triangle/` — 화면에 컬러 트라이앵글 1개
- [ ] WebGL JS shim ~200 LOC (triangle 1개에 필요한 surface만)
- [ ] **Falsification 자리 (docs/122 §4):** slot lifecycle이 GL 컨텍스트
  loss와 자연스럽게 상호작용 하는가? authority가 GPU resource ownership을
  잘 표현하는가? F1-F6 신호 기록.

**Phase 2 (4-6주, 베타 후) — Dungeon crawler PoC**
- [ ] 사용자가 Pergyra만으로 던전 1 floor 렌더링 + 캐릭터 이동
- [ ] **WebGPU 직접 고려** — bind group / pipeline state object가 Pergyra
  intent / zone과 *훨씬* 자연스럽게 매핑. WebGL은 stepping stone.
- [ ] 1년 freeze recognition window의 핵심 evidence source
  (docs/122 §2.5 신호 매트릭스 입력).

### Out of scope (이 plan에서 *안* 함)
- DOM 직접 바인딩 (Pergyra 정체성과 안 맞음 — 얇은 JS shell만 손으로)
- HTTP server / 풀 networking stdlib (별도 plan)
- WASM GC types (proposal unstable, docs/120에 명시적 거부)
- JS 백엔드 (위 결정 사유)
- WASI 풀 surface (브라우저 target 우선)

### Verification 체크포인트
- Phase 0: `pgy --target=wasm32` 가 valid `.wasm` 산출, 브라우저 로드 OK,
  콘솔 출력 OK
- Phase 1: triangle 화면 표시, slot 누수 없음 (Chrome DevTools WebGL
  inspector), F1-F6 falsification 결과 `examples/webgl_triangle/FALSIFICATION.md`
- Phase 2: 던전 floor 60fps 안정, 사용자 피드백 evidence 수집

### 참조
- `docs/117_backend_strategy_positioning.md` — dual-emit 정책. WASM은
  LLVM family target 추가지 새 backend 아님
- `docs/120_vision_and_capability_audit.md` §4 — vision territory 정합성
- `docs/122_managing_intent_drift.md` §4 — falsification 프로토콜 적용
- memory: `project_killer_usecase_dungeon_crawler.md` — 핵심 동기

## 0c. Forward Plan — Intent-Compress (post-beta priority 0, self-host 직전)

**입안일:** 2026-05-02. **상태:** 계획 (★ Core Goal step 3에 박힘).
**scope:** intent block 의 *제거 가능한 유일한 verbosity*. compressed
form을 *디폴트*로, verbose 는 *명시 옵션*으로. 4 clause (`who`/`where`/
`requires`/`authorized by`) 를 컨텍스트에서 추론.

### 왜 이 sprint가 다른 trade-off와 *질적으로 다른가*

다른 trade-off (slot↔lifetime, layer 혼재, Result-first verbose) 는
*thesis나 mandate가 비용을 정당화*. intent verbose 는 그렇지 않음 —
*제거 가능한 비용*. 5가지 차이:

1. **Thesis가 요구하지 않음** — DDD primitive 1급 thesis 는 *intent 가
   언어 시민*이라 말하지, *8 clause 의례*를 요구하지 않음. clause 추론
   은 thesis *약화 아님*, ergonomics 표현. Rust lifetime elision 과 동일
2. **Signature 자리 가림** — intent block 은 학습자가 *5분 안에 만나는*
   Pergyra 의 signature 구문. 첫 인상이 무거우면 *thesis 평가 전에* 떠남
3. **라이브러리 비교 역전** — 현재 syntax 로 Camunda saga / Python 데코
   레이터보다 *길어 보임*. 마케팅 narrative ("언어 차원 강제") 가 syntax
   로 *역행*되는 자리
4. **Minimum floor 높음** — toy intent 불가능 (8 clause 강제). *낮은
   floor + 깊은 ceiling* 자연 확장 막힘. docs/120 §4.5 educational entry
   path 도 이 자리에서 막힘
5. **Self-host 진입 비용** — verbose intent 를 Pergyra 컴파일러 자체에
   서 다시 쓰는 비용. compress 후 self-host 진입 시 *훨씬 깔끔*. 이게
   step 3 가 step 4 *직전* 자리인 이유

### Direction A — compressed-default + 4 clause 추론

**Before (현재):**
```pgy
intent ProcessPayment {
    who: Customer,
    where: PaymentZone,
    requires: customer.balance >= amount,
    authorized by: customer.payment_authority,
    precondition: not order.paid,
    success: order.paid = true,
    failure: order.paid = false,
    compensate: refund_handler(order)
}
```

**After (compressed default):**
```pgy
intent ProcessPayment for Customer in PaymentZone {
    requires balance >= amount;
    authorized;
    success: order.paid = true;
}
```

전체 verbose form 은 명시 가능 — *예외 자리에서만*. 평균 케이스는 5-line
이내.

### 4 clause 추론 규칙 (sketch)

| Clause | 추론 source |
|---|---|
| `who:` | (a) 호출 site receiver (`customer.process_payment(...)` → who=customer) (b) intent 가 subject 안에 선언된 경우 그 subject (c) 명시 안 되면 require error |
| `where:` | (a) 호출 site 의 `zone` 컨텍스트 (b) intent 가 zone 안에 선언된 경우 그 zone (c) 명시 안 되면 world scope (d) world scope 도 ambiguous 하면 require error |
| `requires:` | (a) 본문 분석 — `who.field` 사용 시 자동 `who.field.exists` (b) numeric ops → 범위 추론 (c) ambiguous 자리는 명시 권고 (require 아님 — *strict mode flag* 로 강제 가능) |
| `authorized by:` | (a) 호출 site 의 `authority` 컨텍스트 propagate (b) `who` 가 authority 가지면 self (c) 명시 안 되면 require error (보안 자리이므로 *fail-closed*) |

### 충돌 해소

**explicit > inferred 일관**. 사용자가 명시한 자리는 *항상* 우선. 단
explicit 와 inferred 가 *다르면* warning (silently override 안 함).

### Sprint 분할

**Phase 1 (1일, AI-assisted)** — 추론 규칙 design + AST/HIR 변경 설계
- [ ] 4 clause 추론 규칙 finalize
- [ ] AST 에 `inferred_who` / `inferred_where` 등 메타 필드
- [ ] HIR/MIR 은 *expanded form* 유지 (verification/debug 정합)
- [ ] semantic phase 에서 expansion 위치 결정

**Phase 2 (1일)** — 구현
- [ ] semantic phase clause 추론 구현
- [ ] explicit-vs-inferred 충돌 검출
- [ ] backend-compare gate 정합 유지 (양 백엔드 같은 expanded form 사용)

**Phase 3 (1일)** — 진단 + 테스트 + 문서
- [ ] 추론 실패 진단 (*"이 자리에서 `who` 추론 불가, 명시 필요. 호출
  receiver 또는 enclosing subject 가 없음"*)
- [ ] negative test cases (추론 실패 자리들)
- [ ] examples/ 의 verbose intent 들을 compressed form 으로 마이그레이션
- [ ] dnd_tavern_campaign / 결제 saga mock 양 backend 회귀
- [ ] docs/121 §3 carrier/coherence 자리에 *compressed form* 정합 추가
- [ ] docs/120 §4.4 self-host 항목에 *intent-compress 가 self-host 진입
  자격* 명시 (이미 박혀 있음, 강화)

### 검증 체크포인트

- 모든 examples/ 양 백엔드 회귀 zero
- AIR drift fact 정합 — expanded form 이 동일하면 AIR fact 도 동일
- backend-compare gate 69/69 유지
- *대표 intent 5개 LOC 측정* — 평균 5-line 이내 도달
- 마이그레이션 후 *educational angle* 가능성 검증 (toy intent 가능)

### Out of scope (이 sprint 에서 *안* 함)

- `precondition` / `success` / `failure` / `compensate` clause 자체
  변경 — 이건 thesis 의 핵심 표현, 추론 안 함
- 새 intent semantic 추가 — 이건 별도 ticket
- Educational entry path full 작업 (docs/120 §4.5 후보) — 이 sprint
  *그것을 unblock* 만 함, 본 작업은 별도

### 의존성 / 정합

- **Step 2 (dogfood) 의 evidence 가 input** — dogfood 가 *어느 clause가
  과잉 required 인지* 보여줘야 추론 규칙 정확. dogfood 없이 시작하면
  *추측*
- **Step 4 (self-host) 가 consumer** — self-host 컴파일러 자체가
  compressed form 사용. step 3 → step 4 순서 강제
- **docs/120 §4.4 (self-host) 와 §4.5 (educational, 후보)** 모두 이
  sprint 후 시작 가능

### 비용 추정 정정

직전 분석에서 "4-7개월" 추정했으나 *AI-assisted 환경에서 3일 컷* 으로
재추정 (BDFL 결정). 추론 규칙은 *데이터-구조 자체가 단순*하고 (4 clause,
~5 source 별 추론 패턴), AI 가 implementation/test/diagnostic 패턴 빠르게
churn. *진짜 시간 비용은 dogfood evidence 수집* (별도 step). sprint
자체는 dogfood 후 3일.

### 참조

- `docs/121_types_as_domain_medium.md` §3 — carrier/coherence/negative-space
- `docs/120_vision_and_capability_audit.md` §4.4 — self-host 진입 자격
- `docs/122_managing_intent_drift.md` §4 — falsification 프로토콜 (sprint
  내 적용)
- memory `feedback_marketing_language_drift.md` — marketing claim 과
  syntax 정합성 자리
- ★ Core Goal step 3 — 시퀀스 자리 anchor

## UTF-8 Progress Note - 2026-05-01 - Dogfood-first WebGL Bridge Gate

2026-05-01 update:
- Beta progress is now tracked as two numbers: user-visible feature progress is
  about 70%, while strict beta readiness is about 60%. The delta is
  CFG/AIR/DAG/MIR/ABI source-of-truth closure, not missing surface syntax.
- WebGL/WASM is no longer framed as "native LLVM wasm before beta". The beta
  dogfood entry path is `Pergyra -> C backend --emit-c -> optional Emscripten`.
  Native LLVM wasm and richer render modules stay beta+1.
- Added `make dogfood-webgl-test-smoke`. The smoke emits C for an `extern "C"`
  host log plus one frame callback and verifies that the generated C preserves
  the bridge calls. If `emcc` is installed, it also links an HTML/JS wasm shell;
  otherwise it reports a skip after the C bridge is validated.
- This is a dogfood-path gate, not a new keyword or new semantic axis. It does
  not change the runtime-none contract, which still explicitly rejects false
  freestanding lowering claims.

## UTF-8 Progress Note - 2026-05-01 - Hot-path Dispatch / Lookup Audit

2026-05-01 update:
- Closed one active Category A seam: `src/semantic/type_checker_builtins_resolve.c`
  no longer uses a 120+ entry sequential `strcmp` chain. It now owns a sorted
  builtin registry table and resolves through `bsearch`.
- Verification: `make test-semantic LLVM_ENABLED=0` and
  `make type-resolution-dag-test-smoke LLVM_ENABLED=0` passed with isolated
  `BUILD_DIR`/`BIN_DIR`. The DAG smoke kept `retired_resolver_calls=0` and
  `materializer_unresolved=0`.
- Intent observability scan status: the old implementation-header/TU
  duplication claim is stale. `intent_observability_usage.h` is declaration-only,
  and both C/LLVM entry points use the scan once per backend compile. The
  structural AST walk is now owned by `src/parser/ast_analysis.c` via
  `ast_contains_identifier_call(...)`; `intent_observability_usage.c` only
  supplies the intent-observability predicate and MIR traversal. Block-level
  source arrays and routine AST payloads are no longer scanned for this fact;
  declaration inventory AST scans remain compatibility debt until whole-program feature
  facts are promoted into semantic/MIR analysis flags.
- Safety note: do not replace the current `strncmp(name, "Intent", 6)` prefix
  guard with `memcmp` unless caller-provided string storage is proven at least
  6 bytes. `strncmp` is the safe prefix cutoff for arbitrary C strings.
- Remaining high-value follow-up: move whole-program feature facts
  (`uses_intent_observability`, later `uses_parallel` / `uses_async` /
  `uses_unsafe`) into semantic/MIR analysis flags so codegen consumes facts
  instead of rescanning AST payloads. `transpiler_expr_type_infer.h` still
  duplicates builtin return-type knowledge and should be retired behind semantic
  typed facts rather than expanded with more name checks.
- Closed parser AST growth debt: `src/parser/ast.c` no longer uses
  `realloc(count + 1)` for program/block/extern/namespace/parallel/call node
  lists. These lists now carry explicit capacity, grow geometrically, and leave
  the AST unchanged on allocation failure.
- Closed semantic scope lookup hot path: `Scope` now keeps an append-only
  open-addressed symbol index for current-scope duplicate checks and lookups
  while preserving the original `symbols` array for whole-scope iteration.
- Closed runtime intent active-handle lookup debt for the stable runtime
  surfaces: generated-C inline runtime and LLVM/export runtime now both keep a
  handle-to-active-slot index. Sequential active scans remain only where the API
  is explicitly index/enumeration based.
- Verification: `make test-semantic LLVM_ENABLED=0`, `make test-abi
  LLVM_ENABLED=0`, and `PGY_OBSERVABILITY_BACKENDS=c make
  observability-schema-test-smoke LLVM_ENABLED=0` passed with isolated
  `BUILD_DIR`/`BIN_DIR`.

post-beta 우선순위 정리 audit. 3개 Explore agent 병렬 실행, codegen
dispatch / semantic lookup / runtime data structure 3축 결과 통합. *정확성
회귀 zero, but 큰 프로그램 컴파일 / 긴 trace / 큰 AST 빌드에서 누적 비용
큰 자리들*. 베타 closure 위협 없음.

### Category A — Stdlib builtin dispatch (`strcmp` 체인 50+ 분기)
- `src/codegen/transpiler_expr_stdlib_scalar_builtin.h` 7-174 (15+ 분기)
- `src/codegen/transpiler_expr_stdlib_collection_builtin.h` 106-460 (23+
  분기). 추가로 line 147-219 부근 `strcmp(key, "Int"/"Long"/"Bool")`
  3-way ternary 5+ 곳 중복
- `src/codegen/transpiler_expr_stdlib_builtin.h` 112-289 (외부 dispatch
  11+ 분기)
- 처방: gperf 또는 정렬 + bsearch 단일 테이블, key_type 사전 분류
  enum 도입. 표 한 번에서 jump

### Category B — Symbol / scope / type lookup 선형 strcmp
- `src/semantic/symbol_table.c:141-143` — `scope_lookup_current` 선형
- `src/semantic/type_env.c:45-56` — `type_env_lookup_variable` scope chain
  × 선형 strcmp
- `src/codegen/transpiler_decl_lookup.c:113-118` — 캐시 있음, cold start
  O(N)
- `src/parser/parser_decl_hints.c:47-55, 99-108` — hint table 선형 strcmp
- `src/codegen/transpiler_statement_dispatch.h:59-62` — typed_var 선형
- `src/codegen/llvm_backend_type_map.c:147-156` — type alias 선형
- `src/semantic/type_checker_builtins_query_domain.c:19-26, 38-45, 62-69,
  76-100, 149+` — zone/relation/effect/world/state 5종 선형
- 처방: `src/runtime/pgy_runtime_builtin_hashmap_inline.h` 패턴을
  컴파일러측 owner로 분리 (e.g. `src/common/compiler_hashmap_inline.h`),
  Scope/TypeEnv/program-level decl index 도입. core symbol resolution이
  컴파일러의 가장 hot path — 영향 큼

### Category C — 다중 pass / 준-quadratic
- ✅ `src/semantic/slot_analyzer.c` — live slot collection is now 1-pass
  geometric growth, and function/branch live-set membership uses sorted
  pointer sets instead of O(after × before) nested scans.
- [~] `src/compiler/air_evidence.c` — MIR pin-cleanup evidence collection now
  iterates MIR routines/blocks first and only matches actual pin-region blocks
  against AIR pin boundaries. Remaining AIR cost item is broader HIR routine /
  boundary matching and typed boundary-id indexing.
- ✅ `src/compiler/air.c` — AIR evidence inventory and owned-name storage now
  use explicit capacity growth instead of `realloc(count+1)`.
- ✅ `src/compiler/air_verify.c` — AIR drift inventory now uses explicit
  capacity growth instead of `realloc(count+1)`.
- ✅ `src/compiler/hir.c` — call-graph closure now builds a sorted
  `HIRRoutineNameIndex` once and resolves direct calls through indexed lookup
  instead of scanning every routine for every call.
- ✅ `src/semantic/type_checker_resolution_metadata.c` — DAG metadata lookup now
  uses an AST-node pointer index instead of scanning every metadata entry on
  each lookup. The raw arrays remain for ordered iteration / ownership cleanup.
- ✅ `src/semantic/semantic.c` — stdlib preload append paths now consume AST
  program capacity and use geometric growth for the loaded-module list instead
  of `count+1` realloc.
- ✅ `src/compiler/dir.{h,c}` / `src/compiler/dir_collect.c` — DIR node,
  edge, owned-name, intent, participant, step, and intent-step name arrays now
  use explicit capacity growth instead of `count+1` realloc. This keeps the
  domain graph storage owner aligned with the same IR-storage contract as AIR
  and semantic preload.
- ✅ `src/compiler/hir.{h,c}` / `src/compiler/hir_routines.c` — HIR top-level
  category arrays, item/decl/routine arrays, and routine callee-id lists now use
  explicit capacity growth. Remaining HIR storage debt is scoped to CFG-local
  block/statement/predecessor/name arrays in the CFG owners.
- ✅ `src/compiler/hir_analysis.c`, `src/compiler/hir_cfg.c`,
  `src/compiler/hir_cfg_phi.c` — HIR signature/direct-call collection and CFG
  fact arrays (predecessors, dominance frontier, dom-tree children, local defs,
  phi candidates) now use explicit capacity growth. Remaining HIR storage debt
  is narrowed to CFG lowering block/statement construction.
- ✅ `src/compiler/hir_lower_intent_cfg.c` — intent CFG block and statement
  construction now uses explicit capacity growth. Remaining HIR lowering debt is
  the general statement CFG builder in `hir_lower_cfg_blocks.c`.
- ✅ `src/compiler/hir_lower_cfg_blocks.c` / `src/compiler/hir_lower_cfg.c` —
  general function-body CFG block and statement construction now uses explicit
  capacity growth and carries `cfg.block_capacity` through lowering. HIR no
  longer has known `count+1` append storage in its stable lowering/analysis
  owners.
- ✅ `src/compiler/rir.{h}` / `src/compiler/rir_facts.c` — RIR scope, fact,
  operation, and state-summary storage now uses explicit capacity growth instead
  of `count+1` realloc. The RIR fact owner now follows the same storage contract
  as HIR/DIR/AIR.
- ✅ `src/compiler/mir.h`, `src/compiler/mir_base_helpers.h`,
  `src/compiler/mir_cleanup.c`, `src/compiler/mir_intent.c` — MIR routine/block
  storage, instruction append/insert, cleanup predecessor append, and intent
  instruction append now use explicit capacity growth.
- ✅ `src/compiler/mir_decl_headers.h` / `src/compiler/mir_liveness_dce.h` —
  MIR declaration-header and value-summary storage now uses explicit capacity
  growth.
- ✅ `src/compiler/mir.h`, `src/compiler/mir_base_helpers.h`,
  `src/compiler/mir_ssa_rename.h`, `src/compiler/mir_ssa_use_edges.h`,
  `src/compiler/mir_liveness_dce.h` — MIR SSA/use/liveness name-list arrays now
  carry explicit capacities and grow geometrically. Remaining MIR reallocs in
  this owner are deliberate DCE shrink operations or fixed-size copies, not
  append-path `count+1` storage.
- 처방: routine/boundary id 인덱싱 (이름 strcmp 매칭 → id 비교), AIR
  evidence 빌드는 outer 1회 인덱스 빌드 후 inner는 hashmap probe. slot
  collect 1-pass

### Category D — Runtime hot path 자료구조
- ✅ `src/runtime/pgy_runtime_intent_trace_inline.h` — intent registry
  handle 조회는 handle→slot inline index로 고정됨.
- ✅ `src/runtime/pgy_runtime_intent_trace_inline.h` — trace append는
  `trace_len`을 추적해 기존 trace 길이 `strlen` 재계산을 피함.
- ✅ `src/runtime/pgy_runtime_intent_trace_events_inline.h` — step begin은
  빈 participant/slot/from/to/failure 필드를 미리 `strdup("")`하지 않고
  필요할 때만 materialize함.
- ✅ `src/parser/ast.c` — AST list append uses explicit capacity and
  geometric growth; the old `realloc(count+1)` O(N²) path is closed.
- ✅ `src/semantic/type_checker_flow_resources.h` /
  `src/semantic/type_checker_flow_loops.h` — body-safety resource snapshots now
  carry explicit capacity and grow through all-or-nothing reserve copies. This
  removes the prior multi-`realloc(count+1)` append path from parallel/loop
  ownership snapshots and avoids partial-realloc pointer loss on OOM.
- ✅ `src/semantic/type_system.h` / `src/semantic/type_env.c` — type environment
  variable/type bindings now carry explicit capacities and grow geometrically
  instead of reallocating on every append.
- ✅ `src/parser/parser_expr.c` — call pipe-prepend now uses the existing
  `AST_CALL.arg_capacity` field instead of reallocating to `old_count + 1`.
  Broader parser AST-list capacity cleanup remains a separate parser-owner
  task because many node variants still expose count-only arrays.
- ✅ `src/parser/parser.c`, `src/parser/parser_async.c`, `src/parser/ast.h` —
  destructuring names, async function parameters, async block statements, and
  select cases now carry explicit capacity fields and grow geometrically.
- ✅ `src/parser/parser_decl.c`, `src/parser/ast_constructors.c`,
  `src/parser/ast.h` — function parameters and nominal field/method lists now
  carry explicit capacities and grow geometrically during parse.
- ✅ `src/parser/parser_decl_function_clause.c`, `src/parser/ast.h` —
  function/action `requires` and `authorized by` clause arrays now use explicit
  capacities instead of count-only append reallocs.
- ✅ `src/parser/parser_type.c`, `src/parser/ast_types.h`,
  `src/parser/ast_domain_tail_constructors.c` — generic parameter lists,
  where-clause constraint lists, type-bound lists, and event-handler function
  type parameter lists now use explicit capacities.
- ✅ `src/parser/ast_domain_data.h` /
  `src/semantic/type_checker_intent_action_contract.c` — inherited intent-step
  `who`, `requires`, and `authorized by` lists now carry explicit capacities
  and avoid semantic `count+1` append paths.
- `src/runtime/pgy_runtime_builtin_hashmap_inline.h:113-120` — open
  addressing linear probing + strcmp per probe
- ✅ `src/runtime/pgy_runtime_queue_inline.h` — queue grow now uses `realloc`
  when `head == 0`; wrap-around queues keep the ordered-copy path.
- 처방: handle→slot inline hashmap, 빈 문자열 단일 sentinel 공유, AST
  capacity 별도 추적 + `next_pow2` grow, secondary hash 또는 quadratic
  probing

### 좋은 패턴 (이미 정확 — 회귀 방지용 기록)
- ✅ `src/common/arena.{c,h}` — bump + linked block + O(1) destroy
- ✅ `src/runtime/pgy_runtime_channel_inline.h` — 링버퍼 정확
- ✅ `src/runtime/pgy_runtime_plain_slot_inline.h` — debug-only safety
  check, 릴리즈 zero-overhead
- ✅ `src/codegen/transpiler_decl_lookup.c` — `last_decl_lookup_*` 캐시
- ✅ `src/parser/ast_destroy.c` — 정확 (단 arena로 옮기면 O(1))

### Sprint 우선순위 (베타 closure 후)

**Sprint Q (1-2주, 베타 closure 직후)**
- Q1. Category A 테이블화 — stdlib builtin dispatch 50+ 분기 → 단일 표
  + bsearch (또는 gperf). `key_type_classify(key)` 단일 함수로 통합
- Q2. Category B hashmap — Scope / TypeEnv / type alias map 도입.
  `compiler_hashmap_inline.h` 신규 owner
- Q3. AST geometric growth — `ast_add_*` capacity 별도 추적 + 기하 grow
- 검증: backend-compare 69/69, `make test-{air,semantic,mir,parser}`
  zero 회귀, 큰 .pgy 컴파일 시간 측정 표

**Sprint R (2-3주, Q 후) — 분석 패스 단축**
- R1. [~] Routine/boundary id 인덱스 (HIR routine-name call graph closed;
  AIR typed boundary-id indexing remains)
- R2. ✅ slot analyzer 1-pass collect + sorted live-set membership

**Sprint S (1주, R 후) — Runtime trace 정리**
- S1. ✅ Intent registry 핸들→슬롯 inline index
- S2. ✅ step begin 빈 문자열 allocation 제거
- S3. ✅ trace string append length tracking (`trace_len`)

### Out of scope (이 audit에서 *안* 다룸)
- AST arena 이행 (별도 큰 ticket)
- step name interned-id (별도 ticket, 언어 차원 변경 가능성)
- LLVM 백엔드 자체 최적화 패스 추가
- mimalloc/jemalloc 등 allocator 교체 (vision territory)
- gperf 의존성 추가 부담 시 정렬 + bsearch로 대체 가능 — 결정 필요

### 우선순위 정합
- §0a (WASM/WebGL) / §0b (Server backend) 와 *직교*. 어느 쪽 path를 먼저
  가든 Sprint Q/R/S 모두 이득
- 사용자 1인 프로젝트면 Sprint Q만 우선 처리해도 큰 데모에서 차이 큼
- 베타 closure 정합성 위협 없음 — 베타 후 우선순위 1 후보

## UTF-8 Progress Note - 2026-04-30 - AIR/DAG/CFG Contract Tightening

- AIR DAG evidence now reports actual generic/ability compatibility fact
  counts and treats any non-zero metadata materializer fallback as
  `AIR_DRIFT_DAG_FALLBACK_PRESENT` under strict evidence. This keeps DAG
  fallback debt visible at the abstraction-safety layer instead of letting it
  hide behind successful metadata hit counters.
- AIR MIR evidence now records global cleanup-block evidence as
  `AIR_EVIDENCE_MIR_CLEANUP` with `cleanup-block` provenance. MIR still owns
  cleanup generation and validation; AIR audits that the MIR cleanup fact exists
  and remains observable through the evidence graph.
- LLVM MIR fallback control is aligned with the compiler CFG-owned control
  contract. `llvm_mir_stmt_is_cfg_container(...)` now rejects fallback emission
  for `with`, `unsafe`, `defer`, `if`, `while`, `for`, `select`, `match`,
  `break`, `continue`, and `return`, matching the compiler-side
  `mir_stmt_ast_is_cfg_owned_control(...)` policy.
- Deleted the untracked root `ast` grep-output artifact. `src/**/*.inc` remains
  at zero files, and representative production owners stay under the 600 LOC
  split-review threshold.
- Local gates: `make cfg-body-dataflow-test-smoke`, `make test-air`,
  `make air-drift-test-smoke`, `make type-resolution-dag-test-smoke`,
  `make type-resolution-resolver-inventory-test-smoke`, `make test-mir`, and
  `make llvm-test-backend-compare` are green. Backend compare reports `69/69`
  cases passed.
- Follow-up DAG seam cleanup: metadata annotation readers are now centralized
  behind `semantic_type_resolution_lookup_annotation_nullable(...)` and
  `semantic_type_resolution_lookup_annotation_or_unknown(...)`. The resolver
  inventory gate now requires annotation-sensitive direct seams to stay at
  zero; local gates `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, and `make test-semantic` are green
  (`2359/0` semantic tests).
- Follow-up DAG public seam cleanup: raw resolved-type lookup is no longer
  exported through the semantic mega-header. It now lives behind the private
  metadata-owner header `type_checker_resolution_metadata_internal.h`, and the
  resolver inventory smoke rejects any re-export through
  `type_checker_internal.h` or non-metadata owners. This keeps
  `semantic_type_resolution_lookup_annotation_nullable(...)`,
  `semantic_type_resolution_lookup_annotation_or_unknown(...)`, and
  `semantic_type_resolution_lookup_or_materialize(...)` as the stable DAG-facing
  APIs outside metadata materialization owners.
- Follow-up DAG stage materializer gate: `type-resolution-dag-test-smoke` now
  parses and caps `stage-metadata-materialize` totals directly. Calls, failed
  materializations, and suppressed diagnostics must all remain `0`, not merely
  the family counters. This prevents a compatibility materializer from
  reappearing as a hidden successful path.
- Follow-up DAG writer inventory gate: resolved-type metadata recorders are now
  smoke-gated to graph, stage-signature, and metadata materialization owners.
  New semantic owners cannot write DAG resolved-type facts directly without
  failing `type-resolution-resolver-inventory-test-smoke`.
- Follow-up DAG stage-signature fallback removal: signature staging no longer
  calls the metadata materializer after a metadata miss. It now consumes graph
  dependency evidence and existing metadata only, then returns `TYPE_UNKNOWN`
  for unresolved quiet staging. The retired compatibility-family recorder was
  removed and the inventory smoke rejects reintroducing it or a stage-signature
  materializer call.
- Follow-up DAG diagnostic read-only tightening: metadata diagnostics now use
  `semantic_type_resolution_lookup_metadata_type_ref(...)` for generic argument
  checks instead of the materializing type-ref helper. The resolver inventory
  smoke rejects reintroducing materializer lookup from metadata diagnostics, so
  diagnostic-only paths cannot create new DAG resolved-type facts.
- Follow-up DAG helper inventory cap: metadata-first type-ref helper use is now
  owner-classified and capped at 15 total references, including the central
  declaration and implementation. New
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)` call sites
  fail `type-resolution-resolver-inventory-test-smoke` until they are
  deliberately classified, which prevents silent expansion of materializing DAG
  seams.
- Follow-up DAG generic-param evidence tightening: class, function, ability,
  and nominal staging generic parameters now register as
  `SYMBOL_TYPE_PARAM` with `TYPE_KIND_GENERIC`, not as class-like placeholder
  symbols. `type-resolution-dag-test-smoke` now requires non-zero
  `GENERIC_PARAM` graph evidence (`generic_param_nodes=29` locally), keeping
  generic params visible as generic evidence instead of declaration evidence.
- Follow-up DAG class-field seam removal: class/subject/vessel field signature
  staging now records metadata before graph-backed skip accounting, and
  `type_checker_class_decl.c` consumes annotation metadata instead of the
  materializing type-ref helper. The helper inventory is now capped at 14
  references, with local DAG stats at `graph-backed skips=2450`,
  `metadata_hits=7582`, and all materializer/retired-resolver counters at 0.
- Follow-up DAG domain/world field seam removal: relation/effect/zone/world
  field staging now records metadata before semantic owner checks consume the
  types. `type_checker_decls_domain_helpers.c` and
  `type_checker_world_helpers.c` now read annotation metadata instead of the
  materializing type-ref helper. The helper inventory is capped at 12
  references, with local DAG stats at `graph-backed skips=1980`,
  `metadata_hits=8052`, and all materializer/retired-resolver counters at 0.
- Follow-up DAG effective generic arg seam tightening: ability where validation
  now consumes centralized `collect_effective_generic_arg_types(...)` evidence.
  `type_checker_ability_where.c` is annotation-only again; the one remaining
  materializing lookup for effective ability args lives in
  `type_checker_generic_effective_args.c`, so owner-local ability validation no
  longer creates DAG facts as a side effect. ABI/runtime layout is unchanged,
  and the implementation did not grow the generic support implementation header.
  Intent role-field require checks now consume the same centralized effective
  generic arg type evidence, leaving `type_checker_intent_role_fields.c` under
  the 600 LOC split-review line.
- Follow-up generic class specialization evidence tightening: class
  specialization where-clause validation now consumes the same centralized
  effective generic arg type evidence instead of building a local Type array
  from effective arg nodes. This removes duplicate dependency/materialization
  work while keeping the materializing seam count capped at 12. Local DAG stats:
  `graph-backed skips=1980`, `metadata_hits=8044`, and all
  materializer/retired-resolver counters at 0.
- Follow-up intent binding owner split: intent participant/value lookup and
  transfer-target alias resolution moved to
  `type_checker_intent_bindings.c`. `type_checker_intent_role_fields.c` now
  stays focused on ability require-field validation and zone-binding
  derivation, dropping to 499 LOC without changing ABI/runtime layout or DAG
  fallback policy. Local gates: `make test-semantic`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`,
  `make build-source-inventory-test-smoke`, and
  `make semantic-tu-size-test-smoke`.
- Follow-up intent type owner split: intent-local type-ref resolution,
  participant/value type resolution, and step where-source labeling moved to
  `type_checker_intent_types.c`. `type_checker_intent_decl.c` dropped to 529
  LOC and now focuses on orchestration validation rather than carrying the
  materializing type helper inline. The resolver inventory still gates the same
  12 metadata-first type-ref helper references and all fallback/materializer
  counters remain at 0.
- Follow-up DAG intent inventory owner split: intent declaration precollect
  moved from the general declaration graph owner to
  `type_checker_resolution_graph_intent.c`. This keeps intent DAG inventory
  facts close to the intent surface and drops
  `type_checker_resolution_graph_decl.c` to 481 LOC without changing DAG stats
  or metadata/fallback counters.
- Follow-up DAG zone command inventory owner split: zone refresh/apply/link/
  detach/unlink/maintain dependency precollect moved to
  `type_checker_resolution_graph_zone_commands.c`. The remaining
  `type_checker_resolution_graph_zone_inventory.c` now owns only zone
  slot/shared/layer type inventory and is 76 LOC. This is a responsibility
  split, not a mechanical line-count split, and preserves the existing DAG
  stats (`graph-backed skips=1980`, `metadata_hits=8044`) with all
  materializer/retired-resolver counters at 0.
- Follow-up DAG graph validation owner split: the old
  `type_checker_resolution_graph_core.h` implementation header is removed.
  Graph cycle validation and topo ordering now live in
  `type_checker_resolution_graph_validate.c`, while
  `type_checker_resolution_graph_core.c` keeps node/edge/path/dependency
  primitives below the 600 LOC split-review signal.
- Follow-up split-policy correction: the 600 LOC rule is now a split-review
  trigger, not a mechanical slicing mandate. New `_helpers` owners are
  discouraged unless they name a real feature/fact owner, and near-duplicate
  helpers should become data-driven dispatch. As a small cleanup proof,
  `llvm_stmt_let_collections.c` now uses one enum-driven
  `llvm_stmt_diag_collection(...)` helper for missing type-argument and missing
  runtime-export diagnostics instead of two parallel helpers. Syntax gate:
  `gcc -DPGY_LLVM_ENABLED -fsyntax-only src/codegen/llvm_stmt_let_collections.c`.
- Follow-up CFG/MIR root identity validation: MIR validation now rejects
  overlapping entry/cleanup/rollback/invalidation roots, not only invalid root
  indexes. This prevents cleanup chain corruption from being hidden behind a
  valid block index. Local gate: `make test-mir cfg-body-dataflow-test-smoke`.
- Follow-up 600 LOC owner closure: `mir_cfg_contract_validate.h` no longer sits
  on the split threshold. Cleanup-edge fact lookup moved to
  `mir_cfg_contract_cleanup_fact.h` (26 LOC), leaving
  `mir_cfg_contract_validate.h` at 584 LOC. Local gates: `make test-mir` and
  `make cfg-body-dataflow-test-smoke`.
- Follow-up AIR/MIR evidence consumption: AIR now records whether MIR input was
  attached and default strict verification requires `AIR_EVIDENCE_MIR_PIN_CLEANUP`
  for `pin` execution boundaries once MIR evidence is available. This keeps MIR
  as the cleanup source of truth while preventing AIR from treating a pin
  boundary as closed without the matching MIR `pin-unpin-cleanup-edge` evidence.
  Local gate: `make test-air` (`48/0`).
- Follow-up AIR/CFG cleanup fact tightening: AIR no longer accepts an orphan
  `pin-unpin-cleanup-edge` instruction as pin cleanup evidence. The MIR pin
  block must also have a real cleanup successor that targets the routine cleanup
  block, so AIR consumes the CFG cleanup fact rather than trusting a standalone
  instruction string.
- Follow-up AIR evidence inventory shape tightening: strict evidence inventory is
  now boundary-shape checked. Global evidence cannot attach to a concrete
  boundary; HIR CFG evidence requires same-boundary HIR routine evidence; RIR
  authority evidence requires same-boundary RIR boundary evidence and a declared
  participant; MIR pin cleanup evidence can only target a `pin` execution
  boundary. Local gates: `make test-air`, `make air-drift-test-smoke`,
  `make air-json-schema-test-smoke`, and `make cfg-body-dataflow-test-smoke`.
- Follow-up AIR observability evidence closure: observability/trace schema is
  now a global `AIR_EVIDENCE_OBSERVABILITY_SCHEMA` node with provider
  `runtime-observability-schema`, subject `pgy.intent.observability.v1`, and
  a fact count derived from the runtime schema vocabulary. This moves the
  trace/observability ABI contract from JSON-only output into AIR evidence
  inventory. Local gates: `make test-air air-drift-test-smoke
  air-json-schema-test-smoke air-backend-nonimpact-full-test-smoke`.
- Follow-up DAG materializing seam reduction: abstract ability method
  signatures, projection path field readers, zone-authority subject-slot readers,
  world domain/shared type readers, expression-level method-call return,
  host-expression, constructor field, and intent participant/transfer/
  inherited-action readers now use
  `semantic_type_resolution_lookup_metadata_type_ref(...)` instead of the
  materializing type-ref helper. The owner-local fallback seam inventory is now
  gated at `0` (`fallback seams=0 cap=0`) while `retired_resolver_calls=0`,
  `materializer_fallbacks=0`, and `stage_materialize_calls=0` remain gated.
  Remaining DAG gaps are not simple fallback replacements: action-contract
  inheritance, intent role-field derivation, host overlay authority checks,
  generic ability where-clause diagnostics, and generic default validation need
  explicit stage/effective-argument evidence before their materializing
  owner-local seams can be removed without breaking semantic behavior.
- Follow-up AIR JSON schema gate: `make air-json-schema-test-smoke` now runs
  `pgy --air-json` on a stable intent/zone fixture and gates the
  `pgy.air.graph.v1` summary, boundary, evidence, drift, and observability
  shape. Python parses the JSON when present; when Python is absent, the smoke
  falls back to literal schema checks so platform CI does not fail merely
  because an interpreter is missing.
- Post-beta/beta+1 modeling pain point: add a zone-first authoring path that
  lets users model a business graph primarily with `zone` plus passive
  `struct/object/vessel` shapes, then progressively introduce `subject`,
  `authority`, and `projection` only when state-transition auditing, boundary
  mutation, or selective exposure is actually needed. This is not a new keyword
  track; it is a clause-density and progressive-disclosure track. Goal:
  reduce the need to spell `subject`/`authorized by`/projection clauses for
  every rich business object and avoid turning domain modeling into a compiler
  puzzle.
- Follow-up modeling guard: `docs/121_types_as_domain_medium.md`,
  `docs/19_design_philosophy.md`, zone-shape diagnostics, semantic regression,
  and `documentation-quality-test-smoke` now agree that `subject` is an
  identity-bearing state-transition host, not "important information";
  `authority` is boundary/mutation permission, not an importance rank; and
  passive business graph state should remain `struct`/`object`/`vessel` until a
  transition, handoff, authority, or projection contract is actually needed.
  Local gates: `make test-semantic` (`2366/0`) and
  `make documentation-quality-test-smoke`.
- Follow-up CI hardening: `cfg-body-dataflow-test-smoke` no longer hard-fails
  solely because Python is absent. When Python is available it still runs the
  full source/document contract audit; otherwise it falls back to a shell
  literal contract check and still runs the compiler HIR/RIR/MIR smoke. Local
  gates covered both paths.
- Follow-up runtime authority CI hardening:
  `runtime-authority-contract-test-smoke` now has the same shape. Python keeps
  the full raw-literal audit; no-Python runners get a shell literal contract
  fallback that still verifies shared authority code/reason/stderr macros,
  runtime include usage, raw literal bans, and LLVM authority token exports.
- Follow-up runtime panic CI hardening:
  `runtime-panic-contract-test-smoke` now also keeps its Python structural audit
  when available, but no-Python runners execute a shell literal fallback that
  verifies the shared panic class/reason surface, panic emitter ownership,
  released-slot/secure-token/unwrap/checked-arithmetic lowering hooks, and the
  core docs contract. `runtime-panic-abi-test-smoke` intentionally remains a
  Python-required executable ABI test because it validates subprocess exit code
  and stderr panic class behavior.
- Follow-up AIR CI hardening: `air-drift-test-smoke` now keeps the full Python
  source/document/test audit when available, but no-Python runners execute a
  shell literal fallback for the core AIR contract: verification-only
  architecture, strict evidence, `AIREvidenceNode`, MIR pin cleanup evidence,
  DAG generic evidence, driver synthesis hook, diagnostic docs, backend
  non-impact policy, and AIR proof obligations. Local gates covered both paths.
- Follow-up `.inc`/owner-size recheck: source production `.inc` inventory is
  still `0`. Local gates green:
  `semantic-inc-size-test-smoke`, `semantic-tu-size-test-smoke`,
  `production-header-size-test-smoke`, `backend-inc-size-test-smoke`,
  `test-inc-size-test-smoke`, and `inc-sentinel-test-smoke`.

## UTF-8 Progress Note - 2026-04-30 - AIR Payload Containment

- AIR final scope is now explicit: AIR is the 1.0 abstraction-safety closure
  layer, not the owner of the whole language. Beta keeps AIR Phase 1 narrow
  (`IntentNode`, `BoundaryNode`, strict evidence, drift facts). 1.0 requires
  first-class `EvidenceNode`s that audit HIR CFG, DIR, RIR, MIR cleanup/pin, and
  DAG generic/ability/module facts without letting AIR become a codegen IR or a
  replacement for CFG/DAG/ownership/runtime propagation.
- AIR now has first-class `AIREvidenceNode` inventory in addition to legacy
  per-boundary compatibility flags. HIR routine, HIR CFG, RIR boundary, and RIR
  authority evidence are recorded as provenance-carrying nodes and validated as
  AIR inventory. This is the first code-level step toward the AIR 1.0
  `EvidenceNode` contract while keeping existing driver diagnostics stable.
- AIR evidence inventory also carries the stable observability/trace schema as
  global evidence. `pgy.intent.observability.v1` is no longer only a JSON dump
  literal; it is validated as an evidence node sourced from the runtime schema.
- AIR strict evidence now treats `AIREvidenceNode` as authoritative whenever an
  evidence inventory is present. Legacy per-boundary booleans remain as cached
  summaries for dumps and compatibility fixtures, but they can no longer satisfy
  strict HIR/RIR/MIR evidence by themselves once inventory nodes exist.
- AIR consumer migration step: `air_boundary_has_evidence(...)` is now the
  public boundary evidence query. Driver diagnostics and AIR dumps use it, so
  user-facing AIR output consumes evidence inventory before legacy cached flags.
- AIR now has the first MIR evidence seam: `air_collect_mir_evidence(...)`
  records `pin-unpin-cleanup-edge` as `AIR_EVIDENCE_MIR_PIN_CLEANUP` for the
  matching AIR `pin` execution boundary. AIR still does not create or validate
  MIR cleanup facts; MIR remains the owner and AIR only audits provenance.
- AIR boundary walking and HIR containment now descend through payload carriers
  that can hide already-stable boundary kinds: event subscribe/unsubscribe
  handler payloads, party-instance assignment values, party shared-field
  initializers, world roster/zone initializers, and domain-slot initializers.
  These nodes are not new AIR boundary kinds; they only forward the walker to
  nested IO/parallel/channel/execution boundaries.
- `src/test_air.c` now includes an event-handler payload regression where
  `ReadFile(...)` is nested under an `AST_EVENT_SUBSCRIBE` handler. AIR must
  synthesize the IO boundary at the nested call AST. `AST_EVENT_SUBSCRIBE` and
  `AST_EVENT_UNSUBSCRIBE` are also classified as AIR execution boundaries, so
  event subscription is no longer invisible to the abstraction-safety layer.
  Local gate: `make test-air air-drift-test-smoke` (`51/0` AIR tests).
- AIR evidence provenance is now non-empty by invariant. HIR routine evidence,
  RIR boundary evidence, and RIR authority evidence flags with empty provenance
  names are rejected as `PGY_AIR_INVARIANT_INVALID`. Local gate: `make
  test-air air-drift-test-smoke` (`51/0` AIR tests).

## UTF-8 Progress Note - 2026-04-29 - Runtime Frontier LLVM Owner And Parallel MIR Preservation

- CFG/MIR cleanup validation is tightened: reachable non-cleanup blocks with a
  cleanup successor must now carry a materialized `cleanup-edge` MIR fact, and
  rollback/invalidation cleanup blocks must carry their named cleanup-edge
  facts. This prevents backend consumers from silently relying on topology
  fields without the explicit MIR cleanup fact inventory. `make test-mir` is
  green.
- MIR regression now corrupts rollback and invalidation cleanup fact names and
  requires `mir_validate(...)` to reject both cases. Cleanup topology alone is
  no longer enough for beta body-safety evidence; the named MIR fact inventory
  must stay intact.
- CFG loop fixed-point equality now compares resource `used_states` as well as
  consumed/released state. This closes a narrow body-safety drift where loop
  convergence could ignore borrow/use facts while still merging ownership
  facts. `cfg-body-dataflow-test-smoke` now gates the comparison directly.
- Large-owner cleanup continued after `.inc` closure:
  `transpiler_mir_ssa_emit.h` now delegates SSA lookup helpers to
  `transpiler_mir_ssa_lookup.h`, and runtime Set raw exports now live in
  `pgy_runtime_lib_set_raw_exports.h` with compiler runtime-cache dependency
  tracking. `llvm_backend_type_map.c` now delegates early forward-declaration
  eligibility checks to `llvm_backend_forward_declare.h`, and
  `llvm_expr_assignment_member_projection.h` now delegates read-side member
  access emission to `llvm_expr_member_access.h`. Runtime file-path resolution
  now lives in `pgy_runtime_lib_file_path_core.h`, with runtime-cache
  dependency tracking. `pgy_abi_spec.h` now keeps ABI layouts while
  `pgy_abi_spec_asserts.h` owns compile-time layout assertions. These former
  owners are now below the 600 LOC split-review line. `slot_security.c` now
  delegates crypto/token encryption helpers to `slot_security_crypto_ops.h` and
  context/statistics helpers to `slot_security_context_ops.h`. `world_roster.c`
  now delegates execution-plan/statistics/visualization/free helpers to
  `world_roster_plan_stats.h`. `party_runtime.c` now delegates parallel
  dispatch/thread coordination to `party_runtime_dispatch.h`. `pgy_lsp.c` is
  now dispatch-only; protocol helpers, document navigation handlers, hover
  lookup, and diagnostic publication live in separate `src/lsp/` translation
  units. `ast.h` now delegates domain-heavy AST payload shapes to
  `ast_domain_data.h` as named structs, not `.inc`-style field fragments. The
  current non-test production scan has 0 `.c/.h` owners above the 600 LOC
  split-review line, and `production_header_size_smoke.sh` now uses 600 LOC as
  its default cap for compiler/runtime/codegen/semantic/parser/LSP headers.
- AIR strict evidence now records HIR input presence. When HIR input exists,
  each AIR boundary must have matching HIR routine provenance; RIR-only
  boundary evidence is no longer enough to make a boundary look complete.
  `air_dump()` also prints `hir_input=yes/no`, and `make test-air` is green.
- LLVM declaration inventory helper ownership is now split without changing the
  public include seam: `llvm_inventory_internal.h` is a 185 LOC facade/domain
  inventory owner, `llvm_inventory_decl_lookup.h` owns MIR header-first
  declaration lookup at 257 LOC, and `llvm_inventory_host_methods.h` owns host
  method metadata accessors at 209 LOC. `make pgy` and
  `make mir-declaration-inventory-test-smoke` are green.
- Runtime slot manager lifecycle ownership is now below the split-review line:
  `slot_manager.c` is 564 LOC, while `slot_manager_query_lock.c` owns query,
  TTL cleanup, locking, stats, and fast wrappers at 240 LOC. `make
  test-security` and `make test-abi` are green.
- Lexer debug ownership is now split: `lexer.c` is 573 LOC and
  `lexer_token_debug.c` owns token stringification/debug printing at 127 LOC.
  `make test-parser` and `make test-semantic` are green.
- Parallel runtime ownership is now split without changing the public runtime
  include: `pgy_parallel.h` is a 494 LOC shared task/await facade,
  `pgy_parallel_blocking.h` owns the blocking pool at 146 LOC, and
  `pgy_parallel_coroutine.h` owns coroutine scheduling at 292 LOC. `make pgy`
  and `make test-abi` are green.
- Intent parser ownership is now split without changing parser exports:
  `parser_intent.c` is a 468 LOC declaration/default propagation owner and
  `parser_intent_step.h` owns step clause parsing at 297 LOC. `make
  test-parser` and `make test-semantic` are green.
- Expression parser string ownership is now split: `parser_expr.c` is a 524 LOC
  precedence/call/primary owner and `parser_expr_string.h` owns
  multiline/interpolation helpers at 150 LOC. `make test-parser` and
  `make test-semantic` are green.
- Slot pool performance ownership is now split: `slot_pool.c` stays focused on
  pool/list allocation below the 600 LOC split-review threshold, while
  `slot_pool_perf.c` owns timestamp, cache prefetch/alignment, and benchmark
  helpers. `make test-datastructures`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- RIR builder ownership is now split: `rir_builder.c` is a 281 LOC
  scope-orchestration owner, while `rir_builder_walk.c` owns AST body walking,
  call/resource op materialization, and block-condition walking at 363 LOC.
  `make test-rir`, `make test-air`, `make test-mir`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Runtime LLVM export ownership is split without changing the public runtime
  include seam: `pgy_runtime_lib_slot_array_io_string_exports.h` is now an
  8 LOC facade over secure-slot, device-slot, array/map, and IO/string owners
  at 161/84/239/296 LOC. The runtime object cache freshness list also tracks
  the new leaf owners, so prebuilt LLVM runtime objects cannot stay stale after
  a leaf export edit. `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Runtime channel/qubit export ownership is split without changing the public
  runtime include seam: `pgy_runtime_lib_channel_quantum_exports.h` is now a
  7 LOC facade over channel-int, channel-string, and qubit-state owners at
  327/319/69 LOC. The runtime object cache freshness list tracks those leaf
  owners too, so channel/qubit export edits invalidate cached LLVM runtime
  objects correctly. `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Runtime raw collection export ownership is split without changing the public
  runtime include seam: `pgy_runtime_lib_raw_collection_exports.h` is now an
  8 LOC facade over common helper, raw Queue, raw HashMap, and raw Set owners
  at 13/117/431/153 LOC. The runtime object cache freshness list tracks those
  leaf owners too. `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Type-resolution DAG retired-resolver naming debt is closed: the obsolete
  `type_checker_resolve.c` owner is gone. Retired compatibility counters now
  live in `type_checker_resolution_retired.c`, while assignability and
  constructed-type helpers live in `type_checker_type_helpers.c`. The resolver
  inventory smoke now rejects reintroducing `type_checker_resolve.c` or
  `type_checker_resolve.h`, and `semantic-core-shape` requires the new split
  owners. Local gates: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke test-semantic semantic-core-shape-test-smoke
  semantic-tu-size-test-smoke` are green.
- LLVM domain method forward declarations are metadata-first for hosted domain
  methods. `llvm_domain_forward.c` now resolves method name, params, and return
  type through `MIRDeclMethod` accessors before falling back to the temporary AST
  payload, and `mir-declaration-inventory-test-smoke` rejects direct AST
  param-count/return-type reads in that forward-declaration section.
- C MIR residual statement emission now keeps executable parallel-family
  statements even when the same AST also carries a `MIR_INST_RESOURCE_OP`
  observability hook. The hook is not allowed to replace task/channel runtime
  lowering. This fixes the C backend hang in `parallel_channel_sum`, where the
  channel receives were emitted but the parallel send body was dropped.
- CFG/MIR pin cleanup has an early-return regression now:
  `src/test_mir.c` checks that a pin-region block with a `return` terminator
  still carries the matching `pin-unpin-cleanup-edge` fact, and
  `cfg-body-dataflow-test-smoke` requires that fixture. This tightens the
  `ReleaseAfterUnpin(slot, all_cfg_exits)` bridge beyond normal fallthrough
  pin blocks.
- CFG/MIR pin cleanup also has branch-return coverage now:
  `PinBranchReturns` verifies that terminating `if`/`else` arms inside a pin
  region keep cleanup successor routing plus the `pin-unpin-cleanup-edge` fact.
  This closes a concrete all-exit cleanup regression class before the broader
  branch/join ownership lattice work.
- CFG/MIR pin cleanup also covers loop-control exits now: `PinLoopControl`
  verifies that `break` and `continue` lowered as `HIR_BLOCK_GOTO` inside a pin
  region still carry cleanup successor routing plus the `pin-unpin-cleanup-edge`
  fact. This tightens the all-exit cleanup bridge for loop exits without
  claiming full loop ownership/lifetime lattice closure.
- Backend compare now wraps generated executable runs with
  `PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS` (default 30s) when `timeout` is
  available, so parity regressions fail as test failures instead of wedging CI.
  Local gate: `make llvm-test-backend-compare` is green again (`196/0` ABI
  same-process, `65/65` backend compare).

- LLVM world sync no longer owns the whole transitive frontier scheduler body:
  `src/codegen/llvm_domain_world_frontier.c` now owns the bounded world
  frontier loop, derived-state recompute loop, zone-generation dirty detection,
  and overflow abort blocks. `src/codegen/llvm_domain_world_sync.c` is reduced
  to sync orchestration at 164 LOC, while the new frontier owner is 470 LOC.
  `runtime-frontier-contract-test-smoke` now gates the split owner directly.
- A real CFG/MIR drift bug was fixed: `parallel { ... }` was incorrectly listed
  as a CFG-owned control container even though HIR/MIR does not yet lower
  parallel into explicit CFG edges. MIR DCE therefore removed the parallel
  channel send before `select_ready`. `parallel` is now preserved as a
  side-effecting MIR statement until a future true CFG lowering exists.
- `make cfg-body-dataflow-test-smoke` now enforces that split explicitly:
  `AST_PARALLEL_BLOCK` must not appear in the CFG-owned control classifier,
  must remain in the MIR DCE side-effect set, and a parallel-send/select
  fixture must retain the parallel statement in MIR. This keeps the contract
  honest: AIR can observe `parallel`, but CFG does not yet own its execution
  edges.
- AIR inspection is now first-class: `pgy --air <source.pgy>` dumps the AIR
  verification summary after HIR/DIR/RIR evidence collection and before driver
  drift failure. AIR remains verification-only and is still absent from
  `CompilerIRBundle`, but evidence/drift state is now directly inspectable.
- AIR now classifies `AST_TASK_GROUP` as a `parallel` boundary source named
  `task-group`, and strict evidence requires matching same-AST RIR operation
  evidence for every beta-stable parallel boundary. RIR now materializes
  `AwaitRemote`, `Spawn`, `Async`, `Parallel`, and `TaskGroup` ops, so
  `task-group` is no longer a HIR-only exception.
- World handoff evidence is now same-AST specific for parsed/source-backed
  boundaries: a same-alias RIR `Move` / `Claim` in the same scope is not enough
  unless it points at the same intent-step AST. This closes a source-provenance
  false-positive in AIR strict evidence.
- RIR now has explicit channel boundary ops: `ChannelSend`, `ChannelRecv`, and
  `ChannelSelect`. AIR channel evidence requires the matching same-AST op when
  source provenance exists, so channel strict evidence no longer passes through
  a generic RIR scope alone. `make test-rir` now gates parsed-source `ch <-`,
  `<- ch`, and `select` lowering into those ops, not just manually assembled
  RIROp evidence.
- RIR now has explicit IO boundary evidence for the beta-stable IO builtin set:
  `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`,
  `Input`, `ReadLine`, `Now`, and `Sleep` lower to `RIR_OP_IO`. AIR accepts IO
  strict evidence only from a matching source/provenance op; the parsed
  `ReadFile` AIR test is now positive exact-evidence coverage instead of a
  deliberate missing-evidence negative.
- Parser call source spans were tightened for builtin calls and `AST_CALL`
  nodes. AIR no longer needs to treat parsed `ReadFile(...)` as a step-level
  fallback in the common path; containment matching remains only as a defensive
  fallback for older/no-span AST producers.
- AIR HIR CFG evidence is now containment-aware: nested boundary ASTs inside a
  CFG-carried statement or terminator value satisfy HIR evidence only when the
  enclosing CFG statement actually contains the boundary. This closes the
  `with { ReadFile(...) }` execution-boundary seam without accepting
  routine-name-only evidence. The matcher now follows the same core executable
  and expression forms as the AIR boundary walker, including loop conditions,
  parallel/async/task-group bodies, spawn/call/assignment subexpressions,
  arrays/tuples, await/channel/select, match, unsafe/defer, event invoke, and
  lambda body.
- AIR boundary walking and HIR containment now also descend into `let`
  declaration and destructuring initializers. A boundary hidden behind
  `let content = ReadFile(...)` inside an intent-step block is now synthesized
  as an AIR IO boundary instead of being treated as ordinary local syntax.
- Local gates: `make pgy`, `make llvm-test-smoke`,
  `make cfg-body-dataflow-test-smoke`, `make runtime-frontier-contract-test-smoke`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, and `make air-drift-test-smoke`.

## UTF-8 Progress Note - 2026-04-29 - HIR Intent CFG Evidence

- Intent routines now get a minimal HIR CFG materializer instead of relying on
  routine-only summaries for AIR boundary evidence.
- `src/compiler/hir_lower_cfg.c` remains the function-body CFG owner at 598
  LOC, while `src/compiler/hir_lower_intent_cfg.c` owns ordered intent-step
  clause CFG materialization at 184 LOC. This keeps the CFG lowerer family
  under the 600 LOC split-review threshold without adding `.inc` files.
- The intent CFG is deliberately an ordered clause/inventory CFG, not a full
  runtime propagation scheduler. It gives AIR strict evidence a concrete HIR
  CFG block containing the same step/clause AST boundary, so parsed-source
  intent boundaries can no longer pass on routine provenance alone.
- MIR population now preserves intent `MIR_INST_STMT` semantic carriers after
  CFG statement reconstruction. Intent participant/zone/authority/causes facts
  stay MIR inventory even when the source intent routine has HIR CFG.
- Local gates: `make test-hir`, `make test-air`,
  `make cfg-body-dataflow-test-smoke`, and `make air-drift-test-smoke`.

## UTF-8 Progress Note - 2026-04-29 - DAG Compatibility Inventory Tightening

- Type-resolution DAG fallback remains closed: `materializer_fallbacks=0`,
  alias/non-alias stage metadata materialization is 0, and direct semantic owner calls
  into `resolve_type_node(...)` stay smoke-gated.
- `compatibility-resolver` calls now report AST-kind inventory:
  `ast_type`, `channel`, `future`, `event_handler`, and `other`. Current
  semantic-suite max inventory is `0` compatibility calls: public semantic
  regression helpers now use `semantic_type_resolution_lookup_type_ref_or_materialize(...)`
  instead of entering the compatibility resolver directly.
- `tests/type_resolution_dag_smoke.sh` now gates that accounting and also
  requires compatibility resolver body fallbacks (`cache misses`) to stay `0`.
  The compatibility API call cap is tightened from 1000 to 0 because the
  global counter is now read as a max/last inventory value instead of summing
  repeated per-context stats lines.
  This makes the remaining DAG debt precise: the compatibility resolver body is
  removed, and the frozen semantic DAG smoke keeps the retired surface at
  `0` calls.
- `resolve_type_node(...)` is no longer exposed from the public
  `type_checker.h` surface. It remains declaration-only in the private
  resolver compatibility header, and
  `type-resolution-resolver-inventory-test-smoke` now rejects both public header
  re-exposure and semantic regression tests that call the compatibility resolver
  directly.
- The private compatibility evaluator body is removed. Only the compatibility audit
  counters remain, so `PGY_TYPE_RES_STATS=1` can continue reporting
  `retired_resolver_calls=0` and resolver body fallbacks at `0`. The resolver inventory
  smoke rejects reintroduced `resolve_type_node` evaluator bodies.

## UTF-8 Progress Note - 2026-04-29 - C Let Slot Owner Split

- C backend let-declaration lowering no longer carries Slot/DeviceSlot,
  ReadView/WriteView/MoveToken, and Slot/SecureSlot sugar logic inside the
  mixed `emit_let_decl(...)` owner. Those paths now live in
  `src/codegen/transpiler_let_slot_emit.h`.
- `src/codegen/transpiler_let_emit.h` is now 505 LOC and
  `src/codegen/transpiler_let_slot_emit.h` is 297 LOC, so the let-declaration
  owner family is below the 600 LOC split-review threshold without adding
  `.inc` files.
- Local gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green.
- C domain role/ability ownership is also split away from propagation
  provenance. `transpiler_domain_provenance_emit.h` now owns hidden
  epoch/cause field emission and projection-chain bounded recompute, while
  `transpiler_domain_role_ability_emit.h` stays focused on role/ability
  lowering. Current sizes are 237 LOC and 452 LOC respectively.
- Parity gate after both C owner splits: `make llvm-test-backend-compare`
  remains green (`196/0` ABI same-process, `65/65` backend compare).
- C function/class/flow ownership is now below the 600 LOC split-review
  threshold. `transpiler_class_decl_emit.h` owns non-generic class declaration
  lowering, while `transpiler_func_class_flow_emit.h` keeps function fallback,
  generic class specialization, with-slot, and return lowering. Current sizes
  are 138 LOC and 594 LOC respectively. Local gates: `make pgy`,
  `make test-transpile`, `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green. Parity gate:
  `make llvm-test-backend-compare` remains green (`196/0` ABI same-process,
  `65/65` backend compare).
- C MIR block ownership is also back below the split threshold:
  `transpiler_mir_emit_predicates.h` owns the small MIR emission predicate
  wrappers, leaving `transpiler_mir_block_emit.h` focused on block statement
  emission at 589 LOC. Local gates: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green.
- C declaration lookup ownership is now split by concern:
  `transpiler_decl_lookup.c` keeps named declaration, alias, inventory, and
  method-list lookup at 419 LOC, while `transpiler_decl_host_lookup.c` owns
  current-host, owner-host, nominal-host, and nominal-method lookup at 216 LOC.
  Local gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  are green (`196/0` ABI same-process, `65/65` backend compare).
- C type mapping ownership is now split by concern:
  `transpiler_type_mapping_helpers.h` keeps primitive/collection/slot/result
  mapping and suffix helpers at 563 LOC, while
  `transpiler_type_render_helpers.h` owns AST type-name rendering at 102 LOC.
  Local gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  are green (`196/0` ABI same-process, `65/65` backend compare).
- CFG contract validation ownership is now below the split threshold:
  `mir_cfg_contract_validate.h` keeps CFG cleanup, successor, and predecessor
  validation at 551 LOC. `mir_cfg_contract_pin.h` owns pin cleanup edge
  validation at 39 LOC, while
  `mir_cfg_contract_control.h` owns CFG-owned AST control classification at
  32 LOC. Local gates: `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make abi-ownership-shape-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green.
- MIR SSA/local type ownership is now split by concern:
  `transpiler_mir_ssa_names.h` keeps SSA name resolution, SSA map setup,
  claim-shape predicates, and implicit-field rendering at 357 LOC, while
  `transpiler_mir_local_type_lookup.h` owns AST body local type lookup and
  expression fallback inference at 293 LOC. Local gates: `make pgy`,
  `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  are green (`196/0` ABI same-process, `65/65` backend compare).

## UTF-8 Progress Note - 2026-04-29 - Runtime Frontier Policy And C Owner Split

- 2026-04-30 follow-up: LLVM frontier overflow emission now has one owner.
  World derived overflow, world transitive-frontier overflow, zone overflow,
  and projection-chain overflow all route through
  `llvm_emit_frontier_overflow_abort(...)` instead of open-coded abort blocks.
  `runtime-frontier-contract-test-smoke` includes the shared helper in both
  world/zone and projection contract bundles so future LLVM emitter drift is
  caught at the helper seam, not by duplicated backend-local snippets.
- Stable world outer frontier scheduling now consumes
  `pgy_frontier_world_transitive_pass_limit(...)` in both the C emitter and the
  LLVM world sync emitter. The helper currently delegates to the existing
  monotone `zone_count + state_count + 1` world pass bound, but the contract
  name makes the world zone-sync plus derived-state recompute family a shared
  source-of-truth policy instead of a backend-local constant.
- `make runtime-frontier-contract-test-smoke` now gates the transitive frontier
  policy helper in `src/runtime/pgy_frontier_policy.h`, the dedicated
  `make runtime-frontier-policy-test-smoke` arithmetic check, and both C/LLVM
  world emitters, while keeping the broader world-zone propagation family open
  as the remaining runtime propagation blocker.
- C/LLVM world frontier emitters now carry a separate derived-state
  changed-any fact. A bounded derived recompute that converges after changing a
  world state still causes the outer transitive frontier to run one more pass
  before dirty flags are cleared. This closes the prior dirty-flag-only seam.
- Frontier pass-limit formulas now saturate through the same u32-bounded
  source-of-truth helpers (`pgy_frontier_pass_limit_add*`) before C/LLVM
  emission. This keeps the C `size_t` loops and LLVM i32 loop counters from
  drifting on oversized generated frontier families.
- C backend world/select/event emission is split by owner:
  `transpiler_world_select_event_emit.h` now owns world emission only,
  `transpiler_select_emit.h` owns `select`, and `transpiler_event_emit.h` owns
  event declarations/subscription lowering. Current sizes are 370, 155, and
  103 LOC respectively, keeping this family below the 600 LOC split-review
  threshold without adding `.inc` files.
- Local gates: `make pgy`, `make runtime-frontier-contract-test-smoke`,
  `make runtime-frontier-policy-test-smoke`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.

## UTF-8 Progress Note - 2026-04-29 - AIR Evidence Diagnostic Consumer

- AIR evidence requirements are now public AIR policy queries:
  `air_boundary_requires_hir_evidence(...)` and
  `air_boundary_requires_rir_evidence(...)`.
- The driver AIR drift diagnostic consumes those AIR facts and distinguishes
  missing HIR CFG evidence, missing RIR boundary evidence, and missing authority
  evidence. This removes stale RIR-only wording after strict evidence started
  requiring HIR CFG evidence for implementation boundaries.
- AIR HIR evidence now separates routine provenance from CFG provenance:
  `has_hir_routine_evidence` and `has_hir_cfg_evidence` are distinct flags, and
  `hir_cfg_evidence_count` increments only when the matching HIR routine carries
  generated CFG for the same boundary AST when one is available. This keeps
  routine-only intent summaries from masquerading as body CFG proof.
- HIR evidence matching is now source-specific. A `HIR_TOPLEVEL_INTENT` routine
  no longer satisfies every AIR boundary by kind alone; it must match the intent
  owner, step, or boundary source name. `test_air` includes a negative case that
  rejects unmatched top-level intent HIR evidence while accepting matching RIR
  boundary evidence.
- `make air-drift-test-smoke` gates the public policy queries and the
  HIR/RIR-specific diagnostic wording. `make test-air` is green (`28/0`).

## UTF-8 Progress Note - 2026-04-29 - DAG Type-Ref Shortcut Tightening

- `semantic_type_resolution_lookup_metadata_type_ref(...)` now materializes
  stable constructed type refs without a resolver compatibility body.
  This moves constructed stable shells reached through the type-ref API onto the
  DAG metadata path, not the recursive resolver path.
- `semantic_type_resolution_lookup_type_ref_or_materialize(...)` is now the
  named metadata-first API for semantic owners that still need diagnostic
  materialization on unresolved refs.
- `semantic_stage_resolve_type_quiet(...)` now asks the metadata type-ref API
  before recording a compatibility stage fallback. This keeps signature-stage
  compatibility callers metadata-first even when they are not fully graph-skipped.
- Intent participant/value/where type refs and zone authority subject-slot type
  refs now use the same metadata-first helper before the diagnostic materializer
  path. Ability where refs, class/func signature refs, action contract refs,
  domain slot refs, world slot refs, expression/member/operator annotation refs,
  generic defaults/contract refs, async channel parameter refs, ownership refs,
  and projection path refs also consume this helper. This narrows the first
  semantic-owner seam without moving the whole domain AST lowering rewrite into
  beta.
- `type-resolution-resolver-inventory-test-smoke` now fails if a semantic owner
  bypasses the metadata-first helper and calls the diagnostic materializer
  directly. Only central metadata/diagnostic compatibility owners are allowed to
  call `semantic_type_resolution_lookup_or_materialize(...)`.
- Signature-stage quiet resolution no longer keeps a direct diagnostic
  materializer call. After metadata preflight misses it routes through
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)`, and the
  resolver-inventory smoke removed
  `type_checker_resolution_stage_signature.c` from the direct-materializer
  allowlist.
- Stable constructed-type diagnostic argument resolution also uses the
  metadata-first type-ref helper. The only remaining direct
  `semantic_type_resolution_lookup_or_materialize(ctx, ...)` call is the
  central metadata type-ref helper's fallback branch in
  `type_checker_resolution_metadata.c`.
- The direct materializer smoke allowlist is now narrowed to that central
  metadata owner only; `type_checker_resolve.c` remains counter-only and is no
  longer allowlisted for direct materializer calls.
- DAG gates remain green: `type-resolution-dag-test-smoke` reports
  `materializer_fallbacks=0`, `stage_materialize_alias=0`, and `stage_materialize_non_alias=0`.

## UTF-8 Progress Note - 2026-04-28 - LLVM Intent/Domain Owner Split

- LLVM intent declaration ownership is below the 600 LOC split-review
  threshold. `llvm_intent.c` now owns orchestration only, while
  `llvm_intent_setup.c` owns entry/participant binding,
  `llvm_intent_step_context.c` owns MIR/AST step carrier context validation,
  and `llvm_intent_cleanup.c` owns cleanup/rollback/invalidation tail
  emission. The MIR-only carrier diagnostic path remains explicit instead of
  falling back to AST helper inventory.
- LLVM domain declaration ownership is also below the threshold.
  `llvm_domain.c` now owns domain struct registration orchestration,
  `llvm_domain_forward.c` owns sync/method forward declarations plus ability
  vtable and role forward registration, and `llvm_domain_struct_fields.c`
  owns effect-pool and projection-state field helpers.
- Local gate: `make llvm-test-smoke` is green after both splits. Current owner
  sizes: `llvm_intent.c` 555 LOC, `llvm_domain.c` 568 LOC,
  `llvm_domain_forward.c` 306 LOC, and `llvm_domain_struct_fields.c` 80 LOC.
- This closes the immediate LLVM intent/domain review-band slice. Remaining
  backend debt is now concentrated in declaration inventory/bootstrap seams,
  projection overlay/C emitter owners, and C/LLVM parity edge coverage.
- C backend orchestration ownership is also back below the threshold:
  `transpiler.c` now stays at 587 LOC after moving public entry/result
  lifecycle to `transpiler_entry.c`, runtime thread-pool requirement scanning
  to `transpiler_thread_pool.c`, and small include/impl-ability declaration
  emitters to `transpiler_misc_decl.c`.
- Backend parity gate after the C split is green: `make llvm-test-backend-compare`
  reports ABI same-process `196 passed, 0 failed` and backend compare
  `64/64 passed, 0 failed`.
- Projection overlay ownership is also below the threshold:
  `transpiler_overlay_projection.h` now stays at 533 LOC after moving
  host-field/self-cell probes to `transpiler_overlay_host_fields.h` and
  zone relation/effect bind-layer emission to `transpiler_overlay_zone_bind.h`.
- Backend parity gate after the projection overlay split is still green:
  `make llvm-test-backend-compare` reports ABI same-process `196 passed,
  0 failed` and backend compare `64/64 passed, 0 failed`.
- LLVM zone sync ownership is now below the threshold:
  `llvm_domain_zone_sync.c` stays at 510 LOC after moving relation clause
  lowering (`link` / maintained relation / `unlink`) to
  `llvm_domain_zone_sync_relations.c`.
- Local gates after the zone sync split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- LLVM world sync ownership is now below the threshold:
  `llvm_domain_world_sync.c` stays at 592 LOC after moving world command
  directive lowering plus state/zone-slot lookup helpers to
  `llvm_domain_world_sync_directives.c` behind
  `llvm_domain_world_sync_internal.h`.
- Local gates after the world sync split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- LLVM runtime declaration ownership is now below the threshold:
  `llvm_runtime.c` stays at 533 LOC after moving raw collection export
  declarations to `llvm_runtime_raw_collections.c` and channel export
  declarations to `llvm_runtime_channels.c` behind `llvm_runtime_internal.h`.
- Local gates after the runtime registry split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- LLVM expression boundary/projection helper ownership is now below the
  threshold: `llvm_expr_boundary_projection_helpers.h` stays at 470 LOC after
  moving projection nominal lookup, nested vessel path resolution,
  projection-path value loading, and `ProjectSubject` emission to
  `llvm_expr_projection_path_helpers.h`.
- Local gates after the expression helper split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare). Projection-path
  helpers are shared by expression, host spawn literal, assignment projection,
  and domain projection sync emission.
- LLVM host/spawn literal helper ownership is now below the threshold:
  `llvm_expr_host_spawn_literal_helpers.h` stays at 345 LOC after moving
  async await-task result materialization, direct function-call argument
  emission, generic callee monomorphization, and spawn-expression wrapper
  lowering to `llvm_expr_spawn_call_helpers.h`.
- Local gates after the spawn/call split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- CFG body-dataflow consumer tightening: MIR statement population no longer
  preserves HIR-expanded control containers (`if`, `while`, `for`, `select`,
  `match`, `break`, `continue`) as fallback `MIR_INST_STMT` instructions when
  the block already has CFG successor edges. `for` preheader initialization is
  now a dedicated `MIR_INST_LOOP_INIT` fact consumed by C and LLVM. For-loop
  condition and backedge emission now consume the header `MIR_INST_BRANCH`
  metadata instead of re-reading `target->source_ast`. The loop variable and
  start/end expressions are carried on MIR instructions (`arg0`, `expr0`,
  `expr1`) and validated by `mir_validate()`. `mir_validate()` rejects
  CFG-owned control statements that reappear as fallback STMTs, preventing
  C/LLVM drift from emitting both MIR CFG edges and AST control flow. MIR DCE
  now consumes the same `mir_stmt_ast_is_cfg_owned_control(...)` classifier
  instead of keeping its own CFG-control AST switch, so statement population,
  validation, and DCE share one source of truth for control-container STMTs.
- Follow-up closure: CFG-owned `for value in List<T>` now uses the same MIR
  facts on both backends. C and LLVM emit a MIR-owned index slot, list-size
  condition, list-get body binding, and backedge increment instead of falling
  through AST loop lowering. `tests/cases/backend_compare/for_in_list_int`
  locks the C/LLVM parity path.
- Gates after this CFG slice are green: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, and
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65` backend
  compare).
- CFG/AIR handoff tightening: `with`, `unsafe`, and `defer` are now included in
  the CFG-owned boundary set when a MIR block already has successor edges. MIR
  statement population skips these boundary containers in expanded CFG blocks,
  and `mir_validate()` rejects them if they reappear as fallback `MIR_INST_STMT`
  instructions. `parallel` remains AIR-visible and semantic-flow checked, but is
  deliberately not CFG-owned until HIR/MIR has a real parallel CFG lowering.
  MIR DCE now preserves `parallel` as a side-effecting STMT so channel sends and
  task effects cannot be erased before backend emission.
- AIR abstraction-boundary lift: AIR now has an explicit `execution` boundary
  kind for `with`, `unsafe`, `defer`, and pin-block AST metadata. These
  boundaries are sync execution boundaries and strict evidence requires HIR/CFG
  evidence instead of RIR resource-boundary evidence. The AIR walker now also
  descends into `with` bodies, so nested IO/time boundaries inside a `with`
  block are visible to AIR instead of being hidden behind the execution
  container. This closes the first CFG -> AIR handoff seam: AIR can now see
  body/execution boundary facts that CFG already owns, rather than treating
  them as ordinary AST containers.
- ABI ownership shape gate added: `make abi-ownership-shape-test-smoke` now
  ties the implemented Slot/Pin ABI shape, runtime pin generation/thread/token
  invariants, C/LLVM pin/unpin lowering, MIR cleanup evidence, backend compare
  pin fixtures, and Zone-Bound Handle docs contract into one shell-only gate.
- MIR declaration inventory smoke is shell-only now. It still blocks raw
  declaration/routine inventory access outside the helper owners, requires MIR
  method metadata accessors, and keeps `declaration-side MIR-only debt` visible
  without requiring Python on CI runners.
- Runtime ABI lifetime smoke is shell-only now. It preserves the borrowed
  runtime string, result-owned string/array, runtime-owned file-handle, macro
  export, and ownership proof-doc checks without requiring Python on CI
  runners.

## UTF-8 Progress Note - 2026-04-28 - Semantic Owner Split

- `src/semantic/type_checker_builtins_stdlib_body.c` is now below the 600 LOC
  split-review threshold after moving `List` / `Set` / `Queue` / `Array`
  builtin typing to `src/semantic/type_checker_builtins_stdlib_collections.c`
  behind `type_check_stdlib_collection_call(...)`.
- Current stdlib builtin owner sizes: `type_checker_builtins_stdlib_body.c`
  510 LOC, `type_checker_builtins_stdlib_collections.c` 356 LOC,
  `type_checker_builtins_stdlib_scalar.c` and
  `type_checker_builtins_stdlib_map.c` remain existing focused owners.
- Local gates for this slice: `make test-semantic semantic-core-shape-test-smoke
  type-resolution-dag-test-smoke type-resolution-resolver-inventory-test-smoke`
  (`2359/0`, `materializer_fallbacks=0`, fallback seams=0).
- Builtin query implementation-header debt is now split into named owners:
  `type_checker_builtins_query.c` owns generic builtin arity, borrowed
  boundary store rejection, `HasProjection`, `HasLayer`, and `HasState`;
  `type_checker_builtins_query_world.c` owns `HasZone` and
  `HasZoneProjection` / `HasZoneLayer` / `HasZoneState`;
  `type_checker_builtins_query_channel.c` owns channel send/recv/close
  builtin typing; and `type_checker_builtins_query_domain.c` owns the shared
  domain lookup helpers. The `type_checker_builtins_query*.h` files are now
  declaration-only guards.
- Slot builtin debt is also split: `type_checker_builtins_slotops.c` owns
  slot lifecycle/view/move/device-slot builtins,
  `type_checker_builtins_secure_token.c` owns secure-token validation, and
  `type_checker_builtins_resolve.c` owns builtin name resolution. The old
  slotops implementation header is now declaration-only.
- Nominal builtin dispatch is now split as a real TU:
  `type_checker_builtins_nominal.c` owns the non-intent-observability builtin
  dispatcher path, `type_checker_builtins_intent_observability.c` owns the
  `IntentLast*` / `IntentHistory*` / `IntentActive*` / `IntentRecent*`
  observability family, and `type_checker_builtins_nominal.h` is
  declaration-only. Both implementation owners are under the 600 LOC
  split-review threshold.
- Slot analyzer summary debt is split: `slot_analyzer_summary.c` now owns
  access/function-alias/parameter summary behavior, while
  `slot_analyzer_escape.c` owns escape record/collect/mask behavior. Both are
  below the 600 LOC split-review threshold and the semantic shape gate tracks
  both owners.
- Function declaration implementation-header debt is closed:
  `type_checker_func_decl.c` owns function type/scope/body orchestration,
  `type_checker_func_action_contract.c` owns action-specific
  within/causes/authorized-by validation, and `type_checker_host_helpers.c`
  owns shared host/overlay/domain-slot helpers. The old
  `type_checker_program.h` and `type_checker_host_helpers.h` implementation
  bodies are gone, and all three owners are below the 600 LOC split-review
  threshold.
- `src/semantic/type_checker_expr.h` is now declaration-only; the expression
  dispatcher/member implementation moved to `type_checker_expr.c`.
- `src/semantic/type_checker_expr_call.c` now owns public call dispatch:
  builtin/stdlib calls, slot method sugar, static-member calls, hosted
  nominal method dispatch, and embedded-world-zone mutation rejection.
- `src/semantic/type_checker_expr_host.c` now owns host-field/method lookup
  and host-method call typing behind explicit `expr_*` seams. This avoids
  reusing the old implementation-header helper names and keeps
  `type_checker_internal.h` declaration-only.
- `src/semantic/type_checker_resolve.c` no longer owns the retired recursive
  type-node compatibility body that used to rely on an implementation-header
  side effect. The obsolete `type_checker_resolve.h` compatibility header has
  been deleted, and DAG lookup/materialization callers use metadata-first APIs
  instead of linking against a compatibility resolver.
- `src/semantic/type_checker_resolution_helpers.c` now owns the
  metadata-first `resolve_named_type(...)`, alias lookup, symbol-kind labels,
  and embedded-world-zone mutation guard that used to live in
  `type_checker_resolution_helpers.h`. The header is declaration-only and
  `type-resolution-resolver-inventory-test-smoke` now rejects implementation
  bodies in both resolver helper headers.
- DAG named builtin/shell lookup no longer lives as a local string table inside
  `resolve_named_type(...)`. Stable scalar builtins and stable shell names now
  route through the metadata owner
  `semantic_type_resolution_metadata_named_builtin_or_shell_singleton(...)`,
  and `type-resolution-resolver-inventory-test-smoke` rejects reintroducing
  `strcmp(name, "...")` builtin/shell tables in the compatibility helper.
- The retired `resolve_type_node(...)` compatibility entry no longer has an
  evaluator body. Stable builtin/named/constructed refs stay metadata-owned
  through `semantic_type_resolution_lookup_metadata_type_ref(...)` and
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)`.
- The metadata type-ref API records stable constructed types before returning
  unresolved, and the signature-stage quiet resolver consumes that same API
  before fallback accounting. This keeps the recursive resolver compatibility
  seam absent without hard-crashing valid diagnostic paths.
- Intent/zone/domain/world/action/class/function local type-ref helpers now
  consume `semantic_type_resolution_lookup_type_ref_or_materialize(...)`, the
  named metadata-first API that falls back to diagnostic materialization only
  when metadata cannot answer. The same API now covers expression,
  generic-default, async-channel, ownership, and projection-path refs; direct
  materializer calls are blocked outside central compatibility owners.
- Current sizes: `type_checker_expr.h` 10 LOC,
  `type_checker_expr.c` 353 LOC,
  `type_checker_expr_call.c` 433 LOC,
  `type_checker_expr_host.c` 212 LOC,
  `type_checker_resolve.c` 44 LOC,
  `type_checker_resolution_helpers.c` 282 LOC, and
  `type_checker_resolution_helpers.h` 22 LOC. The expression semantic owner
  family is now below the 600 LOC split-review threshold.
- Local gates: `make test-semantic`,
  `make type-resolution-resolver-inventory-test-smoke`, and
  `make type-resolution-dag-test-smoke` are green (`2359/0`,
  fallback seams=0, `materializer_fallbacks=0`).

## UTF-8 Progress Note - 2026-04-28 - HIR CFG Phi Owner Split

- `src/compiler/hir_cfg.c` now keeps CFG structural analysis only:
  predecessor finalization, reachability, dominance/frontier, dominator tree,
  natural loops, and CFG summary finalization.
- Local-def collection, SSA-name collection, phi-candidate placement, and phi
  materialization moved to `src/compiler/hir_cfg_phi.c` behind the private
  `src/compiler/hir_cfg_internal.h` seam.
- Current sizes: `hir_cfg.c` 388 LOC, `hir_cfg_phi.c` 222 LOC, and
  `hir_cfg_internal.h` 8 LOC. This closes the last active HIR CFG owner-size
  review-band item without reintroducing `.inc` files.
- Local gates: `make test-hir test-mir cfg-body-dataflow-test-smoke`
  (HIR 14/0, MIR 14/0).

## UTF-8 Progress Note - 2026-04-28 - Semantic Effects Helper Header Debt Split

- `src/semantic/type_checker_helpers_effects.h` is now declaration-only. The
  former implementation-header body moved to named semantic owners:
  `type_checker_helpers_effects.c` for effect/type helper behavior,
  `type_checker_projection_path.c` for projection source field-path
  resolution, and `type_checker_world_embedding.c` for world constructor
  zone-embedding handoff diagnostics.
- Current sizes: `type_checker_helpers_effects.c` 504 LOC,
  `type_checker_projection_path.c` 177 LOC,
  `type_checker_world_embedding.c` 132 LOC, and
  `type_checker_helpers_effects.h` 11 LOC.
- This removes another implementation-style private header from semantic core
  and keeps these owners under the 600 LOC split-review threshold.
- Local gates: `make test-semantic semantic-core-shape-test-smoke`
  (`2359/0`).

## UTF-8 Progress Note - 2026-04-28 - CFG Flow Effect Owner Split

- `src/semantic/type_checker_flow_effects.h` is now declaration-only. The
  branch-effect conflict, unreachable-statement, and effect-delta merge
  implementation moved to the real owner
  `src/semantic/type_checker_flow_effects.c`.
- `src/semantic/type_checker_flow.c` stays focused on CFG body-flow
  orchestration and fact consumption. Effect diagnostics no longer live in an
  implementation-style private header, which keeps the CFG cleanup direction
  aligned with the no-`.inc` / named-owner rule.
- Current sizes: `type_checker_flow.c` 457 LOC,
  `type_checker_flow_effects.c` 122 LOC, and
  `type_checker_flow_effects.h` 33 LOC.
- Local gates: `make cfg-body-dataflow-test-smoke test-semantic`
  (`2359/0`), `make semantic-core-shape-test-smoke
  backend-inc-size-test-smoke type-resolution-dag-test-smoke`
  (`materializer_fallbacks=0`).

## UTF-8 Progress Note - 2026-04-28 - CFG Body Flow Flag Consumption Tightening

- `src/semantic/type_checker_flow.c` now routes fallthrough/terminator flag
  consumption through named helpers: `flow_record_statement_result()`,
  `flow_has_fallthrough()`, and `flow_terminating_flags()`. This keeps the
  semantic body-flow owner focused on CFG fact consumption instead of open-coded
  flag masks at each join.
- `tests/cfg_body_dataflow_smoke.sh` now gates those helper seams alongside the
  existing all-path return, unreachable statement, resource snapshot, defer, and
  parallel boundary terms.
- Local gates: `make cfg-body-dataflow-test-smoke test-semantic`
  (`2359/0`).

## UTF-8 Progress Note - 2026-04-28 - MIR Cleanup CFG Shape Validation

- `src/compiler/mir_cfg_contract_validate.h` now rejects cleanup blocks that
  carry normal CFG successors and cleanup blocks that are also marked as pin
  regions. Cleanup/rollback/invalidation must remain exceptional cleanup-chain
  blocks, not normal body-flow blocks.
- `src/test_mir.c` adds a negative corruption regression that mutates an intent
  cleanup block to point at a normal successor and expects `mir_validate()` to
  reject it with the cleanup-block/normal-CFG-successor diagnostic.
- `tests/cfg_body_dataflow_smoke.sh` now gates the validator terms so this
  cannot regress into an undocumented MIR convention.
- Local gates: `make test-mir cfg-body-dataflow-test-smoke` (MIR 14/0).

## UTF-8 Progress Note - 2026-04-28 - Zone Lifecycle Authority Presence Split

- `src/semantic/type_checker_zone_decl.c` no longer owns the repeated
  lifecycle `by <subjectSlot>` presence diagnostics for authority-bearing
  zones. Apply/link/detach/unlink/maintain authority-presence checks now route
  through `type_check_zone_lifecycle_authority_presence()` in
  `src/semantic/type_checker_zone_decl_authority.c`.
- Current zone semantic owner sizes are `type_checker_zone_decl.c` 487 LOC and
  `type_checker_zone_decl_authority.c` 300 LOC. This keeps the zone declaration
  family under the 600 LOC split-review threshold while moving authority policy
  wording into the authority owner.
- Local gates: `make semantic-core-shape-test-smoke`; `make test-semantic`
  (2359/0).

## UTF-8 Progress Note - 2026-04-28 - Intent Authority/Participant Owner Split

- `src/semantic/type_checker_intent_decl.c` no longer owns the full
  authority/authorized-by validation body. The missing `authorized by` contract
  diagnostic and authorized participant-to-zone-authority resolution moved to
  `src/semantic/type_checker_intent_authority.c`.
- Intent `who` participant validation, zone subject-slot matching, transfer
  source subject-slot matching, and action-match detection moved to
  `src/semantic/type_checker_intent_participants.c`.
- Current intent semantic owner sizes are `type_checker_intent_decl.c` 504 LOC,
  `type_checker_intent_authority.c` 242 LOC, and
  `type_checker_intent_participants.c` 115 LOC, keeping the family under the
  600 LOC split-review threshold without adding `.inc` files.
- This is an incremental domain-checker slimming slice, not a full
  Domain-AST-to-Core-AST rewrite. Intent declaration orchestration still owns
  step order and summary flow; authority and participant proof/diagnostic
  ownership are now named semantic owners.
- Local gates: `make semantic-core-shape-test-smoke`; `make test-semantic`
  (2359/0).

## UTF-8 Progress Note - 2026-04-28 - HIR CFG Contract Validation

- HIR routine finishing now validates CFG shape immediately after body lowering
  and validates predecessor mirrors immediately after predecessor
  materialization. `hir_validate_cfg_shape()` rejects open fallthrough blocks,
  invalid successor indices, inconsistent terminator successor flags, missing
  branch conditions, and block-id drift before dominance/frontier/loop/phi
  analysis can consume the graph.
- `hir_validate_cfg_predecessors()` verifies that every successor has the
  matching predecessor edge and every predecessor points back to the block it
  names. This turns CFG structural consistency into a compiler-owned gate
  rather than an assumption inside dominance/MIR lowering.
- `tests/cfg_body_dataflow_smoke.sh` now gates the validation seam. This does
  not close the larger blocker that semantic body safety must fully consume
  CFG/dataflow facts; it closes the underlying HIR CFG invariant layer those
  future consumers rely on.
- HIR CFG summaries now also materialize `return_block_count` and
  `normal_exit_block_count`. A reachable `HIR_BLOCK_UNREACHABLE` is the
  lowered normal fallthrough exit, so this gives later semantic/MIR consumers a
  direct CFG fact for "may fall through" instead of rediscovering it from AST
  traversal.

## UTF-8 Progress Note - 2026-04-28 - MIR Active Inventory API Seam

- `MIRProgram` now exposes `mir_active_inventory()` and
  `mir_active_externs()` as the shared declaration inventory read seam.
  C backend `transpiler_active_inventory()` and LLVM backend
  `llvm_active_inventory()` no longer duplicate their own
  `ASTNodeType -> mir->...` declaration-array switch.
- `tests/mir_declaration_inventory_smoke.sh` now gates that both C and LLVM
  active inventory helpers consume the MIR public API seam.
- This does not claim dedicated declaration IR closure yet: `MIRProgram` still
  carries AST declaration arrays. The closed slice is the duplicated backend
  mapping seam, so the future dedicated decl-IR replacement has one compiler
  API boundary to replace instead of two backend-local switches.

## UTF-8 Progress Note - 2026-04-28 - C Intent Emitter Owner Split

- `src/codegen/transpiler_intent_emit.h` is no longer a single 965 LOC
  declaration emitter. Intent signature/runtime-entry emission moved to
  `src/codegen/transpiler_intent_prologue_emit.h`, and cleanup/rollback/
  invalidation tail emission moved to
  `src/codegen/transpiler_intent_cleanup_emit.h`.
- Current C intent owner sizes are `transpiler_intent_emit.h` 577 LOC,
  `transpiler_intent_prologue_emit.h` 186 LOC, and
  `transpiler_intent_cleanup_emit.h` 278 LOC, all below the 600 LOC
  split-review threshold.
- Local gate: `make pgy` and `make llvm-test-backend-compare` are green with
  ABI same-process `196 passed, 0 failed` and backend compare `64/64 passed,
  0 failed`.
- This closes the C intent emitter owner-size slice. Remaining codegen review
  band debt is now concentrated in LLVM/domain/projection owners and
  declaration/top-level inventory bootstrap, not this C intent emitter.

## UTF-8 Progress Note - 2026-04-28 - C MIR CFG Consumer Parity Closure

- C backend MIR emission no longer lets CFG-expanded range `for` or
  `match case` nodes fall through to the generic expression emitter.
  `src/codegen/transpiler_mir_cfg_control_emit.h` now owns C-side MIR branch
  condition rendering for range-loop headers and `Option`/`Result` match
  cases, plus range-loop init/backedge maintenance.
- Explicit CFG containers are now skipped as opaque AST statements on the C
  MIR block path. Range `for` emits only its MIR-owned init in the predecessor
  block, branch conditions are emitted by MIR terminators, and loop backedges
  perform the increment before jumping back to the header.
- Pin-view SSA copies are constrained at CFG edges: `view` bindings from
  `pin ... as view` are region-local resources and are no longer phi-copied
  into break/exit successors. This closes the previous
  `pin_break_cleanup_block` compile failure.
- MIR phi-copy emission now has a named owner in
  `src/codegen/transpiler_mir_phi_emit.h`; `transpiler_mir_ssa_emit.h` is back
  below the 600 LOC split-review threshold.
- MIR terminator emission now has a named owner in
  `src/codegen/transpiler_mir_terminator_emit.h`, and residual MIR statement
  helpers now live in `src/codegen/transpiler_mir_stmt_emit.h`. This brings
  `transpiler_mir_func_emit.h`, `transpiler_mir_block_emit.h`, and
  `transpiler_mir_ssa_emit.h` all below the 600 LOC split-review threshold.
- Local gate: `make llvm-test-backend-compare` is green again with
  ABI same-process `196 passed, 0 failed` and backend compare `64/64 passed,
  0 failed`.
- This closes the C MIR CFG/body emitter owner-size slice for the current
  private-header threshold. Remaining backend debt is now higher-level:
  declaration/top-level inventory bootstrap and broader CFG/AIR semantic
  consumption, not these C MIR emission owners.

## UTF-8 Progress Note - 2026-04-28 - LLVM MIR CFG Control / Declaration Metadata Closure

- LLVM MIR block emission no longer treats CFG-expanded `for`, `select`, and
  `match` containers as opaque AST statement payloads. Range-for init/header/
  backedge increment, select channel-readiness dispatch, receive bind target
  declaration, and match-case condition lowering now live on the MIR CFG
  control path.
- `src/codegen/llvm_mir_block_emit.h` was reduced to 430 LOC by moving the
  CFG-control lowering owner into the real translation unit
  `src/codegen/llvm_mir_cfg_control.c` (363 LOC). No production `.inc` file was
  reintroduced.
- `src/codegen/llvm_internal.h` is no longer the LLVM backend mega-header for
  every private prototype. Private API declarations moved to
  `src/codegen/llvm_internal_api.h`, and fixed limits / dynamic-array helpers
  moved to `src/codegen/llvm_limits_internal.h`. Current sizes are
  `llvm_internal.h` 574 LOC, `llvm_internal_api.h` 325 LOC, and
  `llvm_limits_internal.h` 54 LOC.
- `src/codegen/llvm_registry.c` no longer mixes resource/type registries with
  scope/function/class/callable/enum registry ownership. Slot/view/future/
  channel/Rc/Weak/container variable tracking and Slot/SecureSlot/PinnedSlot/
  array/slice/list/set/queue/hashmap type helper materialization moved to
  `src/codegen/llvm_registry_resources.c`. Current sizes are
  `llvm_registry.c` 532 LOC and `llvm_registry_resources.c` 415 LOC.
- LLVM method signature accessors are now MIR-declaration-metadata only. The
  old AST-method fallback path in `llvm_mir_decl_method_*` accessors is gone;
  missing enum/class method metadata is a hard MIR-only LLVM path error.
- Local gates: `make pgy`, `make llvm-test-smoke`,
  `make mir-declaration-inventory-test-smoke`, and
  `make type-resolution-resolver-inventory-test-smoke`.
- Remaining LLVM/MIR blocker is not this MIR CFG statement slice. The blocker
  is declaration/top-level inventory bootstrap debt: some enum/class method
  iteration still starts from AST-carried inventory before consuming MIR
  declaration metadata. That must move to a dedicated declaration IR/inventory
  owner before beta can claim backend source-of-truth closure.
- Current owner-size caveat: production `.inc` debt under `src/` is closed, but
  several production `.c/.h` owners remain in the 600-1,000 LOC review band
  (`llvm_intent.c`, `transpiler_overlay_projection.h`, `llvm_domain.c`,
  `pgy_lsp.c`, and related backend/tooling owners). These are review-band
  debts, not `.inc` debts.

## UTF-8 Progress Note - 2026-04-28 - World Semantic Owner Split

- `src/semantic/type_checker_world_decl.c` no longer owns world/zone lookup
  helpers or shared domain slot validation. World-zone/state lookup,
  world-state target resolution, zone layer/state lookup, and world lifecycle
  target resolution moved to `src/semantic/type_checker_world_helpers.c`
  behind `src/semantic/type_checker_world_internal.h`.
- Shared relation/effect/zone domain slot type validation and initializer
  checking moved to `src/semantic/type_checker_domain_slots.c`, so the world
  declaration pass now stays focused on world symbol declaration, roster/zone
  visibility, world state composition, lifecycle direction checks, shared
  fields, and hosted methods.
- Current sizes are `type_checker_world_decl.c` 588 LOC,
  `type_checker_world_helpers.c` 186 LOC, `type_checker_domain_slots.c` 115
  LOC, and `type_checker_world_internal.h` 28 LOC. The world semantic owner
  family is now below the 600 LOC split-review threshold.
- Local gates: `make pgy`, `make test-semantic` (2359/0),
  `make type-resolution-dag-test-smoke`, and
  `make type-resolution-resolver-inventory-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - HIR CFG Dispatch/Loop Edge Closure

- `src/compiler/hir_lower_cfg.c` now lowers `break` and `continue` as explicit
  CFG terminators when they appear inside `while` / `for` bodies. A loop
  context carries the loop-exit target for `break` and the loop-header target
  for `continue`, so HIR dominance/frontier/loop-depth facts no longer treat
  loop control as opaque statement payload.
- Labeled loop control is now target-correct in HIR CFG. Nested
  `break outer` / `continue outer` resolve through the loop-context parent chain
  to the named loop's exit/header instead of the nearest loop.
- `match` and `select` no longer remain opaque in HIR CFG. Both lower through
  an explicit dispatch/join helper where each case is a branch condition,
  fallthrough case/default bodies join through CFG edges, and terminating cases
  remain closed. This keeps channel readiness cases visible to later HIR/MIR
  consumers instead of hiding them as a single AST payload.
- `unsafe` blocks no longer hide nested body control flow from HIR CFG. Nested
  `return` / branch terminators inside `unsafe` now flow through the same
  terminator and reachability model as ordinary blocks.
- `src/test_hir.c` adds `HIR CFG lowers loop break and continue edges
  explicitly`, `HIR CFG lowers match cases and default as explicit edges`,
  `HIR CFG lowers select cases and default as explicit edges`,
  `HIR CFG lowers unsafe block body control flow`, and
  `HIR CFG resolves labeled loop control to the named loop` to lock this
  behavior.
- Local gates: `make test-hir` (14/0), `make cfg-body-dataflow-test-smoke`,
  `make test-rir`, and `make test-mir`.
- HIR owner split continues without adding `.inc`: `hir_destroy()` and the
  synthetic executable teardown path now live in `src/compiler/hir_destroy.c`,
  while declaration/routine construction and hidden method routine extraction
  live in `src/compiler/hir_routines.c` behind `src/compiler/hir_internal.h`.
  Current HIR owner sizes are `hir.c` 421 LOC, `hir_routines.c` 419 LOC,
  `hir_lower_cfg.c` 598 LOC, and `hir_cfg.c` 599 LOC, so the active HIR owner
  set is under the 600 LOC split-review threshold.
- Compiler driver owner debt also moved below the split-review threshold
  without adding `.inc`: result/diagnostic object ownership now lives in
  `src/compiler/compiler_result.c`, LLVM emission/native-link orchestration and
  disabled-LLVM stubs live in `src/compiler/compiler_llvm.c`, and runtime object
  cache freshness/path construction lives in `src/compiler/compiler_runtime_cache.c`.
  Current owner sizes are `compiler.c` 289 LOC, `compiler_llvm.c` 350 LOC,
  `compiler_result.c` 79 LOC, `compiler_toolchain.c` 543 LOC, and
  `compiler_runtime_cache.c` 138 LOC.
- Driver pipeline owner debt is also below the split-review threshold:
  `src/compiler/driver_app.c` now owns pipeline orchestration only, while JSON
  diagnostic routing, AIR drift diagnostics, Reason/Fix emission, and code/
  cause/fix mapping live in `src/compiler/driver_diag.c`. Current sizes are
  `driver_app.c` 457 LOC, `driver_diag.c` 358 LOC, and `driver_diag.h` 16 LOC.
- Module normalizer owner debt is closed under the same no-`.inc` rule:
  `src/compiler/module_normalizer.c` now owns module-level orchestration,
  namespace shells, explicit export scanning, and statement-list traversal,
  while rename-scope state, shadowed-name tracking, type/generic/call/reference
  rewriting, and AST node reference normalization live in
  `src/compiler/module_normalizer_refs.c` behind
  `src/compiler/module_normalizer_internal.h`. Current sizes are
  `module_normalizer.c` 261 LOC, `module_normalizer_refs.c` 565 LOC, and
  `module_normalizer_internal.h` 39 LOC.
- Scaffold owner debt is also split below the 600 LOC review threshold:
  `src/compiler/driver_scaffold.c` now owns filesystem helpers, single-file
  scaffold templates, and command dispatch, while simulator/project directory
  templates live in `src/compiler/driver_scaffold_project.c` behind
  `src/compiler/driver_scaffold_internal.h`. Current sizes are
  `driver_scaffold.c` 473 LOC, `driver_scaffold_project.c` 351 LOC, and
  `driver_scaffold_internal.h` 16 LOC.
- RIR builder include debt has been converted into real translation units:
  the old implementation-style `rir_builder.h` is now
  `src/compiler/rir_builder.c`, intent-scope collection lives in
  `src/compiler/rir_builder_intent.c`, RIR fact/utility materialization lives
  in `src/compiler/rir_facts.c`, RIR vocabulary names live in
  `src/compiler/rir_names.c`, RIR dump/destroy public-surface ownership lives
  in `src/compiler/rir_public_surface.c`, RIR validation / DIR contract
  checking lives in `src/compiler/rir_validation.c`, HIR-backed RIR flow
  enrichment lives in `src/compiler/rir_flow.c`, and shared helper seams are
  declared in `src/compiler/rir_internal.h`. Current sizes are
  `rir_builder.c` 552 LOC, `rir_validation.c` 525 LOC, `rir_facts.c` 487 LOC,
  `rir_flow.c` 457 LOC, `rir.c` 429 LOC, `rir_public_surface.c` 287 LOC,
  `rir_builder_intent.c` 135 LOC, `rir_names.c` 110 LOC, and
  `rir_internal.h` 64 LOC. The RIR owner family is now under the 600 LOC
  split-review threshold without reintroducing an implementation-style
  include file.
- Local gates for the owner split: `make pgy`, `make LLVM_ENABLED=0 ... pgy`,
  `make test-hir`, `make test-rir`, `make test-air`, `make test-mir`,
  `make air-drift-test-smoke`,
  `make test-semantic`, `make type-resolution-dag-test-smoke`,
  `make semantic-fixture-isolation-test-smoke`,
  direct `pgy scaffold subject|simulator|project` smoke,
  `make backend-inc-size-test-smoke`, `make documentation-quality-test-smoke`,
  and `make beta-readiness-checklist-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - DAG Fallback Recheck

- Rechecked the type-resolution DAG gates. Current local stats are
  `graph-backed skips=3140 retired_resolver_calls=0 retired_resolver_unique_nodes=0
  metadata_entries=3436 metadata_owned=257 metadata_hits=6756
  materializer_fallbacks=0`.
- Metadata unresolved audit families are all zero:
  `metadata_unresolved_named=0 metadata_unresolved_generic_named=0
  metadata_unresolved_compound=0 metadata_unresolved_other=0
  metadata_named_builtin_shell=0 metadata_named_generic_class=0
  metadata_named_alias=0 metadata_named_non_class_symbol=0
  metadata_named_missing_symbol=0`.
- Stage metadata materialization is now zero for both alias and non-alias replay:
  `stage_materialize_alias=0 stage_materialize_non_alias=0 alias_materialized=6
  alias_diagnostic_unresolved=78 alias_diagnostic_resolver_calls=0`.
- `type-resolution-resolver-inventory-test-smoke` now gates 12
  `semantic_type_resolution_lookup_resolved_annotation(...)` seams outside the
  metadata owners. These are remaining DAG source-of-truth seams for
  annotation-sensitive readers, not recursive resolver fallback.
- DAG stage replay is now split by owner family:
  `type_checker_resolution_stage_nominal.c` owns class/enum/ability/role,
  `type_checker_resolution_stage_systemic.c` owns party/roster/world/intent,
  and `type_checker_resolution_stage_domain_decl.c` owns relation/effect/zone.
  The top-level stage owner is now 88 LOC and split owners are 239 LOC or
  smaller, all under the 600 LOC split-review threshold.
- DAG metadata alias-chain and cycle materialization is now split into
  `type_checker_resolution_metadata_alias.c`. The central metadata owner is
  268 LOC, the alias owner is 315 LOC, and alias/cycle provenance no longer
  shares the central lookup/materializer orchestration body.
- The old recursive alias resolver and `SemanticContext.alias_resolution_*`
  stack are removed. `resolve_named_type(...)` now routes alias names through
  `semantic_type_resolution_lookup_metadata_name_or_alias(...)`, so alias
  chain/cycle semantics have a single metadata owner.
- `resolve_named_type(...)` is now metadata-first for builtin, scope, generic
  parameter, nominal, and alias names. If DAG metadata cannot answer, it falls
  back to the existing user-facing diagnostic path; successful stable names no
  longer bypass metadata before checking local scope.
- Valid alias stage materialization now uses the metadata-only lookup before
  reporting diagnostic unresolved inventory. Valid aliases no longer leak through the
  recursive resolver compatibility body, and
  `tests/type_resolution_dag_smoke.sh` now gates alias compatibility fallback at zero.
- The central metadata materializer no longer falls through to
  `resolve_type_node(type_node, ctx)`. Unsupported metadata shapes are recorded
  as explicit materializer fallback and return unresolved; the DAG smoke keeps
  that fallback count at zero, while resolver-inventory smoke rejects any
  recursive escape-hatch reintroduction.
- Added semantic regression `graph-backed forward alias materializes nested
  constructed type`, covering a function signature that consumes a later alias
  to `Channel<Slot<Int>>`. This pins the source-order pain point where nested
  constructed aliases could regress into recursive fallback or unknown-type
  behavior.
- Pain point found while checking: semantic cross-module cases still use fixed
  temporary import filenames. `test_semantic` now enters an isolated repo-local
  `.tmp/pgy-semantic-test.*` cwd before running cases, and
  `tests/type_resolution_dag_smoke.sh` runs the semantic binary from its own
  `.tmp/pgy-type-resolution-dag.*` cwd. This isolates fixture files for direct
  parallel semantic binary runs and for `test-semantic` +
  `type-resolution-dag-test-smoke`. Parallel `make test-semantic` targets are
  still not supported because they can relink the same binary while another
  process executes it. `semantic-fixture-isolation-test-smoke` is wired into
  runnable Linux/macOS/Windows CI paths.
- `tests/type_resolution_resolver_inventory_smoke.sh` now also checks that the
  materializer fallback recorder stays in its central owner and recursive
  metadata escape hatches stay at zero. New owner-local fallback users remain
  rejected.
- Remaining DAG blocker is no longer metadata materializer fallback volume.
  The blocker is retiring the recursive resolver implementation as an
  evaluator source and making the graph/topo materializer the only semantic
  evaluation path for stable type refs.
- Local gates: `make type-resolution-dag-test-smoke`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make semantic-fixture-isolation-test-smoke`, and concurrent
  `make test-semantic` + `make type-resolution-dag-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - AST Destroy Owner Split

- `src/parser/ast.c` no longer owns AST destruction. Mutation helpers remain
  in `ast.c`; generic/where/comment destruction plus non-domain destroy cases
  moved to `src/parser/ast_destroy.c`; domain/world/zone/intent/party/ability/
  event destroy cases moved to `src/parser/ast_destroy_domain.c`.
- Current owner sizes by `wc -l`: `ast.c` 65, `ast_destroy.c` 393,
  `ast_destroy_domain.c` 456, and `ast_destroy_internal.h` 11. The AST runtime
  owner family is now below the 600 LOC split-review threshold.
- Local gate: `make test-parser`.

## UTF-8 Progress Note - 2026-04-28 - Parser Declaration/Type Owner Split

- `src/parser/parser_decl.c` no longer owns generic parameter parsing, type
  argument parsing, where-clause parsing, type alias parsing, name-token
  helpers, or function/action clause parsing.
- Type/name/generic parsing moved to `src/parser/parser_type.c`, and
  function/action clause parsing moved to
  `src/parser/parser_decl_function_clause.c`.
- Current owner sizes by `wc -l`: `parser_decl.c` 327,
  `parser_type.c` 351, and `parser_decl_function_clause.c` 230. The declaration
  parser family is now below the 600 LOC split-review threshold.
- Local gate: `make test-parser`.

## UTF-8 Progress Note - 2026-04-28 - Parser Statement Dispatch Owner Split

- `src/parser/parser.c` no longer owns top-level statement dispatch. Parser
  lifecycle, token movement, error handling, program parsing, and block/let/
  with/parallel leaf parsers remain in `parser.c`; declaration/statement
  dispatch moved to `src/parser/parser_statement_dispatch.c`.
- Current owner sizes by `wc -l`: `parser.c` 414 and
  `parser_statement_dispatch.c` 460. The core parser owner family is now below
  the 600 LOC split-review threshold except for `ast.h`, which remains the
  AST shape header.
- Local gate: `make test-parser`.

## UTF-8 Progress Note - 2026-04-28 - Zone Declaration Owner Split

- `src/semantic/type_checker_zone_decl.c` no longer owns zone shape warnings,
  projection refresh/publish/bind rule validation, or state alias validation.
- Zone shape warnings moved to `src/semantic/type_checker_zone_shape.c`,
  projection rules moved to `src/semantic/type_checker_zone_projection_rules.c`,
  and maintained-state / state-alias validation moved to
  `src/semantic/type_checker_zone_state.c`.
- Current owner sizes by `wc -l`: `type_checker_zone_decl.c` 558,
  `type_checker_zone_projection_rules.c` 91,
  `type_checker_zone_state.c` 263, and `type_checker_zone_shape.c` 42.
  The zone declaration semantic owner family is now below the 600 LOC
  split-review threshold.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Function Call Constructor Owner Split

- `src/semantic/type_checker_helpers_late.c` no longer owns constructor-like
  symbol call validation for subject/class, relation/effect/roster/world/zone
  overlay constructors, and world-zone embedding handoff diagnostics.
- Those checks moved to `src/semantic/type_checker_call_constructor.c`, while
  the late helper owner stays focused on callable symbol dispatch, argument
  ownership validation, generic call-site where-clause validation, and return
  type materialization.
- Current owner sizes by `wc -l`: `type_checker_helpers_late.c` 799 and
  `type_checker_call_constructor.c` 220. The late helper owner is still in the
  600-1,000 LOC review band but is no longer mixing constructor diagnostics
  with function-call argument flow.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Function Call Late Helper Owner Split

- `src/semantic/type_checker_helpers_late.c` no longer owns active slot-view
  discovery/owner-escape checks, callee parameter contract lookup, callable
  parameter escape-summary lookup, or call-site generic where-clause
  validation.
- These moved to named owners:
  `type_checker_slot_view_active.c`,
  `type_checker_call_contract_helpers.c`, and
  `type_checker_call_generic_where.c`. The late helper owner now stays focused
  on callable symbol dispatch, argument type/ownership flow, and return type
  materialization.
- Current owner sizes by `wc -l`: `type_checker_helpers_late.c` 488,
  `type_checker_slot_view_active.c` 146,
  `type_checker_call_contract_helpers.c` 60, and
  `type_checker_call_generic_where.c` 160. The function-call late-helper owner
  family is now below the 600 LOC split-review threshold.
- Local gates: `make pgy`, `make test-semantic`,
  `make type-resolution-dag-test-smoke`, and
  `make type-resolution-resolver-inventory-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - LLVM Statement Owner Split

- `src/codegen/llvm_stmt.c` no longer owns zone-action effect runtime
  propagation helpers or generic type-argument rendering helpers.
- Zone-action effect propagation moved to `llvm_stmt_zone_action.c`; generic
  type-argument rendering moved to `llvm_stmt_type_render.c`. The statement
  owner now stays focused on statement dispatch, defers, return/if/block
  emission, and expression-statement forwarding.
- Current owner sizes by `wc -l`: `llvm_stmt.c` 573,
  `llvm_stmt_zone_action.c` 275, and `llvm_stmt_type_render.c` 74. The LLVM
  statement owner family is now below the 600 LOC split-review threshold.
- Local gates: `make pgy` and `make llvm-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - LLVM Let Statement Owner Split

- `src/codegen/llvm_stmt_let_with.c` no longer owns collection/channel/array
  let specializations or callable/lambda registration post-processing.
- Collection-like let lowering moved to `llvm_stmt_let_collections.c`, and
  callable let registration moved to `llvm_stmt_let_callable.c`. The let owner
  now stays focused on Slot/View/MoveToken sugar, class-constructor lets,
  initializer storage/coercion, and typed variable/future registration.
- Current owner sizes by `wc -l`: `llvm_stmt_let_with.c` 562,
  `llvm_stmt_let_collections.c` 258, and `llvm_stmt_let_callable.c` 79. The
  LLVM let statement owner family is now below the 600 LOC split-review
  threshold.
- Local gates: `make pgy` and `make llvm-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - LLVM MIR CFG Match Destructor Fix

- `src/codegen/llvm_mir_cfg_control.c` now handles `Option` and `Result`
  destructor patterns (`Some/None`, `Ok/Err`) when a source `match` has been
  expanded into MIR CFG case branches. The previous MIR CFG path compared the
  whole aggregate value with `icmp`, which made `projection_abi` fail LLVM
  verification for `match Option<Int>`.
- The same MIR CFG path now materializes the case payload binding
  (`Some(v)`, `Ok(v)`, `Err(e)`) before the case branch body consumes it. This
  restores the `projection_abi` expected output (`49`) instead of defaulting
  the payload to `0`.
- Local gates: `make pgy`, direct `projection_abi` LLVM compile/run probe, and
  the ABI same-process precheck inside `make llvm-test-backend-compare`
  (`196 passed, 0 failed`).
- Follow-up C backend parity debt from this note is now closed by the
  2026-04-28 C MIR CFG consumer parity slice: full
  `make llvm-test-backend-compare` is green with backend compare `64/64`.

## UTF-8 Progress Note - 2026-04-28 - Runtime Slot Pin Owner Split

- `src/runtime/slot_manager.c` no longer owns `PergyraSlotPin` /
  `PergyraSlotUnpin`. Pinned view validation, secure payload open/seal,
  stale-generation rejection, release-while-pinned rejection, and Pin token
  validation moved to `src/runtime/slot_manager_pin.c`.
- Current owner sizes by `wc -l`: `slot_manager.c` 564,
  `slot_manager_query_lock.c` 240, and `slot_manager_pin.c` 185. Query,
  TTL cleanup, locking, stats, and fast wrappers moved to
  `slot_manager_query_lock.c`, so the slot lifecycle owner is now below the
  600 LOC split-review threshold.
- Local gates: `make test-security` (142/0) and `make test-abi` (58/0 plus
  C/LLVM ABI pipeline smoke).

## UTF-8 Progress Note - 2026-04-28 - Type System Inference/Effect Owner Split

- `src/semantic/type_system.c` no longer owns lightweight expression inference
  or function/resource effect mask helpers. `type_infer_expression` /
  `type_unify` moved to `src/semantic/type_infer.c`, and function effect/body
  summary plus effect lattice helpers moved to `src/semantic/type_effects.c`.
- Current owner sizes by `wc -l`: `type_system.c` 598, `type_infer.c` 254,
  and `type_effects.c` 106. The type core owner is now below the 600 LOC
  split-review threshold.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Intent Transfer Contract Owner Split

- `src/semantic/type_checker_intent_decl.c` no longer owns the full
  transfer/handoff diagnostic block inside the main intent declaration pass.
  Transfer source/target alias validation, zone-binding checks, transfer target
  versus current zone contract checks, and `using` versus transfer-target
  consistency diagnostics moved to `src/semantic/type_checker_intent_transfer.c`.
- Current owner sizes by `wc -l`: `type_checker_intent_decl.c` 797 and
  `type_checker_intent_transfer.c` 207. The main intent declaration owner is
  still in the 600-1,000 LOC review band, but the handoff contract check now
  has a named owner seam.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Intent Action Contract Helper Split

- `src/semantic/type_checker_intent_helpers.c` no longer owns action-contract
  inheritance, redundant contract diagnostics, or contract-source summary
  formatting. The helper owner is now limited to intent condition/involves and
  projection-adjacent utility routines.
- Action-contract inheritance and redundant-step warnings moved to
  `src/semantic/type_checker_intent_action_contract.c`; contract-source summary
  formatting moved to `src/semantic/type_checker_intent_contract_summary.c`.
- Current owner sizes by `wc -l`: `type_checker_intent_helpers.c` 145,
  `type_checker_intent_action_contract.c` 481, and
  `type_checker_intent_contract_summary.c` 341. This closes the old
  `type_checker_intent_helpers.c` 883 LOC semantic owner-size debt under the
  600 LOC split-review threshold.
- Local gates: `make test-semantic` (2359/0) and
  `make type-resolution-dag-test-smoke` (graph-backed skips=3140,
  retired_resolver_calls=0, metadata_entries=3436, metadata_hits=6756,
  materializer_fallbacks=0).

## UTF-8 Progress Note - 2026-04-28 - Ownership Constructor Diagnostic Split

- `src/semantic/type_checker_ownership_diag.c` no longer owns constructor-field
  escape diagnostic formatting. That path moved to
  `src/semantic/type_checker_ownership_diag_constructor.c`.
- Current owner sizes by `wc -l`: `type_checker_ownership_diag.c` 550 and
  `type_checker_ownership_diag_constructor.c` 70. This closes the previous
  611 LOC ownership diagnostic owner over the 600 LOC split-review threshold.
- Local gate: `make test-semantic` (2359/0).

## UTF-8 Progress Note - 2026-04-28 - Semantic Domain Contract Owner Split

- `src/semantic/type_checker_decls_domain_helpers.c` no longer owns zone
  relation/effect contract validation. Contract arity checks, endpoint-kind
  matching, and provenance-heavy zone relation/effect diagnostics moved to
  `src/semantic/type_checker_domain_contracts.c`.
- Current owner sizes by `wc -l`: `type_checker_decls_domain_helpers.c` 448
  and `type_checker_domain_contracts.c` 537. Both are below the 600 LOC
  split-review threshold.
- Local gate: `make test-semantic` (2357/0).
- Result: the domain helper family is no longer a semantic owner-size blocker.
  The next semantic split candidates are `type_checker_helpers_late.c`,
  `type_checker_intent_decl.c`, `type_system.c`, and
  `type_checker_zone_decl.c`.

## UTF-8 Progress Note - 2026-04-28 - AST Public API Header Split

- `src/parser/ast.h` no longer owns the public AST constructor/manipulation
  prototype surface. Those declarations moved to `src/parser/ast_api.h`, which
  is included by `ast.h` for source compatibility.
- `ast.h` is now 848 LOC and stays focused on the `ASTNode` shape after the
  earlier `ast_types.h` vocabulary split. `ast_api.h` is 137 LOC.
- Local gates: `make test-parser pgy`,
  `make production-header-size-test-smoke inc-sentinel-test-smoke`, and
  touched-file `git diff --check`.
- Result: `ast.h` remains in the 600-1,000 LOC review band but is no longer
  near the hard cap. The next parser owner candidates are `ast.c` 894,
  `parser_decl.c` 887, and `parser.c` 867.

## UTF-8 Progress Note - 2026-04-28 - AST Print Family Owner Split

- AST print ownership is now split below the 600 LOC review threshold across
  the full printer family. Intent printers and intent contract provenance moved
  to `src/parser/ast_print_intent.c`; event printers moved to
  `src/parser/ast_print_event.c`; domain/world/zone printers remain in
  `src/parser/ast_print_domain.c`.
- Current AST print owner sizes by `wc -l`: `ast_print.c` 553,
  `ast_print_domain.c` 539, `ast_print_inline.c` 382,
  `ast_print_intent.c` 253, `ast_print_event.c` 76,
  `ast_print_generics.c` 63, and `ast_print_misc.c` 11.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Result: the AST print family is no longer in the 600-1,000 LOC
  split-review band. The next parser owner queue stays on `parser.c`,
  `parser_domain.c`, `parser_decl.c`, `ast.h`, and `ast.c`.

## UTF-8 Progress Note - 2026-04-28 - Parser Declaration Hint Owner Split

- `src/parser/parser.c` no longer owns top-level declaration hint inventory.
  Declaration hint name extraction, registration, capacity growth, and lookup
  moved to `src/parser/parser_decl_hints.c`.
- `parser.c` is now 867 LOC and remains focused on parser lifecycle,
  token movement, diagnostics, synchronization, statement finalization, and
  program/statement dispatch. The new `parser_decl_hints.c` owner is 111 LOC.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Remaining parser 600-1,000 LOC queue after this slice was
  `parser_domain.c`, `parser_decl.c`, `parser.c`, `ast.h`, and `ast.c`;
  `parser_domain.c` is closed by the newer relation/projection split note
  below.

## UTF-8 Progress Note - 2026-04-28 - Parser Domain Relation/Projection Owner Split

- `src/parser/parser_domain.c` no longer owns relation/effect declaration
  parsing or projection-sync helper parsing. Relation/effect declarations
  moved to `src/parser/parser_domain_relation_effect.c`; projection group
  parsing, domain group keyword matching, and projection field maps moved to
  `src/parser/parser_domain_projection.c`.
- Current domain parser owner sizes by `wc -l`: `parser_domain.c` 493,
  `parser_domain_relation_effect.c` 283, `parser_domain_projection.c` 184,
  `parser_domain_world.c` 384, `parser_domain_zone.c` 449,
  `parser_domain_roster.c` 165, and `parser_domain_event.c` 57.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Result: the parser domain family is below the 600 LOC split-review
  threshold. Remaining parser queue is now `ast.h` 973, `ast.c` 894,
  `parser_decl.c` 887, and `parser.c` 867.

## UTF-8 Progress Note - 2026-04-28 - AST Print Inline/Generic Owner Split

- `src/parser/ast_print.c` no longer owns inline expression printing,
  compact one-line printing, operator spelling, escaped string rendering, or
  generic/where-clause inline rendering. Those moved to
  `src/parser/ast_print_inline.c` and `src/parser/ast_print_generics.c`.
- Current AST print owner sizes by `wc -l`: `ast_print.c` 553,
  `ast_print_inline.c` 382, `ast_print_generics.c` 63. The central AST print
  owner is now below the 600 LOC split-review threshold.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Follow-up AST print queue item was `ast_print_domain.c`; it is now split in
  the newer progress note above.

## UTF-8 Progress Note - 2026-04-28 - AST Owner Split

- The last 1,000+ LOC production `.c` parser owners are split. AST printing now
  has domain and misc owners (`src/parser/ast_print_domain.c`,
  `src/parser/ast_print_misc.c`), and AST construction now has core,
  domain-constructor, and clone owners (`src/parser/ast_constructors.c`,
  `src/parser/ast_domain_constructors.c`, `src/parser/ast_clone.c`).
- Current parser owner sizes by `wc -l`: `ast.c` 894, `ast_print.c` 553,
  `ast_print_domain.c` 539, `ast_print_inline.c` 382,
  `ast_print_intent.c` 253, `ast_print_event.c` 76,
  `ast_print_generics.c` 63, `ast_constructors.c` 545,
  `ast_domain_constructors.c` 598, `ast_clone.c` 109, `ast.h` 973, and
  `ast_types.h` 272. No production `.c` or `.h` owner remains above the
  1,000 LOC hard risk line.
- Local gates: `make test-parser pgy`, `make test-semantic` (2357/0),
  `make semantic-tu-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke documentation-quality-test-smoke
  beta-readiness-checklist-test-smoke`, and
  `make runtime-frontier-contract-test-smoke`.
- Next owner-split queue is the 600-1,000 LOC band, not 1,000+ production `.c`
  cleanup: `parser.c`, `parser_domain.c`, selected semantic owners, LLVM
  domain/frontier owners, `slot_manager.c`, and the AST public header split.

## UTF-8 Progress Note - 2026-04-28 - LLVM Backend Type Map Owner Split

- `src/codegen/llvm_backend.c` no longer mixes context lifecycle/type-layout
  bootstrap with AST/Pergyra type-name rendering and LLVM type resolution.
- Type rendering, generic container resolution, `pergyra_type_to_llvm`,
  `ast_type_to_llvm`, and early forward-declare eligibility moved to
  `src/codegen/llvm_backend_type_map.c`.
- `llvm_backend.c` is now 379 LOC and is a context lifecycle / backend entry
  owner. `llvm_backend_type_map.c` is 638 LOC, so it is below the 1,000 LOC
  hard cap but remains in the 600 LOC split-review band.
- Local gates: `make pgy` and `make llvm-test-smoke` remain green. This
  removes `llvm_backend.c` from the leading owner queue and keeps the next LLVM
  priority on domain frontier/parity owners.

## UTF-8 Progress Note - 2026-04-28 - LLVM Zone Frontier State Owner Split

- `src/codegen/llvm_domain_zone_sync.c` no longer owns all bounded-frontier
  bookkeeping inside the main zone sync emitter.
- Previous-state allocation, previous-state snapshotting, state/layer reset,
  and frontier-continue change detection moved to
  `src/codegen/llvm_domain_zone_frontier_state.c`, with declarations in
  `src/codegen/llvm_domain_zone_sync_internal.h`.
- `llvm_domain_zone_sync.c` is now 776 LOC and remains the zone propagation
  action/maintain/link/unlink orchestration owner. The new frontier-state owner
  is 276 LOC.
- Local gates: `make pgy`, `make runtime-frontier-contract-test-smoke`, and
  `make llvm-test-smoke` remain green. This keeps runtime propagation frontier
  evidence tied to a named LLVM owner instead of one monolithic sync function.

## UTF-8 Progress Note - 2026-04-28 - Stdlib Builtin Semantic Owner Split

- `src/semantic/type_checker_builtins_stdlib_body.c` is no longer a 1,000+ LOC
  owner. Scalar/string/math builtin checks moved to
  `src/semantic/type_checker_builtins_stdlib_scalar.c`, and `HashMap` builtin
  checks moved to `src/semantic/type_checker_builtins_stdlib_map.c`.
- The dispatcher body is now 834 LOC and stays below the hard cap while the
  new owners stay small (`scalar` 182 LOC, `map` 147 LOC).
- Local gate: `make test-semantic pgy` remains green at 2357/0. This removed
  the stdlib builtin dispatcher from the 1,000+ production `.c` owner queue.

## UTF-8 Progress Note - 2026-04-28 - Zone Declaration Authority Owner Split

- `src/semantic/type_checker_zone_decl.c` is no longer a 1,000+ LOC owner.
  Zone authority ability validation, duplicate authority diagnostics,
  layer-slot type validation, and relation/effect pool beta rejects moved to
  `src/semantic/type_checker_zone_decl_authority.c`.
- `type_checker_zone_decl.c` is now 929 LOC and stays focused on
  lifecycle/state rule validation; the new authority/layer owner is 180 LOC.
- Local gate: `make test-semantic pgy` remains green at 2357/0. This removed
  the zone declaration validator from the 1,000+ production `.c` owner queue.

## UTF-8 Progress Note - 2026-04-28 - Intent Helper Owner Split

- `src/semantic/type_checker_intent_helpers.c` is no longer a 1,000+ LOC owner.
  Role require-field validation plus intent transfer/zone-binding derivation
  moved to `src/semantic/type_checker_intent_role_fields.c`, and intent-clause
  control-transfer rejection moved to
  `src/semantic/type_checker_intent_control.c`.
- `type_checker_intent_helpers.c` is now 883 LOC. The new role/transfer owner
  is 544 LOC and the control-transfer owner is 136 LOC, so both remain below
  the 600 LOC split-review threshold.
- Local gates: `make test-semantic pgy` and
  `make semantic-tu-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke`. The remaining 1,000+ production `.c` owners are
  now `ast.c`, `ast_print.c`, `parser_domain.c`, and
  `type_checker_decls_domain_helpers.c`.

## UTF-8 Progress Note - 2026-04-27 - CI Documentation Gate Portability

- `documentation-quality-test-smoke` no longer depends on Python. The gate is
  now a shell-only UTF/documentation/async-surface checker using `grep`, `find`,
  and optional `iconv` when present.
- This keeps Windows CI from requiring a Python runtime solely for
  documentation wording checks, while preserving the same beta surface checks:
  required docs/examples exist, docs/examples avoid replacement characters,
  `TODO.md` has the readable UTF-8 Korean title, async docs use named `spawn`
  as the beta-stable task creation surface, executable examples do not use
  capture-bearing anonymous `async { ... }`, and AIR/RemoteFuture wording stays
  pinned.
- The old Python heredoc path also carried a mojibake-sensitive string literal,
  so removing it eliminates a hidden syntax-failure path that was masked on
  Python-missing CI runners.
- `runtime-frontier-contract-test-smoke` is also shell-only now. It preserves
  the same bounded frontier C/LLVM/source-of-truth checks, requires the
  dedicated `runtime-frontier-policy-test-smoke` arithmetic gate to stay wired,
  and keeps whitespace-normalized doc terms without requiring Python on CI
  runners.
- `beta-readiness-checklist-test-smoke` is shell-only now as well. The gate
  still checks the stable subset docs, stdlib freeze, module resolver contract,
  unicode policy, test-suite freeze, observability schema, memory/concurrency
  model, async positioning, Pin/Lease diagnostics, ABI ownership shape, CI
  matrix, Makefile support matrix, and README support wording, but no longer
  requires Python on Windows/macOS/Linux CI.
- `formal-semantics-test-smoke` is shell-only for proof-pack contract checks.
  It still runs `coqc docs/semantics/proofs/SlotCalculus.v` when Coq is
  installed, but missing Python can no longer block the formal semantics gate.
- `inc-sentinel-test-smoke` is shell-only now. The `.inc` ban, `.cases.h`
  include ownership, empty-fragment rejection, and orphan-fragment rejection are
  enforced with POSIX shell, `find`, `grep`, `sed`, and `realpath`.

## UTF-8 Progress Note - 2026-04-27 - AIR Global Verification Layer

- AIR now exposes `air_verify(...)` as the global validation entry point. It
  validates AIR inventory shape, authority participant shape, and evidence
  provenance before computing drift/evidence failures.
- `air_check_drift(...)` remains as a compatibility wrapper over
  `air_verify(...)`; new compiler/docs language should describe AIR as the
  verification layer, not just a drift helper.
- `src/test_air.c` now covers invalid boundary inventory rejection and the
  wrapper compatibility path. Local gate: `make test-air air-drift-test-smoke`.
- The verification pass now has a real owner TU, `src/compiler/air_verify.c`,
  so `src/compiler/air.c` stays focused on AIR synthesis and below the 600 LOC
  split-review threshold.
- AIR inventory validation now rejects missing backing arrays for non-zero
  intent/boundary/drift counts and boundary step-index mismatch before drift
  recomputation. `src/test_air.c` covers both cases.
- AIR inventory validation now also rejects empty intent owner/step names, empty
  boundary owner/source names, boundary-owner mismatch against the referenced
  intent owner, and invalid boundary sync-class shape before drift computation.
  `src/test_air.c` covers owner mismatch and world-boundary sync-shape mismatch.
- AIR drift inventory validation now rejects stale placeholder drift nodes,
  invalid intent/boundary references, and empty drift messages before
  recomputation. `src/test_air.c` covers invalid drift inventory.
- AIR evidence validation now rejects authority evidence without boundary
  evidence and authority evidence on non-authority boundaries, keeping authority
  provenance as a layered proof contract.
- AIR synthesis regression coverage now includes the stable execution boundary
  set (`parallel`, `async`, `channel-send`, `channel-recv`, `select`) in
  addition to spawn and IO boundaries.
- AIR invariant failures now use `PGY_AIR_INVARIANT_INVALID` with
  `air:invariant:invalid` and `report-compiler-bug`, so compiler graph
  corruption is not conflated with user-facing intent boundary drift.
- Systems-language identity is now beta-gated through
  `docs/19_design_philosophy.md`, `docs/100_beta_readiness_checklist.md`, and
  `docs/107_beta_stable_subset.md`: Pergyra is a systems language with domain
  extensions, and raw escape / optional runtime / C FFI ABI stability /
  compile-time determinism are non-negotiable substrate obligations.
- `make codegen-determinism-test-smoke` now emits representative frozen
  backend fixtures twice through C and LLVM and compares normalized generated
  artifacts. This is the initial compile-time determinism gate; remaining work
  is expanding it to the full frozen backend fixture set.
- `--runtime=none` is now parsed and beta-gated through
  `PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED`. Runtime-dependent surfaces are
  explicitly rejected, and pure sources are blocked at the freestanding
  lowering gap so the compiler cannot silently link the default runtime while
  claiming no-runtime semantics. Remaining systems-language blocker:
  implement verified freestanding C/LLVM lowering plus system-tier raw pointer
  escape.
- `SlotRawPointer(...)` is now reserved and explicitly rejected through
  `PGY_SEM_RAW_ESCAPE_UNSTABLE`; `unsafe { ... }` remains a lexical marker
  only. This blocks marketing-vs-implementation drift until raw pointer /
  inline-asm / MMIO escape has ABI lowering and diagnostics.

## UTF-8 Progress Note - 2026-04-27 - Slot Security Owner Split

- `slot_security.c` no longer owns platform fingerprint helpers or memory
  primitive fallbacks directly. `slot_security_platform.c` owns Windows/Linux
  hardware fingerprint retrieval, and `slot_security_memory.c` owns secure
  memory lock/unlock/wipe, constant-time compare, memory barrier, and timestamp.
- This keeps token/context/audit orchestration focused while preserving the
  public `slot_security.h` ABI. Follow-up split: `slot_security_crypto.c` owns
  AES/HMAC token encryption and `slot_security_sealed_payload.c` owns sealed
  payload obfuscation, MAC verification, and shadow recovery.
- `slot_security.c` is now below the 1,000 LOC hard cap at 794 LOC. Local gates
  rerun: `make test-security test-abi runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke production-header-size-test-smoke
  documentation-quality-test-smoke beta-readiness-checklist-test-smoke`.
- Slot manager security monitoring is also split: `slot_manager_security_stats.c`
  owns security event logging, anomaly detection, and security stats printing.
  `slot_manager_scope.c` owns secure scope lifecycle plus the high-level
  `pergyra_*` secure slot wrappers. `slot_manager.c` drops to 1,329 LOC while
  keeping claim/read/write/release, pin/lease, token validation, and secure slot
  primitives in one owner.

## UTF-8 Progress Note - 2026-04-27 - Semantic Owner TU Size Closure

- `type_checker_helpers_late.c` tripped `semantic-tu-size-test-smoke` at 1,031
  LOC after the late helper migration. The active slot view boundary diagnostic
  now has a named owner TU, `type_checker_slot_view_boundary.c`.
- The split keeps `type_checker_helpers_late.c` focused on late call-path and
  borrowed-boundary argument validation at 974 LOC, while the new boundary
  owner carries the pin/await diagnostic wording and article helper at 66 LOC.
- `make semantic-tu-size-test-smoke semantic-core-shape-test-smoke` and
  `make test-semantic` are green after adding the new TU to `SEMANTIC_SOURCES`.

## UTF-8 Progress Note - 2026-04-27 - Runtime Slot Utility Owner Split

- `slot_manager.c` no longer owns the public type-tag/hash/CAS/memory-barrier
  utility family. Those functions moved to `slot_type_utils.c`, leaving
  `slot_manager.c` focused on table lifecycle, pin/lease, secure slot, scope,
  TTL, and statistics behavior.
- This is a small owner-boundary cleanup rather than a semantic change:
  `SlotHandle`, `SlotEntry`, and runtime ABI layout remain unchanged.
- `make test-security test-abi runtime-abi-lifetime-test-smoke` and the
  include/header gates are green after adding the new runtime TU to
  `RUNTIME_SOURCES`.

## UTF-8 Progress Note - 2026-04-27 - Non-Pin Handle Expiration Model

- Slot pinning is not the general stale-handle answer. Pin/Lease only keeps a
  slot live for lexical repeated access. Non-pin stale-handle cases must be
  handled by a layered contract: arena lane checks, CFG/body dataflow,
  zone/world channel-only crossing, token transport rejection, and runtime
  generation/token validation.
- `docs/118_slot_model_rigor_audit.md` now records the stale-handle scenario
  matrix: function escape, long-lived collection/field storage, async/spawn
  capture, channel/world handoff, and copied-handle release divergence.
- First-class Zone-Bound Handle typing is now a beta-freeze decision item. If
  `SlotHandle<T> in Zone` or `handle@zone` enters beta, the compiler must add
  zone-scope <= handle-scope facts and diagnostics. If it does not enter beta,
  the stable behavior remains conservative `BORROW_TRACKED` / anchored-handle
  rejection plus runtime generation/token hard-fail.

## UTF-8 Progress Note - 2026-04-27 - Slot Pin ABA Wrap Guard

- Slot pin audit found a concrete wording/implementation seam: the runtime ABI
  is not a 64-bit generation handle. It currently uses a 32-bit `slotId` plus a
  32-bit generation field, with fresh `slotId` assignment on each claim.
- To prevent ABA id reuse at the current ABI width, `SlotClaim` now tombstones
  the zero-id sentinel and the manager before `slotId` wrap, returning
  `SLOT_ERROR_OUT_OF_MEMORY` rather than reusing an old id.
- `make test-security` now covers the zero-id guard, wrap guard, tampered-view
  generation unpin rejection, and double-unpin rejection as part of the Slot
  pin/lease runtime test. `docs/74_slot_pinning_caching.md`,
  `docs/semantics/08_slot_capability_calculus.md`, and
  `docs/118_slot_model_rigor_audit.md` now state this honestly instead of
  implying a 64-bit/tombstone ABI that was not implemented.
- `docs/semantics/proofs/SlotCalculus.v` now mirrors the implementation model:
  claim requires a fresh non-sentinel id, zero/wrap ids cannot be claimed,
  tampered pinned-view generations cannot unpin, and double-unpin is impossible
  in the small-step sketch.

## UTF-8 Progress Note - 2026-04-27 - Formal Semantics Proof Boundary

- Slot capability calculus is now part of the formal proof pack via
  `docs/semantics/08_slot_capability_calculus.md`.
- `docs/semantics/proofs/SlotCalculus.v` is intentionally labeled as a
  proof-sketch, not completed beta mechanized proof. It now models selected
  Slot capability invariants: stale handle read/write/release rejection,
  mode-specific issued-token read/write/pin/release requirements, unissued-token
  read/write/pin/release rejection, pinned-handle release rejection, and pin
  non-eviction.
- `make formal-semantics-test-smoke` now forbids overclaim terms in the Coq
  artifact and runs `coqc` when the local toolchain provides it.
- Linux GitHub Actions now installs `coq`, so the formal semantics smoke becomes
  an actual Coq type-check gate in CI instead of a local optional check.
- Local 2026-04-27 note: WSL smoke passed but skipped Coq type-check because
  `coqc` is not installed locally; CI remains the authoritative Coq gate.
- Runtime evidence for the Slot capability calculus was rechecked with
  `make test-security` (142/142 passed): generation guard coverage now includes stale-generation
  read/write/pin/release rejection, `SlotIsValid` false, zero-id sentinel and
  slot-id wrap tombstone before ABA reuse, tampered-view generation unpin
  rejection, and double-unpin rejection, plus
  release-while-pinned, scope-release-while-pinned, TTL cleanup skip while
  pinned, secure invalid token rejection, revoked-token rejection, concurrent
  secure write rejection, raw secure-slot release rejection, and
  release-after-unpin.
- `runtime-panic-abi-test-smoke` now covers forged zero-token read/write/release
  rejection for inline C runtime and exported C/LLVM-linkable secure-slot
  entrypoints. SecureSlot token ABI is now build-mode stable: inline C,
  exported runtime, and LLVM-linkable runtime use the same `PgyToken<T>` layout
  with read/write capability bits, and no-`PGY_SAFE_SLOTS` invalid-token /
  released-slot secure paths remain hard-fail checked. The old release-mode
  SecureSlot macro has been removed so future inline ABI drift is blocked.
  `pgy_abi_spec.h` now includes debug/release SecureSlot layout rows for all
  stable primitive payloads (`Int`, `Long`, `Float`, `Double`, `Bool`,
  `String`), and `make test-abi` checks runtime size/token offsets against the
  spec.
  Authority-token mismatch is now a real runtime contract surface:
  `authority-token-mismatch` code/reason, queryable snapshot state, `make
  test-security` direct coverage, `authority_failure_abi` C/LLVM ABI coverage,
  and `authority_failure_surface` backend-compare coverage. The remaining
  secure/authority invariant parity work is richer domain-boundary denial.
  Unsupported authority-token transport is now explicitly rejected on the
  current beta transport surfaces: blocking channel send/receive,
  non-blocking/timeout channel helpers, channel close, cancellation payloads,
  and direct named `spawn` boundaries.
- This keeps the beta proof line honest: theorem statements and regression
  evidence are required now; completed machine-checked proof remains a separate
  hardening gate until CI type-checks it.

## UTF-8 Progress Note - 2026-04-26 - DAG Metadata Materialization Tightening

- Non-generic nominal class type references now materialize through
  `semantic_type_resolution_lookup_or_materialize(...)` metadata instead of
  falling through to the central recursive resolver.
- Generic class references with explicit/default type parameters are
  deliberately excluded from this shortcut so default type argument resolution
  and generic mismatch provenance remain owned by the generic contract path.
- Intermediate DAG smoke stats before the follow-up tightening:
  `graph-backed skips=3137 metadata_entries=3248
  metadata_owned=244 metadata_hits=4724 materializer_fallbacks=1601
  metadata_unresolved_named=1594 metadata_unresolved_generic_named=7
  metadata_unresolved_compound=0 metadata_unresolved_other=0 stage_materialize_alias=83
  stage_materialize_non_alias=0 alias_materialized=5 alias_diagnostic_unresolved=78
  alias_diagnostic_resolved=0 alias_diagnostic_cycle_unresolved=78`.
- `type_resolution_dag_smoke.sh` now gates the tighter beta line:
  `metadata_entries>=3000`, `metadata_hits>=4500`, `metadata_owned>=200`, and
  `materializer_fallbacks<=1601`, with unresolved audit family accounting required to
  sum exactly to the total fallback count.
- That slice moved the remaining DAG closure mostly to named-symbol
  materialization (`1594/1601` fallback events), not compound type
  construction. The next target is to split
  imported/non-class nominal, alias-diagnostic, and visibility-sensitive named
  references instead of widening the generic shortcut.
- Follow-up tightening: known non-class scope symbols now materialize through
  metadata using the same `scope-type lookup` contract as `resolve_named_type`.
  Current stats are `metadata_entries=3346 metadata_hits=4935
  materializer_fallbacks=1296 metadata_unresolved_named=1289
  metadata_unresolved_generic_named=7 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=1281
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now requires `metadata_entries>=3300`,
  `metadata_hits>=4900`, and `materializer_fallbacks<=1296`.
- Verified locally: `make type-resolution-dag-test-smoke` and
  `make type-resolution-resolver-inventory-test-smoke`.
- 2026-04-27 tightening: alias chains now short-circuit in the metadata
  materializer when the chain resolves, and alias cycles are detected in the
  metadata path before falling through to recursive materialization. This keeps
  cycle diagnostics/provenance alive while removing repeated alias fallback
  churn. Current stats are `metadata_entries=3346 metadata_hits=4935
  materializer_fallbacks=15 metadata_unresolved_named=8
  metadata_unresolved_generic_named=7 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=15` and requires
  `metadata_named_alias==0`. It also requires the fallback total to equal the
  diagnostic-only family sum (`builtin_shell + generic_named + missing_symbol`)
  and keeps compound/other/generic-class/non-class-symbol fallback at zero. The
  remaining fallback set is diagnostic-only: bare generic shells, invalid
  generic-named forms, and missing symbol negative cases.
- 2026-04-28 tightening: stable constructed type arguments now use a shared
  metadata-only resolver so Slot/collection/Result shell materialization does
  not duplicate generic argument lookup logic, and already-proven invalid stable
  constructed shells stop before the central recursive materializer. Explicit
  user generic class specializations also materialize through DAG metadata while
  preserving where/default validation provenance. At this intermediate point,
  no-arg default generic class specialization intentionally remained on the
  diagnostic path because the validator preserved instantiated provenance such
  as `Box<Item>`.
  Current stats are `metadata_entries=3354 metadata_hits=4950
  metadata_owned=249 materializer_fallbacks=10 metadata_unresolved_named=8
  metadata_unresolved_generic_named=2 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=10` and
  `metadata_unresolved_generic_named<=2`.
- 2026-04-28 follow-up tightening: nested stable constructed arguments now
  materialize before fallback, so `Channel<Slot<T>>` / similar wrapper chains no
  longer hit the recursive resolver. Current stats are
  `metadata_entries=3356 metadata_hits=4950 metadata_owned=251
  materializer_fallbacks=8 metadata_unresolved_named=8
  metadata_unresolved_generic_named=0 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=8` and requires
  `metadata_unresolved_generic_named==0`. The remaining fallback set is only bare
  `Box`/constructor-shell provenance plus missing-symbol negative diagnostics.
- 2026-04-28 default-specialization tightening: class where diagnostics now
  format the actual path from effective type arguments, so no-arg default
  generic class specializations keep provenance such as `Box<Item>` while still
  materializing through DAG metadata. Current stats are
  `metadata_entries=3358 metadata_hits=6744 metadata_owned=253
  materializer_fallbacks=6 metadata_unresolved_named=6
  metadata_unresolved_generic_named=0 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=0
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=6` and requires
  `metadata_named_builtin_shell==0`; the only remaining unresolved audit family is
  missing-symbol negative diagnostics.
- 2026-04-28 final materializer tightening: bare unknown named types now emit
  `PGY_SEM_UNKNOWN_TYPE` directly from the metadata path instead of entering the
  recursive resolver. Current stats are `metadata_entries=3358
  metadata_hits=6744 metadata_owned=253 materializer_fallbacks=0
  metadata_unresolved_named=0 metadata_unresolved_generic_named=0
  metadata_unresolved_compound=0 metadata_unresolved_other=0
  metadata_named_builtin_shell=0 metadata_named_generic_class=0
  metadata_named_alias=0 metadata_named_non_class_symbol=0
  metadata_named_missing_symbol=0`. The DAG smoke gate now requires central
  metadata materializer fallback to stay exactly `0`.

## UTF-8 Progress Note - 2026-04-26 - Overall Beta Audit Follow-up

- Tooling conformance is green locally with `make tooling-conformance-test-smoke`.
  The formatter smoke is invoked through `bash`, so Linux execute-bit drift on
  mounted worktrees should not reproduce the old `fmt_smoke.sh Permission
  denied` failure.
- Production runtime/codegen/compiler/semantic `.inc` debt is closed for beta:
  production `.inc` count is 0, and only `src/tests/**/*.inc` fixtures remain.
  The new structural policy is stricter than the old include cleanup:
  production `.c` / private owner `.h` files above 600 LOC require split review
  and a named follow-up seam; 1,000 LOC is only the hard stop / risk line.
  `production-header-size-test-smoke` now caps every production owner header at
  1,000 LOC with no per-file temporary exception; `llvm_internal.h` was split
  by moving declaration inventory helpers behind `llvm_inventory_internal.h`,
  and the LLVM inventory helper family is now split into lookup,
  host-method metadata, and domain/routine inventory owners.
  LLVM statement parallel/async/select lowering now lives in
  `llvm_stmt_parallel_async.c`, reducing `llvm_stmt.c` to 3,078 LOC with
  backend compare still green. LLVM domain method/provenance helpers now live
  in `llvm_domain_method_helpers.c`, reducing `llvm_domain.c` to 3,340 LOC
  while keeping backend compare green. LLVM world sync lowering now lives in
  `llvm_domain_world_sync.c`, and LLVM zone sync lowering now lives in
  `llvm_domain_zone_sync.c`; `llvm_domain.c` is down to 1,649 LOC. The former
  `llvm_domain_core_helpers.h` mega-header was split into focused owner
  headers for role lookup, decl parts, projection count/value/sync body, and
  zone binding so the build stays warning-clean instead of hiding unused static
  helpers. Build, header/inc gates, docs gates, and backend compare remain
  green. LLVM statement ownership is now split into focused owners:
  `llvm_stmt_type_infer.c`, `llvm_stmt_let_helpers.c`,
  `llvm_stmt_let_with.c`, `llvm_stmt_with.c`, `llvm_stmt_loop_match.c`, and
  `llvm_stmt_parallel_async.c`, plus `llvm_stmt_zone_action.c` for
  zone-action effect propagation and `llvm_stmt_type_render.c` for generic
  type-argument rendering. `llvm_stmt.c` is down to 573 LOC, the dispatcher
  owner is below the 600 LOC split-review threshold, and backend smoke remains
  green.
  Parser orchestration/declaration debt is also below the 1,000 LOC hard cap:
  `parser.c` is 977 LOC and `parser_decl.c` is 887 LOC after extracting
  doc-comment, export, enum, pin-block, lexical-zone, declaration-clause, and
  declaration-lookahead owners. The next lean cleanup target is large real TUs,
  starting with parser AST / AST-print / parser-domain owners, the largest
  semantic domain helpers, and remaining body-loop subowners inside
  `llvm_emit_intent_decl`.
- Lean debt-slice follow-up: C backend type-alias declaration emission now has
  a real owner in `src/codegen/transpiler_type_alias.c`; the old body was
  removed from `transpiler_emitters_base_b_part_c.inc`. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Lean debt-slice follow-up: C backend type-requirement checks now have a real
  owner in `src/codegen/transpiler_type_require.c`; the old
  `src/codegen/transpiler_emitters_type_require.inc` include body was deleted,
  reducing the source `.inc` cap to 159 and keeping
  `transpiler_emitters_base_a_part_a.inc` at 905 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend extern declaration emission now has a
  real owner in `src/codegen/transpiler_extern.c`; `emit_extern_block(...)` was
  removed from `transpiler_emitters_base_b_part_b.inc`, reducing that near-cap
  include body from 998 LOC to 957 LOC. `tests/inc_sentinel_smoke.sh` now uses
  the current 159 source-`.inc` cap by default. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend type declarator rendering now has a real
  owner in `src/codegen/transpiler_type_declarator.c`; event-handler
  declarators, function pointer declarators, and function signatures were
  removed from `transpiler_helpers_core_b_part_c.inc`, reducing it from 992 LOC
  to 849 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend LogBanner normalization now has a real
  owner in `src/codegen/transpiler_log_normalize.c`; multiline indentation
  normalization was removed from `transpiler_expr_emitters_part_a.inc`,
  reducing it from 991 LOC to 878 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: generated-C runtime intent exit cleanup now has a
  private inline owner in `src/runtime/pgy_runtime_intent_exit.h`;
  `pgy_intent_exit_export(...)` keeps the same inline ABI name, while
  `pgy_runtime_part_ba_part_b.inc` drops from 996 LOC to 894 LOC. Local gate
  used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C DeviceSlot/SecureSlot macro bodies now
  have a private inline owner in `src/runtime/pgy_runtime_slot_macros.h`;
  built-in instantiation remains in `pgy_runtime_part_ba_part_c.inc`, which
  drops from 996 LOC to 808 LOC. Local gate used:
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C intent last-history step accessors now
  have a private inline owner in `src/runtime/pgy_runtime_intent_history.h`;
  `pgy_runtime_part_ba_part_a.inc` drops from 989 LOC to 867 LOC while the
  borrowed string ABI remains guarded. `runtime_abi_lifetime_smoke.sh` now reads
  the private inline headers that participate in the generated-C runtime family.
  Local gate used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C intent last/active borrowed exports now
  have a private inline owner in
  `src/runtime/pgy_runtime_intent_active_exports.h`; the registry/state half
  remains in `pgy_runtime_part_ba_part_a.inc`, which drops from 867 LOC to
  558 LOC. `runtime_abi_lifetime_smoke.sh` now tracks active and recent export
  owners separately so future movement cannot hide behind concatenated runtime
  text. Local gate used: `make runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke`, plus `make -B pgy
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke test-abi`.
- Lean debt-slice follow-up: LLVM-linkable intent borrowed exports now have a
  matching private owner in `src/runtime/pgy_runtime_lib_intent_exports.h`;
  `pgy_runtime_lib_part_b_part_c.inc` drops from 852 LOC to 315 LOC while
  keeping `intent_active`, `intent_recent`, and `intent_failure` ABI pipeline
  cases green on C and LLVM. This keeps generated-C inline and LLVM-linkable
  runtime export ownership symmetric instead of letting `part_b_part_c.inc`
  carry mixed intent-observability and slot-operation bodies.
- Lean debt-slice follow-up: LLVM method-call projection sync helpers now have
  a private owner in `src/codegen/llvm_expr_call_projection_sync.h`;
  `llvm_expr_call_methods_part_a.inc` drops from 880 LOC to 671 LOC while the
  world/zone projection sync call sites keep the same include order. Local gate
  used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus
  targeted backend compare for `world_embedded_branch_projection_visibility`,
  `world_embedded_action_frontier`, `world_embedded_action_pool_frontier`, and
  `world_zone_projection_visibility`.
- Lean debt-slice follow-up: LLVM method-call domain action sync and
  slice/member-call helpers now have a private owner in
  `src/codegen/llvm_expr_call_methods_domain_slice.h`; the remaining
  `llvm_expr_call_methods_part_a.inc` body is removed while `llvm_expr.c`
  include order remains stable. The production source `.inc` inventory is now
  92 files / 28,467 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM call dispatch now has a private owner in
  `src/codegen/llvm_expr_call_dispatch.h`; the former
  `llvm_expr_calls_main.inc` body is removed while the call-family shim order
  remains stable. The production source `.inc` inventory is now 91 files /
  27,842 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression host/self, projection binding,
  spawn expression, operator suffix, enum lookup, and number/string literal
  helpers now have a private owner in
  `src/codegen/llvm_expr_host_spawn_literal_helpers.h`; the former
  `llvm_expr_helpers_part_b.inc` body is removed while `llvm_expr.c` helper
  include order remains stable. The production source `.inc` inventory is now
  90 files / 27,221 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend role method emission, ability/vtable
  emission, hidden provenance helpers, and role operator aliases now have a
  private owner in `src/codegen/transpiler_domain_role_ability_emit.h`; the
  former `transpiler_domain_role_part_a.inc` body is removed while the
  domain-role shim order remains stable. The production source `.inc` inventory
  is now 89 files / 26,601 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression boundary call argument helpers,
  projection field helpers, world/zone lookup helpers, and host-class lookup
  helpers now have a private owner in
  `src/codegen/llvm_expr_boundary_projection_helpers.h`; the former
  `llvm_expr_helpers_part_a.inc` body is removed while `llvm_expr.c` helper
  include order remains stable. The production source `.inc` inventory is now
  88 files / 25,996 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR routine lookup, active SSA name
  resolution/rendering, token-local filtering, and local type-name lookup now
  have a private owner in `src/codegen/transpiler_mir_ssa_names.h`; the former
  `transpiler_emitters_mir_inventory_ssa_names.inc` body is removed while the
  MIR inventory/SSA shim order remains stable. The production source `.inc`
  inventory is now 87 files / 25,395 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend primitive, slot/channel, constructed
  generic, and local type-name rendering now have a private owner in
  `src/codegen/transpiler_type_mapping_helpers.h`; the former
  `transpiler_helpers_core_types.inc` body is removed while the helper-core shim
  order remains stable. The production source `.inc` inventory is now 86 files /
  24,796 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend world sync declaration, select lowering,
  and event declaration/subscription lowering now have a private owner in
  `src/codegen/transpiler_world_select_event_emit.h`; the former
  `transpiler_domain_role_part_d.inc` body is removed while the domain-role shim
  order remains stable. The production source `.inc` inventory is now 85 files /
  24,198 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression assignment, member lvalue/member
  access, projection invalidation, and embedded world projection assignment sync
  now have a private owner in
  `src/codegen/llvm_expr_assignment_member_projection.h`; the former
  `llvm_expr_values.inc` body is removed while `llvm_expr.c` include order
  remains stable. The production source `.inc` inventory is now 84 files /
  23,617 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable runtime authority rejection state,
  checked arithmetic exports, panic invariant export, and file-path
  normalization helpers now have a private owner in
  `src/runtime/pgy_runtime_lib_authority_file_core.h`; the former
  `pgy_runtime_lib_part_a.inc` body is removed while `pgy_runtime_lib.c` include
  order remains stable. The production source `.inc` inventory is now 83 files /
  23,031 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-abi-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable raw set tail exports, intent
  active/recent registry helpers, intent trace mutation, and MIR trace hooks now
  have a private owner in
  `src/runtime/pgy_runtime_lib_set_intent_trace_exports.h`; the former
  `pgy_runtime_lib_part_b_part_b.inc` body is removed while `pgy_runtime_lib.c`
  include order remains stable. The production source `.inc` inventory is now
  82 files / 22,449 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-abi-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: RIR flow semantic flags, state merge rules, and
  HIR CFG enrichment now have a private owner in `src/compiler/rir_flow.h`; the
  former `rir_flow.inc` body is removed while `rir.c` include order remains
  stable. The production source `.inc` inventory is now 81 files / 21,877 LOC.
  Local gate to use for this slice: `make -B pgy type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR SSA identifier contract helpers now
  have a private owner in `src/codegen/transpiler_mir_ssa_contract.h`;
  `transpiler_emitters_base_a_part_d.inc` drops from 849 LOC to 677 LOC. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile`.
- Lean debt-slice follow-up: C backend MIR emission contract/resource-hook
  helpers now have a private owner in
  `src/codegen/transpiler_mir_emission_contract.h`; the remaining
  `transpiler_emitters_base_a_part_d.inc` body is removed while the base-A shim
  keeps include order stable. The production source `.inc` inventory is now
  95 files / 30,368 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: RIR lowering/enrichment now has a private owner in
  `src/compiler/rir_builder.h`; the former `rir_builder.inc` body is removed
  while `rir.c` keeps the flow -> build -> names -> validation include order.
  The production source `.inc` inventory is now 94 files / 29,733 LOC. Local
  gate to use for this slice: `make -B pgy type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic function-body checking now has a private
  owner in `src/semantic/type_checker_program.h`; the former
  `type_checker_program.inc` body is removed while the top-level semantic TU
  include order remains stable. The production source `.inc` inventory is now
  93 files / 29,099 LOC. Local gate to use for this slice:
  `make -B pgy test-semantic semantic-core-shape-test-smoke
  cfg-body-dataflow-test-smoke type-resolution-dag-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend slot/device builtin expression emitters
  now have a private owner in `src/codegen/transpiler_slot_builtin_emit.h`;
  `transpiler_expr_emitters_part_a.inc` drops from 797 LOC to 531 LOC while
  preserving slot sugar, secure slot token, and runtime panic codegen smoke.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile runtime-panic-codegen-test-smoke`.
- Lean debt-slice follow-up: C backend expression type inference now has a
  private owner in `src/codegen/transpiler_expr_type_infer.h`;
  `transpiler_helpers_core_b_part_c.inc` drops from 797 LOC to 296 LOC. This
  keeps generic/default-return inference in the same include order while
  separating the expression-type owner from spawn/generic helper tails. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile`.
- Lean debt-slice follow-up: C backend statement dispatch now has a private
  owner in `src/codegen/transpiler_statement_dispatch.h`;
  `transpiler_emitters_base_b_part_c.inc` drops from 803 LOC to 546 LOC. This
  leaves `part_c` focused on block emission and intent helper tails instead of
  carrying the top-level statement switch. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile` plus
  targeted backend compare for `break_continue`, `parallel_channel_sum`, and
  `intent_header_interleaved`.
- Lean debt-slice follow-up: generated-C `HashMap<String>` and map-keys inline
  runtime now has a private owner in `src/runtime/pgy_runtime_map_string_inline.h`;
  `pgy_runtime_part_ba_part_d.inc` drops from 767 LOC to 377 LOC and is now
  focused on List/Set inline runtime. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  plus targeted backend compare for `map_get_string`, `map_keys`,
  `list_get_string`, `queue_pop_string`, and
  `intent_failure_observability_strings`.
- Lean debt-slice follow-up: C backend MIR function emission now has a private
  owner in `src/codegen/transpiler_mir_func_emit.h`;
  `transpiler_emitters_base_b_part_a.inc` drops from 766 LOC to 162 LOC. This
  keeps the MIR emit-state snapshot helpers in the original part while moving
  the large `emit_func_decl_from_mir_named(...)` body behind a named owner.
- Lean debt-slice follow-up: generated-C runtime array sort kernels and scalar
  std/log/math helpers now have private owners in
  `src/runtime/pgy_runtime_array_sort_inline.h` and
  `src/runtime/pgy_runtime_scalar_std_inline.h`;
  `pgy_runtime_part_ba_part_c.inc` drops from 759 LOC to 535 LOC and is now
  focused on built-in type instantiation plus HashMap core.
- Lean debt-slice follow-up: LLVM-linkable runtime core exports now have a
  private owner in `src/runtime/pgy_runtime_lib_core_exports.h`; logging,
  time/sleep, and `pgy_int_to_string(...)` moved out of
  `pgy_runtime_lib_part_b_part_a.inc`, reducing it from 986 LOC to 909 LOC.
  Local gate used: `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: C backend `let` destructuring lowering now has a
  private owner in `src/codegen/transpiler_destructure_emit.h`;
  `transpiler_emitters_base_b_part_c.inc` drops from 976 LOC to 873 LOC. Local
  gate used: `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  targeted backend compare for `destructure_array` and
  `destructure_tuple_return`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: generated-C queue macro and built-in queue
  implementations now have a private owner in
  `src/runtime/pgy_runtime_queue_inline.h`; `pgy_runtime_part_ba_part_e.inc`
  drops from 969 LOC to 773 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke
  test-abi`, targeted backend compare for `queue_pop_string` and
  `parallel_channel_sum`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: generated-C `HashMap<Int>` key adapters for
  `Int`/`Long`/`Bool` keys now have a private owner in
  `src/runtime/pgy_runtime_map_int_key_inline.h`; `pgy_runtime_part_ba_part_d.inc`
  drops from 963 LOC to 815 LOC. Local gate used: `make -B pgy`,
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke`, targeted backend compare for `map_keys` and
  `map_get_string`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: LLVM-linkable primitive slot exports for
  `Slot<Double>`, `Slot<Bool>`, and `Slot<String>` now have a private owner in
  `src/runtime/pgy_runtime_lib_slot_exports.h`; `pgy_runtime_lib_part_b_part_d.inc`
  drops from 947 LOC to 790 LOC while exported ABI symbol names remain
  unchanged. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-abi-test-smoke
  runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Lean debt-slice follow-up: LLVM-linkable standard string/conversion/math/random
  exports now have a private owner in `src/runtime/pgy_runtime_lib_std_exports.h`;
  `pgy_runtime_lib_part_b_part_e.inc` drops from 817 LOC to 761 LOC and now
  starts at the channel runtime section. `runtime_abi_lifetime_smoke.sh` now
  reads runtime-lib private owner headers so result-owned string checks follow
  the real include order. Local gate used: `make runtime-abi-lifetime-test-smoke
  test-abi backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable raw `List<T>` collection exports now
  have a private owner in `src/runtime/pgy_runtime_lib_list_raw_exports.h`;
  `pgy_runtime_lib_part_b_part_a.inc` drops from 909 LOC to 759 LOC and is now
  focused on raw queue/map exports. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Lean debt-slice follow-up: MIR declaration-header inventory helpers now have
  a private owner in `src/compiler/mir_decl_headers.h`; `mir_public_part_a.inc`
  drops from 959 LOC to 789 LOC and now starts at `mir_lower(...)`. Local gate
  used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Lean debt-slice follow-up: RIR public vocabulary name helpers now have a
  private owner in `src/compiler/rir_names.h`; `rir_public.inc` drops from
  911 LOC to 804 LOC while RIR validation/dump vocabulary remains unchanged.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke air-drift-test-smoke
  test-abi`.
- Lean debt-slice follow-up: C backend parallel capture analysis now has a
  private owner in `src/codegen/transpiler_parallel_capture.h`;
  `transpiler_emitters_base_b_part_b.inc` drops from 957 LOC to 730 LOC while
  parallel capture typing and slot capture behavior remain unchanged. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  parallel-core-contract-test-smoke runtime-panic-codegen-test-smoke` plus
  targeted backend compare for `parallel_channel_sum`.
- Lean debt-slice follow-up: C backend stdlib call lowering now has a private
  owner in `src/codegen/transpiler_expr_stdlib_builtin.h`;
  `transpiler_expr_emitters_part_d.inc` drops from 946 LOC to 26 LOC while
  stdlib/string/collection call behavior remains unchanged. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke` plus targeted backend compare for
  `string_io`, `array_builtins`, `list_get_string`, and `map_get_string`.
- Lean debt-slice follow-up: C backend overlay/projection invalidation and
  zone-layer bind helpers now have a private owner in
  `src/codegen/transpiler_overlay_projection.h`; the old
  `transpiler_helpers_core_a_part_b.inc` include body was removed, lowering the
  source `.inc` count to 158. `runtime_frontier_contract_smoke.sh` now checks
  the real world frontier owner in `transpiler_domain_role_part_d.inc` instead
  of the adjacent zone frontier part. Local gate used:
  `make runtime-frontier-contract-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` plus targeted backend compare for
  `world_embedded_branch_projection_visibility` and
  `world_embedded_action_frontier`.
- Lean debt-slice follow-up: C backend `let` declaration lowering now has a
  private owner in `src/codegen/transpiler_let_emit.h`;
  `transpiler_emitters_base_a_part_a.inc` drops from 905 LOC to 138 LOC while
  MIR inventory/SSA helper declarations remain in the original base-A part.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile` plus targeted backend compare for
  `destructure_array`, `array_builtins`, and `map_keys`.
- Lean debt-slice follow-up: C backend MIR block statement emission now has a
  private owner in `src/codegen/transpiler_mir_block_emit.h`; the old
  `transpiler_emitters_base_a_part_c.inc` include body was removed. Source
  `.inc` total drops to 49,911 LOC, with only `transpiler_emitters_intent.inc`
  still above 900 LOC. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  type-resolution-dag-test-smoke air-drift-test-smoke` plus targeted backend
  compare for `destructure_array`, `destructure_tuple_return`,
  `host_method_class_return`, and `world_embedded_branch_projection_visibility`.
- Lean debt-slice follow-up: C backend intent declaration emission now has a
  private owner in `src/codegen/transpiler_intent_emit.h`; the old
  `transpiler_emitters_intent.inc` include body was removed. Source `.inc`
  total drops to 48,949 LOC, and no production `.inc` file remains above 900
  LOC. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile runtime-panic-codegen-test-smoke` plus
  targeted backend compare for `intent_authority_snapshot` and
  `intent_failure_observability_strings`.
- Lean debt-slice follow-up: generated-C runtime intent-recent accessors,
  panic helpers, and checked arithmetic exports now have a private owner in
  `src/runtime/pgy_runtime_panic_checked_inline.h`;
  `pgy_runtime_part_ba_part_b.inc` drops from 894 LOC to 705 LOC and the
  runtime ABI lifetime inventory reads the new header in generated-runtime
  include order. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke
  runtime-panic-abi-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Current highest-value implementation order is now:
  1. CFG/body dataflow source-of-truth for function/action/intent safety.
     - Latest closure slice: MIR cleanup block creation now consumes RIR policy
       ops, conservative semantics, flow-block summaries, and resource facts
       for rollback/invalidation decisions. The former intent-step AST
       invalidation scanner is removed and gated out by
       `cfg-body-dataflow-test-smoke`.
     - RIR flow owner split: `src/compiler/rir_flow_state.h` owns the
       resource-state merge lattice and helper predicates; `rir_flow.h` is down
       to 420 LOC and stays focused on HIR CFG enrichment / bounded dataflow
       iteration.
  2. DAG source-of-truth completion for named symbols, module contracts, and
     generic consumer paths.
  3. AIR strict-evidence negative expansion for transfer/world/boundary cases.
  4. Runtime frontier scheduler generalization beyond the already-covered
     bounded recompute slices.
  5. ABI ownership/pinning parity and diagnostic quality gate hardening.
  6. Cross-platform support matrix enforcement.
- The first removable blocker under that list is still owner debt that slows
  every P0/P1/DAG/AIR change. MIR ABI layout lookup now has a private owner in
  `src/compiler/mir_abi_layout.h`; `mir_public_part_b.inc` drops from 753 LOC
  to 420 LOC and now focuses on MIR validation/dump surfaces. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- CFG contract validation now has a private owner in
  `src/compiler/mir_cfg_contract_validate.h`; `mir_public_part_a.inc` drops
  from 743 LOC to 290 LOC and no longer mixes public MIR entry points with
  cleanup/rollback/invalidation graph contract checks. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- RIR validation now has a private owner in `src/compiler/rir_validation.h`;
  `rir_public.inc` drops from 741 LOC to 269 LOC and now keeps only
  destroy/dump public surfaces. This makes AIR/CFG evidence validation a named
  owner instead of a mixed public include body. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- C backend MIR intent inventory helpers now have a named owner in
  `src/codegen/transpiler_mir_inventory_intent.h`; the old
  `transpiler_emitters_mir_inventory_intent.inc` include body is gone and the
  existing SSA include-order shim now references the owner header directly.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- C backend call/spawn/channel expression emission now has a named owner in
  `src/codegen/transpiler_expr_call_spawn_emit.h`; the old
  `transpiler_expr_emitters_part_e.inc` body is gone and the expression emitter
  shim includes the owner header directly. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- C backend builtin-call dispatch now has a named owner in
  `src/codegen/transpiler_expr_builtin_dispatch.h`; the old
  `transpiler_expr_emitters_part_b.inc` body is gone and the expression emitter
  shim includes the owner header directly. This keeps builtin dispatch out of
  split `.inc` ownership without changing call lowering order. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Semantic builtin-query checks now have named owners in
  `src/semantic/type_checker_builtins_query.c`,
  `src/semantic/type_checker_builtins_query_world.c`,
  `src/semantic/type_checker_builtins_query_channel.c`, and
  `src/semantic/type_checker_builtins_query_domain.c`. The corresponding
  query headers are declaration-only guards, so builtin query behavior no
  longer depends on include-order side effects.
- Semantic builtin nominal/type contract checks now have a named owner in
  `src/semantic/type_checker_builtins_nominal.c`; intent observability is split
  further into `src/semantic/type_checker_builtins_intent_observability.c`.
  `type_checker_builtins_nominal.h` is declaration-only while preserving
  `Rc`/`Weak`/`Box`/allocator and intent-observability builtin dispatch order.
- Slot analyzer escape handling moved to
  `src/semantic/slot_analyzer_escape.c`, leaving
  `src/semantic/slot_analyzer_summary.c` below the 600 LOC review threshold
  and focused on access/parameter summary behavior.
- Generated-C runtime pool/FSM/timer helpers now have a named owner in
  `src/runtime/pgy_runtime_pool_fsm_timer_inline.h`; `pgy_runtime_part_ba_part_e.inc`
  now starts at parallel/zone authority support instead of mixing object-pool,
  FSM, timer, cooldown, authority, result, and option helpers in one body.
  Runtime ABI lifetime inventory and compiler runtime-cache freshness track
  the new owner header directly.
- Semantic expression checking now has a named owner in
  `src/semantic/type_checker_expr.h`; the old `type_checker_expr.inc` body is
  gone and CFG body-dataflow smoke follows the new owner path.
- C backend function/class/control-flow emission now has a named owner in
  `src/codegen/transpiler_func_class_flow_emit.h`; the old
  `transpiler_emitters_base_b_part_b.inc` body is gone while preserving the
  base-B include order.
- Generated-C runtime Box/Arena/Allocator/Array/Rc/primitive-slot helpers now
  have a named owner in `src/runtime/pgy_runtime_memory_array_slot_inline.h`;
  the old `pgy_runtime_part_ba_part_b.inc` body is gone. Runtime panic contract,
  ABI lifetime inventory, and compiler runtime-cache freshness track the new
  owner header directly.
- Semantic relation/effect/projection helper logic now has a named owner in
  `src/semantic/type_checker_helpers_effects.h`; the old
  `type_checker_helpers_effects.inc` body is gone and CFG body-dataflow smoke
  tracks the new helper path.
- LLVM domain core helpers now have a named owner in
  `src/codegen/llvm_domain_core_helpers.h`; the old
  `llvm_domain_helpers_part_a.inc` body is gone and `llvm_domain.c` includes
  the owner header directly. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- LLVM-linkable runtime channel/qubit exports now have a named owner in
  `src/runtime/pgy_runtime_lib_channel_quantum_exports.h`; the old
  `pgy_runtime_lib_part_b_part_e.inc` body is gone and
  `runtime_abi_lifetime_smoke.sh` reads the new owner header in generated
  runtime include order. Local gate used: `make backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- LLVM-linkable raw Queue/Map/Set exports now have a named owner in
  `src/runtime/pgy_runtime_lib_raw_collection_exports.h`, and secure/device
  slot, array, file IO, and string helper exports now have a named owner in
  `src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h`. The old
  `pgy_runtime_lib_part_b_part_a.inc` and `pgy_runtime_lib_part_b_part_d.inc`
  bodies are gone. Runtime panic/lifetime smokes now check the new owner
  headers and compiler runtime cache freshness tracks them directly. Local
  gate used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke
  runtime-panic-codegen-test-smoke test-abi`.
- Rejected shortcut: using the alias symbol's already-materialized `sym->type`
  directly inside metadata alias lookup breaks module visibility and generic
  ability provenance tests. Alias DAG closure must preserve export/private
  provenance and effective generic-bound facts instead of trusting the symbol
  cache as the source of truth.
- Sprint process change: beta closure now uses a lean debt-slice loop. Pick one
  owner, complete the implementation slice, run the slice-local gate, and defer
  wider regression to the slice boundary. Full regression is still required
  before closure, but the inner loop must be implementation-first, not
  test-threshold-first.

## UTF-8 Progress Note - 2026-04-26 - DAG Owner Seam Centralization

- All owner-local type resolver seams now route through
  `semantic_type_resolution_lookup_or_materialize(...)` instead of owning direct
  fallback helper calls.
- The old `semantic_type_resolution_resolve_or_fallback(...)` helper is removed;
  `type-resolution-resolver-inventory-test-smoke` caps named fallback seams at 0
  and fails if new owner-local fallback users appear.
- Previous DAG smoke stats before nominal metadata materialization tightening:
  `graph-backed skips=3137 metadata_entries=2044
  metadata_owned=123 metadata_hits=3300 materializer_fallbacks=4135
  stage_materialize_alias=83 stage_materialize_non_alias=0 alias_materialized=5
  alias_diagnostic_unresolved=78 alias_diagnostic_resolved=0
  alias_diagnostic_cycle_unresolved=78`.
- This was not full DAG source-of-truth at the time. The central recursive
  fallback has since been removed; remaining closure is keeping imported
  ability/default/bound/module/nominal consumers and diagnostics aligned with
  graph/topo materialization rather than compatibility wording.
- Verified locally: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke` and `make test-semantic`.

마지막 업데이트: 2026-04-25

## 현재 상태 냉정 평가 (2026-04-12 재정렬)

### 종합 판단: Late-Stage Alpha

- 베타 readiness 추정: 약 `60%`
- 현재 표현: `late-stage alpha / beta-closure sprint`
- 보정 이유:
  - 기능 표면만 보면 core/foundation 구현은 넓지만, beta는 기능 개수가 아니라 end-to-end 신뢰도다
  - HIR/MIR CFG skeleton은 이미 있지만, 함수/action/intent body 안전성의 semantic source-of-truth가 아직 CFG/dataflow로 승격되지 않았다. all-path return, use-before-init, move/borrow join, drop cleanup, zone/effect transition, parallel/channel boundary를 AST/helper traversal만으로 닫으면 strict beta 신뢰도가 부족하다
  - AIR abstraction safety는 Phase 1 데이터 구조 / synthesis / drift checker baseline과 driver semantic-validation wiring이 들어왔다. Intent ↔ implementation drift 검출은 `docs/104_air_compiler_architecture.md`와 `make air-drift-test-smoke`로 gate에 들어왔고, strict evidence는 기본값으로 승격됐다. missing HIR CFG / RIR boundary / RIR authority evidence는 `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`로 hard-fail 되며, `authorized by` participant 이름과 RIR authority fact / authorize op subject가 일치해야 한다. authority evidence 누락 진단은 `Reason:` 안에 expected authority participant list를 포함한다. AIR drift message와 synthesized intent/boundary/authority name은 owned lifetime으로 관리되고, repeated drift check가 이전 message를 안전하게 해제하는 회귀 테스트와 parsed-source AIR teardown-safe boundary source 회귀가 있다. `where + transfer`는 더 이상 zone boundary 하나로 접히지 않고 zone boundary와 world-handoff boundary를 모두 합성한다. world-handoff evidence는 이제 matching RIR intent scope만으로 통과하지 않고 boundary source alias에 대한 RIR `Move`/`Claim` transfer op를 요구한다. implementation boundary evidence는 이제 HIR CFG proof도 요구하므로 `parallel` / `channel` / IO / execution boundary는 RIR evidence만으로 통과하지 않는다. parsed-source missing-authority-evidence negative와 parsed-source IO execution-boundary missing-evidence negative는 full driver JSON path에서 step source span과 `stage/code/cause_ir/fix_source`까지 고정됐다. expression boundary evidence는 더 이상 owner-name-only RIR scope match로 통과하지 않는다. `PGY_AIR_STRICT_EVIDENCE=0`은 개발/디버그 opt-out이다. `make air-backend-nonimpact-test-smoke`는 relaxed AIR와 default strict AIR가 intent/zone, cross-world transfer, handoff frontier, world projection, relation/effect, authority-failure fixture set에서 같은 C/LLVM 텍스트를 생성하는지 비교한다. `make air-backend-nonimpact-full-test-smoke`는 full frozen backend-compare fixture sweep을 같은 방식으로 돌리고 Linux CI gate로 승격됐다. `make air-strict-backend-compare-test-smoke`는 strict evidence 상태에서 C/LLVM 실행 parity까지 검증한다. parser/lexer baseline JSON routing은 `stage`, `code`, `cause_ir`, `fix_source`까지 닫혔다. 남은 blocker는 AIR transfer/world source negative 확장, Windows native evidence, parser-specific code split / multi-error accumulation이다
  - CFG 소비자 정리: `type_checker_flow_match.c`가 match pattern binding, match exhaustiveness, redundancy, total-coverage lattice를 소유한다. `type_checker_flow.c`는 branch/join, loop/defer/parallel boundary, body return/unreachable flow orchestration에 집중하며 435 LOC로 내려갔다. `semantic-core-shape-test-smoke`는 `type_checker_flow.c`와 `type_checker_flow_match.c`가 모두 600 LOC 이하인지 검사한다.
  - 2026-04-27 AIR IO boundary tightening: intent-step execution scan now treats the stable resource IO/time builtin set as AIR `io` boundaries, not only `ReadFile` / `WriteFile` / `ReadLine`. The gated set is `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`, `Input`, `ReadLine`, `Now`, and `Sleep`; `Print` / `Log*` remain observability output calls rather than AIR resource-boundary evidence in Phase 1. `src/test_air.c` keeps the set synchronized with `src/compiler/air_boundary.c`.
  - 2026-04-27 AIR owner split: dump/vocabulary functions moved to `src/compiler/air_dump.c`; `src/compiler/air.c` is back under the 600 LOC split-review threshold and keeps synthesis/drift ownership focused.
  - 2026-04-29 AIR await-boundary closure: `await` is now a synthesized AIR `parallel` boundary source, not just a recursive operand walk. Strict evidence accepts it only when RIR exposes the exact same-AST `AwaitRemote` operation; generic scope-name evidence such as a scope named `await` is rejected. HIR/CFG evidence is still required for implementation-boundary proof. AIR boundary AST traversal moved to `src/compiler/air_boundary_walk.c`; `src/compiler/air_boundary.c` now owns boundary taxonomy/policy only.
  - 2026-04-29 CFG-owned control classifier closure: `mir_cfg_contract_control.h` now has a real include guard and is consumed by both MIR statement population and MIR CFG validation. The duplicated CFG-owned control switch in `mir_stmt_population.h` was removed, so fallback `MIR_INST_STMT` filtering and validator rejection share one classifier.
  - Type-resolution DAG가 아직 semantic source-of-truth가 아니므로 declaration order / module contract / generic consumer path drift 위험이 남아 있다
  - 장기 모듈화 stop condition도 아직 멀다. semantic 800 LOC 초과 `.inc` 조건과 runtime/codegen/compiler 1,000 LOC 초과 `.inc` 조건은 닫혔지만, 여러 split은 아직 include-order 보존 상태라 실제 owner/TU extraction 부채가 남아 있다
  - 따라서 공식 진행률은 “기능 표면 성숙도”가 아니라 “베타 신뢰도 readiness” 기준으로 약 60%로 본다

## Beta taxonomy freeze: core / foundation / style

베타 기준은 이제 기능 나열이 아니라 언어 정체성 기준으로 나눈다.

- Core language: `intent`, `world`, `zone`, `subject`, `relation`, `effect`, `projection`, `authority`, `handoff`, runtime observability, anchored ownership boundary, generic contract system, module visibility/export contract, `parallel`.
- Generic contract는 core다. exact/ability/multi-bound/default type arg actual resolution은 FP/OOP 편의가 아니라 domain contract를 표현하는 타입 언어다.
- Foundation layer: primitive values, `func`, `let`, control flow, callable/lambda baseline, `Option`/`Result`, stable collections, core 실행에 필요한 runtime ABI.
- Style / compatibility surface: OOP convenience, FP combinator libraries, app infra, richer async helpers.
- Execution family split: `parallel`은 core execution primitive이고, `spawn`/`async`/`await`/`select`/`channel`/cancel은 그 아래 execution family다. fiber/coroutine은 language core가 아니라 runtime scheduling/suspension mechanism이다.
- Accelerator split: AI-first/GPU 방향은 `pgy.accel.spray` 논리 모듈로 예약한다. 이는 `parallel` / ownership / module visibility 위에 올라가는 accelerator library/runtime 축이며 core keyword 확장이 아니다.
- Render split: Skia/shader/render graph 방향은 `pgy.render.skia` 논리 모듈로 예약한다. renderer/shader는 core keyword가 아니라 Spray/Execution 위의 생태계 모듈이다.
- Compatibility split: OOP/FP/DOP는 각각 `pgy.compat.oop`, `pgy.compat.fp`, `pgy.compat.dop`로 분리한다. 기존 언어 스타일을 수용하되 core identity로 설명하지 않는다.
- FP compatibility update: Zig `comptime`-style type-level computation,
  user-customizable compile-time errors, and Sbv-style symbolic execution DSLs
  are tracked as post-beta `pgy.compat.fp` research/module work, not beta core
  language work.
- Interop split: 외부 언어 연동(JVM 캐스팅/JNI 브릿지, Python C-API 등)은 `pgy.interop.*` 생태계 모듈로 분류하며, 베타 마일스톤에서는 완전히 제외(Out of Beta)한다.

업데이트 정책:

- `pgy.core`는 가장 자주 개선하되 가장 작고 강하게 검증한다.
- `pgy.foundation`은 core보다 느리게 움직이며 ABI/backend parity를 깨지 않는다.
- `pgy.accel.spray`, `pgy.render.skia`, `pgy.compat.*`, `pgy.std.*`, `pgy.kit.*`는 모듈 생태계로 진화한다. 빠른 실험은 허용하지만 core keyword를 늘리지 않는다.

실행 규칙:

- B0 blocker는 `core + foundation stable subset`에만 붙인다.
- `pgy.fp`식 Functor/HKT 추상화, class-heavy OOP 확장, coroutine/fiber 고도화는 beta identity blocker가 아니다.
- `pgy.accel.spray`는 post-beta design surface다. 베타 전에는 새 GPU 키워드나 backend-specific CUDA/ROCm/Metal 문법을 열지 않고, module boundary와 ownership 원칙만 고정한다.
- `pgy.render.skia`와 `pgy.compat.dop`도 post-beta design surface다. 베타 전에는 shader/layout keyword를 열지 않고 module boundary만 고정한다.
- 단, `parallel`은 core이므로 slot/resource/effect conflict, cancellation/fairness, C/LLVM lowering parity는 beta 품질 기준으로 계속 관리한다.
- Source of truth: `docs/99_language_module_taxonomy.md`
- Machine-readable manifest: `docs/language_module_manifest.json`
- Representative case tags: `docs/language_module_cases.json`
- Drift gate: `make module-taxonomy-test-smoke`
- Parallel core/execution split gate: `make parallel-core-contract-test-smoke`
- Operational beta checklist: `docs/100_beta_readiness_checklist.md`

## Formal semantics / mathematical proof obligations

베타는 “테스트가 통과한다”만으로 닫히지 않는다. stable subset마다 타입 보존, 진행, ownership safety, authority soundness, projection freshness, DAG soundness, module visibility non-interference, backend parity 같은 수학적 불변식이 문서화되어야 한다.

- Source of truth: `docs/semantics/`
- Stable index: `docs/102_formal_semantics_and_proof_obligations.md`
- Drift gate: `make formal-semantics-test-smoke`
- 상태: `IN PROGRESS / BLOCKER-DOC`
- 베타 기준:
  - [x] 수학 library 문서(`docs/45_math_layer_design.md`)와 언어 의미론 증명 문서를 분리한다.
  - [x] stable beta subset의 semantic domain, judgment, theorem/proof-obligation vocabulary를 고정한다.
  - [ ] B0 항목마다 theorem statement + current regression evidence + remaining proof obligation을 최신 코드 상태와 맞춘다.
  - [ ] runtime propagation, DAG, MIR declaration inventory, ABI ownership, C/LLVM parity의 남은 blocker를 proof obligation으로 추적한다.
  - [ ] beta 문구에서 Lean/Coq/기계증명 완료처럼 보이는 표현을 금지한다. 기계증명은 별도 executable model 또는 proof assistant artifact가 생기기 전까지 post-beta/v1 hardening으로 둔다.
  - [~] **[NEW]** Runtime panic / unwinding model (abort vs unwind)의 정책 명시 및 C/LLVM backend parity 증명 추가. Panic class vocabulary와 released-slot / invalid-secure-token / double-release / device-slot / out-of-bounds / authority-mismatch / OOM / divide-by-zero / internal-invariant hard-fail contract는 `src/runtime/pgy_runtime_panic_contract.h`, `make runtime-panic-contract-test-smoke`, `make runtime-panic-abi-test-smoke`, `make runtime-panic-codegen-test-smoke`로 고정했다. Generated C/LLVM `Array<T>`/`Slice<T>` indexing, temporary function-return indexing, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access와 `Unwrap(Err)` / `UnwrapOption(None)` misuse도 checked runtime helper / panic contract로 고정했다. 남은 것은 새 panic class가 추가될 때마다 같은 executable parity gate를 요구하는 것이다.
  - [~] **[NEW]** Secure slot 및 authority token의 위변조 불가능성(Unforgeability) 형식 불변성(Formal Invariants) 문서화. Secure slot invalid-token/denied-capability export path는 silent fallback에서 panic contract로 이동했다.
  - [ ] **[NEW]** Intent 시스템의 Rollback/Cleanup 보장에 대한 Formal Closure (상태 기계 증명) 문서화.

운영 규칙:

- 테스트/스모크/백엔드 비교는 proof evidence이지 proof 자체가 아니다.
- undocumented mathematical assumption이 필요한 surface는 stable이 아니라 `IN PROGRESS`, `explicit reject`, 또는 `OUT OF BETA`로 내려야 한다.
- FP functor/HKT, full ownership, full quantum, GPU/Spray, Skia/render graph는 현재 beta proof scope 밖이다.

## Missing beta gate audit

현재 strict beta 기준에서는 다음 항목을 별도 gate로 본다. 이 항목들은 기능 확장이 아니라 이미 있는 core/runtime/tooling 표면의 신뢰도 계약이다.

- [~] Runtime panic / unwinding model: OOM, divide-by-zero, out-of-bounds, slot violation, token mismatch, authority mismatch, invariant break의 abort/unwind/recoverable 정책을 `Runtime Panic Parity` proof obligation으로 올렸다. `src/runtime/pgy_runtime_panic_contract.h`가 panic class vocabulary를 소유하고, inline/exported typed slot read/write/release는 released-slot 및 double-release에서 더 이상 기본값/no-op로 빠지지 않는다. `make runtime-panic-abi-test-smoke`가 released-slot, invalid-secure-token, double-release, device-slot, out-of-bounds, authority-mismatch, OOM, divide-by-zero executable evidence를 제공한다. `make runtime-panic-codegen-test-smoke`는 generated C/LLVM divide/modulo-by-zero와 `Array<T>`/`Slice<T>` index, temporary function-return index, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access, `Unwrap(Err)`, `UnwrapOption(None)` parity를 검증한다. 남은 것은 새 hard-fail class가 추가될 때마다 같은 executable parity gate를 요구하는 것이다.
- [~] Secure slot / authority secret invariant: token unforgeability, secure-slot mismatch denial, authority token non-forgeability, authority transfer single-owner invariant, runtime snapshot secret non-exposure를 `Secure Token Unforgeability` / `Authority Transfer Single-Owner` proof obligation으로 올렸다. inline/exported secure slot read/write/release invalid-token 및 denied-capability path는 `PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN`로 고정했고 secure-slot double-release도 `PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE`로 고정했다. `make runtime-panic-abi-test-smoke`가 invalid-token/double-release executable evidence를 제공한다. authority-token mismatch는 `authority-token-mismatch` runtime code/reason, `make test-security`, `authority_failure_abi`, `authority_failure_surface`로 C/LLVM parity regression까지 닫았다. unsupported authority-token transport는 channel send/receive/helper/close, cancellation payload, direct named `spawn`에서 explicit reject로 닫았다. 남은 것은 richer domain-boundary denial이다.
- [ ] Intent formal closure: step ordering, compensation/rollback/invalidation, effect propagation, observability ABI stability를 beta-stable contract로 고정한다.
- [ ] Zone/world/authority/handoff formal closure: zone generation, world embedding, handoff frontier, projection freshness, authority rejection query surface를 beta-stable contract로 고정한다.
- [ ] Diagnostic quality gate: 모든 user-facing error가 severity, stable code, source span when available, `Reason:`, `Fix:`를 갖도록 품질 기준을 registry smoke와 별도 gate로 둔다.
  - 진행: intent clause explicit reject 중 `spawn`/channel control-transfer AST가 parser source span을 보존하도록 고쳤고, `make diagnostics-json-test-smoke`가 `on: spawn ...`와 `on: ch <- value`의 `PGY_SEM_INTENT_STEP_INVALID` JSON line/column + `cause_ir` + `fix_source`를 고정한다.
- [ ] Cross-platform CI matrix: Linux/WSL, Windows native/MSYS2/MinGW, macOS의 support level을 stable/experimental/out-of-beta로 명시한다.
  - 진행: Windows LLVM support detection은 executable `llvm-config --libs core` evidence가 있을 때만 `WINDOWS_LLVM_READY=1`이 되도록 좁혔다. `C:/Program Files/LLVM/lib` 같은 library folder 존재만으로 LLVM smoke/backend-compare를 실행하지 않는다. 현재 beta 계약은 Linux C+LLVM, Windows C-only이며 Windows LLVM은 실제 MSYS2 runner green evidence가 생길 때만 승격한다.
  - 진행: README support matrix에 macOS는 dedicated runner/support contract가 생길 때까지 out-of-beta로 명시했다.
- [~] Beta stable subset definition: keyword, syntax, API, AST-visible shape, runtime ABI, backend parity 범위를 `docs/107_beta_stable_subset.md`에서 freeze한다. 남은 일은 이 문서의 각 stable 항목을 해당 semantic/runtime/C/LLVM regression row와 1:1로 연결하는 것이다.
- [~] Stdlib beta freeze list: stable/experimental/out-of-beta API와 breaking-change policy를 명시한다.
  - 진행: `docs/108_stdlib_beta_freeze.md`가 builtin stdlib, stable `use` modules, known experimental modules, out-of-beta ecosystem work를 분리한다. `make stdlib-test-smoke`가 builtin stdlib probe와 stable `use` module probe를 C/LLVM 양쪽에서 고정한다. 남은 일은 third-party package/version/supply-chain policy다.
- [~] Tooling conformance: LSP/fmt/debugger의 beta-stable 범위를 명시한다.
  - 진행: `make tooling-conformance-test-smoke`가 formatter idempotence/compile smoke, LSP initialize/hover/completion capability, debugger CLI parse+semantic+quit path를 executable gate로 고정한다. DAP, binary breakpoint, variable watch, rich refactor, multi-file workspace LSP는 아직 beta-stable tooling subset이 아니다.
- [~] Package/module resolver surface: manifest, version resolution, import path, supply-chain integrity를 stable/experimental/out-of-beta로 분류한다.
  - 진행: `docs/109_package_module_resolver_contract.md`가 beta-stable module surface를 `import "relative/path.pgy";`, importing-file-relative resolution, namespace/export visibility, circular import rejection으로 고정했다. package surface는 `pgy init <name>` scaffolding만 stable이다.
  - 진행: `pgy install`은 더 이상 소스 파일 경로로 오인되지 않고 explicit out-of-beta rejection을 낸다. `make package-module-resolver-test-smoke`가 doc contract, `pgy init`, `pgy install` reject, missing import JSON, circular import JSON을 고정한다.
  - 남음: dependency version solving, lockfile, registry, checksum/signature verification, remote import, supply-chain integrity는 beta 이후 resolver/package-manager track으로 유지한다.
- [~] Test quality gate: pre-beta mandatory suite, fuzz/property status, coverage/perf baseline을 추적한다.
  - 진행: `docs/111_beta_test_suite_freeze.md`가 mandatory pre-beta gates, platform gates, fuzz/property/coverage non-claims, regression policy를 freeze했다. `make beta-test-suite-freeze-test-smoke`가 freeze doc과 Makefile target 존재를 검사한다.
  - 남음: 실제 fuzz corpus, property-based generator, coverage percentage threshold는 beta 이후 품질 트랙으로 유지한다. 현재 beta gate는 named stable-surface coverage다.
- [~] Observability/tracing schema: event schema, intent history, authority failure state, runtime registry, trace format version을 고정한다.
  - 진행: `docs/112_observability_trace_schema.md`가 beta-stable schema를 `IntentLast*`, `IntentHistory*`, `IntentActive*`, `IntentRecent*`, authority failure snapshot(`ok/zone/participant/code/reason`), runtime-borrowed string ABI, C/LLVM identical trace output으로 고정했다.
  - 진행: `make observability-schema-test-smoke`가 `intent_trace_abi`, `intent_recent_abi`, `intent_active_abi`, `intent_failure_abi`, `authority_failure_abi`를 C/LLVM 양쪽에서 expected stdout과 비교한다.
  - 남음: general event streaming, structured JSON trace export, distributed trace correlation, user-code registry hooks, stable binary trace format, richer multi-instance timeline query는 beta 이후로 유지한다.
- [~] Memory/concurrency model: `parallel`, task, channel, cancellation, visibility/happens-before 최소 계약을 문서화한다.
  - 진행: `docs/113_memory_concurrency_model.md`가 beta-stable happens-before, channel, cancellation, explicit out-of-beta memory model 범위를 고정했다. `parallel` join visibility, shared `ref`/`ref` 허용, `ref`/`own` 및 `own`/`own` task-boundary reject, copy-only non-blocking receive/cancel/close를 stable contract로 묶었다.
  - 진행: `make memory-concurrency-model-test-smoke`가 `parallel-core-contract-test-smoke`와 targeted C/LLVM backend compare(`parallel_channel_sum`, `parallel_channel_dual`, `triple_paradigm`)를 실행한다.
  - 남음: full weak-memory ordering, user-selectable memory order, scheduler fairness guarantee, lock-free correctness, anonymous async closure capture/lifetime, cross-thread `Arc<T>` / `Send` / `Sync` trait system은 beta 이후로 유지한다.
- [~] String/unicode policy: normalization, comparison, locale, escape handling, unsupported policy를 명시한다.
  - 진행: `docs/110_string_unicode_policy.md`가 UTF-8 string payload preservation, byte-length `StringLength`, byte-exact/normalization-blind equality/search를 beta-stable로 고정했다.
  - 진행: Unicode identifiers, normalization, locale-sensitive collation/case folding, grapheme iteration, display width, mixed-encoding source files는 explicit out-of-beta로 고정했다. `make unicode-policy-test-smoke`가 C/LLVM UTF-8 string execution과 Unicode identifier reject를 검증한다.
  - 남음: full Unicode text model을 도입하려면 post-beta에 scalar/grapheme/locale vocabulary와 별도 stdlib text module을 설계한다.

Checklist source of truth:

- `docs/100_beta_readiness_checklist.md`
- AIR source of truth: `docs/104_air_compiler_architecture.md`
- Drift gate: `make beta-readiness-checklist-test-smoke`
- AIR drift gate: `make air-drift-test-smoke`
- AIR backend non-impact gate: `make air-backend-nonimpact-test-smoke`
- AIR full backend non-impact hardening: `make air-backend-nonimpact-full-test-smoke`
- AIR strict backend execution parity: `make air-strict-backend-compare-test-smoke`

## 구조/운영 폐인 포인트 보드 (2026-04-20)

이 섹션은 기능 backlog가 아니라, 실제 작업 효율과 베타 신뢰도를 계속 깎는 구조 debt / 운영 pain point를 고정한다.

우선순위 제안:
- `P0`: function/action/intent body CFG + dataflow를 semantic source-of-truth로 승격
- `P1`: `.inc` 분할을 실제 `.c`/`.h` 모듈로 전환
- `P2`: hint namespace (`code` / `cause_ir` / `fix_source`)를 레지스트리 기반으로 고정
- `P3`: type-category vocabulary를 2-3층으로 압축
- `P4`: 빌드/샌드박스/중간-stage JSON/artifact 문제를 공식 경로 기준으로 정리
- `P9`: arena 패턴을 scratch/result lifetime 기준으로 명시 도입
- `P9b`: repeated `Slot` / `SecureSlot` hot-loop access는 Pin/Lease 문서 기준으로 분리한다. 기본 path는 매 접근 검증이고, fast path는 scope-entry capability lease + automatic unpin cleanup이어야 한다. Runtime ABI baseline은 `PgyPinnedView` / `PergyraSlotPin` / `PergyraSlotUnpin` + `make test-security` 회귀로 시작했고, plain token-bearing pin rejection, scope release while pinned, TTL cleanup skip while pinned, secure invalid-token/capability rejection, concurrent secure write rejection, release-after-unpin persistence를 닫았다. Generated inline `PgySlot_*` / `PgySecureSlot_*` ABI now also has typed `PgyPinnedSlotView_*` / `PgyPinnedSecureSlotView_*` wrappers plus LLVM-linkable `pgy_pin_read_*`, `pgy_pin_write_*`, and `pgy_unpin_*` exports; `make test-memory` covers occupied/token validation, cleanup helper behavior, double-unpin hard-fail, and secure invalid-token pin rejection. C source-block emission now lowers pin blocks to typed wrapper variables with GCC cleanup hooks through `src/codegen/transpiler_block_emit.h`, while `src/runtime/pgy_runtime_plain_slot_inline.h` owns the plain Slot wrapper macro under the 600 LOC split threshold. C and LLVM MIR emission now consume MIR pin-region/view-alias metadata on successor/return exits, emitting explicit typed `pgy_pin_*` / `pgy_unpin_*` calls before control leaves the pin region; `tests/compare_backends.sh` covers plain read, secure read, plain write, secure write, mixed plain+secure source-level pin blocks, normal successor cleanup, direct return cleanup, conditional branch-to-return cleanup, loop `break`/`continue` cleanup, and secure boundary-slot parameter pinning. Source syntax `pin slot as view: ReadView<T>|WriteView<T> { ... }`는 parser/semantic surface로 활성화됐고, AST `Pin Block` metadata, HIR/MIR pin-region metadata, MIR `pin-unpin-cleanup-edge` cleanup fact까지 내려간다. MIR validator now rejects reachable pin-region blocks that lack the matching `pin-unpin-cleanup-edge` fact for source slot/view/access mode, and `src/test_mir.c` has a negative corruption regression for that contract. Pin/Lease semantic diagnostic vocabulary는 `PGY_SEM_PIN_ESCAPE`, `PGY_SEM_PIN_PARALLEL_CONFLICT`, `PGY_SEM_PIN_AWAIT_BOUNDARY`, `PGY_SEM_PIN_QUBIT_REJECT`, `PGY_SEM_PIN_TOKEN_INVALID`로 registry/docs에 고정했고 `make diagnostic-registry-test-smoke`, `make beta-readiness-checklist-test-smoke`, `make diagnostics-json-test-smoke`가 drift를 막는다. Existing `ViewRead(...)` / `ViewWrite(...)` semantic surface now enforces `WriteView<T>` exclusive access for the same source slot while keeping shared `ReadView<T>` / `ReadView<T>` accepted. It also emits pin-specific diagnostics for return escape, await boundary, parallel boundary/acquisition, defer cleanup registration, and QubitSlot rejection, and `make diagnostics-json-test-smoke` verifies their CLI JSON route. Generic ownership baseline은 unresolved `TYPE_KIND_GENERIC`을 `BORROW_TRACKED`로 분류해 generic `own/ref`가 조용히 copy-only로 통과하지 못하게 막는다. 남은 것은 secure-token source diagnostic, exceptional/cancellation all-exit proof expansion, and richer invalid-token source provenance. Source of truth: `docs/74_slot_pinning_caching.md`
- `P9c`: `Rc<T>` / `Weak<T>` 최소 subset은 beta-stable로 닫았다. 범위는 single-thread `Int|Long|Float|Double|Bool|String` payload, explicit lifecycle builtin(`RcNew`, `RcClone`, `RcGet`, `RcDrop`, `RcDowngrade`, `WeakUpgrade`, `WeakDrop`), resolver metadata, semantic builtin typing, C runtime/emitter, LLVM runtime export/lowering, ABI layout smoke, C/LLVM lifecycle backend-compare다. 범위 밖 payload는 backend fallback이 아니라 semantic explicit reject다. `Arc<T>`, cross-thread shared ownership, generic/object payload 확장, default ARC는 beta 밖이다. Source of truth: `docs/100_beta_readiness_checklist.md`, `docs/106_ownership_model_comparison.md`, `src/runtime/pgy_abi_spec.h`
- `P10`: 모듈화/전파 고도화의 compile/runtime 속도 회귀를 별도 baseline으로 추적

### P0. Function CFG / Body Dataflow Closure

판정: `BLOCKER`

핵심 정리:

- CFG가 없는 상태는 아니다. HIR는 function CFG v0, predecessor/reachability, dominator/frontier, loop depth, phi candidate skeleton을 가진다.
- MIR도 HIR CFG와 RIR op를 묶어 routine/block/instruction/cleanup block, SSA version map, def/use, cleanup/rollback/invalidation exceptional CFG, liveness/DCE vertical slice까지 가지고 있다.
- 남은 blocker는 이 CFG/MIR infra를 **함수 본문 의미론의 source-of-truth**로 승격하는 것이다. 현재 body safety의 일부는 여전히 AST/helper traversal, local summary, backend fallback에 기대고 있어 strict beta 기준으로 부족하다.

베타 완료 조건:

- [ ] Function/action/intent body마다 `BasicBlock`, `Edge`, `Terminator`, reachability, exceptional cleanup edge가 semantic pass에서 직접 소비된다.
- [x] 반환형이 있는 routine은 모든 reachable normal path에서 return/value terminator를 가진다는 all-path return 검사를 CFG body summary로 고정한다.
- [~] definite assignment/use-before-init 검사를 CFG dataflow로 이동하고 branch/join/loop widening 진단을 고정한다. stable local `let` 표면은 parser `=` 요구와 `PGY_SEM_UNINIT_LOCAL` backstop으로 봉인됐고, wider delayed-assignment lattice는 아직 열려 있다.
- [~] move/use-after-move, borrow/ref lifetime, boundary escape를 CFG join facts로 계산한다. `QubitSlot` loop break/continue join regression, anchored `Slot<T>` branch/join release-state regression, `own subject` branch/join consumed-state regression, parallel subject transfer join/conflict regression, parallel `ref`+`own` boundary conflict regression, parallel `ref`+`ref` shared-read acceptance regression, direct named-call `spawn ref` ownership-boundary rejection regression, anonymous async spawn explicit reject regression은 닫혔고, closure/lambda/general longer-lived borrow lifetime은 남아 있다. `mut ref`/`ref mut` surface가 없으므로 mutable-borrow overlap은 beta-out-of-scope로 봉인한다.
- [~] owned resource drop/cleanup insertion point를 normal return, early return, break/continue, intent cancel/rollback/invalidation edge에서 같은 규칙으로 계산한다. `defer` cleanup terminator와 resource-state snapshot/restore 격리, direct `type_check_statement()` fallback convergence, anchored slot branch/join state tracking은 닫혔다. MIR validator now also rejects reachable pin-region blocks that lack the matching `pin-unpin-cleanup-edge` fact, so pin unpin cleanup is no longer just a generated convention. 남은 것은 full drop insertion/validation과 exceptional/cancellation all-exit proof expansion이다.
- [ ] zone/effect/relation transition facts를 path-sensitive summary로 올려 branch/join/handoff에서 stale state와 conflict를 같은 vocabulary로 진단한다.
- [~] `parallel`/channel/task boundary에서 moved value, borrowed reference, authority-bearing token, cancellation cleanup fact를 CFG summary로 검증한다. parallel task-local terminator isolation, moved/released resource/boundary join, duplicate resource/boundary consume diagnostic, `ref`+`own` boundary conflict, blocking channel-send resource consume/join, direct named-call `spawn ref` ownership-boundary rejection, direct named-call `spawn Token<T>` authority-boundary rejection, anonymous async spawn explicit reject, `SendTimeout`/`TrySendStatus`/`SendTimeoutStatus` transport rejection, `TryRecv`/`RecvTimeout` movable receive explicit reject, authority `Token<T>` channel helper rejection, copy-only cancellation payload reject, copy-only channel close는 닫혔고, broader channel receive/backpressure summary, closure/lambda/general borrowed-reference task lifetime, cancellation cleanup fact는 남아 있다.
- [~] Interprocedural body summary를 고정한다: `may_return`, `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`, `effects`, `requires_zone`, `spawns_task`, `sends_channel`. 1차 구조로 function type의 `body_summary_mask`와 semantic recorder는 들어갔다. direct function call은 callee summary 중 caller-relevant transitive facts를 소비하고 declaration-known `own/ref` parameter boundary facts도 기록한다. method/host call도 같은 declaration-known summary facts를 기록한다. lambda body summary는 lambda function type에 격리되어 outer routine으로 새지 않고, function-typed lambda binding 호출은 같은 callee-summary path로 전파된다. 남은 것은 intent/helper call까지 넓히고 zone/effect/runtime propagation과 C/LLVM lowering이 이 summary bit를 직접 소비하게 만드는 일이다.
- [ ] 진단은 block/path provenance를 포함한다: source path, branch/join edge, previous state, Reason, Fix.
- [ ] MIR/C/LLVM lowering은 같은 CFG/dataflow facts를 소비하고, frozen subset parity regression으로 묶는다.

실행 순서:

1. 현재 HIR/MIR CFG fact inventory와 semantic 소비 지점을 표로 만든다.
2. `--hir-cfg`, `--mir`, RIR flow-block dump를 묶는 smoke를 추가해 CFG fact drift를 막는다.
3. all-path return + reachability + definite assignment를 CFG 기반으로 먼저 승격한다.
4. ownership move/borrow/drop cleanup을 CFG dataflow로 이동한다.
5. zone/effect/relation transition, handoff frontier, projection freshness를 body CFG summary와 runtime propagation scheduler에 연결한다.
6. parallel/channel/task boundary summary를 추가하고 C/LLVM backend compare에 frozen cases를 넣는다.

검증 목표:

- `make test-semantic`
- `make ir-pipeline-test-smoke`
- `make type-resolution-dag-test-smoke`
- `make cfg-body-dataflow-test-smoke`
- `make llvm-test-backend-compare`

Source of truth:

- `docs/103_cfg_body_dataflow_need.md`

### P1. `.inc` 스파게티를 실제 모듈로 절단

- 문제:
  - 현재 `type_checker.c` 및 transpiler/LLVM 일부는 “모듈화”가 아니라 “파일 분할된 단일 translation unit”에 가깝다
  - IDE jump/symbol lookup/forward decl 순서 관리가 모두 수동
  - formatter/linter/외부 edit가 include 순서/파일 갱신 타이밍에 민감하게 깨진다
- 영향:
  - 대형 수정 시 edit conflict / implicit declaration / include ordering failure가 반복됨
  - ownership/generic/provenance 같은 횡단 작업이 불필요하게 느려진다
- 기본 방침:
  - 우선 `semantic/type_checker_*`에서 ownership / generic / module-contract / diagnostics 축부터 실제 `.c`/`.h` export 구조로 절단
  - declaration-side MIR-only hot path도 helper family를 `.c` 경계로 분리
  - 장기 목표선은 `docs/92_inc_split_roadmap.md`의 Target State A-D로 고정한다
  - stop condition: semantic에는 800 LOC 초과 `.inc` 없음, codegen/runtime에는 1,000 LOC 초과 `.inc` 없음, `type_checker.c`는 orchestration-only, backend declaration path는 dedicated inventory reader 또는 hard error만 허용
  - speed stop condition: `test-abi-perf`와 `perf-summary` baseline을 유지하고, 모듈화 slice 후 worst-case compile time이 2배 이상 튀면 회귀 후보로 기록
  - `.inc`는 generated table / local macro table / private test fixture 같은 제한 용도로만 남긴다
- 준비 작업:
  - [ ] `type_checker`를 최소 5축으로 절단
    - [x] diagnostic emission/snapshot: `type_checker_diag.c`
    - [x] ownership classification: `type_checker_ownership_classify.c`
    - [x] channel transport validator: `type_checker_channel_transport.c`
    - [x] ownership diagnostics/consumers: `type_checker_ownership_diag.c`
    - [x] generic contract diagnostics: `type_checker_generic_diag.c`
    - [x] ability reference formatting seam: `type_checker_ability_ref.c`
    - [x] stdlib use validator seam: `type_checker_stdlib_use.c`
    - [x] module contract diagnostic seam: `type_checker_module_contract_diag.c`
    - [x] ability fields validator seam: `type_checker_ability_fields.c`
    - [x] ability matcher / subject ability lookup seam: `type_checker_ability_match.c`
    - [x] ability where-bound validator seam: `type_checker_ability_where.c`
    - generic consumer pipeline
    - [x] module contract / authority consumer: `type_checker_module_contract.c`
  - 진행: ownership 공용 enum/entrypoint를 `type_checker_ownership_internal.h`로 분리 시작
  - 진행: ownership diagnostics forward declaration도 `type_checker_ownership_diag_internal.h`로 분리 시작
  - 진행: ownership escape diagnostic renderer/helper family는 `type_checker_ownership_diag.c`로 실제 TU 분리 완료
  - 진행: ownership support helper(`semantic_assignment_target_path`, `semantic_borrowed_boundary_root_name`)도 `type_checker_ownership_support_internal.h`로 분리 시작
  - 진행: ownership consumer seam(`return` / `assign` / `call`)도 `type_checker_ownership_consumers_internal.h`로 분리 시작
  - 진행: `param_summary`도 raw include block이 아니라 `semantic_check_param_summary_escapes(...)` consumer helper로 승격
  - 진행: channel transport seam도 `type_checker_channel_transport_internal.h`로 분리 시작
  - 진행: channel transport validator/reporters는 `type_checker_channel_transport.c`로 실제 TU 분리 완료
  - 진행: high-arity generic mismatch helper도 `type_checker_generic_diag.c`로 실제 TU 분리 완료
  - 진행: module contract consumer 선행 seam인 ability reference display/name/signature helper는 `type_checker_ability_ref.c`로 실제 TU 분리 완료
  - 진행: stdlib use validator는 `type_checker_stdlib_use.c`로 실제 TU 분리 완료
  - 진행: subject ability mismatch diagnostic은 `type_checker_module_contract_diag.c`로 실제 TU 분리 완료
  - 진행: ability `fields` validator는 `type_checker_ability_fields.c`로 실제 TU 분리 완료
  - 진행: `find_type_decl_by_name`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격
  - 진행: ability ref matching / role ability lookup / subject ability lookup은 `type_checker_ability_match.c`로 실제 TU 분리 완료
  - 진행: `find_ability_decl_by_name` / `collect_effective_generic_arg_nodes`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격
  - 진행: ability where-bound consumer validation은 `type_checker_ability_where.c`로 실제 TU 분리 완료
  - 진행: `format_type_constraint_bounds`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격 후 별도 TU로 분리
  - 진행: `semantic_type_resolution_record_type_ref_dependency`는 graph core TU로 이동해 include-order static helper 의존을 제거
  - 진행: `semantic_type_resolution_collect_type_refs`는 `type_checker_resolution_graph_collect.c`로 이동해 DAG inventory collector의 첫 실제 TU seam을 만들었다
  - 진행: generic contract inventory / string dependency / required ability collector helpers도 `type_checker_resolution_graph_collect.c`로 이동
  - 진행: top-level declaration graph registration도 `type_checker_resolution_graph_collect.c`로 이동해 inventory pass의 bootstrap helper debt를 더 줄였다
  - 진행: local-contract graph node/dependency + zone/world/projection label formatters는 `type_checker_resolution_graph_labels.c`로 이동해 graph inventory `.inc`를 1,835 LOC까지 축소했다
  - 진행: projection source resolver는 `type_checker_resolution_graph_domain.c`로 이동하고 `find_zone_domain_slot`을 internal API로 승격해 graph/domain split 선행 seam을 만들었다
  - 진행: event declaration precollector는 `type_checker_resolution_graph_decl.c`로 이동해 declaration-kind collector 분리도 시작
  - 진행: enum declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고 `semantic_stage_method_array`를 internal API로 승격해 inventory `.inc`를 1,765 LOC까지 축소
  - 진행: ability declaration precollector와 action-contract precollector도 `type_checker_resolution_graph_decl.c`로 이동해 inventory `.inc`를 1,648 LOC까지 축소
  - 진행: role/class/party/roster declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고, relation/effect domain inventory precollector는 `type_checker_resolution_graph_domain.c`로 이동해 inventory `.inc`를 1,299 LOC까지 축소
  - 진행: intent declaration precollector는 `type_checker_resolution_graph_decl.c`로, world inventory precollector는 `type_checker_resolution_graph_world.c`로 이동해 inventory `.inc`를 870 LOC까지 축소
  - 진행: zone refresh projection field-map DAG collector는 `type_checker_resolution_graph_zone.c`로 이동했고, graph inventory body는 `type_checker_resolution_graph_inventory.c`로 승격했다. `type_checker_resolution_graph_inventory.inc`는 제거되어 DAG inventory include-order debt가 닫혔다
  - 진행: projection builtin target-field resolver는 recursive fallback 대신 DAG metadata lookup-only seam으로 낮췄다. projection source/target mismatch 진단은 projection validator가 소유하고, target field type materialization은 DAG metadata가 소유한다. fallback seam cap은 31에서 30으로 내려갔다. 이후 type graph precollect를 top-level symbol pass 앞에 배치하고 `program_resolve_type_quiet(...)`를 metadata lookup-only로 낮춰 event/function placeholder가 recursive fallback 없이 DAG metadata를 쓰게 했다. fallback seam cap은 30에서 29로 내려갔다. domain query projection source-field resolver도 class/vessel field DAG metadata lookup-only로 낮춰 cap은 28로 내려갔다. party/roster shared-field resolver도 declaration metadata lookup-only로 낮춰 cap은 26으로 내려갔다. ability abstract method signature resolver와 role host-type resolver도 lookup-only로 낮춰 cap은 24로 내려갔다. function/action body expression/lambda/event handler precollect 확장 후 event/lambda handler resolver도 lookup-only로 낮춰 cap은 23으로 내려갔다. body flow resolver도 graph metadata lookup-only로 낮춰 cap은 22로 내려갔다. type-alias statement resolver도 DAG metadata lookup-only로 낮춰 cap은 21로 내려갔다. `world_decl` lookup-only 전환은 subject/zone nominal materialization이 아직 부족해 semantic 77개 실패를 만들었으므로 보류했다
  - 진행: world/zone local-contract stage replay는 `type_checker_resolution_stage_domain.c`로 이동했고, top-level DAG stage replay는 `type_checker_resolution_stage.c`로 승격해 `type_checker_resolution_stage.inc`를 제거
  - 진행: `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`는 standalone TU로 빌드되며 hidden include-order helper 의존을 internal/header 계약으로 승격
  - 진행: `type_checker_intent_decl.c` standalone TU 승격 중 드러난 implicit helper dependency를 internal/header 계약으로 승격하고, `-Werror=implicit-function-declaration -Werror=implicit-int`를 기본 CFLAGS로 고정해 같은 종류의 C 모듈화 버그를 빌드 단계에서 차단
  - 진행: `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`도 hard implicit-declaration CFLAGS 아래에서 빌드되도록 helper/header 의존을 명시
  - 진행: `type_checker_class_decl.c`가 class/extern declaration checking을 소유하고, `type_checker_program.c`가 top-level semantic orchestration을 소유한다. 관련 graph/worklist/effect/stats helper를 internal API로 승격해 `type_checker_program.inc`를 624 LOC까지 축소
  - 진행: `type_checker_builtins_projection.c`가 `ToObject` / `ToTObject` semantic projection checker를 소유하고, `type_checker_builtins_nominal.inc`를 659 LOC까지 축소
  - 진행: expression operator/indexed-access checker를 `type_checker_expr_ops.c`로 분리하고, static member path / consumed-boundary helper를 `type_checker_expr_names.c`로 이동했다. `type_checker_expr.inc`는 758 LOC, `type_checker_helpers_late.inc`는 773 LOC가 되어 둘 다 semantic 800 LOC stop condition 아래로 내려갔다
  - 진행: event declaration/subscription/invoke semantic은 `type_checker_event.c`로 승격했고, QubitSlot compile-time state / entangle pool / movable-resource-use validation은 `type_checker_qubit.c`로 승격했다. `type_checker.c`는 481 LOC로 내려가 600 LOC 이하 stop condition을 만족한다
  - 진행: domain slot/projection/overlay helper body를 `type_checker_decls_domain_helpers.c`로 승격하고, intent inheritance/derivation helper body를 `type_checker_intent_helpers.c`로 승격했다. `type_checker_decls_domain_helpers.inc`는 제거됐고 `type_checker_decls_a.inc`는 1-line forwarding stub으로 축소
  - 완료: semantic `.inc` 800 LOC stop condition은 `make semantic-inc-size-test-smoke`로 고정. 현재 `src/semantic`에는 800 LOC 초과 `.inc`가 없다
  - 완료: semantic core shape stop condition은 `make semantic-core-shape-test-smoke`로 고정. `type_checker.c <= 600 LOC`, event/qubit owner TU, DAG inventory `.c` ownership을 CI에서 검사한다
  - 진행: C backend MIR inventory/SSA emitter include를 5-line shim + `transpiler_emitters_mir_inventory_intent.inc` / `transpiler_emitters_mir_inventory_ssa_names.inc` / `transpiler_emitters_mir_inventory_ssa_emit.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다
  - 진행: C backend `emit_program(...)` bootstrap은 direct declaration-array reads 대신 `transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view를 소비한다. `make mir-declaration-inventory-test-smoke`가 C/LLVM declaration-side codegen의 raw declaration inventory access를 helper owner로 제한한다
  - 진행: C backend expression emitter include를 7-line shim + `transpiler_expr_emitters_builtins.inc` / `transpiler_expr_emitters_call_a.inc` / `transpiler_expr_emitters_call_b.inc` / `transpiler_expr_emitters_members.inc` / `transpiler_expr_emitters_tail.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다. 검증: `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - 진행: LLVM call emitter include를 17-line shim + `llvm_expr_call_constructors.inc` / `llvm_expr_call_arrays.inc` / `llvm_expr_call_collections_base.inc` / `llvm_expr_call_domain_queries.inc` / `llvm_expr_call_events.inc` / `llvm_expr_call_intent_observability.inc` / `llvm_expr_call_log.inc` / `llvm_expr_call_math.inc` / `llvm_expr_call_result_option.inc` / `llvm_expr_call_slots.inc` / `llvm_expr_call_task_channel.inc` / `llvm_expr_calls_part_a.inc` / `llvm_expr_calls_part_b.inc` / `llvm_expr_calls_part_c.inc` / `llvm_expr_calls_part_d.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다. enum/class constructor, array builtin, `ListNew`/`Set*` base collection, domain query builtin, event invocation, intent observability, log, scalar math, Result/Option, slot/device-slot builtin, task/channel lowering은 `llvm_emit_call`에서 분리되어 별도 owner include가 됐다. 검증: `make test-transpile -j2`, `make backend-inc-size-test-smoke`, `make llvm-test-backend-compare -j2`
  - 진행: C backend base emitter B include를 6-line shim + `transpiler_emitters_base_b_part_a.inc` / `transpiler_emitters_base_b_part_b.inc` / `transpiler_emitters_base_b_part_c.inc` / `transpiler_emitters_base_b_part_d.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다. 검증: `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - 완료: Tier 1 runtime/codegen/compiler `.inc > 1000 LOC` gate는 닫힘. `pgy_runtime_part_ba.inc`, `pgy_runtime_lib_part_b.inc`, `transpiler_emitters_base_a.inc`, `transpiler_helpers_core_a.inc`, `transpiler_helpers_core_b.inc`, `transpiler_domain_role.inc`, `llvm_expr_helpers.inc`, `mir_public.inc`, `llvm_expr_call_methods.inc`, `llvm_domain_helpers.inc`를 모두 safe mechanical split으로 1,000 LOC 아래로 낮췄다
  - 완료: `tests/backend_inc_size_smoke.sh` / `make backend-inc-size-test-smoke` 추가. `src/runtime`, `src/codegen`, `src/compiler`의 `.inc <= 1000 LOC`를 CI에서 고정
  - 검증: `make backend-inc-size-test-smoke`, `make test-mir test-transpile test-abi -j2`, `make llvm-test-backend-compare -j2`
  - 진행: `type_checker_helpers_late.c` standalone TU 빌드 중 드러난 call-path helper include-order 의존을 `type_checker_internal.h` prototype과 직접 include 계약으로 고정했다
  - 진행: `type_checker_decls_a.inc -> type_checker_decls_domain_helpers.inc`, `type_checker_decls_intent.inc -> type_checker_world_decl.c`, `type_checker_helpers_effects.inc -> type_checker_helpers_host.inc` 사이 dangling return-type seams 제거
  - 진행: `type_checker_resolution_graph_core.inc` → inventory include 경계의 dangling `static void` seam 2개를 명시 return type으로 정리
  - 진행: `generic_params_required_count`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격
  - 완료: required ability resolver와 action required-ability validator는 `type_checker_module_contract.c`로 실제 TU 분리 완료
  - 완료: `type_checker_module_contracts.inc` 제거. module contract include-order 구조 debt는 닫힘
  - [ ] `.inc` 내부 static helper 중 교차 참조 심한 심볼 목록 작성
  - [x] include-order에 의존하는 implicit declaration 경로 제거를 빌드 계약으로 승격 (`-Werror=implicit-function-declaration`, `-Werror=implicit-int`)
  - [~] declaration-side MIR-only debt는 helper-gated state까지 닫혔다. 남은 단계는 `MIRProgram` 안 AST-shaped declaration inventory를 dedicated declaration metadata model로 분리하는 일이다

  - 진행: ownership return / assignment rebind / array literal store / boundary validation / call argument / destructuring / let-binding / parameter escape-summary consumers는 `.inc`에서 실제 TU로 승격했다. 삭제된 파일: `type_checker_ownership_return.inc`, `type_checker_ownership_assign.inc`, `type_checker_ownership_array_store.inc`, `type_checker_ownership_boundaries.inc`, `type_checker_ownership_call.inc`, `type_checker_ownership_destructure.inc`, `type_checker_ownership_destructure_stmt.inc`, `type_checker_ownership_let.inc`, `type_checker_ownership_let_boundary.inc`, `type_checker_ownership_let_claim.inc`, `type_checker_ownership_let_infer.inc`, `type_checker_ownership_let_slot.inc`, `type_checker_ownership_let_value.inc`, `type_checker_ownership_param_summary.inc`. 현재 `src/semantic/type_checker_ownership_*.inc`는 0개다
  - 원칙 강화: 베타 기준에서는 behavior-owning `.inc`를 beta+1 정리가 아니라 blocker로 본다. generated table / local macro table / private test fixture 외 `.inc`는 owner `.c` 또는 명시적 generated artifact로 옮긴다
  - 원칙 강화: `.inc` 제거 과정에서 여러 behavior family를 하나의 mega-TU로 합치지 않는다. `make semantic-tu-size-test-smoke`가 새 semantic owner TU는 1,000 LOC 이하로 제한하고, 기존 초대형 TU는 개별 cap으로 더 커지지 못하게 막는다
  - 완료: builtin query/slot include-chain seam은 TU owners로 승격됐다. `type_checker_builtins_query*.h`와 `type_checker_builtins_slotops.h`는 declaration-only이고, query/world/channel/domain/slotops/secure-token/builtin-resolve behavior는 named `.c` owner가 소유한다

### P10. 속도 / 빌드 성능 baseline

- 문제:
  - 장기 모듈화가 translation unit 수를 늘리면 incremental build는 좋아질 수 있지만 full build/link 또는 generated backend compile 시간이 튈 수 있다
  - 현재 `test-abi-perf`는 존재하지만 raw log가 길어 worst-case 추적이 어렵다
- 기본 방침:
  - `make test-abi-perf`로 benchmark-only ABI/runtime baseline을 캡처한다
  - `make perf-summary PERF_LOG=<log>`로 C/LLVM compile/run 평균과 worst-case를 요약한다
  - representative case는 `tests/bench_backend.sh <source.pgy> dev`로 C/LLVM wall time + RSS를 직접 확인한다
  - generated/native compile warning은 속도 noise가 아니라 build hygiene bug로 보고 즉시 닫는다
- 현재 baseline (2026-04-24, local WSL):
  - `make test-abi-perf`: 320 passed, 0 failed
  - `perf-summary`: C 32 cases, avg compile 0.569s, max 1.783s (`intent_authority_snapshot_abi`), avg run 0.001s
  - `perf-summary`: LLVM 32 cases, avg compile 0.187s, max 0.251s (`projection_abi`), avg run 0.002s
- 진행: `make perf-contract-test-smoke`가 synthetic `test-abi-perf` log를 통해 `perf_summary` log grammar, C/LLVM case count, average compile/run, worst-case compile/run case selection을 CI에서 고정한다. 이 gate는 baseline 숫자 자체를 고정하지 않고, perf evidence가 machine-readable 상태를 유지하는지 검사한다.
  - representative `relation_effect_propagation/main.pgy`: C dev 1.03s / 46MB, LLVM dev 0.72s / 60MB after `realpath` warning fix
- 진행:
  - [x] `tests/perf_summary.sh` 추가
  - [x] `make perf-summary PERF_LOG=<log>` 추가
  - [x] generated C/LLVM compile path의 POSIX `realpath` implicit declaration 경고 제거
- 남음:
  - [ ] CI에서 benchmark-only 수치를 artifact로 저장할지 결정
  - [ ] release/beta notes에 perf-summary baseline 첨부
  - [ ] worst-case compile 2배 이상 증가 시 regression 후보로 자동 표시

### P2. hint namespace 레지스트리화

- 문제:
  - `cause_ir` / `fix_source` literal이 세션 단위로 계속 늘어나는데 중앙 레지스트리가 없다
  - `docs/72`류 문서는 `code` 위주고, `cause_ir` / `fix_source` variant drift를 강제하지 못한다
- 영향:
  - downstream이 diagnostic routing에 이 값을 쓰기 시작하면 오타/drift가 즉시 breaking change가 된다
- 기본 방침:
  - `code`, `cause_ir`, `fix_source`를 모두 registry/enum-like literal set으로 관리
  - 문서와 코드 리뷰 기준에서 “새 literal 추가 시 registry + docs 동시 갱신”을 강제
- 준비 작업:
  - [x] diagnostic literal registry 초안 추가
    - 완료: `src/semantic/diag_codes.h`가 `PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` registry source of truth로 동작하고 `docs/72_diagnostic_codes.md`가 이를 문서화
  - [x] `cause_ir` / `fix_source` 네이밍 규칙 문서화
    - 완료: `docs/72_diagnostic_codes.md`에 `cause_ir` stage/subsystem/condition 규칙과 `fix_source` source-action token 규칙 고정
  - [x] free-form 문자열 신규 추가 지점에 smoke gate 마련
    - 완료: `tests/diagnostic_registry_smoke.sh` / `make diagnostic-registry-test-smoke`가 semantic diagnostic call-site의 `PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` macro 사용과 diagnostic code 문서 sync를 검사

### P3. 타입/ownership 용어 압축

- 문제:
  - anchored handle / movable resource / subject / subject-host / boundary value / capability-bearing / move token 등 용어가 과다
  - 같은 semantic family가 메시지마다 다른 이름으로 노출된다
- 영향:
  - 사용자도 헷갈리고, 구현자도 메시지/문서/테스트 정렬 시 drift가 난다
- 기본 방침:
  - 사용자-facing 핵심 용어를 2-3층으로 압축
  - 세부 분류는 “X의 하위분류”로만 노출
- 준비 작업:
  - [ ] user-facing canonical vocabulary 정리
  - [ ] diagnostics/README/docs 용어 매핑표 작성
  - [ ] old wording grep inventory 후 치환 계획 수립

### P4. 빌드/샌드박스 경로 단순화

- 문제:
  - bash / PowerShell / cmd / MSYS2 / stale object / path rewrite / sed 기반 stamp가 서로 다른 방식으로 깨진다
  - “Nothing to be done” + stale artifact 같은 회귀가 생산성을 크게 깎는다
  - smoke test가 repo root에 runtime artifact를 남기면 dirty worktree와 실제 소스 변경을 구분하기 어려워진다
- 기본 방침:
  - 단일 공식 빌드 경로를 정하고 나머지는 document-only 또는 best-effort로 내린다
  - stale artifact 회피를 위해 강제 재빌드 경로를 공식화
- 준비 작업:
  - [x] 공식 Windows 빌드 경로 1개로 문서화
    - 기준: GitHub Actions `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime
    - plain Linux-hosted `gcc`는 `ci-windows` acceptance line이 아님
  - [x] `llvm_smoke.sh`의 `string_io` smoke가 repo root에 `io.txt`를 남기지 않도록 각 case를 source directory에서 실행하게 정렬
  - [x] LLVM runtime object freshness가 split runtime `.inc` subpart 변경을 보도록 `compiler_runtime_cache_is_fresh(...)` dependency list를 확장. `pgy_runtime_lib_part_b_part_d.inc` 같은 하위 include 수정 후 stale runtime object가 링크되는 문제를 차단
  - [ ] `clean && build` 강제 wrapper / recommended entrypoint 정의
  - [ ] stale `.o` / `.d` 진단 가이드와 강제 재빌드 옵션 정리

### P5. printf-style 진단 포맷팅 축소

- 문제:
  - 일부 semantic diagnostic helper는 인자 개수가 매우 많고, placeholder drift에 취약하다
  - 현재 구조는 `fmt 하드코딩 + structured tags(code/cause/fix)`가 이중으로 공존한다
- 기본 방침:
  - 진단 payload를 struct로 모으고, human-readable render는 renderer/helper layer가 담당
  - 최소한 고인자 helper부터 payload-builder 패턴으로 전환
- 준비 작업:
  - [ ] high-arity diagnostic helper inventory 작성
  - [ ] generic mismatch / authority mismatch / ownership escape에서 payload struct 시범 도입

### P6. channel transport 규칙 공통 validator 수렴

- 문제:
  - `type_checker_async_channel.inc`와 builtin/send-query 계열이 ownership/channel transport 규칙을 중복 구현한다
- 기본 방침:
  - channel transport는 공통 validator 하나로 수렴
  - builtin/send wrappers는 surface adapter만 담당
- 준비 작업:
  - [x] send/try-send/send-timeout/status variants 공통 validator 추출
  - [ ] subject / movable / anchored / boundary mismatch wording 통일
  - 진행: named-transfer requirement와 subject/boundary/anchored borrowed-send/mismatch는 `semantic_channel_transfer_requires_named_binding(...)`, `semantic_report_named_channel_transfer_required(...)`, `semantic_validate_channel_transport_ownership(...)` helper로 1차 수렴
  - 진행: token / move-only send-recv restriction wording도 `semantic_report_channel_transport_policy(...)` helper로 정렬 시작
  - 진행: validator/reporting 구현은 `type_checker_async_channel.inc`에서 제거되고 `type_checker_channel_transport.c`가 source of truth가 됐다

### P7. 중간 stage JSON routing closure

- 문제:
  - HIR/DIR/RIR/MIR 실패 경로 일부가 여전히 plain text 중심이라 `단일 JSON 배열` 계약을 깨뜨린다
- 기본 방침:
  - frontend/backend 끝단뿐 아니라 중간 stage 실패도 structured output 계약에 들어오게 한다
- 준비 작업:
  - [ ] HIR/DIR/RIR/MIR failure emitter inventory 작성
  - [ ] plain-text fallback 제거 우선순위 수립

### P8. stale binary / artifact 회귀 고정

- 문제:
  - stale object/dependency 파일 때문에 소스 수정이 반영되지 않는 경우가 있다
- 기본 방침:
  - “빠른 증분 빌드”보다 “신뢰 가능한 재빌드” 경로를 우선
- 준비 작업:
  - [ ] stale artifact 재현 조건 문서화
  - [ ] 권장 빌드 진입점에서 clean rebuild 선택지를 기본 노출

### P9. arena 패턴 명시 도입

- 문제:
  - transpiler / semantic / diagnostics / type rendering 경로에 임시 문자열/버퍼 churn이 많다
  - `malloc/free`와 context-lifetime scratch allocation이 섞여 있어, early-return/fail path에서 소유권이 산발적이다
  - cache와 임시 문자열이 섞이면 dangling 또는 과도한 copy churn 위험이 커진다
- 기본 방침:
  - arena는 명시적으로 도입한다
  - 단, 전면 치환이 아니라 `scratch arena`와 `result arena`를 수명 기준으로 분리한다
  - cache / long-lived metadata / AST-owned field에는 arena-owned 포인터를 저장하지 않는다
  - arena 간 교차 참조는 raw pointer보다 `index` / stable handle 참조를 기본으로 한다
  - arena는 최소한 `transpiler`, `semantic scratch`, `semantic result`, 필요 시 `type/render scratch`처럼 역할/수명별로 분리한다
  - 타입/역할별 arena 분리는 “누가 free하느냐”보다 “언제 reset되느냐”를 기준으로 설계한다
  - 첫 단계는 transpiler / semantic diagnostics / type render helper의 scratch allocation 수렴이다
- 이 결정이 맞는 이유:
  - 현재 코드베이스는 long-lived cache와 short-lived formatting string이 강하게 섞여 있어, raw pointer 공유보다 index 참조가 훨씬 안전하다
  - Pergyra는 early-return/fail path와 pass-local scratch data가 많아서, 단일 arena보다 역할/수명별 arena 분리가 디버깅과 reset 비용 면에서 낫다
  - 즉, `Arena + Index 참조 + 타입별 arena 분리`가 지금 구조 debt를 줄이는 가장 보수적이고 안정적인 방향이다
- 준비 작업:
  - [x] `scratch arena` / `result arena` lifetime 규칙 문서화
  - [x] arena 간 cross-reference를 `index` / stable handle 기준으로 문서화
  - [x] `TranspilerCtx` scratch arena 적용 범위 확정
  - [x] semantic analyze pass용 scratch arena 도입 지점 정리
  - [x] diagnostic payload/result-owned arena 분리 여부 결정
  - [x] 타입/역할별 arena 분할안 초안 작성
  - [x] `strdup_fmt` / type render / projection path / generic formatter helper의 arena 전환 우선순위 작성
  - [x] cache에 arena-owned 포인터 저장 금지 규칙 문서화
  - [x] 첫 vertical slice:
    - transpiler temporary strings
    - semantic diagnostic formatting scratch strings
    - type-name rendering scratch helpers
  - 진행: `docs/94_arena_index_lifetime_plan.md`로 방향 고정
  - 진행: `TranspilerCtx`의 `arena`를 scratch arena로 명시
  - 진행: transpiler scratch-only temporary 1차 vertical slice 완료
    - zone authority temporary expression
    - intent priority default literal
    - projection refresh `source_expr`
    - event declaration `event_type`
  - 진행: semantic diagnostics result seam 1차 도입
    - `Diagnostic`가 optional payload snapshot을 보존
    - payload emit 경로는 result-owned snapshot으로 복사
    - semantic JSON 출력도 payload 필드를 함께 노출 가능
  - 진행: semantic scratch arena 1차 도입
    - `SemanticContext`에 scratch arena 추가
    - ownership diagnostic path string은 scratch arena를 우선 사용
    - payload snapshot이 result로 복사하므로 helper 내부 free churn 제거
  - 진행: LLVM arena lane 1차 closure
    - `LLVMGenCtx`는 `scratch` + `persistent` lane으로 분리
    - `LLVMGenResult`는 result-owned arena를 보유
    - intent MIR collector / projection path / local grow helper / event invoke / type render helper가 scratch로 수렴
    - synthetic event-handler AST field 저장은 callable signature registry로 치환
    - `*error_message` heap return contract는 result-owned lane으로 수렴
    - 남은 heap 경계는 owner shell(`ctx`, registry destroy, result outer shell)과 runtime ABI contract 수준으로 축소
    - 진행: intent observability(`last/history/active/recent`)와 authority failure snapshot의 stable runtime string exports는 `runtime-borrowed string` ABI로 고정했다. caller는 free하지 않고 다음 runtime registry/snapshot mutation 전까지만 유효하다
    - 진행: `runtime-abi-lifetime-test-smoke`가 stable intent last/history/active/recent 및 authority 문자열 export body에서 allocation/free/strdup이 발생하지 않도록 검사한다
    - 진행: stable string helper returns는 `result-owned string`, stable string-array helper returns는 `result-owned array` ABI로 고정했다. `runtime-abi-lifetime-test-smoke`가 helper payload가 borrowed input pointer, stack buffer, string literal을 반환하지 않고 allocation/copy된 payload를 반환하는지 검사한다
    - 진행: stable file descriptor는 `runtime-owned handle` ABI로 고정했다. `pgy_file_open`은 닫힌 runtime table slot을 재사용하고, `pgy_file_close`는 table entry를 NULL로 비워 재사용 가능 상태로 만든다. `runtime-abi-lifetime-test-smoke`가 이 release/reuse contract를 검사한다
    - 남음: file descriptor 외 runtime-owned handle ownership도 같은 수준의 smoke/문서 계약으로 확장해야 한다
  - 주의: 반환 계약이 있는 expression string은 아직 arena로 옮기지 않음
  - 주의: `slot_ref_expr(...)` scratch 전환 시도는 되돌림. 반환 ownership 경계를 먼저 나눠야 함

### 최근 closure 진행 (2026-04-18)

- declaration-side MIR-only host context를 더 정리
  - transpiler host context가 `current_host_decl -> within_zone -> saved host-name inventory` 순으로 복원되도록 정렬
  - class/zone/relation/effect/world field query helper가 raw host-name state보다 inventory-backed host handle을 우선 사용
  - direct `current_*_name` restore chain 일부를 `transpiler_restore_host_context_local(...)` helper로 접어 산발적 context 복구 코드를 축소
  - emitter hot path의 direct `current_*_name` 참조는 대부분 걷어내고, 남은 사용처를 helper/restore layer로 국소화
  - LLVM declaration helper도 current host lookup을 공용 active-inventory host helper로 접어 naming chain을 축소
  - LLVM MIR/domain emission의 direct `current_class_name` save/restore도 host-name bind/restore helper로 접어 state 관리 중복을 줄임
  - LLVM expr/stmt hot path도 `llvm_current_host_decl_name(...)` 기준으로 정렬해 direct raw host-name read를 더 줄임
  - `HasProjection/HasLayer/HasState/HasZone*` 및 method/field helper가 raw `current_class_name` 대신 host helper를 통과하도록 정리
  - LLVM pipeline의 nominal registration / class method emission도 raw nominal AST array보다 `mir->decl_headers`를 직접 순회하도록 정렬
  - LLVM domain pass도 raw `ctx->mir->{relations,effects,zones,...}` 직접 접근 대신 `llvm_active_domain_inventory(...)` helper를 통과하도록 정렬
  - 즉, declaration-side debt는 이제 emitter 본문보다 inventory bootstrap + helper/restore layer 국소 부위로 더 압축됨
  - C transpiler domain/hosted method emission도 `emit_hosted_methods_from_mir_or_error_local(...)` helper로 수렴
  - party / roster / relation / effect / zone / world method emit는 같은 MIR routine gate와 같은 explicit backend error 정책을 사용
  - relation/effect/zone/world method의 dead AST signature fallback 제거
  - party / roster / relation / effect / zone / world declaration emit entrypoint는 inventory decl을 우선 사용
  - bootstrap residual은 이제 per-domain AST array 직접 순회보다 inventory-backed bootstrap helper 본체 쪽으로 더 압축
- generic contract + type-resolution DAG 회귀를 더 넓힘
  - `role impl ability` 경로가 generic default/where-bound cycle provenance regression에 추가됨
  - 즉, action/intent-step/zone-authority/party-role-slot에 더해 role impl consumer도 staged DAG path 회귀 범위에 포함
- 현재 검증선
  - `test-semantic`: `1617 passed, 0 failed`
  - `test-transpile`: `670 passed, 0 failed`
  - `test-abi`: `84 passed, 0 failed`
  - `ci-linux`: full green 유지
  - LLVM expr/stmt host-helper 정리 이후에도 `test-transpile`, `test-abi` 재통과 확인

### 최근 closure 진행 (2026-04-24)

- runtime propagation/provenance 1차 closure
  - C/LLVM domain hidden cell이 `ready/dirty` bool만 가지던 상태에서 `epoch/cause` provenance cell까지 같은 schema로 확장됨
  - relation/effect/zone/world projection, layer, state, world-derived state가 recompute 시점에 cause-stamped provenance를 남기도록 C/LLVM이 정렬됨
  - LLVM domain struct layout이 그동안 빠뜨리고 있던 `__projection_dirty_*` field를 relation/effect/zone에 다시 포함하도록 parity 수정
  - LLVM projection sync도 C와 같은 dirty-gated recompute 경로로 정렬됨
  - LLVM host-field assignment가 zone/relation/effect host method 안에서 projection invalidation을 만들도록 복구
  - LLVM intent step rebound-zone 경로도 effective zone projection cell을 보수적으로 dirty-mark + sync 하도록 보강
  - 결과: `relation_effect_propagation_abi`, `intent_zone_binding`, `intent_cross_world_transfer`, `intent_rich_history_identity` backend compare drift 제거
  - 새 회귀: transpile domain async/world tests가 provenance hidden field와 stamp write까지 직접 확인
  - 새 진행: `world` derived-state recompute가 C/LLVM 양쪽에서 bounded pass loop를 가지도록 올라왔고, single-pass declaration-order replay에만 의존하지 않게 됨
  - 새 진행: bounded recompute pass-limit overflow는 C의 `PGY_PANIC`과 LLVM의 `abort()` 경로로 hard-fail되도록 고정됨
- 새 회귀: transpile world-derived chain test + `world_fixpoint_abi` smoke가 C/LLVM 양쪽에서 녹색
- 현재 해석: runtime propagation provenance baseline(`dirty/ready + epoch/cause`)은 이제 beta 계약의 일부로 간주하고 다시 약화시키지 않음
- 추가 closure: zone lifecycle sync도 이제 C/LLVM 양쪽에서 bounded frontier loop를 가지며, state/layer replay가 single-batch에만 묶이지 않는다
- 추가 closure: embedded world-zone source assignment도 이제 projection dirty mark 뒤에 같은 turn의 zone sync를 태워 stale `ready/value` drift 없이 projection recompute를 닫는다
- 추가 회귀: `world_embedded_projection_abi`, `world_embedded_method_projection_abi`, `world_embedded_branch_projection_abi`가 C/LLVM ABI smoke에서 녹색이며 embedded zone projection read-after-mutate path를 straight-line assignment, method-call, branch-join slice까지 잠근다
- 추가 회귀: `handoff_projection_frontier_abi`가 C/LLVM ABI smoke에서 녹색이고 `handoff_projection_frontier`가 backend-compare에서 녹색이다. v1 handoff materialization 이후 source projection은 source snapshot을, target projection은 target mutation 결과를 보도록 잠근다
- 추가 회귀: `handoff_world_state_frontier_abi`와 `handoff_world_state_frontier`가 C/LLVM에서 녹색이다. active world-owned zone을 `transfer:` 대상으로 넘긴 뒤 projection-backed world state와 `all` composed state가 같은 tick에서 fresh하게 보이는 최소 frontier를 잠근다
- 추가 회귀: `handoff_layer_state_frontier_abi`와 `handoff_layer_state_frontier`가 C/LLVM에서 녹색이다. `transfer:` 이후 action-caused effect가 target zone layer/state와 active world-derived layer/state alias까지 같은 tick에서 fresh하게 전파되는 경로를 잠근다
- 추가 회귀: `world_embedded_action_frontier_abi`와 `world_embedded_action_frontier`가 C/LLVM에서 녹색이다. embedded world-zone subject action call이 action-caused effect layer/state와 active world-derived layer/state alias까지 같은 tick에서 fresh하게 전파되는 경로를 잠근다
- 추가 회귀: `world_embedded_action_pool_frontier_abi`와 `world_embedded_action_pool_frontier`가 C/LLVM에서 녹색이다. embedded world-zone subject action call의 fixed-capacity effect pool 경로도 같은 frontier 계약으로 잠근다
- 추가 closure: authority/failure handoff queryable baseline은 `intent_authority_snapshot(_abi)`와 `authority_failure(_abi|_surface)`가 C/LLVM 양쪽에서 잠그며, authority reject가 process abort 대신 `last_ok / zone / participant / code / reason` 상태와 intent failure trace로 내려오는 최소 recoverable path를 가진다
- 강한 남은 과제: full bounded fixpoint / transitive frontier scheduler는 **명시적 beta blocker**로 유지. 다만 stable world outer frontier는 C/LLVM 양쪽에서 `pgy_frontier_world_transitive_pass_limit(...)`를 소비하도록 올라왔으므로, 남은 debt는 zone/world frontier loop의 부재나 authority/failure handoff 최소 baseline 부재가 아니라 더 넓은 world-zone propagation family를 같은 source-of-truth frontier policy로 일반화하는 일이다
- 추가 closure: relation/effect/zone projection sync도 bounded transitive recompute loop로 올라왔고 declaration order에 기대지 않는다
- 추가 회귀: `projection_chain_abi`가 C/LLVM ABI smoke, `make test-all`, `make llvm-test-backend-compare`에서 잠겼다
- 추가 gate: `make runtime-frontier-contract-test-smoke`가 C emitter와 LLVM emitter에서 world derived-state bounded recompute, zone lifecycle bounded frontier loop, projection-chain bounded recompute, embedded world-zone action-caused layer/state freshness, authority/failure handoff queryable baseline, pass-limit overflow hard-fail, ABI smoke 등록, backend-compare 등록을 검사한다. 또한 `src/codegen/domain_frontier_policy.h`의 pass-limit source-of-truth helper와 `pgy_frontier_world_transitive_pass_limit(...)`를 C/LLVM emitter가 소비하는지 확인하고, `make runtime-frontier-policy-test-smoke`가 saturating pass-limit arithmetic을 실제 컴파일/실행으로 잠근다. 이 gate는 full bounded fixpoint / transitive frontier scheduler가 다시 single-pass 구현이나 non-queryable authority failure로 후퇴하지 못하게 막는 beta blocker gate다. 남은 runtime propagation closure는 broader world-zone propagation family를 같은 source-of-truth frontier policy로 일반화하는 일이다
- Beta readiness audit: `docs/98_beta_closure_readiness_report.md` records the current codebase verdict, remaining blockers, and concrete closure order. It narrows the next highest-value implementation target to handoff propagation and broader world-zone scheduler generalization.

### 최근 closure 진행 (2026-04-23)

- AST 타입 디스패치 partition 규칙 공식화 — `docs/95_ast_dispatch_partition.md`
  - 전체 AST 타입 (현재 93종) 을 4 카테고리 (type annotation / decl sub-metadata / top-level decl / root) disjoint 분할
  - 각 카테고리별로 "왜 특정 switch 에서 도달 불가인지" 의 **파서 invariant 근거** 를 문서화
  - case label 추가/금지/safety-net 결정 기준 확정
  - 새 AST 타입 추가 시 체크리스트 포함
  - `llvm_stmt.c` 의 top-level decl skip 리스트 + Zone/World forward 가 이 문서 기준으로 정렬됨 (`AST_INTENT_DECL` skip 누락 수정, Zone/World 11종 forward 주석 정확화, `llvm_expr.c` explicit diagnostic 유지)
  - 새 AST 타입 추가 시 docs/95 업데이트 책임 명시

### 최근 closure 진행 (2026-04-22)

- arena scratch slice 3건 추가 흡수 — `docs/94_arena_index_lifetime_plan.md` 업데이트
  - `semantic.c:50` `semantic_preload_stdlib_uses` 의 per-iteration `malloc/free` module path 조립을 function-local `PgyArena` 로 이동. 배치 alloc 하나로 수렴
  - `type_checker.c:1109` enum method name mangling의 `malloc/snprintf/free` 를 `pgy_arena_fmt(&ctx->scratch_arena, ...)` 로 이동. `symbol_create_function` 이 이미 내부 `pergyra_strdup` 으로 이름을 복사하므로 arena 탈출 없음
  - `slot_analyzer.c:1067` `slot_analyze_parallel_block` 의 outer task metadata 배열 3종 (`task_accesses`/`task_counts`/`task_caps`) 을 `sa->ctx->scratch_arena` 로 이동. per-task inner 배열은 여전히 `collect_slot_accesses` 가 heap-owned로 관리
- arena scratch 2차 slice 추가 (같은 날)
  - `type_checker.c:355` type resolution cycle detection 의 `visited`/`path` 배열 → `ctx->scratch_arena`. cycle text는 return-contract helper라 보류
  - `type_checker_flow.c:499` match redundancy 의 `seen` 배열 → `ctx->scratch_arena`
- arena scratch 3차 slice — HIR/MIR 첫 진입 (같은 날, 이후 4차에서 routine-scope로 통합됨)
  - `hir.c:hir_compute_cfg_dominance` 의 `visited`/`postorder`/`idoms` 3배열 → function-local `PgyArena`
  - `hir.c:hir_mark_natural_loop` 의 `in_loop`/`stack` 2배열 → function-local `PgyArena`
  - `mir_ssa_rename.h:mir_apply_ssa_rename` outer 3배열 → function-local `PgyArena`
- arena scratch 5차 slice — LLVM 백엔드 첫 진입 (같은 날, 이후 6차에서 ctx-scope 로 통합)
  - `llvm_register.c:llvm_register_enum_decl` 의 `enum_fields` + per-variant `payload_fields` type-ref 버퍼를 function-local `PgyArena` 로 수렴
  - `llvm_intent.c:llvm_collect_mir_intent_participants` 는 return-ownership 계약이라 deferred
- arena scratch 6차 slice — **LLVMGenCtx ctx-scope scratch arena 도입** (같은 날)
  - `LLVMGenCtx` 에 `PgyArena scratch` 필드 추가
  - `llvm_ctx_create` / `llvm_ctx_destroy` 에서 lifecycle 관리
  - 5차에 function-local 로 시작한 enum type-ref arena 를 `ctx->scratch` 로 수렴. LLVMGenCtx 하나당 init/destroy 한 번만
  - 후속 LLVM scratch 사이트 (미래에 발굴되는) 도 이 arena 재사용 가능
- arena scratch 7차 slice — **LLVM 9 사이트 일괄 흡수** (같은 날)
  - tuple literal (`llvm_expr.c`) 의 vals + tys
  - event handler type / tuple type (`llvm_backend.c:ast_type_to_llvm`) 의 param_types + fields
  - event INVOKE (`llvm_domain.c`) 의 inv_params + call_args
  - class/enum/extern 등록 (`llvm_register.c`) 의 4 param-type 버퍼
  - ability vtable (`llvm_domain.c`) 의 outer vt_fields + per-method ptypes
  - 공통: LLVM C API 가 type/value 배열을 내부 복사하므로 scratch-safe
  - 결과: LLVM 전체의 short-lived type 배열 assembly 가 ctx arena 하나로 수렴
- arena scratch 8차 slice — **LLVM 17 사이트 추가 흡수** (같은 날)
  - `llvm_stmt.c`: lambda param, parallel closure ctx/wrapper/handles, async closure fields, select rotation BBs
  - `llvm_intent.c`: intent function param_types, step completion `completed_allocas`, `saved_participant_ptrs`
  - `llvm_domain.c`: world sync `prev_active_addrs`, domain struct `ftypes` (4 분기), role/class method `ptypes` (2 사이트), vtable `vals`
  - LLVM 쪽 scratch-safe calloc/malloc 은 거의 전수 `ctx->scratch` 로 수렴. 남은 것은 return-ownership 혼재 helper 와 AST-field stored 케이스

- arena scratch 4차 slice — **HIR/MIR routine-scope arena 도입** (같은 날)
  - `hir.h` HIRRoutine / `mir.h` MIRRoutine 에 `PgyArena scratch` 필드 추가
  - 생성: `hir_append_*`, `mir_lower` 루프 내 `memset` 직후 `pgy_arena_init(&routine.scratch, 0)`
  - 파괴: `hir_destroy()` / `mir_destroy()` per-routine cleanup + OOM 경로 (배열 편입 실패 케이스)
  - 3차에 function-local 로 시작한 3개 arena 를 모두 `&routine->scratch` 로 통합 → routine 하나당 init/destroy 한 번만. 여러 HIR/MIR pass 가 같은 arena 를 재사용
  - MIR pass는 `routine->scratch` 만 씀. `routine->hir_routine->scratch` 는 HIR frozen 계약이라 접근 금지 (코멘트로 고정)
- 원칙 유지: `scratch-only local temp 먼저, returned string 나중`. `slot_ref_expr(...)` 같은 반환 ownership 혼재 helper는 아직 보류
- 베타 acceptance line #8 ("scratch/result lifetime과 cache boundary가 문서/구현 기준으로 설명 가능하다") 에 해당 slice 기여

### 최근 closure 진행 (2026-04-21)

- C/LLVM init idiom 축 감사 + 1차 정비 완료 (`docs/93_codegen_idiom_audit.md`)
  - 6 case × 2 backend 매트릭스 고정
  - **Case 1 HIGH divergence 해소**: 함수-바디 `let x: T;` (annotation + no init)을 `PGY_CODE_SEM_UNINIT_LOCAL` 로 거부. C는 scalar-zero, LLVM은 store 생략으로 첫 read에서 값 의미가 갈라지던 잠복 경로를 semantic 레벨에서 차단
  - **Case 2 C backend L815 정리**: `transpiler_c_type_uses_scalar_zero` helper로 scalar/aggregate 분기. 기존 잠복 버그 (`struct Foo x = 0;` invalid C) 제거 (defense in depth)
  - **Case 3 MEDIUM 의도 비대칭으로 확정**: slot claim은 C가 런타임 helper, LLVM이 IR-direct. 현재 runtime observability 수준에서 관측 side effect 0. runtime observability 확장 시 재감사로 deferral
  - 회귀 3종 추가:
    - `function-body let with annotation and no initializer is rejected`
    - `function-body let with aggregate annotation and no initializer is rejected`
    - `subject field let with no initializer does not trigger the uninit-local guard` (negative)
  - 파서 구조 재확인: class/subject field는 ClassField 경로로 분리되어 `AST_LET_DECL`이 아님 → guard가 field-level 의미를 침범하지 않음
  - docs/72 에 `PGY_SEM_UNINIT_LOCAL` 섹션 + docs/93 cross-link 추가

### 최근 closure 진행 (2026-04-20)

- own/ref broader audit를 helper family 기준으로 더 정렬
  - helper call boundary의 `subject` / general boundary value 경로를 공용 borrowed-boundary validator로 접음
  - container store / array literal store borrow-escape를 공용 ownership diagnostic helper로 통합
  - semantic channel send borrow-escape도 공용 ownership diagnostic helper로 승격
  - 즉, `assignment / helper call / channel send / container store / array literal store / constructor field store`가 점점 같은 provenance wording family로 수렴 중
- intent authority mismatch provenance를 더 직접적으로 노출
  - `authorized by` unknown participant / non-subject participant / zone subject-slot mismatch / zone authority mismatch에 `approval boundary provenance` 섹션 추가
  - provenance가 비어 있으면 `no inherited/derived authority provenance was recorded`를 명시적으로 보고
- relation/effect/projection failure depth를 추가 보강
  - invalid projection source / tobject source rejection이 target/source consumer path와 projection contract origin을 직접 보고
  - 즉, projection diagnostics가 단순 type mismatch가 아니라 `target slot <- source slot` 경로를 기준으로 설명되기 시작함
- 현재 베타 blocker 재정렬
  - Windows backend-compare / LLVM parity 복구
  - declaration-side MIR-only 남은 host/inventory helper debt 제거
  - own/ref 일반화의 broader assignment / container / rebind / summary path closure
  - intent/zone/world 및 relation/effect/projection provenance 마지막 심화
- Windows-native compile hygiene를 추가 정리
  - `type_checker_builtins_query.inc`, `type_checker_builtins_nominal.inc`의 `%zu` / extra-arg formatting drift를 제거
  - `type_checker_decls_world.inc`의 world lifecycle diagnostics placeholder-arg mismatch를 제거
  - `type_checker_builtins.c`는 ownership/channel helper를 full internal header include 대신 최소 forward declaration으로 고정해 enum/static helper 재선언 충돌을 피함
  - 현재 기준선:
    - `test-semantic`: `1855 passed, 0 failed`
    - `test-transpile`: `601 passed, 0 failed`
  - 남은 Windows blocker는 semantic compile 단계가 아니라 native MSYS2/MinGW 실행 환경에서의 backend/runtime parity 확인 축으로 이동

### 최근 closure 진행 (2026-04-16)

- declaration-side host context를 inventory-backed handle 쪽으로 한 단계 더 정렬
  - transpiler host lookup이 `current_host_decl -> within_zone -> saved host-name inventory` 순으로 복원되도록 조정
  - zone/relation/effect/world field query helper가 raw `current_*_name` 분기보다 inventory-backed `current_host_decl`를 우선 소비
  - 즉, declaration-side C backend context 복원에서 string name state는 점점 restore hint로만 남고, 실제 host truth는 active inventory 기반 handle로 수렴 중
- explicit/compressed canonical pair examples를 intent-first 독해 규칙으로 다시 정렬
  - large/composite pair source에 `intent -> world/zone -> subject` read order를 직접 명시
- world embedding implicit copy를 warning이 아니라 hard contract로 승격 시작
  - world constructor에 zone binding을 그대로 넘기면 explicit `Clone(...)`를 요구
  - hidden copy semantics를 더 이상 benign warning으로 남기지 않음
- generic contract consumer path를 한 단계 더 닫음
  - omitted trailing default type arg가 user-defined generic class specialization path에서도 effective arg 기준으로 검증되도록 정렬
  - role impl / action requires / zone authority / party role slot에서 `default arg omission + where-bound violation` negative regressions 추가
  - multi-bound / omitted-default / consumer provenance 조합 회귀를 semantic 기준으로 고정
  - ability consumer path / class instantiation-specialization path에서 unresolved effective generic arg를 silent skip하지 않고 structured error로 승격
  - role-side ability require-field type resolution에서도 unresolved effective generic arg를 silent skip하지 않고 structured error로 승격
  - malformed impl ability generic arg가 있어도 뒤쪽 where/require-field 검증으로 partial 진행하던 경로를 차단
  - default generic bound validation에서 unknown parameter / unresolved default type도 structured error로 승격
  - generic function call-site where-clause validation에서도 missing/unresolved effective arg를 silent skip하지 않고 structured error로 승격
- own/ref 첫 일반화 vertical slice 시작
  - existing movable resource value(`QubitSlot`)는 function boundary에서 explicit `own` transfer parameter를 허용
  - `ref QubitSlot`는 아직 미닫힘 subset으로 유지하되, 이유/consumer path/fix가 포함된 structured diagnostic으로 고정
  - 즉, `own/ref`는 여전히 전역 closure 전이지만, move semantics가 이미 있는 resource value에 대해서는 explicit transfer boundary가 부분적으로 열리기 시작함
  - return/channel boundary ownership diagnostics도 `Reason:` / `Fix:` 구조로 정렬
  - function signature anchored-return rejection도 `Reason:` / `Fix:` 구조로 정렬
  - unnamed movable-resource channel send는 moved-here provenance를 설명하는 hard error로 고정
  - local binding 단계에서도 `recv/await` unnamed boundary use, subject rebinding, released-slot move, anchored-handle rebinding을 `Reason:` / `Fix:` 구조로 정렬
  - slot escape analyzer 경고도 return/helper-call/channel/unterminated local claim 경로에서 provenance형 `Reason:` / `Fix:` 구조로 정렬
- relation/effect/projection contract를 더 하드하게 조였다
  - `intent step causes`가 zone effect slot 없이 통과하던 경로를 hard error로 승격
  - `action causes`도 zone effect slot 없이 남는 경로를 structured hard error로 승격
  - authority-bearing `apply/link/detach/unlink/maintain`가 `by <subjectSlot>` 없이 남는 경로를 hard error로 승격
  - duplicate authority, unknown layer relation/effect type도 더 이상 benign warning으로 남기지 않음
  - maintain/detach/unlink duplicate/conflict diagnostics는 `Reason:` / `Fix:` 구조로 정렬
- unresolved declaration entrypoint를 더 줄였다
  - role include unknown role, roster slot unknown party, world roster/zone unknown type을 hard error로 승격
  - generic where-clause consumer path에서 unresolved effective arg도 더 이상 silent skip하지 않음
- declaration-side MIR-only domain method gate를 더 조였다
  - party / roster / relation / effect / zone / world method emission이 MIR routine 없이 AST body로 조용히 fallback하지 않도록 C backend를 정렬
  - role / domain method emission에서 MIR routine 미존재를 LLVM backend hard error로 승격
  - 즉, declaration-side domain method는 MIR inventory가 존재하는 빌드에서 silent fallback이 아니라 explicit backend failure를 계약으로 삼음

### 최근 closure 진행 (2026-04-14)

- declaration-side MIR-only intent inventory를 더 밀었다
  - MIR가 `IntentParticipant(alias,type)` metadata를 직접 운반
  - C/LLVM intent declaration emission이 participant alias/type를 AST 재해석 없이 MIR metadata로 우선 소비
- step-level MIR-only validation을 AST field 존재 검사에서 metadata 존재 검사로 옮겼다
  - `IntentCheck`
  - `IntentEval`
  - `IntentZoneWhere/IntentZoneAlias/IntentZoneFrom`
  - `IntentWho/IntentDispatch`
  - `compensate` 존재 판정
- intent emission cleanup/rollback 경로의 metadata gate를 C/LLVM 둘 다 정렬했다
- 관련 회귀:
  - `test-mir` green
  - `test-transpile` green

즉, intent declaration/step emission은 아직 완전 MIR-only 선언이 끝난 것은 아니지만,
`participant/step contract inventory`를 AST presence에 기대던 가장 거친 fallback는 한 단계 더 제거됐다.

### 베타 기준판 추가 (2026-04-15)

- `docs/70_beta_closure_master_board.md` 추가
  - B0 4축, declaration-side MIR-only debt, parity, runtime observability, surface trust를 한 장으로 고정
  - 베타 acceptance line과 exit rule을 명시
  - 앞으로 TODO의 개별 작업은 이 보드 기준으로 우선순위를 따른다

### 베타 최종 관문 (2026-04-18)

- [ ] **declaration-side MIR-only를 구조적으로 닫기**
  - zone/world/relation/effect declaration/method emission에서 남은 AST/HIR-carried inventory dependency를 더 제거
  - `current_*_name` / host-name 추정 helper보다 inventory-backed host handle / metadata 소비를 우선하도록 정렬
  - transpiler/LLVM 양쪽에서 raw host-name read를 helper/restore layer 밖으로 다시 새지 못하게 회귀로 고정
  - declaration emission failure는 comment/skip/fallback return이 아니라 explicit backend error로 승격
  - C/LLVM 둘 다 declaration-side path에서 `Unknown` / surface-trust-breaking fallback type emission을 계속 제거
  - 문서에서 `MIR-led / HIR-assisted`라고 남겨둔 debt를 실제 구현 기준으로 더 축소하고, 베타 시점 표현과 구현을 일치시킨다

- [x] **AST dispatch / backend fallback trust gate 고정**
  - `docs/95_ast_dispatch_partition.md` 기준으로 AST 타입 partition을 문서화
  - LLVM `stmt/expr` default path는 warning-only가 아니라 structured backend error로 고정
  - Zone/World declaration verb가 expression fallback으로 조용히 `0/null`이 되는 경로를 explicit backend diagnostic으로 차단
  - `tests/ast_dispatch_partition_smoke.sh`와 `make ast-dispatch-test-smoke`를 추가해 partition drift와 silent fallback 회귀를 CI에서 차단
  - Linux `ci-linux` acceptance line에 AST dispatch smoke를 연결

- [x] **type-resolution DAG를 beta blocker로 포함**
  - import resolver와 별개로 semantic type dependency graph를 beta acceptance line에 포함
  - generic default / multi-bound / role impl / action / intent step / party role slot / zone authority / module contract consumer를 같은 graph inventory로 추적
  - alias depth limit / ad-hoc recursive failure보다 path-aware cycle diagnostic을 우선 기준으로 끌어올림
  - 1단계 진행: `topo_order`를 버리지 않고 declaration staged worklist에 연결 시작
  - 반영 문서:
    - `docs/70_beta_closure_master_board.md`
    - `docs/63_feature_depth_matrix.md`
  - 1단계 진행: `world/zone` local contract와 `refresh` projection path를 synthetic graph node로 올리기 시작
  - 1단계 진행: topo worklist가 `LOCAL_CONTRACT` / `PROJECTION_PATH` synthetic node도 다시 소비하기 시작
  - 1단계 진행: synthetic node 소비를 host 전체 재실행이 아니라 label별 narrow handler로 축소
  - 1단계 진행: role impl consumer까지 cycle provenance 회귀를 추가해 ability consumer family를 더 완성
  - 남은 일: staged declaration prepass 범위를 넓히고 graph-backed evaluator를 semantic source-of-truth로 승격
  - ecosystem 확장(`stdlib/pkg/tooling`)은 이 DAG closure 이후 단계로 미룸

- [x] **own/ref 일반화 audit 마감**
  - own/ref는 ownership classifier 기준 stable subset으로 닫힘
  - borrowed value escape는 helper call / channel / return / container store뿐 아니라 broader assignment/member/store path까지 provenance 기준으로 점검
  - 진행: constructor field store(`Holder(packet)` 같은 boundary-visible store)를 borrowed escape 경로로 승격하고 semantic regression 추가
  - 진행: constructor field store도 borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)를 직접 보고하도록 정렬
  - 진행: array literal store(`[packet]`)도 borrowed escape 경로로 승격하고 semantic regression 추가
  - 진행: member assignment / array overwrite 진단이 identifier-only가 아니라 `holder.packet`, `items[0]` 같은 target path provenance를 직접 보고하도록 정렬
  - 진행: new-binding escape도 identifier-only가 아니라 borrowed member/aggregate source path provenance(`packet.view`, `items[0]`)까지 추적하도록 확장
  - 진행: new-binding escape regression도 member source path(`packet.items`)와 array source path(`items[0]`)를 fixture로 고정
  - 진행: container store(`ArrayPush`/`ListPush`/`SetAdd`/`QueuePush`/`MapSet`)도 borrowed member/aggregate source path provenance를 직접 보고하도록 정렬
  - 진행: helper forwarding / builtin channel send(`Send`/`TrySend`/`SendTimeout`/status variants)도 unnamed borrowed member/aggregate source path provenance를 직접 보고하도록 정렬
  - 진행: direct `return` escape도 borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)를 직접 보고하도록 정렬
  - 진행: slot/resource summary 기반 `return/channel/helper` diagnostics도 `summary provenance root` vocabulary로 direct semantic wording에 더 가깝게 정렬
  - 진행: summary-based helper escape는 direct callee wording 대신 `helper/function summary in '<fn>'` 경로로 분리해 drift를 줄임
  - 진행: summary-based return/channel escape도 direct consumer wording 대신 `return summary in '<fn>'` / `channel summary in '<fn>'` 경로로 분리해 drift를 줄임
  - 진행: anchored-handle summary escape도 direct `return/channel/helper` wording 대신 summary wording으로 분리해 own/ref bridge 문구를 정렬
  - 진행: helper-call / container-store / array-literal-store / semantic channel-send diagnostic family를 공용 helper로 통합
  - 진행: nested projection + transitive helper + member rebind 조합도 semantic regression fixture로 추가
  - 진행: movable-resource + nested member source + member rebind target 조합도 semantic regression fixture로 추가
  - 진행: declaration-side MIR-only host truth는 `current_host_decl` / inventory 기준으로 더 좁혔고, `within_zone`를 따라가는 transpiler host recovery fallback과 role-owner direct AST lookup을 제거
  - 진행: own/ref anchored-handle wording을 assignment / let-binding / return / channel / helper family에 맞춰 `boundary-visible handle binding` / `anchored-handle provenance` 기준으로 정렬
  - 완료 판정: direct/summary helper-chain, return/channel/helper, destructure, assignment/member/container/constructor/array path가 current semantic regression으로 고정됨
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver와 universal ownership lattice

- [ ] **generic contract 전경로 audit 마감**
  - generic contract는 `default type arg`, `multi-bound where`, `ability<T> consumer`, `zone authority`, `party role slot`, `impl/reference`, cross-module consumer path를 마지막까지 audit
  - 진행: `party role slot` generic mismatch consumer도 actual/expected type arg + consumer path provenance regression으로 고정
  - 남은 generic consumer path가 없다는 것을 regression으로 증명하고, partial acceptance처럼 보이는 경로를 남기지 않는다

- [ ] **Intent/Zone/World, relation/effect/projection 진단과 provenance 마감**
  - intent/zone/world의 embedding / handoff / authority mismatch에서 contract source, derived zone/using, transfer edge provenance를 계속 강화
  - relation/effect/projection은 propagation edge failure, contract mismatch, branch/join/handoff path에 `Contract source:` / `Reason:` / `Fix:`와 source/target provenance를 일관되게 부착
  - 진행: world embedding/handoff와 intent transfer/authority mismatch의 핵심 경로를 `Contract source:` / `Reason:` / `Fix:` 구조로 재정렬
  - runtime contract provenance와 diagnostic wording을 더 정렬해 “왜 실패했는지 + 계약이 어디서 왔는지 + 어떻게 고칠지”를 한 번에 보이게 한다
  - helper-heavy edge path를 줄이고, compile-time contract 실패를 silent/best-effort runtime sync로 넘기지 않는다
  - 진행: intent step contract-source summary가 `authorized by`, transfer handoff, derived transfer zone provenance를 더 직접적으로 설명하도록 정렬
  - 진행: zone-within action authority mismatch가 `within` / `causes` header를 contract source로 직접 보고하도록 정렬
  - 진행: world embedding / post-embedding mutation diagnostics가 `world <name> zone slot <slot>` contract source와 world-owned authority/handoff destination을 직접 보고하도록 정렬

- [ ] **C/LLVM parity + full CI green을 베타 최종 관문으로 고정**
  - Linux 기준 `parser / semantic / transpile / ABI / backend-compare / llvm smoke / ir-pipeline / example smoke`를 full green으로 유지
  - Windows는 로컬 Linux host에서 강행하지 않고, MSYS2/MinGW + LLVM runner에서 `ci-windows` full green을 다시 고정
  - backend compare는 domain semantics 기준 parity를 계속 확대하고, same-process ABI / launch / runtime environment 차이를 재발하지 않게 잡는다
  - 현재 immediate blocker: Windows `backend-compare`와 LLVM parity의 마지막 crash / launch / runtime mismatch 제거
  - 베타 선언 전 acceptance line은 “부분 green”이 아니라 C/LLVM parity와 expected stdout/stderr/result parity까지 포함한 CI green으로 둔다

실행 가능한 연구용 컴파일러 단계는 넘겼지만, 아직 베타라고 부를 수는 없다.

판정 기준:
- 베타 원칙인 `부분 구현 상태를 남기지 않는다`를 아직 충족하지 못함
- 키워드 부족이 아니라 `구현 depth 불균형`이 문제임
- parser가 받는 surface 중 일부가 semantic/C/LLVM/runtime/test/documentation까지 완전히 닫히지 않음

### 이미 닫힌 축과 더 이상 베타 차단이 아닌 것

- `public/private/export` module boundary
  - top-level nominal/domain/callable visibility 정렬 완료
  - private `func/intent/event` cross-module call 차단 완료
  - private `zone/effect` action-contract leakage 차단 완료
- nominal token split
  - `subject/class/struct/object/tobject`는 lexer token 레벨에서 이미 분리됨
- ability field surface
  - legacy `require` alias 제거, `fields` canonical surface 고정
- generic ability baseline
- `ability<T>`, `requires Ability<T>`, `impl ability Ability<T>`, zone authority generic ref, mismatch diagnostics baseline 존재
- cross-module imported generic ability의 multi-bound zone-authority consumer regression 추가
- 양자 surface
  - 베타 대상에서 제외
  - `v2 / experimental`로만 추적

### 현재 베타를 막는 실제 B0 갭

#### 1. Intent / Zone / World closure

현재:
- intent orchestration, inherited/derived contract, rollback/cleanup carrier, zone/world declaration과 기본 lowering은 존재
- zone/world projection/layer/state query도 존재
- intent runtime observability baseline도 존재
  - `IntentLast*`
  - `IntentHistoryStep*`
  - `IntentActive*`
  - `IntentRecent*`
  - active/recent handle + active-step field query builtin의 semantic/transpiler/runtime/LLVM baseline 연결 완료
  - runtime 내부 recent ring + active registry + typed step history storage 연결 완료
  - ABI regression: `IntentRecent*` trace/failure baseline, failed-intent provenance, world zone query, relation/effect zone state parity 고정
  - backend parity: embedded world -> zone projection visibility regression 고정

남은 것:
- embedding ownership / handoff policy를 surface trust 수준까지 명확히 고정
- richer multi-instance timeline query와 failure provenance 정교화
- cross-layer propagation policy의 더 깊은 closure
- C/LLVM parity를 declaration/runtime/diagnostic까지 같은 품질로 정렬

#### 2. relation / effect / projection closure

현재:
- declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync baseline 존재
- effect join/meet/conflict API와 basic closure 존재
- projection contract diagnostics는 target/source/mode/fix를 포함하는 structured error 쪽으로 보강됨
- backend parity:
  - embedded world -> zone projection visibility regression 고정
  - relation/effect layer + state propagation parity regression 고정

남은 것:
- authority/resource와 effect partial order의 더 완전한 통합
- projection propagation policy 심화
- runtime contract와 deeper propagation failure provenance를 더 설명 가능하게 정리
- C/LLVM parity에서 helper-heavy edge path 감소

#### 3. generic contract closure

현재:
- generic ability declaration/reference baseline 존재
- action / intent step / zone authority / party role slot generic mismatch diagnostics stable 존재
- hidden/default-export generic ability visibility는 action/role impl뿐 아니라 zone authority/party role slot consumer path까지 회귀로 고정
- `ability<T> where ...` bound는 `requires` / `impl ability` / party role slot ref에서 다시 검증됨
- default type argument는 semantic + transpiler + backend compare까지 baseline closure 완료
  - user-defined `class/ability<T = ...>`가 omitted arg 경로에서도 effective specialization으로 정렬됨
  - non-deduced trailing generic parameter default도 function call `where` validation 경로에서 회귀로 고정
  - cross-module omitted default generic ability consumer(`party role slot` / `zone authority`)도 회귀로 고정
- multi-bound `where T: A + B` baseline은 현재 동작함
- hidden/default-export와 generic ability ref 규칙 정렬 완료

남은 것:
- broader type-family generalization을 beta 범위 밖으로 명시
- richer generic constraint validation의 beta contract 범위를 문서/board에 일치시켜 고정
- import/use surface와 diagnostics/tooling 표현을 module contract 기준으로 더 일관되게 정리

#### 4. own/ref closure

현재:
- anchored subset은 닫혀 있음
  - `ref Slot<subject-host>`
  - `own SecureSlot<subject-host>`
- first movable-value transfer slice도 시작됨
  - explicit `own QubitSlot` parameter는 허용
  - `ref QubitSlot` borrow boundary baseline 허용
  - call-site는 `own/default`면 consume, `ref`면 borrow 유지로 분기
  - borrowed `ref QubitSlot`의 `return` / `channel send` escape는 semantic에서 명시 차단
- 관련 진단/예제/문서는 현재 구현 기준으로 정렬됨

판정:
- anchored subset baseline은 이미 있지만, beta-quality 기준에서는 own/ref를 다시 활성 blocker로 본다
- 남은 일은 일반 movable type ownership model, copy vs move-only 분류, assignment/call/return/channel/container/rebind 전경로 analysis, richer provenance diagnostics를 닫는 것이다
- 특히 borrowed movable-resource ownership는 helper-call/return/channel-send baseline이 닫혔고, 다음은 wider movable type generalization과 container/rebind provenance를 더 닫아야 한다
- anchored subset만 stable이라고 보고 넘어가면 ownership story가 partial acceptance로 남는다

### 레이어별 현재 진실

#### 시맨틱

- 강한 부분:
  - nominal family
  - subject/action
  - async/channel/select
  - generic ability baseline
  - visibility/export boundary
- 아직 얕은 부분:
  - richer generic constraint validation
  - general own/ref
  - event closure의 잔여 negative path
  - collection semantic depth

#### 코드 생성

- C backend:
  - 코어 surface는 가장 성숙
  - method owner metadata가 HIR->MIR로 내려와 declaration-side zone/relation/effect/world context 복원 시 이름 추정보다 MIR metadata를 우선 사용
  - 진행: `transpiler_emit_host_method_body_local`의 manual save/restore 상태를 `TranspilerMirEmitState` snapshot helper로 축소
  - 진행: `emit_func_decl_from_mir_named` / AST fallback `emit_func_decl_named`도 `TranspilerMirEmitState` snapshot helper로 수렴
  - 진행: `emit_intent_decl`의 function-scope out/render/return/local-count restore도 `TranspilerMirEmitState` snapshot helper로 수렴
  - 진행: generic class specialization method body도 MIR inventory 존재 시 AST fallback 대신 MIR routine gate / explicit backend error로 정렬
  - 진행: LLVM domain/role missing-routine errors도 `PGY_CODE_LLVM_MIR_ROUTINE_MISSING` / cause / fix structured path로 정렬
- LLVM backend:
  - MIR-led / HIR-assisted hybrid
  - ordinary routine은 MIR 중심이지만 domain declaration과 일부 bootstrap/helper path에 HIR/AST 의존 잔존
  - pure MIR-only라고 부르기에는 아직 이름이 과함

#### 런타임

- 강한 부분:
  - slot / secure baseline
  - async/channel basic runtime
  - basic intent execution/rollback
  - intent observability baseline (`last` / `history` / `active` / `recent`)
- 아직 얕은 부분:
  - richer multi-instance timeline / failure provenance
  - channel backpressure protocol
  - party edge-path completeness
  - richer zone/world runtime policy

### 컬렉션 / 표면 신뢰

- `Map<K, V>`는 현재 `String | Int | Long | Bool` key stable subset까지 올린다
- 이것은 버그가 아니라 현재 contract
- arbitrary key-universal map contract는 아직 generic closure debt로 남는다

### 툴링

- LSP / formatter는 베타 차단 핵심이 아님
- debugger / package manager / WASM도 베타 차단 핵심이 아님
- 이들은 B0 closure 이후에 다루는 것이 맞음

### 베타 직전 정리 원칙

1. 새 키워드/새 축을 더 추가하지 않는다
2. 남은 미완성 surface를 `완성`하거나 `experimental`로 내린다
3. `양자`, `WASM`, `패키지 매니저`, `고급 디버거`는 베타 대상에서 제외한다
4. B0 4개를 닫기 전에는 베타라고 부르지 않는다

---

## 완료 (P0 — Pain Point 수정, 2026-04-12)

- [x] **P0-1: Array for-in `.count` → `.length`** — `transpiler.c`에서 Array는 `.length`, List는 `.count` 사용
- [x] **P0-2: `StringSplit`/`StringJoin` 런타임 구현** — `pgy_runtime.h`에 실제 구현 추가, 시맨틱/C 백엔드 일치
- [x] **P0-3: `None` 심볼 정의** — `type_checker.c`에서 AST_IDENTIFIER 처리, `type_system.c`에서 `Option<unknown>` → `Option<T>` 할당 허용, 코드젠에서 `expected_type` 기반 타입 해결
- [x] **P0-6: defer 변수 스코프 버그 수정** — `type_checker_flow.c`에서 defer body 처리 전/후 resource-state snapshot/restore. cleanup body의 `return`/`break`/`continue`와 QubitSlot release/move는 검사하지만 주변 CFG path와 outer loop flow를 소비하지 않는다. direct `type_check_statement()` fallback도 같은 helper를 사용한다.
- [x] **P1-7: struct/subject Slot 매크로 warning 억제** — `transpiler.c`에서 `#pragma GCC diagnostic push/pop`으로 `-Wunused-function` 억제
- [x] **P1-emit_call 갭 메우기** — `BUILTIN_BOX_ARRAY`, `BUILTIN_PARALLEL` 케이스 추가
- [x] **P0-4: enum match OR 패턴 수정** — `type_checker_flow.c`에서 named variant OR 패턴 허용 + coverage 체크 수정
- [x] **P2-13: match 기반 함수 default return 자동 생성** — `transpiler_emitters_base_b.inc`에서 non-void 함수 끝 fallback return 추가
- [x] **Pain Point 보고서** — `docs/68_pain_point_report.md`에 수정 내역 기록

## 완료 (최근)

- [x] **Windows ABI/backend-compare precheck 실행 경로 정규화**
  - `compiler_run_binary()`가 MSYS 스타일 `/tmp/...` 및 `/<drive>/...` 실행 파일 경로를 그대로 `_spawnvp()`에 넘기던 문제를 수정
  - Windows에서 executable launch는 native Win32 경로로 정규화한 뒤 실행하도록 정렬
- [x] **nested vessel-source projection ambiguity closure**
  - zone `refresh/publish/bind` projection contract 경로에서 ambiguous source path가 `missing`으로 오진되던 분기 순서를 수정
  - builtin `ToObject` / `ToTObject`도 동일한 structured `Reason/Fix` ambiguity diagnostic으로 정렬
  - nested vessel ambiguity semantic regressions 추가
- [x] **generic consumer provenance diagnostics 보강**
  - `action requires` / `zone authority` / `party role slot` / `intent step requires`에서 generic ability mismatch가 `actual type argument` / `actual implementation` provenance를 함께 보고하도록 정렬
  - 관련 semantic 회귀 추가
- [x] **anchored own/ref provenance diagnostics 보강**
  - closed-subset / local-only / missing `own/ref` / `ref` escape 진단에 `Reason/Fix`와 borrowed-here provenance를 추가
  - 관련 semantic 회귀 추가
- [x] **world embedding structured diagnostics 회귀 고정**
  - embedded zone old-binding mutation이 assignment / hosted func-action call 모두에서 `Reason/Fix`와 world-owned-copy provenance를 남기도록 semantic 회귀 강화
- [x] **Windows shell smoke portability 보강**
  - `abi_pipeline_smoke.sh`, `compare_backends.sh`가 `cmp`/`diff` 부재 환경에서도 `git` 또는 Python fallback으로 비교/차이 출력을 수행하도록 정리
- [x] **surface trust docs 정렬 — collection/result/struct baseline**
  - `Array<T>`는 `[]`, `List<T>`는 `ListNew()`, `HashMap<K,V>`는 `MapNew()`를 canonical 생성 surface로 고정
  - `Result<T>` 추출 API는 `Unwrap` / `UnwrapOr` / postfix `?`로 고정, `UnwrapResult()` 표면은 비채택
  - `struct` field의 legacy `let`은 불변 표식이 아니라 declaration introducer임을 문서화하고, 읽기 전용 계약은 `object/tobject`에만 둔다
- [x] **generic default-arg closure 1차 복구** — declaration acceptance만이 아니라 user-defined generic class omission, generic ability impl-reference omission, arity diagnostics range화, semantic/backend parity까지 다시 녹색으로 정렬
- [x] **ABI Unification Infrastructure** — `pgy_abi_spec.h`, `test_abi_spec.c` (28 PASS), `MIRTypeLayout`, `mir_abi_lookup()`, `rir_dump_json()`, dumb emitter Visitor
- [x] **Windows CI Fix** — `TOKEN_TYPE` → `PGY_TOKEN_TYPE`, `TokenType` → `PgyTokenType` (~20개 파일)
- [x] **v2 Quantum Planning** — 양자 연산 미지원 명시, v2 계획 문서화
- [x] **Documentation Index** — `docs/INDEX.md` 생성, 전체 문서 체계화
- [x] **`HashMap<K, V>` stable key subset surface trust 정렬** — semantic annotation/builtins/runtime comment/test를 `String | Int | Long | Bool` key 지원으로 일치시킴
- [x] **mixed `ability + zone` module export 충돌 수정** — default-export `ability`가 sibling zone visibility를 깨뜨리던 정규화 버그 제거, module smoke 회귀 추가
- [x] **nominal host receiver type 오염 수정** — C backend member-call emit 중 static type-name overwrite를 제거해 `Int_Advance`류 오발행 복구
- [x] **MIR cleanup exceptional topology 회귀 복구** — cleanup/rollback/invalidation block edge materialization과 test expectation 정렬
- [x] **`order_analytics` example 실전화** — sketch 수준 surface를 정리하고 compile-smoke covered example로 승격
- [x] **declaration name surface tightening** — declaration name을 일반 식별자로만 제한하고 reserved keyword 재사용 surface 제거
- [x] **anchored-handle diagnostics/test 정렬** — `own/ref` closed-subset 진단 문구와 `DeviceSlot`/anchored-handle semantic test expectation을 현재 구현 기준으로 일치시킴
- [x] **계층형 stdlib/domain kit v0 고정** — `money`, `datetime(Duration/Instant)`, `timer`, `versioning`, `ledger`, `obligation`, `device_adapter` 모듈과 probe 예제 추가, 코어 추가 금지 원칙 문서화

## 베타 클로저 보드

베타 전 원칙:
- `부분 구현` 상태를 남기지 않는다
- 완료시키지 못하는 surface는 내리거나 experimental로 격리한다
- parser가 받는 표면은 semantic/C/LLVM/runtime/test/documentation까지 닫는다

### B0 — 의미론 클로저 필수

- [ ] **Intent/Zone/World semantics 완전 closure**
  - contract reuse/derivation / authority / lifecycle / embedding ownership / runtime observability / C/LLVM parity / regression
  - 이미 존재: intent orchestration, inherited/derived contract, zone/world query, observability baseline
  - 진행: runtime zone/world propagation cell에 `epoch/cause` provenance baseline이 들어갔고, LLVM intent rebound-zone sync도 같은 truth로 정렬됨
  - 진행: world derived-state chain은 이제 bounded recompute loop를 통해 C/LLVM 양쪽에서 같은 규칙으로 계산됨
  - 강한 기준: 이 축은 이제 "얕은 single-pass sync로도 beta 가능" 같은 해석을 허용하지 않음
- 남음: embedding ownership/handoff policy, **handoff와 더 넓은 world-zone propagation family까지 일반화된 bounded fixpoint 기반 cross-layer propagation policy**, richer provenance query surface, declaration/runtime/diagnostic parity
  - 이 축은 언어 정체성 자체이므로 beta 직전까지 열어두지 않는다
- [ ] **relation/effect/projection semantics 완전 closure**
  - effect lattice, authority-resource partial order 통합, refresh/publish/bind/causes 일관화, diagnostics, C/LLVM parity
  - 이미 존재: declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync, effect join/meet/conflict, projection contract diagnostics baseline
- 진행: relation/effect/zone projection hidden cell도 C/LLVM 모두 `dirty/ready + epoch/cause` schema로 정렬됐고 runtime contract provenance baseline이 생김
- 진행: world-derived recompute는 bounded pass loop로 올라왔고, relation/effect/zone projection chain도 bounded transitive recompute loop로 올라왔다
- 강한 기준: projection propagation은 더 이상 "helper replay가 대체로 맞음" 수준으로 두지 않고, transitive semantics가 닫히기 전까지 beta blocker로 유지
- 남음: authority-resource partial order 통합, projection/layer/state를 넘어선 **authority/failure handoff와 더 넓은 world-zone propagation family까지의 full transitive frontier propagation policy**, helper-heavy edge path 감소, declaration/runtime/diagnostic/backend parity의 마지막 shrink
  - 이 축은 domain semantics 핵심이므로 partial 상태로 beta에 올리지 않는다
  - projection diagnostics는 `target/source/projection kind/field path/fix`를 포함하고 `Reason:` / `Fix:` 포맷으로 고정한다
- [x] **generic contract 완전 closure**
  - strict beta-quality 기준으로 stable subset closure에서 재개방
  - `default type arg` actual resolution, `where T: A + B` 전경로 enforcement, `ability<T>` mismatch provenance, instantiation-path parity까지 닫는다
  - 완료: default type arg declaration acceptance / omitted trailing default resolution / generic ability impl-reference omission / arity diagnostics provenance
  - 이미 존재: `ability<T>` baseline, default type arg baseline, omitted trailing default resolution, generic mismatch provenance baseline
  - 진행: `party role slot` generic mismatch도 `consumer path / expected type args / actual type args` vocabulary 회귀로 고정
  - 남음: multi-bound 전경로 enforcement, module-contract propagation, instantiation-path parity, richer mismatch diagnostics, wider C/LLVM regression 확대
  - generic mismatch는 `generic subject / expected type args / actual type args / broken bound / consumer path / fix`를 포함하고 `Reason:` / `Fix:` 포맷으로 고정한다
  - generic은 partial acceptance를 beta에 올리지 않는다
- [x] **own/ref 완전 closure**
  - strict beta-quality 기준으로 anchored subset closure에서 재개방했고, classifier-backed stable subset으로 마감
  - 일반 movable type ownership, move/borrow/escape/rebind/channel/return provenance, diagnostics/test parity까지 닫음
  - 이미 존재: anchored slot subset, anchored diagnostics baseline, anchored regression/docs alignment
  - 완료: summary/direct path family audit와 classifier/docs 최종 정렬
  - 진행: constructor field store escape 경로를 boundary-visible store로 고정하고 회귀 추가
  - 진행: array literal store escape 경로를 boundary-visible store로 고정하고 회귀 추가
  - 진행: assignment rebind escape diagnostic이 member/aggregate target path(`holder.packet`, `items[0]`) provenance를 직접 보고하도록 정렬
  - 진행: nested projection provenance가 constructor field store / member rebind / list/set/queue/map store / array overwrite / helper return summary / channel send / direct return까지 회귀로 고정됨
  - 진행: class/subject consumer matrix는 return / channel / helper / list / set / queue / map / array push / array overwrite / member rebind / constructor field store까지 거의 동형으로 정렬
  - 진행: tuple/object 경로는 기존 `test_semantic.c` 회귀 축에서 channel/new-binding/rebind/return/helper forwarding/queue-map-array overwrite/projection provenance coverage 유지
  - 진행: slot-handle/class helper-chain 회귀도 ownership-boundaries 계열에 추가돼 direct helper/function call family가 transitive chain까지 고정됨
  - 진행: helper/return/channel wording family를 `through ...` 기준으로 정렬
  - ownership diagnostics는 `value / ownership mode / moved|borrowed here / escaped|rebound here / consumer path / fix`를 포함하고 `Reason:` / `Fix:` 포맷으로 고정한다
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver와 universal ownership lattice

### B1 — 베타 신뢰도 필수

- [x] **surface trust 문서 재분류**
  - 완료: `docs/18_language_status.md`, `docs/63_feature_depth_matrix.md`, `README.md`에서 `stable subset / explicit reject / beta-out-of-scope` 기준으로 정렬
  - 규칙: "컴파일은 되지만 partial"인 표면을 stable처럼 쓰지 않고, 어디까지를 닫힌 계약으로 약속하는지 먼저 명시
  - 규칙: broader generalization, arbitrary key support, general ownership, richer observability query 같은 항목은 `beta-out-of-scope`로 분리
- [ ] **stable example / smoke source of truth 확대**
  - canonical examples와 closure examples를 smoke에 직접 연결
  - explicit surface vs compressed surface를 같은 의미로 보여주는 pair example 최소 4쌍 고정
  - 대상: app/web orchestration, game/simulation, async/worker/device, world-handoff/domain propagation
- [ ] **Backend parity final closure**
  - C/LLVM이 domain semantics 기준으로 같은 결과를 내는지 고정
  - 대상: intent/zone/world, relation/effect/projection, ownership boundary, refresh/publish/bind, world embedding/handoff
  - 기준: backend compare / llvm smoke / example smoke / ABI-runtime probe가 Linux/Windows 모두 녹색
- [ ] **experimental surface 제거 또는 격리**
  - 닫지 못한 parser surface는 명시 거부 또는 문법 제거

## Pain point freeze board

원칙:
- 기능을 더 넓히기 전에 반복해서 다시 깨지는 작성/진단 pain point를 먼저 고정한다
- 각 pain point는 `stable contract + regression + docs wording`까지 같이 잠근다
- recoverable failure와 invariant break를 같은 방식으로 처리하지 않는다

### Failure handling policy freeze

분류:
- `recoverable failure`
  - 사용자 코드가 예상 가능한 실패
  - 예: intent failure, authority/boundary rejection, timeout, remote failure, empty/closed operational state
  - 원칙:
    - 프로세스를 죽이지 않는다
    - `Bool` / `Result<T>` / queryable runtime state로 드러낸다
    - reason / boundary / authority / step provenance를 조회 가능하게 남긴다
- `contract violation`
  - 원칙적으로 semantic 단계에서 차단
  - 런타임까지 오면 structured panic
  - 예: released slot access, invalid secure token, ownership boundary 위반
- `internal compiler/runtime bug`
  - 즉시 중단
  - internal error / panic로 명확히 분리
  - 사용자 코드 실패처럼 위장하지 않는다

현재 고정:
- intent/zone/world 쪽 실패는 장기적으로 `recoverable failure`로 수렴시킨다
- slot/token/invariant 계열은 계속 hard fail로 둔다
- `Unwrap(...)`는 panic 성격의 sharp tool로 유지하고, recoverable path의 기본 계약으로 쓰지 않는다

- [ ] **large canonical pair 예제 추가**
  - 큰 예제에서 `explicit`와 `compressed`를 둘 다 stable source of truth로 유지한다
  - 최소 4개 파일 기준으로 관리한다
    - `calendar manage-event`: explicit/compressed
    - `composite intent orchestration`: explicit/compressed
  - 목적:
    - 큰 예제의 전체 계약을 명시형으로 읽을 수 있게 유지
    - 같은 의미를 축약형으로도 바로 복사해 시작할 수 있게 유지
    - smoke에서 두 예제가 모두 실행 가능하도록 고정
- 이 보드는 sugar backlog가 아니라 beta surface trust를 지키기 위한 고정판이다
- P0 pain point가 잠기기 전에는 declaration-side MIR-only debt를 국소 복구 외에는 넓게 건드리지 않는다
- backend 내부 정리는 pain point 기준선과 회귀가 먼저 고정된 뒤에만 다시 확장한다

### P0 — 작성/계약 pain point

- [ ] **contract clause density 고정**
  - 대상: `requires / within / authorized by / causes / refresh / publish / bind`
  - 문제: 같은 의미를 action / intent step / zone에서 중복 기술하게 되어 작성 피로가 커짐
  - 고정 기준:
    - 어디까지 inherited/derived 되는지 vocabulary를 고정
    - 길게 쓰는 버전과 압축 버전의 의미 차이가 문서/진단/예제에서 같아야 함
    - canonical pair와 minimal subset example의 역할을 분리해 source-of-truth를 고정
  - 회귀 기준:
    - semantic regression: inherited/derived contract source가 진단에 노출
    - example smoke: long-form vs compressed-form 예제 둘 다 유지

현재 source-of-truth:
- canonical pair
  - `examples/intent_contract_pair_minimal.pgy`
  - `examples/authority_contract_pair_minimal.pgy`
  - `examples/transfer_contract_pair_minimal.pgy`
- stable minimal subset
  - `examples/action_contract_inheritance_minimal.pgy`
  - `examples/intent_contract_derivation_minimal.pgy`
  - `examples/transfer_move_minimal.pgy`
  - `examples/transfer_move_typed_minimal.pgy`
  - `examples/zone_context_minimal.pgy`

- [x] **contract provenance vocabulary 고정**
  - 완료: beta closure 문서에 contract provenance 표준어를 `derived / inherited`로 고정
  - 규칙: contract source 설명에서는 `inferred`를 쓰지 않고, action에서 재사용된 step clause는 `inherited`, `using/transfer` 등 현재 step에서 계산된 clause는 `derived`로 부른다
  - 규칙: diagnostics / AST print / docs가 같은 용어를 쓰도록 맞추고, `inferred`는 일반 타입 계산이나 non-contract internal analysis 문맥에만 남긴다
  - 대상: contract provenance 잔여 표현, contract source wording, docs/example terminology
  - 문제: compiler type/effect inference와 domain contract 상속/파생이 같은 단어로 섞이면 설명력이 무너짐
  - 고정 기준:
    - domain contract는 `상속 / 파생`과 `inherited / derived`로만 부른다
    - 일반 compiler 의미는 type/effect `inference`에만 남긴다
  - 회귀 기준:
    - parser/semantic diagnostics 기대 문자열 고정

### P0.5 — recoverable failure 분류/고정

- [x] **failure class inventory 정리**
  - 완료: `docs/07_error_handling.md`, `docs/18_language_status.md`, `README.md` 기준으로 `recoverable failure / contract violation / internal bug` inventory를 정리
  - 완료: 현재 recoverable 유지 항목, hard-fail 유지 항목, 후속 downshift 대상(authority rejection 등)을 구분
  - 규칙: runtime invariant guard와 real domain rejection을 같은 실패 층으로 섞지 않음
- 현재 inventory baseline:
  - recoverable 유지:
    - `Result<T>` / `?`
    - `RemoteFuture<T> -> Result<T>`
    - channel timeout / non-blocking / closed state
    - world roster timeout
    - `IntentLast* / History* / Active* / Recent*`
  - hard-fail 유지:
    - released slot / invalid token / token permission mismatch
    - `Unwrap(...)` on `Err`, option unwrap on `None`
    - allocator / box / rc / weak invariant break
    - array / slice bounds violation
    - current runtime zone authority null-guard
      - 참고: 이건 아직 real authority rejection이 아니라 invariant check라서 hard-fail 유지 쪽이 맞다
  - first-wave conversion targets:
    - future real runtime authority rejection
    - intent boundary/authority mismatch provenance at runtime
- [ ] **intent/zone/world recoverable failure baseline**
  - intent failure, authority rejection, boundary mismatch는 process abort 대신 queryable reason/state로 노출
  - runtime observability와 diagnostics wording을 같은 provenance vocabulary로 정렬
  - 참고: runtime propagation provenance(`epoch/cause`) baseline은 완료로 본다
  - 진행: runtime zone authority invariant guard는 `last_ok / zone / participant / code / reason` thread-local snapshot을 남기도록 정렬되어, hard-fail guard와 별개로 최소 queryable failure snapshot baseline은 생겼다
  - 진행: authority failure code/reason/stderr format은 `src/runtime/pgy_runtime_authority_contract.h`로 승격했다. inline C runtime과 LLVM runtime library export가 같은 contract macro를 사용하고 `runtime-authority-contract-test-smoke`가 raw literal drift를 차단한다
  - 진행: intent emitter는 MIR `IntentAuthorizedBy` metadata를 C/LLVM 양쪽에서 수집하고, step-local approval을 `pgy_zone_authority_validate_flags_export(...)`로 검증해 `authority:<step>` recoverable intent failure와 runtime authority snapshot을 같은 경로로 남긴다
  - 진행: intent `authorized by`는 concrete zone subject slot으로 해석되며, 같은 타입의 non-authority slot 또는 ambiguous same-type slot mapping은 semantic hard error로 닫혔다
  - 진행: concrete direct-slot participant alias는 ambiguous same-type 후보보다 우선한다. `subject slot rogue: Adventurer`가 존재하면 `authorized by rogue`는 concrete authority slot으로 닫히며, 이전 후보가 세운 stale ambiguity flag는 무시된다
  - 회귀: `intent authorized participant must resolve to authority slot`, `intent authorized participant reports ambiguous authority slot`
  - 회귀: `dnd_tavern_campaign` example smoke가 multi-subject same-type zone에서 direct authority aliases를 end-to-end로 고정한다
  - 회귀: `intent_authority_snapshot_abi`, `intent_authority_snapshot`
  - 회귀: `authority_failure_abi`, `authority_failure_surface`, `runtime-authority-contract-test-smoke`
  - 남음: missing-zone/missing-participant 이후의 richer authority mismatch/domain-boundary denial reason도 같은 queryable contract로 확장해야 한다
- [ ] **runtime authority guard downshift**
  - 현재 `pgy_zone_authority_check_export(...)`는 null self/null participant invariant guard다
  - 이 guard 자체는 hard-fail 유지
  - 진행: C inline validator, LLVM runtime export, intent step-local `authorized by` validation 모두 마지막 authority validation 결과를 같은 vocabulary(`last_ok`, `zone`, `participant`, `code`, `reason`)로 남긴다
  - 별도 real authority rejection runtime path가 생기면 그쪽을 `recoverable authority failure` 경로로 설계
- [x] **hard-fail boundary 명시**
  - 완료: `README.md`와 `docs/07_error_handling.md`에 hard-fail boundary를 명시
  - 고정 내용: released slot, invalid token, ownership invariant break, unwrap misuse, bounds violation, runtime invariant guard는 계속 panic / hard-fail territory로 둔다
  - 고정 내용: recoverable authority rejection과 invariant guard를 같은 층으로 섞지 않는다는 점을 문서 wording으로 못박음

- [ ] **projection contract diagnostics 고정**
  - 대상: `refresh/publish/bind` source/target/path/field-map 실패
  - 문제: projection은 언어 강점인데 실패 이유가 약하면 가장 먼저 피로를 줌
  - 고정 기준:
    - target slot / source slot / projection kind / field path / fix가 모두 진단에 들어감
    - structured `Reason:` / `Fix:` formatting을 source-of-truth로 고정
  - 회귀 기준:
    - semantic regression: missing source field / ambiguous path / wrong projection kind / duplicate field map
  - 진행: `projection-diagnostic-contract-test-smoke`가 위 4개 베타 필수 진단 케이스와 `Reason:` / `Fix:` / projection consumer path vocabulary를 semantic regression, implementation, proof doc 기준으로 함께 검사한다

현재 source-of-truth:
- stable example
  - `examples/projection_bind_group_minimal.pgy`
  - `examples/projection_refresh_publish_group_minimal.pgy`
- semantic regression
  - `src/test_semantic.c:test_projection_contract_diagnostics`
  - `make projection-diagnostic-contract-test-smoke`

- [x] **surface trust subset 분류 고정**
  - 대상: generics, own/ref, collections, runtime observability
  - 문제: 되는 것처럼 보이는데 실제로는 subset만 되는 surface가 가장 큰 신뢰 손상 지점
  - 고정 기준:
    - `stable subset / explicit reject / beta-out-of-scope`를 TODO/docs/diagnostic에서 같은 말로 쓴다
  - 회귀 기준:
    - semantic tests와 depth docs가 같은 subset을 가리킴
  - 현재 기준 문서:
    - `README.md`의 `Surface trust policy`
    - `docs/18_language_status.md`
    - `docs/63_feature_depth_matrix.md`
    - `docs/64_depth_filling_roadmap.md`

현재 고정하려는 baseline:
- generics
  - stable subset: exact/ability/multi-bound baseline
  - stable subset extension: default type argument actual resolution on implemented declaration/call/module-consumer paths
  - beta-out-of-scope: broader generic generalization
- own/ref
  - stable subset: classifier-backed own/ref surface on copy values + boundary-visible aggregates + movable values + slot handles
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: arbitrary universal ownership lattice beyond current classifier/summary model
  - beta blocker: 없음
- collections
  - stable subset: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`, `HashMap<Bool, T>`
  - explicit reject: unsupported map key kinds
  - beta-out-of-scope: arbitrary key-universal collection contracts
- runtime observability
  - stable subset: `last / history / active / recent`
  - explicit reject: 없음
  - beta-out-of-scope: richer multi-instance timeline query와 deeper failure provenance query

### P1 — 내부 구조 pain point

- [ ] **declaration-side MIR-only debt 고정**
  - 대상: declaration inventory / metadata helper / duplicated named-decl lookup
  - 문제: routine body는 MIR로 정리돼도 decl-side helper debt가 남으면 parity bug가 반복됨
  - 고정 기준:
    - backend lookup은 공통 inventory helper를 사용
    - 남은 debt는 “기능 미구현”이 아니라 “AST-carried decl metadata 구조 debt”로 분리해서 기록
  - 회귀 기준:
    - LLVM/C backend helper duplication 감소
    - debt ledger와 TODO 표현 정렬
  - 현황:
    - 진행: MIR declaration emit state restore는 helper 하나로 묶였고, role host lookup은 active inventory-only 쪽으로 더 좁아졌다
    - 진행: 조기 return 경로의 `current_host_decl` / `current_func_decl` 복구가 emitter 본문 중복 대신 공용 restore helper를 타게 됐다
    - role / party / roster / relation / effect / zone / world declaration method body의 AST fallback는 제거됨
    - 남은 debt는 declaration inventory / naming helper / named-decl lookup의 구조 정리 쪽으로 축소됨
    - 진행: `emit_func_decl_from_mir_named(...)`가 outer host restore에서 raw saved host-name fallback보다 `saved_host_decl + current_func_decl`를 우선 쓰도록 정렬
    - 진행: host restore/current-host lookup이 inventory에서 host decl을 못 찾으면 raw `current_*_name` 상태를 유지하지 않고 host handle을 비우도록 정렬
    - 진행: `transpiler_restore_host_context_local(...)` 시그니처도 `saved_host_decl` 중심으로 축소해 decl-side restore에서 raw name 인자를 제거
    - 현재 inventory:
      - `src/codegen/transpiler_helpers_core_b.inc`: `current_host_decl_name` 상태 자체와 일부 host naming helper 정리
      - `src/codegen/llvm_pipeline.c`: AST-carried declaration inventory를 담는 `MIRProgram` bootstrap 경로
      - 공통 과제: current_* name 상태와 ad-hoc named lookup를 MIR declaration metadata query로 치환
    - 최근 정리:
      - `current_field_type_name`, `current_host_method_decl`, `find_nominal_host_method_decl`는 active inventory 경유 lookup로 정렬됨
      - transpiler host context 복구는 `current_host_decl -> within_zone -> saved host-name inventory` 순으로 정렬됨
      - transpiler emitter hot path의 direct `current_*_name` 참조는 helper/restore layer 위주로 축소됨
      - LLVM declaration helper / MIR-domain emission / expr-call builtin path도 `llvm_current_host_decl_name(...)`와 bind/restore helper 쪽으로 이동함
      - LLVM `llvm_current_host_decl(...)`는 더 이상 `current_class_name` 재조회 fallback에 의존하지 않고 bound host handle / `within_zone`만을 truth로 사용함
      - `llvm_pipeline.c`의 nominal declaration registration과 class-method enumeration도 raw `decl_header->methods` 직접 접근보다 active nominal inventory / `llvm_find_host_decl_methods_in_context(...)` 경유로 이동함
      - `llvm_register.c`의 active nominal registration도 `mir->decl_headers` 직접 순회 대신 active nominal inventory 기준으로 정렬됨
      - `make mir-declaration-inventory-test-smoke`를 추가해 C/LLVM declaration/domain/nominal active inventory helper seam과 pipeline/domain 소비 경로를 static gate로 고정했다. 새 raw MIR declaration array access는 owner 파일 밖에서 조용히 늘어날 수 없다
      - C backend `emit_program(...)`의 executable metadata도 `mir->has_*` / `mir_find_function_decl(...)` 직접 접근 대신 `transpiler_active_*` helper를 통과하도록 정렬했다
      - C backend `emit_program(...)`의 ability/type/extern/function/intent/domain/event declaration bootstrap 순회도 direct `mir->...` array/count 접근 대신 `transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view를 사용하도록 정렬했다
      - `MIRDeclMethod`는 hosted method identity, routine link, signature metadata까지 담고 LLVM nominal/enum prototype registration은 `llvm_mir_decl_method_*` helper를 통해 이 row를 먼저 소비한다
      - 남은 핵심 debt는 LLVM pipeline의 AST-carried declaration inventory bootstrap와 helper/restore layer 바깥의 raw host-name state 제거

- [x] **ownership vocabulary / payload cleanup 1차 고정**
  - 대상: semantic ownership diagnostics / payload helper family / wording drift
  - 완료:
    - `src/semantic/type_checker_ownership_boundaries.inc`의 ownership helper 9종이 `DiagPayload`/`semantic_emit_payload(...)` 패턴으로 정렬됨
    - semantic direct `semantic_error_with_hints(...)` 호출은 ownership-boundary helper 내부에서 제거됨
    - vocabulary 1차 정리:
      - `anchored handle` → `slot handle (anchored)`
      - `movable resource handle` / `movable resource` → `slot handle (movable)`
      - `capability-bearing` → `authority-bearing` (ownership/domain wording 기준)
    - semantic 회귀는 현재 wording 기준으로 다시 고정됨
  - 검증:
    - `make test-semantic` → `1872 passed, 0 failed`
    - `make test-transpile` → `601 passed, 0 failed`
  - 남은 것:
    - P3 잔여 세분류(`boundary value (subject)` 등) 추가 압축
    - payload/helper family를 ownership 바깥 semantic diagnostics로 더 확장
    - own/ref call/consumer path에서 classifier 기반 trivial copy-only semantics를 더 넓게 적용
    - destructure target binding / nested projection / helper-chain wording을 consumer kind 기준으로 더 세분화

- [ ] **type-resolution DAG 엔진 도입**
  - 대상: semantic type resolution / generic consumer resolution / declaration dependency scheduling
  - 문제: 현재는 `resolve_type_node(...)` 중심의 재귀 해석 + scope lookup + ad-hoc validation이 주축이라, module import graph는 분명하지만 type dependency 자체는 compiler-wide DAG로 관리되지 않는다
  - 최근 진행:
    - `TypeResolutionGraph` inventory + cycle diagnostic + topo derivation은 실제 활성 상태
    - staged worklist는 provider-first 역순 topo 순회로 고정됨
    - local contract / projection synthetic node는 label별 narrow handler로 소비됨
    - generic `default_type` / generic constraint / `where` bound는 staged DAG resolver 경로에 편입됨
    - graph regression은 world lifecycle / relation-effect propagation / generic consumer schedule / alias cycle provenance / generic default-bound cycle provenance / action-intent-zone-party ability consumer provenance까지 포함
    - graph validator cycle과 compatibility alias-resolution cycle이 모두 `Contract source:` / `Reason:` / `Fix:` 구조로 정렬됨
    - 진행: type constraint bound formatter는 `type_checker_type_constraint.c`로 실제 TU 분리 완료
    - 진행: graph node/edge/path/cycle-format primitive는 `type_checker_resolution_graph_core.c`로 실제 TU 분리 완료
    - 진행: named dependency edge recorder와 즉시 cycle diagnostic 발행 경로는 `type_checker_resolution_graph_core.c`로 실제 TU 분리 완료
    - 진행: type-ref dependency recorder도 `type_checker_resolution_graph_core.c`로 이동했고, `find_type_alias_decl`의 cross-include dangling return-type seam을 명시 선언으로 정리
    - 진행: type-ref collector는 `type_checker_resolution_graph_collect.c`로 이동했고, graph core/include 경계의 dangling `static void` seam을 제거
    - 진행: generic contract inventory / string dependency / required ability collector helpers는 `type_checker_resolution_graph_collect.c`로 이동해 declaration collector들의 공통 의존을 TU 경계로 승격
    - 진행: top-level declaration graph registration은 `type_checker_resolution_graph_collect.c`로 이동해 inventory `.inc`를 1,962 LOC까지 축소
    - 진행: local-contract graph node/dependency + zone/world/projection label formatters는 `type_checker_resolution_graph_labels.c`로 이동해 inventory `.inc`를 1,835 LOC까지 축소
    - 진행: projection source resolver는 `type_checker_resolution_graph_domain.c`로 이동하고 `find_zone_domain_slot`을 internal API로 승격해 inventory `.inc`를 1,809 LOC까지 축소
    - 진행: event declaration precollector는 `type_checker_resolution_graph_decl.c`로 이동해 inventory 본체에서 declaration-kind collector를 첫 절단
    - 진행: enum declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고 `semantic_stage_method_array`를 internal API로 승격해 inventory `.inc`를 1,765 LOC까지 축소
    - 진행: ability declaration precollector와 action-contract precollector도 `type_checker_resolution_graph_decl.c`로 이동해 inventory `.inc`를 1,648 LOC까지 축소
    - 진행: role/class/party/roster declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고, relation/effect domain inventory precollector는 `type_checker_resolution_graph_domain.c`로 이동해 inventory `.inc`를 1,299 LOC까지 축소
    - 진행: intent declaration precollector와 world inventory precollector를 각각 `type_checker_resolution_graph_decl.c`, `type_checker_resolution_graph_world.c`로 이동해 inventory `.inc`를 870 LOC까지 축소
    - 진행: zone projection field-map collector를 `type_checker_resolution_graph_zone.c`로 분리했고, 남은 inventory body를 `type_checker_resolution_graph_inventory.c`로 승격해 inventory `.inc`를 제거
    - 진행: world/zone local-contract stage replay를 `type_checker_resolution_stage_domain.c`로 분리하고, 남은 stage 본체를 `type_checker_resolution_stage.c`로 승격해 stage `.inc` 제거
    - 진행: class/extern declaration checker를 `type_checker_class_decl.c`로, top-level semantic orchestration을 `type_checker_program.c`로 분리해 program `.inc`를 624 LOC까지 축소
    - 진행: `ToObject` / `ToTObject` projection checker를 `type_checker_builtins_projection.c`로 분리해 builtins nominal `.inc`를 659 LOC까지 축소
    - 진행: domain helper와 intent helper를 각각 `type_checker_decls_domain_helpers.c`, `type_checker_intent_helpers.c`로 승격해 semantic `.inc` 800 LOC stop condition을 달성하고 `make semantic-inc-size-test-smoke`로 회귀 방지
    - 진행: C backend `transpiler_emitters_mir_inventory_ssa.inc`를 3개 하위 slice로 분리하고 `make test-transpile`, `make llvm-test-backend-compare`로 parity 회귀 통과
    - 진행: standalone TU 승격 중 드러난 dangling return-type seams와 implicit helper dependency를 제거해 `make test-all`, `make llvm-test-backend-compare` 회귀 통과
    - 진행: implicit declaration / implicit int는 기본 CFLAGS에서 에러로 고정되어 이후 DAG/semantic split 중 hidden helper dependency가 즉시 실패하도록 정렬
    - 진행: `type_resolution_intern_node` / `type_resolution_add_edge` / `type_resolution_find_path` / `type_resolution_format_cycle`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격
    - 진행: DAG stage 안에서 retired resolver compatibility surface를 `PGY_TYPE_RES_STATS=1` 통계에 노출했다. 현재 beta gate는 `stage-compat-family`의 alias/non-alias fallback을 모두 0으로 고정하고, graph stats와 topo validation을 함께 확인한다. 중앙 metadata materializer의 마지막 recursive escape hatch도 제거되어 unsupported shape는 explicit fallback inventory로만 기록된다
    - 진행: type-alias stage는 metadata-only lookup으로 성공 경로를 materialize하고, 실패 경로는 recursive resolver 없이 diagnostic unresolved inventory로 분리한다. `make type-resolution-dag-test-smoke`는 alias compatibility fallback 0, alias materialization 존재, alias diagnostic unresolved accounting을 함께 gate한다
    - 진행: DAG edge가 이미 존재하는 named type-ref는 generic argument를 포함해 stage에서 `resolve_type_node(...)`를 다시 호출하지 않고 graph-backed skip으로 처리한다. `stage-graph-backed: skips=N` 통계가 추가됐고 `type-resolution-dag-test-smoke`가 skip 합계가 0으로 퇴행하지 않는지 검사한다
    - 진행: graph precollect TU 안에서 enum methods가 `semantic_stage_method_array(...)`를 호출하던 impurity를 제거했다. 이제 enum method signature/contract도 precollect action contract 경로로만 graph edge를 수집한다
    - 진행: DAG stage helper를 `type_checker_resolution_stage_lookup.c` / `type_checker_resolution_stage_stats.c`로 분리했고, 이후 alias/nominal/systemic/domain-decl replay owner를 각각 `type_checker_resolution_stage_alias.c`, `type_checker_resolution_stage_nominal.c`, `type_checker_resolution_stage_systemic.c`, `type_checker_resolution_stage_domain_decl.c`로 분리했다. `type_checker_resolution_stage.c`는 88 LOC top-level dispatch owner가 됐고, graph precollect, stage lookup, stage stats, alias diagnostics, and stage replay families are now separated by file boundary
    - 진행: generic where/default validation은 `type_checker_generic_validation.c`로 이동했다. `type_checker_resolution_graph_*.c`와 `type_checker_resolution_graph_core.inc`는 더 이상 `resolve_type_node(...)`를 직접 호출하지 않으며, `semantic-core-shape-test-smoke`가 이 resolver-free graph-layer 경계를 검사한다
    - 진행: graph precollect가 context-independent builtin type refs(`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `QubitSlot`, `Void`)를 `SemanticContext.type_resolution_metadata`에 기록한다. owner resolver seams는 이 metadata를 먼저 조회하고, unsupported shape는 explicit fallback inventory로 기록될 뿐 recursive fallback으로 내려가지 않는다
    - 진행: graph metadata가 resolver-stable constructed/anchored-handle shells(`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`, `Box<T>`, `Rc<T>`, `Weak<T>`, `Channel<T>`, `Future<T>`, `RemoteFuture<T>`, `Token<T>`, `DeviceSlot<T>`, `HashMap<String|Int|Long|Bool, T>`, `Option<T>`, `Result<T,E>`, `Slot<T>`, `SecureSlot<T>`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`)를 materialize할 수 있다. graph가 만든 `Type` shell은 metadata owned lane으로 기록하고 semantic context destroy에서 해제한다
    - 진행: graph metadata가 tuple shell과 event-handler/function shell도 materialize한다. channel/future AST node는 inner fact collect 직후 constructed shell을 기록하므로 recursive fallback에 덜 의존한다
    - 진행: `resolve_type_node(...)` wrapper 자체가 metadata-first가 되어, 남은 explicit compatibility allowlist도 recursive materialization 전에 DAG facts를 먼저 소비한다
    - 진행: `resolve_generic_type_arg(...)`도 metadata-first 조회 후 fallback으로 내려간다. constructed builtin/generic consumer path의 recursive resolver 의존 면적을 줄였다
    - 진행: owner-local resolver seams는 `semantic_type_resolution_lookup_or_materialize(...)` 공용 materializer로 수렴했다. resolver 구현체 밖에서 직접 `resolve_type_node(...)`를 호출하면 `type-resolution-resolver-inventory-test-smoke`가 실패한다. Central metadata owner도 `type_checker_resolution_metadata_diagnostics.c`를 분리해 stable-shell arity, invalid constructed HashMap key, unknown bare named diagnostics를 별도 owner가 맡고, alias-chain/cycle materialization은 `type_checker_resolution_metadata_alias.c`가 맡는다. central metadata materializer recursive escape hatch는 제거됐고 central metadata owner는 268 LOC, alias owner는 315 LOC로 분리됐다. 낡은 `resolve_type_alias_decl(...)`와 `SemanticContext.alias_resolution_*` stack도 제거되어 direct named alias resolution은 metadata alias owner만 통과한다. `resolve_named_type(...)` itself is now metadata-first for stable builtin/scope/generic/nominal/alias names, and the resolver-inventory smoke rejects recursive alias resolver debt if it reappears
    - 진행: party/role ability lookup은 `type_checker_domain_role_lookup.c`로 분리했다. 이후 projection contract diagnostics와 overlay scope setup도 각각 `type_checker_domain_projection.c` / `type_checker_overlay_common.c`로 분리되어 `type_checker_decls_domain_helpers.c`는 972 LOC까지 낮아졌다. 남은 helper owner는 zone/effect/relation slot helper 책임에 집중한다
    - 진행: `type-resolution-dag-test-smoke`가 graph-backed skips뿐 아니라 retired compatibility resolver call cap, metadata entries/owned/hits, metadata materializer fallback count, zero stage metadata materialization, alias-stage split accounting을 검사한다. 최신 local stats: `graph-backed skips=3140 retired_resolver_calls=0 retired_resolver_unique_nodes=0 metadata_entries=3436 metadata_owned=257 metadata_hits=6756 materializer_fallbacks=0 stage_materialize_alias=0 stage_materialize_non_alias=0 alias_materialized=6 alias_diagnostic_unresolved=78 alias_diagnostic_resolver_calls=0 alias_diagnostic_resolved=0 alias_diagnostic_cycle_unresolved=78`
    - 진행: DAG smoke는 이제 graph-backed skip/metadata entry/metadata hit/owned metadata가 단순히 0보다 큰지만 보지 않고 beta floor(`skips>=3000`, `entries>=1500`, `hits>=2400`, `owned>=45`)와 retired compatibility resolver cap(`retired_resolver_calls<=0`)를 검사한다. DAG source-of-truth 사용량이 크게 후퇴하면 CI에서 즉시 잡는다
    - 진행: 중앙 metadata materializer의 마지막 recursive fallback은 0으로 닫혔다. `type-resolution-dag-test-smoke`는 `materializer_fallbacks==0`과 모든 metadata unresolved audit family 0을 고정한다
    - 진행: stage metadata materialization surface는 alias/non-alias 모두 0으로 고정됐다. `type_checker_resolution_stage_alias.c`가 unique alias diagnostic unresolved accounting과 optional trace를 소유한다. 성공 alias materialization과 diagnostic unresolved inventory를 별도 계측하고, 남은 78건은 recursive resolver 재진입이 아니라 alias-cycle diagnostic coverage에서 나오는 unresolved inventory다. `alias_diagnostic_resolver_calls==0` gate가 이 경계를 차단한다
    - 진행: program-level symbol inventory가 ability declarations도 predeclare한다. `type_check_ability_decl(...)`은 자기 자신의 predeclare만 재사용하고 같은 이름의 다른 ability는 기존처럼 duplicate diagnostic으로 처리한다. forward source order에서 generic default/where, zone authority, party role-slot ability consumer가 provider 후행이어도 통과하는 regression을 추가했다
    - 진행: `tests/cases/backend_compare/forward_ability_order/main.pgy`를 backend compare suite에 추가했다. provider-after-consumer generic default/alias/zone-authority/party-role-slot ability ordering이 semantic-only가 아니라 C/LLVM 출력 동등성까지 유지되는지 검사한다
    - 진행: `tests/compare_backends.sh` 기본 실행은 `tests/cases/backend_compare/*/main.pgy`가 default case array에 빠져 있으면 실패한다. 명시 인자 기반 targeted run은 유지하되, CI/default path에서 새 parity case가 조용히 누락되는 drift를 차단했다. 이 gate로 기존 passing case 8개(array builtins/inline access, slice inline access, intent observability rollback, list/map/queue get-string, try-operator result)를 default C/LLVM parity suite에 편입했다
    - 진행: `type-resolution-resolver-inventory-test-smoke`가 direct resolver allowlist와 함께 metadata-first wrapper, execution/anchored-handle metadata materializer coverage를 static gate로 고정한다
    - 진행: `type-resolution-resolver-inventory-test-smoke`가 새 `semantic_type_resolution_resolve_or_fallback(...)` 사용자를 금지하고 named fallback seam 총량을 0개로 고정한다. gate 출력은 현재 fallback seam count를 직접 보여주며, `semantic_type_resolution_lookup_or_materialize(...)` 내부의 central recursive escape hatch도 0개로 고정한다
    - 진행: fallback seam gate의 기존 하한선(`30개 미만이면 실패`)을 debt-reduction에 맞지 않는 규칙으로 보고 제거했다. 이제 0개 상한만 growth guard로 유지하며, seam 축소는 CI 성공 경로다
    - 진행: `type_checker_module_contract.c`의 ability contract bookkeeping은 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only seam으로 낮췄다. ability 존재/visibility/generic arity/where provenance는 ability-specific validator가 계속 소유하며, fallback seam inventory는 39에서 38로 감소했다
    - 진행: `type_checker_ability_fields.c`의 ability `fields` requirement validation도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. field contract diagnostics는 ability-specific validator가 계속 소유하며, fallback seam cap은 32에서 31로 감소했다
    - 진행: `type_checker_builtins_projection.c`의 projection target-field resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. projection field diagnostics는 projection validator가 계속 소유하며, fallback seam cap은 31에서 30으로 감소했다
    - 진행: `type_checker_program.c`의 quiet top-level placeholder resolver는 graph precollect 이후 metadata lookup-only로 전환했다. event/function forward placeholders가 recursive fallback 없이 precollected DAG facts를 소비하면서 fallback seam cap은 30에서 29로 감소했다
    - 진행: `type_checker_builtins_query_domain.inc`의 projection source-field resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. HasProjection/HasZoneProjection 계열 field diagnostics는 domain query validator가 계속 소유하며, fallback seam cap은 29에서 28로 감소했다
    - 진행: `type_checker_party_decl.c`와 `type_checker_roster_decl.c`의 shared-field type resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. party/roster shared field diagnostics는 각 declaration validator가 계속 소유하며, fallback seam cap은 28에서 26으로 감소했다
    - 진행: `type_checker_ability_decl.c`의 abstract method signature resolver와 `type_checker_role_decl.c`의 host-type resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. ability/role declaration diagnostics는 각 owner validator가 계속 소유하며, fallback seam cap은 26에서 24로 감소했다
    - 진행: function/action body precollector가 local let / with-slot annotation뿐 아니라 expression subtree, call type args, lambda param/return/body, event subscription handler, spawn/channel/return/branch expressions까지 따라간다. 이 기반으로 `type_checker_event.c`의 event/lambda handler type-ref resolver를 DAG metadata lookup-only로 낮췄고 fallback seam cap은 24에서 23으로 감소했다. `type_checker_flow.c`의 flow-local type resolver도 DAG metadata lookup-only로 낮춰 cap은 22로 감소했다. `type_checker.c`의 type-alias statement resolver도 DAG metadata lookup-only로 낮춰 cap은 21로 감소했다
    - 확인된 남은 blocker: `type_checker_program.inc`의 function body param/return/domain-slot materialization seam은 단순 lookup-only로 낮추면 direct semantic unit path에서 graph metadata bootstrap 없이 segfault가 난다. 이 seam은 direct semantic unit bootstrap 또는 null-safe diagnostic path가 먼저 필요하다
    - 확인된 남은 blocker: `type_checker_intent_decl.c`의 intent participant/value/where resolver seam은 단순 lookup-only로 낮추면 semantic suite 후반 parallel execution path에서 segfault가 난다. intent declaration은 graph precollect가 있지만 direct semantic/bootstrap path와 step/local binding materialization이 아직 lookup-only 계약을 만족하지 않으므로 explicit fallback seam으로 남긴다
    - 확인된 남은 blocker: `type_checker_helpers_host.inc`의 host helper resolver는 단순 lookup-only로 낮추면 intent/zone authority positive path가 subject-slot type metadata 부족으로 무너진다. 이 seam은 zone/world/host subject-slot nominal metadata를 DAG에 보존한 뒤 제거해야 한다
    - 완료: `type_checker_generic_validation.c`의 generic where/default validation resolver는 generic default effective-arg fact와 where-bound provenance가 DAG metadata에 올라온 뒤 metadata-only lookup으로 전환했다. resolver inventory gate가 이 owner의 materializing helper 재도입을 차단한다
    - 확인된 남은 blocker: `type_checker_generic_support.inc`의 boundary type helper seam은 단순 lookup-only로 낮추면 `ref class` / `ref subject` escape diagnostics 150개가 빠진다. 이 seam은 generic/nominal boundary category fact와 ref/own escape classifier가 DAG metadata에서 같은 type category를 볼 수 있을 때 제거해야 한다
    - 확인된 남은 blocker: `type_checker_ability_where.c`의 ability where-bound resolver는 단순 lookup-only로 낮추면 generic ability multi-bound mismatch provenance가 사라져 `Cloneable` bound mismatch 진단 회귀가 난다. 이 seam은 ability where-bound effective-arg / multi-bound provenance fact를 DAG metadata에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_operator_expr.inc`의 operator overload method signature resolver는 단순 lookup-only로 낮추면 semantic suite가 event/misc path 진입 전후에 segfault할 수 있다. 이 seam은 method param/return signature metadata와 operator overload candidate summary를 DAG에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_zone_decl.c`의 zone authority subject-slot type seam은 단순 lookup-only로 낮추면 generic ability mismatch provenance가 사라진다. 이 seam은 zone authority generic ability fact를 DAG metadata에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_class_decl.c`의 class/vessel field resolver는 단순 lookup-only로 낮추면 vessel/subject-vessel field acceptance가 깨진다. 이 seam은 class/vessel field nominal flavor metadata를 DAG에 보존한 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_world_decl.c`의 shared/domain-slot resolver는 단순 lookup-only로 낮추면 zone/world/intent positive paths가 `subject slot ... requires a subject type`로 무너진다. 이 seam은 world domain-slot subject/zone nominal materialization을 DAG metadata에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_ownership_let.c`의 let annotation resolver는 단순 lookup-only로 낮추면 direct semantic unit path에서 graph metadata 없이 `ClaimSlot` annotation이 들어와 segfault할 수 있고, broader program path에서는 `Slot`/`ReadView`/`WriteView`/`QubitSlot`/anchored own-ref paths가 `<unknown>`으로 무너질 수 있다. 이 seam은 direct semantic unit bootstrap 또는 null-safe diagnostic path와 anchored-handle constructed-type metadata coverage를 같이 닫은 뒤 제거해야 한다
    - 진행: domain/intent declaration resolver는 owner-local type-ref seam으로 수렴했다. slot/shared/named domain refs와 intent involves/value/where refs가 각각 하나의 owner seam을 공유하면서 fallback seam inventory는 38에서 34로 감소했다
    - 진행: alias/generic-parameter helper와 resolution-stage diagnostic fallback도 owner-local seam으로 수렴했다. fallback seam inventory는 34에서 32로 감소했다
    - 진행: zone authority participant resolver가 exact/qualified-tail direct slot match를 먼저 인정하고, direct match 반환 시 stale ambiguity flag를 지운다. 같은 타입 subject slot이 여럿 있어도 `authorized by rogue`가 실제 `subject slot rogue: Adventurer`로 concrete하게 닫히면 false-positive ambiguous로 떨어지지 않는다
    - 진행: `type_checker_intent_decl.c`의 participant/value/where local seam 3개는 graph metadata-first 조회 후 recursive fallback으로 내려간다
    - 진행: `type_checker_decls_domain_helpers.c`의 slot/shared/named-ref local seam 3개는 graph metadata-first 조회 후 recursive fallback으로 내려간다
    - 진행: `type_checker_intent_helpers.c`의 direct resolver 호출은 `intent_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. transfer-derived using/where, ability generic arg, role-field checks는 이 seam을 통해 다음 DAG metadata 전환을 탄다
    - 진행: `type_checker_helpers_host.inc`의 direct resolver 호출은 `host_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. projection source fields, hosted method return/param, zone authority/domain slot checks는 이 seam을 통해 다음 DAG metadata 전환을 탄다
    - 진행: `type_checker_program.c`의 forward-declaration type materialization은 quiet resolver seam 1개로 수렴했고, `type_checker_program.inc`의 function-body param/return/domain-slot materialization body resolver seam은 graph metadata-first 조회 후 fallback으로 내려간다
    - 진행: `type_checker_event.c`의 event signature/lambda handler materialization은 graph-backed metadata lookup-only로 전환됐다. 다음 DAG slice는 ownership let / zone authority / world domain-slot / ability where-bound처럼 semantic provenance가 남은 owner seams다
    - 진행: `type_checker_world_decl.c`의 shared field/domain slot materialization은 `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam으로 수렴했다. world shared/slot checks는 이 seam에서 graph-backed metadata로 교체할 수 있다
    - 진행: `type_checker_role_decl.c`, `type_checker_generic_contracts.inc`, `type_checker_helpers_late.c`, `type_checker_expr.inc`의 직접 resolver 호출도 각각 role/generic-contract/late-helper/expr local seam 1개로 수렴했다
    - 진행: `type_checker_generic_validation.c`, `type_checker_ability_where.c`, `type_checker_module_contract.c`, `type_checker_ability_decl.c`, `type_checker_class_decl.c`, `type_checker_operator_expr.inc`, `type_checker_ownership_destructure_stmt.inc`도 local resolver seam으로 수렴했다. 남은 direct count는 대부분 resolver 본체, 주석, 또는 명시 seam이다
    - 진행: `type_checker.c`, `type_checker_ability_fields.c`, `type_checker_builtins_projection.c`, `type_checker_builtins_query_domain.inc`, `type_checker_flow.c`, `type_checker_generic_support.inc`, `type_checker_helpers_effects.inc`, `type_checker_ownership_let*.inc`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`, `type_checker_zone_decl.c`의 단발 direct resolver 호출도 local seam으로 수렴했고, zone domain-slot seam은 graph metadata-first 조회를 사용한다
    - 완료: `make type-resolution-resolver-inventory-test-smoke`를 추가해 새 `resolve_type_node(...)` 직접 호출이 resolver 본체/stage metadata materialization/core fallback/local seam allowlist 밖에 생기면 실패하도록 고정했다. `ci-linux`에도 연결했다
    - 검증: 2026-04-25 local WSL/Linux `make ci-linux` full green. Windows/MSYS2 native runner는 이 머신에 없으므로 별도 CI 환경 acceptance line으로 유지
  - 목표:
    - import graph와 별개로 `type provider -> type consumer` 그래프를 분리 구축한다
    - declaration / alias / generic default / where-bound / ability consumer / zone authority consumer를 DAG node/edge로 승격한다
    - namespace-only reference나 declaration inventory 조회가 불필요한 concrete type materialization을 강제하지 않게 한다
    - cycle는 generic/alias/type consumer path 기준으로 path-aware diagnostic으로 보고한다
    - incremental compile 시 invalidation 범위를 declaration/type dependency 단위로 줄인다
  - 1차 구현 원칙:
    - 기존 `resolve_type_node(...)`를 한 번에 폐기하지 않는다
    - 먼저 graph inventory + topo scheduling + cycle diagnostic을 추가하고, 그 다음 recursive resolver를 graph-backed evaluator로 치환한다
    - import/module loader의 DFS cycle detection과 type-resolution DAG를 혼합하지 않는다
  - 단계:
    - Phase A: declaration/type provider inventory와 consumer edge 수집
    - Phase B: topo evaluation + SCC/cycle diagnostic 고정
    - Phase C: generic default arg / multi-bound / ability consumer / zone authority를 DAG consumer로 편입
    - Phase D: incremental invalidation / cache / backend-facing resolved metadata 재사용
  - 회귀 기준:
    - dependency loop diagnostic에 cycle path/provenance가 나온다
    - graph-backed cycle과 alias fallback cycle 모두 `Contract source:`를 포함한다
    - namespace-only reference는 불필요한 full type materialization을 유발하지 않는다
    - generic consumer/default/bound resolution이 graph-backed evaluation에서도 기존 semantic 계약과 같은 결과를 낸다
    - C/LLVM compile path가 동일한 resolved-type metadata를 재사용한다
    - `PGY_TYPE_RES_STATS=1`에서 stage graph-backed skip 수, compatibility fallback 호출량, family breakdown, suppressed diagnostic 수가 보인다. 이 값은 남은 DAG migration debt의 직접 지표이며 숨겨진 fallback을 추가하면 smoke에서 즉시 드러나야 한다

- [x] **runtime observability baseline vs richer query 구분 고정**
  - 대상: `IntentLast* / IntentHistory* / IntentActive* / IntentRecent*`, zone/world inspection
  - 문제: baseline이 이미 있는데 문서가 thin이라고 쓰면 반대로 surface trust를 깎음
  - 고정 기준:
    - baseline observability는 complete로, richer timeline/provenance는 open debt로 분리
  - 회귀 기준:
    - docs/board/status 문구 일치
    - observability regression이 baseline API를 계속 고정

## 완료 (P0 — 즉시 수정)

- [x] **`system()` 명령 주입 제거** — `_spawnvp`/`execvp`로 교체, 경로 검증 추가 (`pgy_path_is_safe`)
- [x] **AES-256 실구현** — XOR 가짜 암호를 FIPS 197 AES-256-CTR + HMAC-SHA256 인증으로 교체 (외부 의존성 없음)
- [x] **`auto __tmp` 제거** — `PGY_RESULT_TRY` 매크로에서 GCC 확장 `auto` 제거, C11 호환 (명시적 타입 파라미터)
- [x] **REPL 고정 파일명** — `_pgy_repl_tmp.*` → `TMPDIR/pgy_repl_{pid}.*` (PID 기반 유니크 경로)
- [x] **`type alias` vertical slice** — `type UserId = Int;` parser/semantic/C/LLVM lowering 연결, 실전 annotation/typedef 경로 확보

## P1 — 다음 단계

- [ ] **CI 하드닝** — Ubuntu + Windows 빌드 매트릭스 유지, AddressSanitizer/UBSan, 더 촘촘한 smoke coverage
- [ ] **CodeQL + secret scanning 활성화** — C/C++ 분석 모드, push protection
- [x] **CHANGELOG.md + 버전 정책 수립** — SemVer, 릴리스 태깅 규칙
  - 완료: `CHANGELOG.md` 존재, Keep a Changelog 포맷, SemVer 명시
- [x] **SECURITY.md** — 보안 취약점 제보 채널, 책임 있는 공개 정책
  - 완료: `SECURITY.md` 생성 (2026-04-18). 지원 버전, 보고 채널, in/out scope, 공격 표면별 mitigation, advisory format 포함

## P1.5 — 언어/컴파일러 보강

- [ ] **MIR DCE statement-level 확장**
  - 현재는 dead SSA/PHI 제거 + `HasState`/`ChannelLength`류 pure-query stmt 제거까지는 동작함
  - 남은 단계: pure expression stmt / dead call / dead resource-op / carrier stmt를 더 세분화하고, side-effect lattice 기준으로 제거 정책을 정교화
  - 목표: MIR-only emitter가 기대하는 metadata carrier를 잃지 않으면서도 불필요한 stmt 제거 범위를 넓힘

- [x] **IR 계층 설계 검토** — HIR/DIR/RIR/MIR 분리 타당성 평가
  - **DIR 유지 결정**: intent domain structure 검증에 필수 (step dependency, zone binding, post-condition)
  - **RIR 유지 결정**: resource state lattice (20-state)는 slot/projection/authority lifecycle 검증에 필요
  - **MIR 유지 결정**: SSA/CFG/cleanup edge는 intent compensation execution path에 필수
  - ~~남은 과제~~: Backend를 HIR 기반 → MIR 기반으로 전환해야 IR 투자 ROI 실현 → **완료**
  - 참고: Rust도 AST→THIR→MIR→LLVM 4단계, Pergyra는 AST→HIR→DIR→RIR→MIR→Backend 6단계
  - DIR은 domain graph로 HIR와 구조가 달라 별도 IR로 유지하는 것이 타당
  - RIR 20-state lattice는 단순화 가능성 검토 (현재: Owned/Borrowed/Synced/Dirty/Stale/Published/Authorized 등)
- [ ] **ability 기반 연산자 dispatch 고도화** — 현재는 `role/impl ability` 메서드에서 `operator_<suffix>_<Type>` alias를 합성해 C/LLVM이 정적으로 호출하는 방식. 장기적으로는 ability/vtable 기반의 직접 dispatch와 더 정교한 overload 우선순위 규칙이 필요
- [ ] **LLVM 연산자 오버로드 회귀 테스트 확장** — 현재 스모크는 `role IntMath for Int` 1건 중심. 비교 연산, 포함된 role, enum/custom type, namespace 경로까지 자동 테스트 확대

## P1.58 — 표준 라이브러리 인프라

- [x] **`use datetime;` 실제 stdlib module화**
- [x] **`use http;` v0.1**
  - `HttpRequest`, `HttpResponse`, `RouteSpec`
  - `OkResponse`, `ErrorResponse`, `JsonResponse`
  - intent adapter handler 예제와 연결
- [x] **`use storage;` v0.1**
  - `SnapshotMeta`, `SnapshotRecord`
  - `StorageSave`, `StorageLoad`, `StorageAppendLog`
  - world/session snapshot 예제와 연결
- [x] **`use page;` v0.1**
  - `PageRoute`, `PageAction`, `PageMessage`
  - `MountPage`, `BindAction`, `RenderSection`
  - projection surface / action binder 예제와 연결
- [x] **쇼핑몰 예제를 stdlib 인프라 사용 버전으로 리프트**
  - `pages/` -> `use page;`
  - `api/` -> `use http;`
  - `report/storage` -> `use storage;`

- [ ] **`pgy scaffold project`에 app-infra starter 추가**
  - intent-first layout + `intents/ subjects/ zones/ world.pgy main.pgy`
  - optional `pages/ api/ report/` app adapter starter

## P1.58 — 표준 라이브러리 개선 (2026-04-06 분석)

- [ ] **stdlib page.pgy 실제 렌더링/컴포넌트 시스템으로 확장**
  - 현재: 단순 데이터 구조 + 렌더링 문자열 함수만
  - 목표: 페이지 라이프사이클(마운트/언마운트/업데이트), 컴포넌트 트리, 상태 관리
  - 제안: `Component` abstract base, `mount()`, `render()`, `update()`, `unmount()` 라이프사이클 훅
- [ ] **stdlib storage.pgy WriteFile 추상화**
  - 현재: `WriteFile` 내장 함수 직접 호출 → 플랫폼 의존성
  - 목표: Slot/Device 인터페이스로 분리 (`StorageDevice` ability)
  - 제안: `ability StorageDevice { Write(path, data) -> Result<Void, Error>; Read(path) -> Result<String, Error> }`
- [ ] **stdlib 전반 Result<T, Error> 패턴 활용**
  - 현재: `WriteFile`, `ReadFile` 실패 시 크래시 가능성
  - 목표: 모든 I/O 연산이 `Result<T, Error>` 반환
  - 제안: `?` 연산자와 조합해 에러 전파 자동화
- [ ] **datetime.pgy 메서드 일관성 개선**
  - 현재: `export class LocalDate` + `export func SameDate()` 혼재
  - 제안: 메서드 일관성 (`a.SameDate(b)` vs `SameDate(a, b)`) — 하나만 남기거나 둘 다 문서화

## IR 파이프라인

- [x] **DIR code layer 시작**
  - declaration graph
  - intent participant/step edge
  - role/ability completeness edge
- [x] **RIR code layer 시작**
  - explicit resource/projection/authority/capability/intent-policy fact
  - explicit resource op
  - scope-level normalized state summary
  - HIR-enriched branch/join `flow-block[...]` lattice summary
- [x] **MIR code layer 시작**
  - block/instruction skeleton
  - phi materialization
  - block-local SSA rename
  - instruction-level `def/use` 시작
  - rollback/invalidation exceptional CFG 시작
- [ ] **RIR lattice propagation 심화**
  - relation/effect/zone/world handle merge는 시작됨, conditional handle invalidation과 world-handoff lattice를 더 밀기
  - conditional authority/projection invalidation fact 확장
- [ ] **MIR full SSA / flow merge**
  - block-level version map은 시작됨, rename을 full def-use chain/liveness 수준으로 확장
  - cleanup convergence root는 시작됨, MIR-level `RIR-flow` merge와 cleanup convergence policy를 더 고도화
- [ ] **MIR DCE 확장 (statement-level)**
  - dead DEF/PHI 제거를 넘어 side-effect-free STMT/unused call 제거
  - 현재는 pure query builtin (`Has*`, `ChannelLength/Capacity/Space/Full/Closed`)만 안전 제거 시작
  - `unused pure let initializer` 제거는 source-local/runtime-backed storage와 충돌해 다시 보류
  - dead identifier-assign 제거는 loop/phi/live-out 오판이 남아 있어 계속 보수 보류
  - 다음 reopen 조건: value summary의 block-boundary / phi provenance를 이용해 loop-carried DEF와 진짜 dead local DEF를 분리
  - user call purity는 아직 보수적으로 side-effect 있다고 간주
  - RESOURCE_OP/CLEANUP_EDGE/abort/IO 등 side-effect 보존 규칙 명시
  - RPO 기반 liveness와 결합해 제거 정확도 개선
## P2.0 — Backend MIR 기반 전환 ✅ 완료

- [x] **emit_program()을 HIR 기반 → MIR 기반으로 전환**
  - **완료**: `emit_func_decl_from_mir_named()` 완전 구현
  - **결과**: MIR routine → SSA locals + CFG → C 코드 생성
  - **지원 기능**:
    - Intent compensation (cleanup blocks)
    - SSA versioned locals (`_pgy_ssa_name_N`)
    - PHI 노드 복사 (join block 진입)
    - BRANCH → if/else gotos
    - RESOURCE_OP → 런타임 함수 호출
  - **테스트**: 428 passed, 0 failed (기존 403 passed, 5 failed)
  - **아키텍처**:
    ```
    Domain IR:   Intent Recover → policy exclusive → step Heal → zone main → participant unit
    Resource IR: IntentBegin I1 → ConflictCheck exclusive → BindZone main → CallAction Recover
    MIR:         bb0: conflict_check(unit) → br !r0, bb_fail, bb1
                 bb1: call recover(unit) → call sync_projection(main, unit)
                 bb_commit: intent_commit(I1) → ret true
                 bb_fail: intent_abort(I1) → ret false
    ```

## P2.1 — LLVM 백엔드 MIR 기반 전환 ✅ 완료

- [x] **LLVM 백엔드 MIR 기반 전환 완료**
  - `src/codegen/llvm_pipeline.c`: MIR routine → LLVM IR 직접 생성
  - `src/codegen/llvm_mir_emit.c`: `llvm_emit_func_from_mir()` 완전 구현
  - SSA locals, PHI nodes, branch terminators, intent compensation 모두 지원
  - 기대 효과 달성: LLVM 최적화 패스 완전 활용, C/LLVM 백엔드 아키텍처 통일
  - C/LLVM 둘 다 MIR 기반으로 통일 → IR 투자 ROI 실현

## P1.55 — 언어 기능 확장

### 기반 타입 시스템
- [x] **태그드 유니언 (enum with data)** — `enum Shape { Circle(Int), Rect(Int, Int) }` 데이터를 가진 enum
  - 완료: variant payload 파싱, variant 생성자 타입 추론, C tagged union / LLVM discriminated struct, LLVM tagged-union regression 및 예제 실행
- [x] **Option<T> / None** — "상자가 비어있을 수 있다"를 타입으로 표현. `-1` sentinel 제거
  - 완료: `Option<T>` constructed type, `Some/None`, `IsSome/IsNone/UnwrapOption`, C/LLVM lowering
  - 완료: `match opt { case Some(v): ... case None: ... }` destructuring
- [x] **디스트럭처링 (SecureSlot)** — `let (slot, token) = ClaimSecureSlot<Int>(lvl)` 패턴 바인딩
  - 완료 (2026-04-19): 파서 `ClaimSlot`/`ClaimSecureSlot` 뒤의 `<T>`를 더 이상 버리지 않고 `AST_CALL.generic_args`에 첨부 (일반 call-site 제네릭 인프라), 시맨틱이 destructuring에서 이 generic arg로 SYMBOL_SLOT + SYMBOL_TOKEN 쌍 등록, MIR emit이 `PgyToken_T token; PgySecureSlot_T slot = pgy_claim_secure_T(&token);` 출력, `transpiler_find_local_type_name_in_block`이 바인딩별 `SecureSlot<T>`/`Token<T>` 반환해 MIR header의 타입 예약 정리, SSA 맵에 self-mapping 등록으로 emission contract 통과
  - 파일: `src/parser/ast.h`, `src/parser/ast.c`, `src/parser/parser.h`, `src/parser/parser_expr.c` (제네릭 인자 보존), `src/semantic/type_checker.c` (destructuring 시맨틱), `src/codegen/transpiler_emitters_base_a.inc` (MIR-level claim emit + ssa map 등록)
  - 회귀: `src/test_transpile.c` "let (slot, token) = ClaimSecureSlot<T>(lvl) emits paired claim"
  - SecureSlot MIR auto-Read + claim 토큰 emit 연관 버그 수정 (2026-04-19): (a) SSA-aware identifier 경로가 `suppress_slot_auto_read` 무시하던 버그로 `pgy_secure_write_Int(&pgy_read_Int(&slot),...)` 같은 잘못된 C 출력 — `!ctx->suppress_slot_auto_read` 가드 추가 + Secure 경로에서 `pgy_secure_read_*` 분기. (b) MIR DCE가 `AST_LET_DECL`을 부작용 없음으로 판정해 제거하던 버그 — `mir_stmt_has_side_effect`에 추가. (c) `transpiler_emit_mir_resource_op` Claim 룰이 SecureSlot에도 `pgy_claim_secure_T()`만 emit하고 토큰은 생략하던 버그 — `PgyToken_T anchor_token;` + `= pgy_claim_secure_T(&anchor_token)` 방식으로 수정. (d) `Token<T>`도 "claim shape"로 인식해 MIR header pre-decl 건너뛰도록 `transpiler_type_name_is_claim_shape` 도입 (slot-like와는 구별 — auto-Read는 여전히 Slot 전용). 결과: destructuring + 비-destructuring SecureSlot 모두 E2E 동작 (`Write/Read/Release` 포함)
  - 파일: `src/compiler/mir.c` (DCE), `src/codegen/transpiler_expr_emitters.inc` (suppress 가드), `src/codegen/transpiler_emitters_base_a.inc` (claim_shape 분리), `src/codegen/transpiler_emitters_base_b.inc` (MIR header 체크), `src/codegen/transpiler_helpers.inc` (claim 토큰 emit), `src/parser/parser_decl.c` (class-body destructuring 에러 메시지)
  - 미처리: LLVM 백엔드 SecureSlot destructuring (LLVM은 이미 "requires explicit annotation" 에러 — 별도 세션), class-body destructuring (`private let (slot, token) = ClaimSecureSlot()`는 명확한 에러 메시지로만 처리 — 별도 세션)
- [x] **튜플 반환 타입 + 디스트럭처링** — `func f() -> (Int, String)` 및 `let (n, s) = f()` 지원
  - 완료 (2026-04-19): Type 인프라에 `TYPE_KIND_TUPLE` 활성화 (union에 `tuple.elements/element_count` 필드 + `type_create_tuple`/`type_is_tuple`/`type_tuple_arity`/`type_tuple_get_element`), AST_TYPE에 `tuple_elements` 필드로 `(T, U, ...)` 표현, `AST_TUPLE_LITERAL` 신규 노드로 `(a, b, ...)` 표현식 지원
  - 파서: `parse_type()`에 `LPAREN` 분기로 튜플 타입 구문 처리 (단일 `(T)`는 기존 `T`로 환원, 빈 `()`는 `Void`, 2개 이상일 때만 튜플), `parser_parse_primary`의 괄호 표현식 경로에 콤마 감지 시 튜플 리터럴로 분기
  - 시맨틱: `resolve_type_node`에 tuple 분기 추가 → `type_create_tuple` 반환, `type_check_expression`에 `AST_TUPLE_LITERAL` 케이스로 요소 타입 수집, `AST_LET_DESTRUCTURE`에서 RHS가 tuple이면 arity 검증 + positional element 타입 할당
  - C 백엔드: `append_type_name`이 튜플을 `(T, U)`로 렌더, `pergyra_type_to_c`가 `(Int, String)` → `PgyTuple_Int_String_t`로 매핑 (depth-tracking 파서), `ensure_tuple_specialization_to`가 `typedef struct { T0 f0; T1 f1; ... } PgyTuple_<suffix>_t;`를 ctx->out에 중복 없이 방출, `emit_expression(AST_TUPLE_LITERAL)`이 compound literal `((PgyTuple_T_U_t){.f0=..., .f1=...})` emit, AST_LET_DESTRUCTURE MIR 경로/기본 경로 둘 다 tuple 분기로 `.f0/.f1/...` 필드 추출
  - LLVM 백엔드: `ast_type_to_llvm`이 tuple AST_TYPE → literal anonymous struct `{T0, T1, ...}`, `llvm_emit_expression(AST_TUPLE_LITERAL)`이 `LLVMGetUndef + InsertValue` 체인으로 집계값 구성, `llvm_emit_let_destructure`가 struct 필드 개수 + 첫 필드 비포인터 heuristic으로 tuple 판정 후 `ExtractValue` per-binding
  - 회귀: `tests/cases/backend_compare/destructure_tuple_return/main.pgy` (C/LLVM 동일: `42/hello/7/11/true`), `compare_backends.sh` case 등록, `test-semantic 1653 passed`, `test-transpile 584 passed`
  - 파일: `src/semantic/type_system.{h,c}`, `src/parser/ast.{h,c}`, `src/parser/parser_decl.c`, `src/parser/parser_expr.c`, `src/semantic/type_checker.{c,_helpers.inc}`, `src/codegen/transpiler.h`, `src/codegen/transpiler_helpers_core_b.inc`, `src/codegen/transpiler_expr_emitters.inc`, `src/codegen/transpiler_emitters_base_{a,b}.inc`, `src/codegen/llvm_backend.c`, `src/codegen/llvm_expr.c`, `src/codegen/llvm_stmt.c`, `src/codegen/llvm_pipeline.c`
  - 후속 수정 (destructure + if 지원): `transpiler_register_with_alias_bindings_in_block`의 Claim-only 제한 제거 — 모든 destructuring 바인딩(array/slice/tuple/일반 call)의 이름을 self-mapping으로 precheck ssa_map에 등록. 실제 emit 경로는 여전히 `<name>.1` 버전드 이름을 MIR emit 시점에 ssa_map에 넣어서 사용 (self-map은 verifier 통과용 가드일 뿐). 결과: `let (a, b, flag) = f(); if flag { ... } else { ... }` 같은 패턴이 array/tuple 둘 다 C/LLVM에서 동작. 파일: `src/codegen/transpiler_emitters_base_a.inc` (register_with_alias_bindings_in_block)
- [ ] **sealed ability** — 구현 가능한 role을 제한 (`sealed ability Combatable` → 같은 모듈 내 role만 impl 가능)
- [x] **문자열 보간** — `f"값은 {x}"` → `StringConcat(...)` series로 lowering
  - 완료: lexer에서 `f"..."` → `TOKEN_INTERPOLATED_STRING`
  - 완료: parser에서 `{expr}` 파싱, `ToString(expr)` + `+` concatenation으로 분해
  - 완료: 기존 `"${expr}"` 레거시 문법도 호환 유지
  - 완료: 베타 stable subset을 `"..."`, `"""..."""`, `"${expr}"`, `f"{expr}"`, escaped f-string brace로 문서화
  - 완료: unmatched interpolation brace는 보간하지 않고 literal text로 보존하도록 parser 회귀 추가
  - beta-out-of-scope: nested brace matching, format specifier, multiline interpolation, custom interpolation protocol

### 에러 처리
- [x] **`?` 연산자** — `Result<T>` 에러 자동 전파. `let val = riskyFunc()?;` → 에러 시 즉시 반환
  - 완료: 시맨틱 검증, C early-return lowering, LLVM `Result<T>` 레이아웃/unwrap/early-return lowering, `pipe_and_try.pgy` C/LLVM 실행 검증
  - LLVM try.err 재구성 버그 수정 (2026-04-19): `let val = Validate(x)?;` 패턴에서 let_decl이 `current_ret_type`을 LHS var 타입(i32)으로 잠시 덮어쓰고 있어, `?`의 try.err 블록이 함수 return 타입 struct 대신 i32로 판정 → `unreachable` emit → 런타임 crash. `ctx->current_func_decl`에서 AST 반환 타입을 재조회해 복구 + Err 값 재구성 (src_err → dst_err 정수/포인터 강제 변환 포함)
  - 파일: `src/codegen/llvm_expr_core.inc`
  - 회귀: `tests/cases/backend_compare/try_operator_result/main.pgy` (C/LLVM 동일), `examples/pipe_and_try.pgy`

### 편의 문법
- [x] **파이프 연산자** — `data |> Transform |> Validate |> Persist` 단방향 데이터 흐름
- [x] **defer** — `defer Release(s)` 스코프 종료 시 자동 실행
- [x] **`let` 타입 추론** — initializer 기반 기본 추론은 현재 구현됨
  - 완료: annotation이 없을 때 initializer 타입으로 추론
  - 남음: 문서/표면 예시를 더 공격적으로 타입 추론 중심으로 정리할지 결정

### 제네릭 클래스
- [x] **제네릭 클래스** — `class Pair<T>` 문법 + 시맨틱 + C 코드젠 (단형화). 예제: `examples/generic_class.pgy`

### Slot 소유권 모델
- [x] **`own`/`ref` 소유권 모델 확정 및 구현** — move 기본, 함수 시그니처에 명시
  - 완료: `own`/`ref` 키워드 (렉서/파서/AST), Slot 대입 시 move 시맨틱, Clone() 명시적 복사
  - `func Upload(own tex: Slot<Texture>)` → 소유권 이전, 원본 무효
  - `func Render(ref tex: Slot<Texture>)` → 빌림, 원본 유효
  - 문서화: `docs/22_ownership_model.md`

### Slot 표면 문법 개선 (P0 우선순위)
- [x] **암묵적 Read + 대입 기반 Write** — Slot의 기본 사용 표면을 일반 변수처럼
  - 완료: 읽기 문맥에서 `Slot<T>` auto-read
  - 완료: `slot = expr` → `Write(slot, expr)` lowering
  - 유지: `Release(slot)`는 계속 명시적

### Slot 최적화 (P0 우선순위)
- [x] **스택 할당 최적화** — 스코프를 벗어나지 않는 Slot은 malloc 대신 alloca
  - 완료: `slot_analyze_escape_flags()` (slot_analyzer.c)
  - 완료: LLVM 백엔드에서 `slot_escapes == false` 시 alloca 생성 (llvm_stmt.c:145-146)
  - 완료: escape analysis로 non-escaping slot 자동 스택 할당

### View 범위 부여 (리뷰 필요 — 미결정)
- [ ] **View에 바이트/인덱스 범위 부여** — 실제 사용 사례 만들어보고 결정
  - 안 A: Slice 기반 — `SliceOf(buf, 0, 1024)` → Slot의 "창문"
  - 안 B: View에 범위 부여 — `ViewRead(buf, offset, length)`
  - **미결정 — 파일 I/O, 네트워크 버퍼, GPU 텍스처 사례를 만들어보고 결정**

### 병렬/채널
- [x] **select 실체화** — 여러 채널 중 먼저 준비된 것을 처리

### 언어 완성도 Tier 1 — 범용 필수
- [x] **for-in 컬렉션 루프** — `for item in array { }` 배열/컬렉션 순회
  - 완료: Array<T>/Slice<T> 특수화 (index loop lowering), 시맨틱 element type 추론
  - 남음: ability 기반 Iterable<T> 프로토콜 (Tier 2)
- [x] **StringSplit / StringJoin** — 문자열 분리/결합 빌트인 실체화
  - 완료: `Split(s, delim) → Array<String>`, `Join(arr, sep) → String`
- [x] **ToInt / ToFloat** — 문자열→숫자 변환 빌트인
- [x] **기본 Math 빌트인** — Sqrt, Pow, Floor, Ceil, Random 추가 (기존 Abs/Min/Max + 신규 5개)
- [x] **ArraySort / ArrayMap / ArrayFilter / ArrayReverse** — 고차 함수 기반 컬렉션 연산
  - 완료: ArraySort(arr) → qsort, ArrayMap(arr, fn) → 새 배열, ArrayFilter(arr, fn) → 조건 필터, ArrayReverse(arr) → 뒤집기
  - fn은 함수 이름 또는 람다 (C 함수 포인터로 lowering)
- [x] **디스트럭처링** — `let (a, b, c) = expr` 배열/컬렉션 positional 바인딩
  - 완료: Array<T> → 인덱스 기반 추출 (`result.data[0]`, `result.data[1]`, ...)
  - MIR 통합 (2026-04-19): MIR DCE가 `AST_LET_DESTRUCTURE` 문을 "부작용 없음"으로 판정해 제거하던 버그 수정 (`mir_stmt_has_side_effect`). 트랜스파일러 MIR emit 루프에서 destructuring을 SSA-renamed 타겟으로 emit, `transpiler_find_local_type_name_in_block`에 AST_LET_DESTRUCTURE 케이스 추가해 로컬 타입 해석 복구
  - LLVM parity (2026-04-19): `llvm_emit_statement`의 AST_LET_DESTRUCTURE 케이스 추가 — 초기화식을 struct 값으로 평가, `ExtractValue(0)`으로 data pointer 추출, 각 바인딩마다 `GEP+Load`로 요소 추출 후 `alloca+store`+`llvm_scope_declare`로 로컬 등록. `llvm_lookup_array_var`로 elem_type 해석
  - 파일: `src/compiler/mir.c`, `src/codegen/transpiler_emitters_base_a.inc` (C 백엔드), `src/codegen/llvm_stmt.c` (LLVM 백엔드)
  - 회귀: `tests/cases/backend_compare/destructure_array/main.pgy` (C/LLVM 동일 출력), `examples/collection_ops.pgy` (hello/world/foo 출력)

### 메타프로그래밍 입장 (결정 완료)
- [x] **TMP 비채택** — 제네릭 monomorphization + ability dispatch로 95% 커버. 문서: `docs/23_metaprogramming_position.md`
- [ ] **향후 코드 생성 필요 시** — 컴파일 타임 플러그인 (proc_macro 모델) 또는 소스 생성기 검토

### 언어 완성도 Tier 2 — 실사용 편의
- [ ] **innate ability** — 같은 모듈 내 role만 impl 허용 (sealed 대신 innate 채택. 문서: `docs/24_visibility_model.md`)
  - 파서 완료, 시맨틱에서 `innate` 키워드 인식 (type_checker_decls.inc 참조)
  - 남음: 모듈 경계 검증 로직 완성
- [x] **제네릭 constraint 시맨틱** — `where T: Comparable` 시맨틱 검증
  - 완료: 파서 + 시맨틱 검증 (type_checker_helpers.inc:1847)
  - 완료: Generic function where-clause constraint validation
- [x] **OR 패턴** — `case 1 | 2 | 3:` match에서
  - 완료: lexer `TOKEN_PATTERN_OR`, parser 파싱, 시맨틱 검증
  - 완료: 리터럴 OR 패턴 지원 (`case 1 | 2 | 3:`)
  - 제한: variant destructuring OR 패턴은 아직 미지원 (`case .Some(v) | .None:`)
- [x] **enum 메서드** — `enum Direction { ... func Name(self) -> String }`
  - 완료: enum body에서 `func` 선언 + `self` 파라미터로 match self 본문 가능, C 컴파일 검증
- [x] **labeled break/continue** — `outer: while { ... break outer; }`
  - 완료: 파서 (`parser.c:1270`), AST (`break_stmt.label`), 시맨틱 (`test_semantic.c:680,714,739`), C 코드젠 (`loop_break_labels[]` + `loop_continue_labels[]`)
  - 검증: outer label break, 알 수 없는 label 거부, continue outer 모두 회귀 테스트 통과
- [x] **Custom error 타입** — `Result<T, E>` where E is user type (현재 String만)
  - 완료 (2026-04-18): 타입명 렌더 `PgyResult_Int_NetError` sanitize, `PGY_RESULT_DEFINE(Int_NetError, int32_t, NetError)` 자동 instantiation (`ensure_result_specialization_to` 신설), 편의 매크로 (`Ok_T_E`, `Err_T_E`, `IsOk_T_E`, `Unwrap_T_E`, `UnwrapOr_T_E`) 자동 생성, Ok/Err builtin이 `ctx->current_return_type`에서 suffix 추출, match pattern Ok/Err 바인딩 `__typeof__` 기반 타입 추론
  - 파일: `src/codegen/transpiler_helpers_core_b.inc` (generic_args_to_c_suffix + ensure_result_specialization_to), `src/codegen/transpiler_expr_emitters.inc` (Ok/Err/Unwrap suffix), `src/codegen/transpiler_emitters_base_b.inc` (match __typeof__), `src/codegen/transpiler.h` (result_specs_*)
  - 회귀: `src/test_semantic.c` "Result<T, E> with enum error type accepts Ok/Err and match destructuring"

### ability 차별화
- [x] **ability ≠ interface 문서화** — ability는 "협업 프로토콜의 자격 조건"이며 슬롯에 부착됨
  - 완료: `docs/24_visibility_model.md`에 `ability ≠ interface` 섹션 추가
  - 정리 내용: ability는 nominal object의 메서드 집합을 직접 모델링하는 interface가 아니라, `requires Ability`, `dyn role slot: Ability`, `zone authority requires Ability`처럼 협업 계약/자격 조건으로 소비되는 surface임을 고정
  - 정리 내용: ability는 subject/role/slot/orchestration contract와 결합되며, 구현 담당은 role impl이고 ability 자체는 "무엇을 구현하라"보다 "어떤 자격으로 참여하라"를 표현한다는 점을 명시

## P1.6 — 자원/오케스트레이션 방향 고정

### 분산 설계 결정 (2026-04-03 확정)
- [x] **RemoteFuture `await` → `Result<T>` 강제** — 원격 자원의 지연/실패를 타입 시스템에서 강제 노출
  - `Future<T>` (로컬) → await → `T` (실패 없음)
  - `RemoteFuture<T>` (원격) → await → `Result<T>` (실패 가능)
  - 시맨틱 체커 + C 코드젠 + 런타임 매크로 구현 완료
  - 테스트: 205 semantic + 141 transpile 통과
- [x] **RemoteFuture에 Claim/Read/Write/Release 차단** — 원격 자원의 동사는 Submit/Await만
  - Read/Write/Release 호출 시 친절한 에러 메시지 출력
  - "RemoteFuture does not support Read(); use 'await' to obtain Result<T>"
- [ ] **원격 Slot은 Claim 없이 Channel 기반 메시지 패싱만** — 분산 락 회피
  - 크로스 World 통신은 `Channel<T>`만 허용
  - 원격 자원에 Claim 동사를 사용하면 컴파일 에러
- [x] **World 경계 = 실패 도메인 경계** — 크로스 World 통신은 Channel만
  - 완료: World 시맨틱 체커 (`type_check_world_decl`, type_checker_decls.inc)
  - 완료: World 코드젠 (C 백엔드, transpiler_helpers.inc)
  - 완료: `HasZoneProjection`, `HasZoneLayer`, `HasZoneState` builtin

### Projection / Domain Query
- [x] **Projection query surface** — `HasProjection(slotName)`으로 relation/effect/zone 문맥에서 object/tobject projection slot의 sync-ready 여부를 질의
  - 완료: semantic + C/LLVM lowering
  - World 내부의 Slot은 로컬 fast path, World 간은 Channel (명시적 비용)

### 스케일링 대응 (레드팀 피드백 기반)
- [ ] **백엔드 역할 컷오프 고정** — C = reference/fallback, LLVM = optimization/mainline
  - 같은 의미론을 두 백엔드에 유지하되, 공격적 최적화와 type-erased fast path는 LLVM에만 집중
  - C 백엔드는 MVP 호환성, 디버깅, 폴백, 부트스트래핑 역할로 제한
  - 새 기능 추가 시 "C에서도 반드시 최적화 경로까지 구현해야 하는가?"를 기본적으로 `아니오`로 둠
- [ ] **매크로 조합 폭발 대응** — C 매크로 monomorphization의 장기 대안
  - 현재: `PGY_SLOT_DEFINE`, `PGY_CHANNEL_DEFINE` 등 타입별 전개 (부트스트래핑 전략)
  - 대안: LLVM 백엔드에서 type-erased 경로 (opaque ptr + vtable) 추가
  - LTO + dead code elimination으로 바이너리 비대화 억제
- [ ] **코드젠 이중화 억제 규칙** — bifurcation trap 방지
  - 동일 기능의 C/LLVM lowering이 영원히 쌍으로 비대해지지 않게 공통 의미론 테스트 우선
  - backend compare / smoke를 계약으로 유지하고, backend-specific fast path는 명시적으로 분리
- [ ] **Async 힙 할당 오버헤드 감소** — 고성능 분산 I/O를 위한 런타임 최적화
  - 현재: `pgy_spawn` + `malloc` per task
  - 대안: Arena allocator 기반 task pool, io_uring/IOCP zero-copy I/O
  - 코루틴 스택은 이미 fiber 기반 (pgy_parallel.h)
  - 단, 언어 코어와 OS 전용 스케줄러를 강결합하지 말 것
- [ ] **BYOS (Bring Your Own Scheduler) 경로 설계** — async 의미론과 스케줄러/I/O 모델 분리
  - 언어는 task/future/channel 의미만 고정
  - 실제 polling/runtime은 플랫폼별 주입 가능 계층으로 분리
- [ ] **ABI 다형성 전략** — 크기가 다른 슬롯 타입의 제네릭 처리
  - 의도적 설계: `Slot<T>` ≠ `SecureSlot<T>` (보안 차원 분리)
  - 다형성 필요 시: `ability` vtable dispatch (Party 시스템에 이미 구현)
  - Boxing 필요 시: `Rc<T>` + ability 조합
  - `Rc<T> + dyn ability`는 explicit high-cost path로 문서화
  - 값 경로(struct), 객체 경로(class), 동적 경로(Rc + dyn ability)를 성능 계약으로 구분

### 기존 항목
- [x] **Slot Protocol 고정** — Claim/Access/Mutate/Transfer/Release 불변 계약
- [x] **Slot/View 계층 마감** — ReadView/WriteView/MoveToken 권한 축소/이전 계층
- [ ] **슬롯을 추상 자원 핸들로 일반화** — 장기적으로 MemorySlot, DeviceSlot, SessionSlot 등 자원 클래스 확장
- [ ] **채널 의미론 강화** — 비동기 제출/대기/수거/후처리 흐름 보강
- [x] **`Future<T>`를 transfer boundary로 고정** — await/recv와 같은 ownership 경계
- [ ] **effect/resource capability 표기 도입** — `local cpu`, `secure device`, `remote` 등 타입/효과 시스템
  - 현재: derived effect mask + spawn/await/channel에서 remote 추론
  - 현재: `/// @effects ...` 선언이 있으면 body derived effect와 mismatch 진단
  - 다음: 시그니처 문법 차원의 선언적 annotation 표면
- [ ] **성능 목표를 orchestration overhead 중심으로 재정의**

## P1.7 — 의미 통일 언어로서의 다음 단계

### 비용 모델 / effect
- [ ] **비용 모델 표면화** — "semantic unity, visible cost" 원칙
  - `local / secure / remote / device` 자원군의 비용 차이를 표면에 드러내기
- [ ] **effect system 2단계** — 선언적 effect 표기, mismatch 진단
  - 부분 완료: structured comment `@effects` 기반 mismatch 진단
  - 부분 완료: source-level `with effects ...` 시그니처 surface
  - 남음: 더 정교한 effect lattice, call-site contract surface

### 상위 계층 모델
- [x] **최종 문맥 계층 / 설계 순서 분리 고정**
  - 조립 계층: `ability -> role -> party -> relation -> effect -> zone -> world`
  - 사용자-facing 설계 순서: `intent -> world -> zone -> subject`
  - 완료: `world`를 최상위 실행/신뢰/실패 경계라는 목표 정의로 문서화
  - 완료: 상위 레이어로 갈수록 덜 구속적이라는 설계 원칙 문서화
  - 완료: `relation`, `effect`, `zone` declaration keyword와 최소 `subject slot` / `object slot` surface를 parser/semantic 표면에 연결
  - 완료: `zone -> relation/effect`, `world -> zone` 최소 조립 slot surface를 parser/semantic에 연결
  - 완료: `relation`, `effect`의 optional `for ...` header로 subject endpoint/target 최소 surface를 연결
  - 완료: `zone`의 `apply effectSlot to targetSlot` 최소 attachment surface를 parser/semantic에 연결
  - 완료: `zone`의 `link relationSlot between left, right` 최소 relation wiring surface를 parser/semantic에 연결
  - 완료: `zone`의 `detach effectSlot from targetSlot`, `unlink relationSlot between left, right` 최소 release surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/detach`, `link/unlink`를 `effect/relation` declaration contract와 기본 타입/arity 수준으로 연결
  - 완료: `zone` subject shape에 대한 권장 lint 추가
  - 완료: `tobject` keyword를 `struct` 호환 projection alias로 추가
  - 완료: `ToObject(TargetStruct, subjectBinding)` 최소 passive projection surface를 semantic/C backend에 연결
  - 완료: `ToTObject(TargetDto, subjectBinding)` 최소 projection surface를 semantic/C backend에 연결
  - 완료: `relation/effect/zone`에 `tobject slot` surface를 연결
  - 완료: `relation/effect/zone`의 domain slot에 optional initializer를 연결해 `object slot view: View = ToObject(View, subject)` 같은 projection wiring을 직접 표현 가능하게 함
  - 완료: `zone`의 `refresh objectSlot from subjectSlot` surface로 projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `publish dtoSlot from subjectSlot` surface로 tobject projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right` surface로 지속 lifecycle rule을 parser/semantic에 연결
  - 완료: `maintain` duplicate/conflict warning (`maintain` + `detach/unlink`) 추가
  - 완료: `zone`의 `authority subjectSlot` surface와 optional `by subjectSlot` authority annotation을 parser/semantic에 연결
  - 완료: `authority subjectSlot requires Ability[, Ability]` ability-gated authority surface를 parser/semantic에 연결
  - 완료: `zone`의 `state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/link/detach/unlink/maintain stateName` shorthand를 parser/semantic에 연결
  - 완료: `HasState(stateName)` zone query builtin을 parser/semantic에 연결하고 C backend에서 zone state field query로 lowering
  - 완료: `HasLayer(layerSlot)` zone query builtin을 parser/semantic에 연결하고 C/LLVM backend에서 zone layer field query로 lowering
  - 완료: `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)` slot-aware state query를 semantic에 연결
  - 완료: `world`의 `state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState` lifecycle surface를 parser/semantic에 연결
  - 완료: `HasZone(zoneOrState)` world query builtin을 parser/semantic에 연결하고 C backend에서 world zone-state/active field query로 lowering
  - 완료: C backend가 zone/world마다 sync helper를 생성하고 method 전후에 `refresh`/`publish` projection과 lifecycle flag를 incremental하게 동기화
  - 완료: `relation`, `effect` declaration이 C/LLVM backend에서 struct + method wrapper로 codegen되고 runtime instance constructor/method path가 연결됨
  - 완료: `zone` layer slot이 C/LLVM에서 typed overlay runtime instance로 유지되고 sync가 subject slot을 layer endpoint/target에 바인딩한 뒤 projection sync까지 수행
  - 완료: direct `apply/link/detach/unlink`와 `maintain effect/relation/state`가 C/LLVM zone sync에서 실제 layer/state propagation으로 연결됨
  - 완료: zone embedded overlay projection read (`self.poison.view.hp`, `self.trust.packet.name`)가 LLVM runtime smoke로 검증됨
  - 완료: `world`가 `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)`로 embedded zone runtime flag를 직접 질의할 수 있음
  - 완료: `ability/role/party/relation/effect/zone/roster/world` 전체 구현
  - 완료: `world`가 `state name: all zoneOrState[, ...]` / `state name: any zoneOrState[, ...]`로 앞서 선언된 zone/state alias를 최소 조합 contract로 합성
  - 남음: richer world-level runtime semantics, 더 깊은 cross-layer propagation policy

### 존재론 모델
- [x] **intent-first 설계 축 / subject-core host 축 분리 고정**
  - 완료: 사용자-facing 설계 순서는 `intent -> world -> zone -> subject`로 문서화
  - 완료: `subject = 상태와 identity를 가진 주체 타입`은 host/naming/lowering 축으로 한정해 문서화
  - 완료: `subject`와 `class`를 서로 다른 nominal flavor로 분리하고 의미론도 1차 분기
  - 완료: legacy host-profile surface를 제거하고 `subject`/`object`/`intent` 중심으로 정리
  - 완료: `entity`는 코어 언어 존재론에 넣지 않고 프레임워크/도메인 용어로 남긴다고 문서화
  - 완료: `object`는 intent를 시작하지 않는 passive state target이라고 문서화
  - 완료: `tobject`는 object의 외부 경계용 축약 투영이라고 문서화
  - 완료: `subject`, `class`, `struct`, `object`, `tobject` declaration flavor를 parser AST에 분리 기록
  - 완료: `subject slot`과 `ToObject` / `ToTObject` source가 `subject` host만 받도록 semantic 분기
  - 완료: `object` keyword alias를 parser/LSP surface에 반영
  - 완료: `object`를 passive state/value 형식으로, `tobject`를 더 좁은 projection/value 형식으로 정리하고 helper method를 허용
  - 완료: `vessel` declaration과 `subject` 내부 `vessel` field surface 추가
  - 완료: `subject` 전용 `action` declaration과 최소 clause (`requires/within/causes/authorized by`) parser/semantic 연결
  - 완료: `subject` 안의 legacy `func` 제거, `action` only 정책으로 승격
  - 완료: `role`/`party`/`authority`를 subject-core host 축으로 더 강하게 제한
  - 완료: C/LLVM method lowering에서 `subject=self-cell`, `class=value self` 1차 분기
  - 완료: legacy host-profile surface를 제거하고 관련 규칙을 `subject`에 통합
  - 완료: `subject` 단일 host surface로 통일
  - 완료: standalone host-profile surface 삭제
  - 완료: object를 effect/relation target으로 semantic/C/LLVM에 연결
  - 완료: domain-local `refresh` / `publish` source를 subject/object까지 확장하고 tobject source는 금지
  - 완료: relation/projection 중심 surface 고정

### 문서 / 스타일 정렬
- [ ] **BSD (Allman) canonical style 전면 고정**
  - 문서/예제/scaffold/formatter 출력은 BSD 기준으로 통일
  - K&R은 parser compatibility로만 남기고 canonical surface로는 취급하지 않음
- [x] **문서 예제 제시 순서 강제**
  - 완료: README entrypoint와 핵심 설계 문서에서 예제 독해 순서를 `intent -> world -> zone -> subject`로 명시
  - 기준 문서: `README.md`, `docs/00_vision.md`, `docs/01_intent_first_design.md`, `docs/22_class_object_model.md`
  - 규칙: `subject`는 core host로 설명하되, 설계의 첫 축으로 가르치지 않음
  - 규칙: compile-order와 teaching-order를 분리해서 명시

### slot 권한 / 자원군 확장
- [ ] **slot 권한 모델 고도화** — 공유 읽기 vs 독점 쓰기, capability narrowing
- [ ] **실제 자원군 확장** — SessionSlot, ChannelSlot, RemoteJob 고도화
- [x] **subject/class/object model 구현 정렬**
  - 완료: subject direct copy/plain value parameter/return 금지, positional constructor
  - 완료: C/LLVM lowering 1차 분기 (`subject=self-cell`, `class=value self`)
  - 완료: legacy host-profile을 `subject` 규칙으로 통합
  - 완료: `subject` 단일 host surface로 통일
  - 완료: plain/secure `Slot<subject>` local object-cell anchor 지원
  - 완료: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` 함수 경계 전달을 semantic + C/LLVM backend에 반영
  - 완료: `Box<class>` explicit handle surface (`Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`)
  - 완료: richer object-handle cell propagation

### orchestration 완성도
- [ ] **오케스트레이션 모델 강화** — select 공정성, timeout, cancellation, backpressure
  - 부분 완료: `TryRecv/RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`
  - 부분 완료: `TrySendStatus/SendTimeoutStatus -> Option<Bool>`로 full/timeout vs closed를 값으로 구분
  - 부분 완료: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`
  - 부분 완료: `select` round-robin 시작 인덱스 fairness
  - 부분 완료: `Cancel(task)` / `IsCancelled()` cooperative cancellation
  - 부분 완료: spawned descendant cancellation propagation
  - 현재 제한: movable resource channel의 non-blocking/timeout transfer는 미지원
  - 현재 제한: pressure observation은 가능하지만 bounded policy/backpressure protocol은 아직 미구현
  - 현재 제한: preemptive cancellation, blocked thread task interruption, structured cancellation scope/lattice는 미지원
- [x] **async/await runtime 고도화** — POSIX ucontext + Windows Fiber 기반 coroutine
- [ ] **Windows coroutine 검증/고정**

### 툴링 / 표준면
- [ ] **stable stdlib surface 재고정**
- [ ] **툴링 단계 진입** — formatter, LSP 진단 품질
- [x] **ontology-first scaffold 정렬**
  - 완료: `pgy scaffold` help를 `subject/class/object/tobject` 우선 분기로 정렬
  - 완료: `class` scaffold kind 추가
  - 완료: `project/simulator` scaffold가 `subject`가 `class`를 소유하고 `object/tobject`로 투영하는 starter shape를 생성
  - 완료: `project` scaffold가 intent-first layout(`intents/`, `subjects/`, `zones/`, `world.pgy`, `main.pgy`)을 실제로 생성
  - 완료: `pgy new`가 `intent-first` / `class-first` / `projection-first` starter를 선택하게 할지 검토
  - 완료: `pgy new` / scaffold output에 ontology decision guide file 별도 생성 검토
  - 완료: intent-first project guide 문서도 scaffold output에 같이 생성할지 검토
    - `intents/`를 프로젝트 table-of-contents로 설명하는 guide 포함
    - intent declaration이 필요한 subject/zone/ability/effect TODO를 역산하는 workflow 예시 포함
  - 완료: intent runtime follow-up
    - rollback policy를 current reverse-order `compensate` beyond v1로 확장하기
    - intent의 cross-world transfer / identity handoff semantics 설계 및 구현
    - current last-intent typed history를 trace id / stream / multi-instance observability로 확장하기

### 대표 프로그램
- [ ] **대표 애플리케이션 3종** — 이종 자원 파이프라인, secure+device+channel, slot/orchestration 철학 증명

## P1.85 — 게임 프레임워크 계층

- [ ] **게임 프레임워크 라이브러리 경계 고정**
  - 원칙: `entity/object pool`은 언어 코어 기능이 아니라 `use pool;` 같은 게임/앱 라이브러리 계층으로 둔다
  - 원칙: `encounter/turn/state machine`, `strategy/AI`, `content tables`도 동일하게 코어 문법이 아니라 프레임워크 surface로 쌓는다
  - 원칙: 이 계층은 “도메인 라이브러리”보다 “generic pattern library + domain injection”으로 정의한다
  - 이유: 코어 언어는 `subject / vessel / object / tobject / relation / effect / zone / world / Slot<T>` 의미론을 유지하고, 대규모 게임 설계는 그 위의 library/DSL 계층으로 올리는 편이 확장성과 설명력이 더 좋다
  - 목표: “게임을 만들 수 있는 코어 언어”와 “게임을 실제로 만드는 프레임워크”를 분리
- [ ] **게임 stdlib/use surface 초안**
  - 후보: `use pool;`, `use fsm;`, `use encounter;`, `use strategy;`, `use tables;`
  - 방향: pool/fsm/strategy/table은 `.pgy` 또는 stdlib 모듈로 제공하고, 언어 키워드로 승격하지 않는다
  - 방향: `Pool<T>`, `StateMachine<TState, TEvent>`, `StrategyTable<TContext, TChoice>`, `WeightedTable<T>`처럼 generic-first naming을 우선한다
  - 방향: GOF 기초 패턴도 inheritance/object graph가 아니라 Pergyra host 기준으로 번역한다
    - `singleton` -> contextual runtime registry / host-local shared state
    - `factory` -> staged template/spec builder
    - `strategy` -> policy card / policy table + function injection
    - `state` -> explicit FSM / transition rule + context application
    - `observer` -> relay bundle / sink spec / report sink / event bus
  - 방향: generic pattern library는 static spec/table만이 아니라 function-typed picker/resolver 주입도 기본 표면으로 포함한다
    - 예: `Picker<TInput, TChoice>`
    - 예: `Resolver<TContext, TResult>`
    - 예: `StrategyApply(context, AggressivePolicy)`
  - 현재 상태: `data/card/table` 경로는 안정, custom function injection도 V1 표면이 올라옴
  - 현재 전략 패턴의 안정 단계:
    - `StrategyCard`
    - `StrategyContext`
    - `ApplyStrategy(card, context)`
  - 이번 예제 기준 라이브러리화 후보:
    - `use strategy;`
      - `WeaponCard` / `CombatStrategyCard`
      - `WeaponFactory<TClass>` 또는 `LoadoutTable<TArchetype>`
      - `StrategyTable<TContext, TChoice>`
      - `ActionTextFactory<TContext>` / `EffectTextFactory<TContext>`
    - `use tables;`
      - `SceneChoiceCard`
      - `CompanionEventCard`
      - `BossPhaseCard`
      - `WeightedTable<T>`
      - `ChoiceTable<TState, TOption>`
    - `use encounter;`
      - `EncounterStateMachine<TState, TEvent>`
      - `TurnLoop<TActor, TAction>`
      - `BossPhaseMachine<TPhase>`
      - `ResolutionLedger<TSnapshot>`
    - `use report;`
      - transcript accumulator
      - exact report writer
      - stdout/results dual sink
    - `use campaign;`
      - scripted / random / player mode runner
      - input script playback
      - seeded choice resolver
- [ ] **GOF 기초 패턴을 Pergyra식 pattern catalog로 정리**
  - 기준 문서: `docs/31_gof_pattern_catalog.md`
  - 기준 예제: `examples/pattern_library_basics/`
  - 목표: 전통 OOP 패턴 이름을 유지하더라도 실제 구현 shape는 `subject / vessel / shared / spec / card / relay`로 재정의
  - 비목표: inheritance / `super` / hidden callback graph를 패턴 구현의 기본값으로 채택하지 않음
- [ ] **DND/campaign 시나리오를 게임 프레임워크 검증장으로 사용**
  - `dnd_tavern_campaign`를 기준으로 pool/fsm/strategy/table이 실제로 충분한지 검증
  - language core 부족이 아니라 framework layer 부족인지 계속 분리해서 기록
  - 지금까지 뽑힌 실제 패턴:
    - 장소/장면 진입 팩토리 (`OpenTavernCampaign`)
    - 게임 상태 머신 (`tavern -> floor1 -> floor2 -> floor3 -> dragon -> epilogue`)
    - 선택 해석기 (`scripted` / `random` / `player`)
    - 장면 카드 / 동료 반응 카드 / 보스 페이즈 카드
    - 전투 loadout/strategy 카드
    - transcript-first report writer
  - 다음 목표:
    - 위 패턴들을 `examples/` 전용 코드가 아니라 `use` 라이브러리 후보로 재구성
    - `world.pgy`의 orchestration 양을 줄이고 encounter/strategy/report 계층으로 분리

## P1.8 — 멀티 타겟

- [ ] **공통 UI IR 고정** — Kotlin/Android 개별 백엔드보다 먼저, 모든 플랫폼이 공유하는 scene/projection UI IR을 정의
  - 목적: native / web / mobile이 같은 UI 의미론과 projection 흐름을 공유하게 함
  - 원칙: 기술 기반은 Qt 방향(native shell / render loop), 선언 철학은 WPF식 projection/binding, 최종 정체성은 Pergyra scene/projection UI
  - 범위: `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
  - 원칙: `subject`를 직접 화면에 그리지 않고 `object` / `tobject` / projection surface를 UI 소비 표면으로 사용
  - 원칙: `zone` / `world` state와 projection dirty sync가 UI IR의 갱신 계약이 됨
  - 순서: UI IR 고정 → native backend 1개 → JS/web backend 1개 → 그 뒤 mobile shell / Kotlin 필요성 재평가
  - 비목표: 플랫폼별 UI 의미론(Qt widget tree, WPF object model, Android View/Compose semantics)을 코어 언어에 직접 들이지 않음
- [~] **JavaScript 백엔드** — `.pgy → JS` 변환으로 브라우저/Node.js 실행 지원
  - 완료: 코어 의미론은 inheritance/super 없이 유지하고, JS lowering은 delegation/composition 중심으로 간다는 정책 초안 문서화
  - 완료: Kotlin backend보다 공통 UI IR이 우선이라는 멀티플랫폼 정책 문서화
  - 남음: JS IR/lowering shape, runtime shim, interop surface (`extern js`) 설계
- [ ] **mobile shell 전략** — Android/iOS는 우선 공통 UI IR consumer로 접근
  - 원칙: 초기 mobile 대응은 JS/web-compatible UI backend 또는 native shell bridge를 우선 검토
  - 남음: Android 전용 Kotlin backend는 공통 UI IR + web/native backend 검증 뒤 필요성을 재평가
- [ ] **WebAssembly 타겟** — LLVM wasm32 backend 활용

## P1.9 — AI-first 인프라 (2026-04-19 positioning 확정)

**맥락**: 경쟁 대상은 C#/Java ↔ Rust 사이 니치이고, 1차 사용자는 frontier LLM(Claude 등)이 주도 + 인간이 리뷰/수정하는 워크플로. "AI가 생성 → 컴파일러/테스트가 검증 → 인간이 리뷰"의 loop이 타이트하게 돌아가는 것이 positioning 핵심.

현재 의도치 않게 갖춰진 AI-friendly 인프라:
- backend-compare 회귀 (C/LLVM 출력 대조) — AI self-verification loop 하네스
- 2000+ test suite + 스모크 체인 — 생성물 즉시 검증 가능한 규모
- Result-first + throw 금지 — AI가 stack trace보다 ErrorCode enum 분기가 쉬움
- 구조화 주석 (WHAT/WHY/ALT/NEXT/EFFECTS/INVARIANTS/RETURNS/THROWS) — prompt-as-code, 의도 보존

부족하고 채워야 할 것:

- [ ] **Language Reference Spec 문서** — 현재 `docs/`는 설계 일지(의사결정 흐름 기록). AI에게 정확한 의미론 제공하려면 "이 언어의 보장"이 한 문서에 정리돼야 함
  - 내용: 타입 시스템 규칙 / Slot 소유권 계약 / effect subsumption / intent rollback 의미 / Result 전파 규칙 / MIR 계약
  - 형태: 단일 파일 (~2000-5000줄), in-context로 한 번에 로드 가능
  - 목적: "Claude가 Pergyra 코드를 새 세션에서 생성할 때 reference로 인용 가능" 수준
  - 현재 `docs/`와 다른 점: 일지는 "왜 이렇게 결정했는가", spec은 "현재 언어가 무엇을 보장하는가"
- [~] **AI-parseable 구조화 에러 메시지** — 현재 진단은 내부자 표현. AI용은 기계 판독 가능한 구조화 필드 필요
  - 현재: `MIR contract breach in Main at line 0: unresolved identifier 'flag' (expected SSA-mapped local)`
  - 목표 형태 (예시):
    ```json
    {
      "severity": "error",
      "stage": "MIR_validation",
      "code": "PGY_MIR_UNRESOLVED_IDENT",
      "location": {"file": "main.pgy", "line": 7, "column": 8},
      "summary": "destructuring binding 'flag' is not SSA-mapped at use site",
      "cause_ir": "a.1 DEF is emitted in block 0 but not propagated to branch-consumer block via ssa_entry_values",
      "fix_source": "ensure destructure binding is referenced within the same block as the destructure, or use let_decl with explicit type to trigger SSA renaming",
      "related_rules": ["MIR.SSA.entry_values", "destructure.binding"]
    }
    ```
  - `--error-format=json` 플래그로 토글, 인간용은 기존 형식 유지
  - 대상: compile, semantic, MIR/LLVM IR 단계 전체
  - 1차 증분 완료 (2026-04-19):
    - `DriverFlags.diag_format` + `--error-format=json|text` CLI 플래그 추가 (`src/pgy_driver.c`, `src/compiler/driver_app.h`)
    - `semantic_result_print_json` — semantic 진단을 JSON 배열로 방출 (severity/stage/location/message 필드, RFC 8259 준수 이스케이프)
    - `driver_emit_single_diag_json` — 단일 에러 JSON 방출 헬퍼 (module_load / backend_c_emit / backend_c_native / backend_llvm_emit / backend_llvm_native 단계 커버)
    - stage 태그: `semantic` / `module_load` / `backend_c_emit` / `backend_c_native` / `backend_llvm_emit` / `backend_llvm_native`
    - 성공 시 `[]` (빈 배열), 실패 시 `[{...}]` — 호출자는 항상 JSON 기대 가능
    - 회귀: `tests/diagnostics_json_smoke.sh` (Python 파서로 shape 검증, 3 케이스: semantic / parse / success)
    - 검증: PowerShell로 3 케이스 모두 정상 동작 확인 (1668 semantic + 601 transpile 회귀 pass)
  - 2차 증분 완료 (2026-04-19):
    - `Diagnostic` 구조체에 `code` 필드 추가 (non-owning `const char*`, 정적 문자열 리터럴 보관) — `src/semantic/type_checker.h`
    - `semantic_error_code` / `semantic_warning_code` 신규 variant — 코드 인자 받아 diagnostic에 실어줌 (레거시 `semantic_error` 는 그대로 NULL 코드로 동작, 단 동일 사이트 중복 emit 시 코드가 있으면 업그레이드)
    - JSON 출력에 `"code"` 필드 선택적 포함 (NULL이면 생략 — 호환성 유지)
    - parser stage 분리: module_load msg가 `"parse error in"`으로 시작하면 `"stage":"parse"`, 그 외 `"module_load"`
    - 초기 코드 부여 사이트 (6종):
      - `PGY_SEM_TYPE_MISMATCH` (assignment)
      - `PGY_SEM_BINOP_TYPE_MISMATCH`
      - `PGY_SEM_UNKNOWN_TYPE`
      - `PGY_SEM_UNDEFINED_SYMBOL` (identifier / member 3 사이트)
      - `PGY_SEM_INFER_COLLECTION` / `PGY_SEM_INFER_GENERIC` / `PGY_SEM_INFER_REQUIRED`
    - smoke test 확장: `code == "PGY_SEM_TYPE_MISMATCH"` 검증 + `stage == "parse"` 검증 (`tests/diagnostics_json_smoke.sh`)
    - 회귀: 1688 semantic + 601 transpile, 0 failed
  - 3차 증분 완료 (2026-04-19):
    - Slot/ownership/parallel/effect 계열 코드 9종 추가:
      - `PGY_SEM_SLOT_RELEASED` (method dispatch 4 사이트 + builtin Read/Write 2 사이트)
      - `PGY_SEM_RELEASE_REQUIRES_OWNER`
      - `PGY_SEM_SLOT_DOUBLE_RELEASE` (method + builtin Release 2 사이트)
      - `PGY_SEM_VIEW_KIND_MISMATCH` (ReadView write / WriteView read)
      - `PGY_SEM_MOVE_TOKEN_MISUSE` (read/write through MoveToken)
      - `PGY_SEM_MOVE_FROM_RELEASED` (let/call/builtin 3 사이트)
      - `PGY_SEM_PARALLEL_SLOT_CONFLICT` (error: mutate-mutate across tasks)
      - `PGY_SEM_PARALLEL_SLOT_RACE_RISK` (warning: read-mutate across tasks)
      - `PGY_SEM_EFFECT_CONFLICT` (warning: effect class 충돌)
    - `docs/72_diagnostic_codes.md` 카탈로그 문서 신규 — 16개 코드 의미/원인/교정 방법, AI 라우팅 가이드, 향후 확장 필드 문서화
    - smoke test 확장: `PGY_SEM_SLOT_RELEASED` 감지 케이스 추가
    - 사용자 기여: `semantic_error_code` / `semantic_warning_code` 선언에 `PGY_PRINTF_LIKE` 속성 추가 (clang/gcc format 경고 체크)
    - 회귀: 1694 semantic + 601 transpile, 0 failed
    - 현재 총 16개 안정 코드, ~25 사이트 커버. 나머지 ~460 사이트는 4차+ 증분 대상
  - 4차 증분 완료 (2026-04-19):
    - `CompilerResult.error_code` / `TranspileResult.error_code` / `LLVMGenResult.error_code` 필드 추가 (모두 owning strdup, destroy에서 free)
    - `TranspilerCtx.backend_error_code` / `LLVMGenCtx.error_code` non-owning `const char *` (정적 literal만)
    - 신규 setter variants: `transpiler_set_backend_error_with_code` / `llvm_set_error_with_code` / `llvm_set_error_at_with_code` (레거시 setter는 code=NULL 경로로 유지)
    - `driver_emit_single_diag_json_with_code(stage, code, message)` — JSON에 code 필드 선택적 포함
    - `driver_route_stage(default_stage, code)` — prefix whitelist (`PGY_SEM_`/`PGY_MIR_`/`PGY_LLVM_`/`PGY_PARSE_`). 모르는 prefix는 default_stage 유지
    - Runner 업데이트: `c_runner.c` (2 사이트) + `llvm_runner.c` (2 사이트) — 기존 호출을 `_with_code` + `driver_route_stage`로 교체
    - MIR/LLVM 코드 5종 신규:
      - `PGY_MIR_UNRESOLVED_LOCAL` — branch terminator의 identifier가 SSA 매핑 없음
      - `PGY_MIR_TOPOLOGY_INVALID` — MIR routine 누락 / kind 불일치 / AST 없음
      - `PGY_MIR_SIGNATURE_UNSUPPORTED` — 지원 안되는 함수 시그니처
      - `PGY_MIR_SSA_LIMIT` — SSA local 4096 초과
      - `PGY_MIR_INTENT_CARRIER_MISSING` — intent step metadata 누락 (C/LLVM 공통, 21 사이트 일괄 업그레이드)
      - `PGY_LLVM_SPEC_LIMIT` — Result\<T,E\> 특수화 한도(MAX_LLVM_RESULT_SPECS=32) 초과
    - 카탈로그 확장: `docs/72_diagnostic_codes.md`에 "MIR Contract" 섹션 5개 엔트리 + "LLVM Backend" 섹션 1개 엔트리
    - smoke test 확장: 33개 Result\<Int, E*\> 특수화로 `PGY_LLVM_SPEC_LIMIT` + `stage=llvm_codegen` 검증 (`tests/diagnostics_json_smoke.sh`)
    - 검증: `[{"severity":"error","stage":"llvm_codegen","code":"PGY_LLVM_SPEC_LIMIT",...}]` end-to-end 확인
    - 회귀: 1694 semantic + 601 transpile, 0 failed (레거시 경로 무손상)
    - 현재 총 22개 안정 코드 (`PGY_SEM_*` 16 + `PGY_MIR_*` 5 + `PGY_LLVM_*` 1), ~50 사이트 커버. `mir_validation` / `llvm_codegen` stage 가 기존 `backend_*_native`와 분리됨
  - 남은 작업 (5차 증분 후보):
    - intent/zone/world / class/ability 관련 `PGY_SEM_*` 코드 점진적 부여 (나머지 ~460 semantic 사이트)
    - LLVM 추가 코드: `PGY_LLVM_TYPE_UNSUPPORTED`, `PGY_LLVM_RUNTIME_MISSING`, `PGY_LLVM_OOM` (개별 사이트 업그레이드)
    - `cause_ir` / `fix_source` 필드 — 현재 message만. MIR/IR 레벨 원인 + 소스 레벨 교정 포인트 분리해 AI가 구분 가능하게
    - parser 레벨 코드 (`PGY_PARSE_*` prefix 예약됨) — parser error 누적형 리팩터 필요
    - `related_rules` 필드 — Language Reference Spec 이후 연결
- [ ] **In-context example corpus 큐레이션** — GitHub에 Pergyra 코드 0개. 훈련 데이터 부재를 in-context examples로 보완
  - `docs/ai_prompt_bundle/` 디렉토리에 몇 개 레벨의 번들 준비:
    - `minimal.md` — 언어 핵심만 (~20KB)
    - `standard.md` — core + stdlib + 5개 패턴 예제 (~100KB)
    - `complete.md` — 위 + 전체 examples + reference spec (~500KB-1MB)
  - 각 번들은 "이 번들만으로 새 세션에서 AI가 Pergyra 코드를 신뢰성 있게 생성 가능한가"를 검증 기준으로
  - 전략적 결정: 1차 audience는 frontier 모델(Claude Opus, Sonnet) 사용자. 소형/저가 모델은 2차
- [ ] **AI iteration-friendly 빌드 툴체인** — 빠른 컴파일 + 기계 판독 출력 + LSP 진단
  - 증분 컴파일 — 현재 단일 TU로 전체 빌드. module 단위 증분으로 전환
  - 테스트 결과 JSON 출력 — 현재 stdout ✓/✗ 형식. AI가 파싱해 다음 액션 결정할 수 있는 JSON 모드
  - LSP 진단 기계 판독 가능 — 위의 구조화 에러 메시지와 공유 포맷
  - backend-compare 실패 시 diff를 구조화 — 현재 unified diff. AI가 "어느 함수의 몇 번째 stdout 라인이 다름"을 바로 인지 가능한 포맷
  - 일부 기반 있음 (`src/lsp/` 디렉토리, `tests/compare_backends.sh` 구조)

**성공 기준**: Frontier 모델이 Pergyra spec bundle을 in-context로 들고, 비자명한 비즈니스 로직 (예: 결제 + 멱등성 + 재시도 정책) 구현을 one-shot에 가깝게 생성할 수 있음. 컴파일/테스트 실패 시 구조화 에러로부터 자기 교정 루프가 ~3회 이내 수렴.

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리

## IR 파이프라인 재구성

- [x] **컴파일러 계약 고정** — `HIR/DIR/RIR/MIR`, resource lattice, intent compensation, projection sync, authority/capability를 `docs/37_compiler_contracts.md`에 고정

- [~] **DIR (Domain IR)** — declaration graph / intent step graph 시작
  - 완료: `src/compiler/dir.h`, `src/compiler/dir.c`, `src/compiler/dir_collect.c`, `src/compiler/dir_collect_domain.c`, `src/compiler/dir_validate.c`, `pgy --dir`, `test-dir`
- 완료: DIR owner split — `dir.c`는 graph storage / lookup / lower orchestration만 담당하고, node/role/party/world/intent collection, zone/relation/effect projection collection, validation/dump는 별도 TU로 분리됨 (`dir.c` 467 LOC, `dir_collect.c` 546 LOC, `dir_collect_domain.c` 274 LOC, `dir_validate.c` 278 LOC)
- 완료: DIR storage growth debt — node/edge/owned-name/intent/participant/step/name arrays use explicit capacity fields and geometric growth; `tests/perf_contract_smoke.sh` rejects the old `count+1` realloc pattern.
  - 완료: intent participant/type edge, step zone/ability/authority/effect edge, step predecessor dependency
  - 완료: role/ability completeness edge, missing-ability-method edge
  - 남음: richer zone/world membership graph
- [~] **RIR (Resource IR)** — slot/resource/authority/lifecycle 의미론 전용 계층
  - 범위: `Slot`, `SecureSlot`, `DeviceSlot`, projection validity, authority, effect/relation lifecycle, intent compensation resource edge
  - 완료: `src/compiler/rir.h`, `src/compiler/rir.c`, `pgy --rir`, `test-rir`
  - 완료: scope별 normalized state summary (`initial_state`, `final_state`, `last_op`, `transition error`)
  - 완료: relation/effect layer slot와 world zone slot도 resource fact로 materialize
  - 출력: 단순 map이 아니라 `Resource Graph + Transfer Ops + Static Ownership Facts`
  - explicit op 정규화:
    - `Claim/Read/Write/Release`
    - `Move/BorrowRead/BorrowWrite`
    - `ProjectRefresh/ProjectPublish`
    - `AttachEffect/DetachEffect`
    - `LinkRelation/UnlinkRelation`
    - `Authorize/AwaitRemote`
    - `CommitIntent/AbortIntent/CompensateIntentStep`
  - state lattice 초안:
    - `Uninit`
    - `Owned`
    - `BorrowedRead`
    - `BorrowedWrite`
    - `Moved`
    - `Released`
    - `Invalid`
    - `Measured`
    - `RemotePending`
  - CFG 의존 branch/join/loop/phi merge는 MIR로 이월
- [~] **MIR (Machine / Execution IR)** — CFG/SSA/liveness/optimization 계층
  - 범위: basic block, explicit instruction, phi, liveness, CFG-dependent resource merge, dead code elimination
  - 완료: `src/compiler/mir.h`, `src/compiler/mir.c`, `pgy --mir`, `test-mir`
  - 완료: HIR CFG -> MIR block bridge
  - 완료: RIR op -> MIR instruction bridge
  - 완료: intent cleanup block skeleton
  - 완료: phi materialization + incoming predecessor value list
  - 완료: block-local SSA rename skeleton
  - 완료: intent cleanup successor edge skeleton
  - 필요: `RIR-flow` merge 정책
  - 필요: richer phi merge policy
  - 필요: cleanup / rollback / detach-invalidation edge 고도화
## Progress Log — 2026-04-24 Parser/Lexer Diagnostic Routing

- 완료: parser/lexer diagnostic routing 1차 gate를 닫았다.
- 구현: `parser_error`는 `PGY_PARSE_SYNTAX`, `parse:unexpected_token`, `check-syntax`를 `Code:` / `Reason:` / `Fix:` 표면으로 출력한다.
- 구현: lexer error token은 `PGY_LEX_INVALID_TOKEN`, `lex:invalid_token`, `remove-or-escape-character`를 같은 표면으로 출력한다.
- 검증: `make parser-lexer-diagnostic-test-smoke`, `make diagnostic-registry-test-smoke`, `make test-parser`.
- 남음: parse/lex diagnostics를 driver JSON diagnostic object로 직접 흘리는 refactor는 별도 Tier 2 작업으로 유지한다.

## UTF-8 Progress Note - 2026-04-25

- `TryRecv` / `RecvTimeout` are now copy-only for the beta surface.
- Ownership-bearing payloads (`QubitSlot`, `Slot<T>`, `SecureSlot<T>`,
  `subject`, boundary-value aggregates, and `Token<T>`) are explicitly rejected.
- Use blocking `<-` receive into a named binding or a plain projection/value
  channel when ownership provenance must cross a channel boundary.

## UTF-8 Progress Note - 2026-04-25 - Cancellation Payload Boundary

- `Cancel(Future<T>)` / `Cancel(RemoteFuture<T>)` are copy-only for beta.
- Ownership-bearing payload futures (`QubitSlot`, `Slot<T>`, `SecureSlot<T>`,
  `subject`, boundary-value aggregates, and `Token<T>`) are explicitly rejected
  until task-boundary cleanup summaries can prove observation/release.

## UTF-8 Progress Note - 2026-04-25 - Channel Close Boundary

- `ChannelClose(Channel<T>)` is copy-only for beta.
- Ownership-bearing queued payload channels (`QubitSlot`, `Slot<T>`,
  `SecureSlot<T>`, `subject`, boundary-value aggregates, and `Token<T>`) are
  explicitly rejected until channel cleanup/backpressure summaries can prove
  drain/release behavior.
## Progress Log - 2026-04-26 - DAG Fallback Seam Cap

- Owner-local resolver files no longer own direct fallback helper seams. They now call
  `semantic_type_resolution_lookup_or_materialize(...)`, which checks DAG
  metadata, materializes stable constructed shells, then falls through to the
  centralized resolver fallback only when imported ability/default/bound/module
  cases still need compatibility materialization.
- `tests/type_resolution_resolver_inventory_smoke.sh` now caps active
  named fallback seams at 0, down from 20. This is still not full DAG
  source-of-truth, but it removes the old fallback helper API and prevents
  owner-local fallback seams from returning.
- Verified locally: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke` and `make test-semantic`.

## Progress Log - 2026-04-27 - DAG Fallback Classification Tightening

- Rechecked DAG closure against the current gates:
  `type-resolution-resolver-inventory-test-smoke` reports owner-local fallback
  seams at 0, while `type-resolution-dag-test-smoke` reports
  `metadata_entries=3436`, `metadata_hits=6756`,
  `materializer_fallbacks=0`.
- The central metadata materializer fallback inventory is now closed at 0.
  Missing-symbol diagnostics, generic-named fallback, and builtin
  constructor-shell/default fallback all stay on metadata-owned paths.
- Constructor-shell provenance, generic default specialization, and
  missing-symbol diagnostics are now expressed without entering the recursive
  materializer. A direct metadata-time reject for bare stable builtin shells was
  tested and rejected because it breaks generic default/multi-bound provenance
  such as `Box<Item>` validation paths; the final closure preserves that
  provenance while keeping central fallback at 0.
- Alias diagnostic inventory no longer calls back into the recursive resolver.
  `type-resolution-dag-test-smoke` gates `alias_diagnostic_resolver_calls==0`;
  the remaining 78 alias entries are repeated alias-cycle diagnostic inventory
  from semantic regression contexts.

## UTF-8 Progress Note - 2026-04-28 - Domain Helper Projection/Overlay Owner Split

- `src/semantic/type_checker_domain_projection.c` now owns projection contract
  diagnostics for domain and zone projection closure. This removes projection
  diagnostic body ownership from the domain helper shell without changing the
  diagnostic wording contract.
- `src/semantic/type_checker_overlay_common.c` now owns overlay
  symbol/shared-field/hosted-method scope setup. The domain helper shell keeps
  only the zone/effect/relation slot helper responsibility.
- `src/semantic/type_checker_decls_domain_helpers.c` is now 972 LOC. With the
  previous stdlib builtin, zone declaration, and intent helper splits, semantic
  production `.c` owners are below the 1,000 LOC hard cap.
- Verified locally: `make test-semantic pgy` remains green at 2357/0, and
  `make semantic-tu-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke` remains green. The active 1,000+ production `.c`
  owner queue is now parser-only: `ast.c`, `ast_print.c`, and
  `parser_domain.c`.

## UTF-8 Progress Note - 2026-04-28 - Parser Domain Owner Split

- `src/parser/parser_domain_roster.c` now owns roster body parsing,
  `src/parser/parser_domain_world.c` owns world body parsing,
  `src/parser/parser_domain_zone.c` owns zone body parsing, and
  `src/parser/parser_domain_event.c` owns event signature parsing.
- `src/parser/parser_domain_internal.h` exposes only the domain parser helper
  seam needed by those owners: identifier-keyword matching, child/slot append,
  domain slot parsing, projection sync parsing, and zone participant parsing.
- `src/parser/parser_domain.c` is now 970 LOC. It keeps relation/effect parsing,
  party/ability/role parsing, and the shared domain helper implementations.
- Verified locally: `make test-parser pgy` remains green. The active 1,000+
  production `.c` owner queue is now AST-only: `ast.c` and `ast_print.c`.

## Progress Log - 2026-04-30 C/LLVM Defer Cleanup Parity

- C backend `defer` no longer lowers through a file-scope GCC cleanup helper.
  That helper could not capture local method state such as `self`, which caused
  backend drift on `subject_method_recursion_defer`.
- `src/codegen/transpiler_defer_emit.h` now mirrors the LLVM lexical defer
  stack: block scopes register defer bodies, normal scope exit emits the current
  scope in LIFO order, `return` emits active defers before leaving, and
  `break`/`continue` emit defers down to the target loop's defer base depth.
- C MIR emission now consumes `AST_DEFER_STMT` directly, opens a MIR function
  defer scope, and emits active defers on MIR return/fallthrough returns. This
  closes the previous gap where source-level C lowering was fixed but
  MIR-emitted subject methods still skipped deferred state mutation.
- MIR no longer classifies `AST_DEFER_STMT` as CFG-owned control, and DCE now
  preserves defer statements as side-effecting statements. This closes the
  nested branch defer loss where `if { defer { ... } }` silently disappeared
  from MIR.
- Dynamic `defer` inside runtime-dependent `if`/match/loop control is now an explicit
  beta reject (`PGY_SEM_DEFER_DYNAMIC_CONTROL`) instead of a shared C/LLVM
  wrong-code path. Static control forms remain allowed; dynamic forms must wait
  for a runtime defer stack model rather than pretending lexical lowering is
  sound.
- The transpiler unit test now asserts the new inline lexical cleanup contract
  and rejects the old `__attribute__((cleanup(_pgy_defer_...)))` sentinel path.
- Verified slice gates: `make test-transpile` (`682/0`), `make
  llvm-test-smoke`, `make llvm-test-backend-compare` (`69/69`), `make
  cfg-body-dataflow-test-smoke`, `make air-drift-test-smoke`, and `make
  type-resolution-dag-test-smoke`.
- CI status note: a monolithic `make ci-linux` run exceeded the local 15 minute
  command window, so it is not claimed as a completed full run. The equivalent
  CI target groups were run in slices: `test-all`, tooling/stdlib/module,
  docs/runtime/diagnostics/IR/example gates, AIR nonimpact, LLVM ABI/campaign,
  and backend compare all completed green locally.

## Progress Log - 2026-05-02 Parser Intent Append Capacity Closure

- Intent declaration parsing now tracks geometric capacities for involves,
  values, binding inventory, step inventory, and intent-level `who` defaults.
  This removes the beta-core parser `count+1 realloc` path for intent headers
  and body clauses.
- Intent step parsing now tracks geometric capacities for `who`,
  `authorized by`, `requires`, `on`, and `compensate` clause collections. The
  parser helper seam now takes explicit capacity pointers instead of hiding
  one-element growth behind `intent_append_node` / `parse_intent_name_list`.
- `tests/perf_contract_smoke.sh` now gates these capacity fields and rejects
  regression to count-only append in `parser_intent.c` and
  `parser_intent_step.c`.
- Verified slice probes: direct GCC compile probes for `parser_intent.c`,
  `parser_intent_step.c`, and `ast_domain_constructors.c`.

## Progress Log - 2026-05-02 Parser Domain Local Buffer Capacity Closure

- World composed-state input-name parsing now uses a local geometric capacity
  buffer instead of rebuilding the temporary array on every input.
- Zone grouped slot/layer name parsing now uses a local geometric capacity
  buffer instead of `malloc(count+1) + memcpy + free` for every name.
- `tests/perf_contract_smoke.sh` now rejects count-only append regressions in
  `parser_domain_world.c` and `parser_domain_zone.c`.
- Verified slice probes: direct GCC compile probes for
  `parser_domain_world.c` and `parser_domain_zone.c`.

## Progress Log - 2026-05-02 Parser Projection Buffer Capacity Closure

- Projection target-name parsing now uses a geometric capacity buffer instead of
  rebuilding the target array per refresh/publish/bind target.
- Projection field-map parsing now uses geometric capacity for paired
  target/source field arrays instead of per-entry `malloc(count+1) + memcpy`.
- `tests/perf_contract_smoke.sh` now rejects count-only append regression in
  `parser_domain_projection.c`.
- Verified slice probe: direct GCC compile probe for
  `parser_domain_projection.c`.

## Progress Log - 2026-05-02 Parser Match Capacity Closure

- Match statement case lists and match-case OR-pattern lists now carry explicit
  capacities and grow geometrically during parsing.
- `tests/perf_contract_smoke.sh` now gates `pattern_capacity` and rejects
  match case / pattern regression to count-only append.
- Verified slice probes: direct GCC compile probes for `parser_stmt.c` and
  `ast_constructors.c`.

## Progress Log - 2026-05-02 Parser Enum Method Capacity Closure

- Enum method lists now carry explicit capacity and grow geometrically during
  parsing instead of reallocating to `method_count + 1` per method.
- `tests/perf_contract_smoke.sh` now rejects enum method append regression to
  count-only growth.
- Verified slice probe: direct GCC compile probe for `parser_enum.c`.

## Progress Log - 2026-05-02 Parser Lambda Parameter Capacity Closure

- Lambda parameter lists now carry explicit capacity and use the shared
  expression-list capacity helper. The old count-only `parser_append_expr_node`
  helper was removed from `parser_expr.c`.
- `tests/perf_contract_smoke.sh` now rejects expression append paths that bypass
  the capacity helper.
- Verified slice probes: direct GCC compile probes for `parser_expr.c` and
  `ast_domain_tail_constructors.c`.

## Progress Log - 2026-05-02 Parser Domain Shared Append Capacity Closure

- Shared domain parser append helpers now require explicit capacity pointers:
  `append_domain_slot` and `append_child_node` no longer own hidden `count+1`
  realloc growth.
- Domain-heavy AST payloads now carry capacity fields for party/roster/world,
  relation/effect/zone, role-slot ability lists, zone authority abilities, and
  ability/role/impl/event parser lists.
- Projection sync append now passes the destination refresh capacity through the
  domain parser helper seam, so relation/effect/zone projection entries use the
  same capacity contract.
- The parser `count+1` append scan for the tracked patterns is now empty.
  `tests/perf_contract_smoke.sh` rejects regression in the shared domain
  helpers.
- Verified slice probes: direct GCC compile probes for `parser_domain.c`,
  `parser_domain_relation_effect.c`, `parser_domain_zone.c`,
  `parser_domain_projection.c`, `parser_domain_world.c`,
  `parser_domain_roster.c`, and `parser_domain_event.c`.

## Progress Log - 2026-05-02 Parser Count+1 Append Scan Closure

- Structured comment tags now carry `tag_capacity` and grow geometrically in
  `parser_doc.c`.
- The tracked `count+1` append scan across `src/**/*.c` and `src/**/*.h` is now
  empty for the grep patterns used by the perf/debt audit.
- `tests/perf_contract_smoke.sh` now gates structured-comment tag capacity and
  rejects regression to `tag_count + 1`.
- Verified slice probe: direct GCC compile probe for `parser_doc.c`.

## Progress Log - 2026-05-02 LLVM MIR Inventory Diagnostic Routing

- Added `llvm_set_mir_inventory_missing(...)` as the central diagnostic helper
  for LLVM MIR inventory gaps. It always attaches
  `PGY_CODE_LLVM_MIR_ROUTINE_MISSING`,
  `PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING`, and
  `PGY_FIX_INSPECT_MIR_INVENTORY`.
- Converted the intent entry path and top-level/class method pipeline missing
  inventory errors, including the top-level executable wrapper path, from
  ad-hoc error routing to the central helper.
  These remain honest hard errors; the change closes diagnostic routing drift,
  not the underlying declaration inventory TODO.
- `tests/mir_declaration_inventory_smoke.sh` now gates the helper and rejects
  plain-error regression for these MIR-missing diagnostics.
- Verified slice probes: direct GCC compile probes for `llvm_error.c`,
  `llvm_intent.c`, and `llvm_pipeline.c` with `PGY_LLVM_ENABLED`.

## Progress - 2026-05-02 - LLVM MIR declaration metadata-first close

- LLVM nominal method forward registration now iterates `MIRDeclMethod`
  metadata arrays through `llvm_host_decl_method_metadata(...)` instead of
  walking `class_decl.methods[]` / `enum_decl.methods[]` AST arrays.
- LLVM class-method routine emission now consumes `MIRDeclMethod.has_routine`
  and `MIRDeclMethod.routine_index`. The old local method-routine fallback scan
  was removed so missing links fail through the shared MIR inventory diagnostic.
- `llvm_set_mir_inventory_missing(...)` remains the required path for
  declaration/routine inventory gaps. `mir_declaration_inventory_smoke.sh`
  now gates metadata-first registration and rejects AST method-array loops.
- The legacy `llvm_find_host_decl_methods_in_context(...)` / `llvm_host_decl_methods(...)`
  AST method-array seam is removed. `llvm_find_host_method_decl_in_context(...)`
  is now metadata-only and returns the method AST only as a diagnostic/codegen
  anchor from `MIRDeclMethod`.
- C backend nominal host-method lookup now also checks `MIRDeclHeader.method_metadata`
  first. When a MIR header exists, `find_nominal_host_method_decl(...)` no
  longer falls back to AST method-array scans for missing methods.
- Zone projection sync and intent-step subject action lookup now delegate to
  the same C backend MIR-aware nominal host-method lookup seam instead of
  open-coding `class_decl.methods[]` scans.
- `transpiler_find_mir_method(...)` now resolves method routines through
  `MIRDeclHeader.method_metadata` before falling back to the legacy routine
  scan. All C method emitters that already use this seam now benefit from the
  linked declaration metadata.
- The public C backend `transpiler_decl_methods_local(...)` AST method-array
  seam is removed. The remaining AST method-list access is quarantined as a
  static fallback inside `transpiler_decl_host_lookup.c` for MIR-absent paths.
- C class/enum method emission now also chooses its method iteration source
  from `MIRDeclHeader.method_metadata` when MIR is present. AST method arrays
  remain only as the no-MIR fallback/emission anchor.
- Shared C domain method body emission (`emit_hosted_methods_from_mir_or_error_local`)
  now also chooses its active method source from `MIRDeclHeader.method_metadata`
  before falling back to AST method arrays.
- Domain forward declarations for party/roster/relation/effect/zone/world now
  also use `MIRDeclHeader.method_metadata` when MIR is present, with AST arrays
  left as the no-MIR fallback.
- The repeated metadata/fallback selection was folded into
  `TranspilerHostedMethodView`, so C backend hosted-method emitters share one
  policy instead of open-coding MIR header checks in each owner.
- `emit_hosted_methods_from_mir_or_error_local(...)` now accepts a
  `TranspilerHostedMethodView` directly, so the shared body-emission helper no
  longer exposes an AST method-array API.
- Local lightweight gate passed:
  `documentation_quality_smoke`, `perf_contract_smoke`, `source_utf8_smoke`,
  `test_inc_size_smoke`, `air_drift_smoke`,
  `type_resolution_resolver_inventory_smoke`,
  `mir_declaration_inventory_smoke`, and `beta_readiness_checklist_smoke`.

## Progress - 2026-05-02 - Debt Ledger Refresh: CFG/AIR/DAG/MIR Runtime Seams

Closed or narrowed in this slice:

- C/LLVM thread-pool requirement detection now shares one owner:
  `src/codegen/thread_pool_usage.c`. The C backend and LLVM pipeline no longer
  carry duplicate AST/MIR walkers for this feature. The helper prefers
  instruction-carried AST provenance; source-array traversal has been removed
  from this usage decision.
- MIR SSA use-edge collection now prefers instruction-carried provenance for
  `DEF`, `RETURN`, and branch operands. Source statement / source terminator
  arrays remain fallback context, not the first source of use facts.
- MIR value-summary collection now counts slot writes from `MIR_INST_DEF`
  anchors through `mir_instruction_slot_anchor(...)`; it no longer walks
  `block->source_statements` for the write summary.
- C backend pending-use materialization now finds local let declarations from
  block `MIR_INST_DEF.ast` provenance before using the function-wide fallback.
  It no longer scans `block->source_statements` directly.
- C backend source-order scheduling now consumes `MIRInstruction`
  `source_statement_index` metadata instead of walking `block->source_statements`.
  Codegen no longer directly consumes MIR block source statement arrays for
  thread-pool usage, intent-observability usage, pending-use materialization, or
  source-order scheduling.
- Runtime intent exit now uses the active-registry index helper for stable
  active-handle lookup instead of scanning all active entries. The remaining
  linear scans are either free-slot allocation or semantic conflict scans, not
  the stable exit lookup path.
- Hosted-method declaration views in both C and LLVM now reject silent AST
  fallback when `requires_mir_metadata` is set and MIR metadata was not
  available. This makes the remaining MIR declaration bootstrap debt fail
  visibly instead of silently drifting back to AST-carried inventory.

Remaining debt after this slice:

- CFG/MIR is closer to source-of-truth for body facts, but not fully closed.
  MIR lowering still carries HIR source arrays as construction input, and
  codegen still has declaration/routine AST compatibility seams, but C backend
  block-local usage/pending/order facts now consume MIR instruction provenance.
  Ownership/drop/zone/effect consumers are not yet all forced through CFG/MIR facts.
- AIR consumes first-class evidence inventory for the covered facts, but it is
  not yet the verifier for every abstraction boundary. Trace/observability ABI
  evidence now flows through a global `AIREvidenceNode`; remaining AIR closure is
  consumer coverage for deeper effect propagation drift and DAG/module evidence,
  not ad-hoc side checks.
- DAG fallback counters remain zero, and materializer fallback now diagnoses as
  retired compatibility debt. The remaining work is not a numeric fallback
  cleanup; it is removing the recursive-resolver compatibility seam from
  semantic judgement paths and proving intent/zone/generic/module owners all
  consume the DAG-facing APIs.
- MIR/LLVM declaration debt is narrowed but not closed. Hosted method metadata
  is metadata-first and no longer silently falls back when MIR metadata is
  required, but `MIRProgram` still carries AST-backed declaration payloads and
  LLVM can still report missing MIR routines for unmaterialized declaration
  paths.
- Runtime propagation still needs the full transitive frontier scheduler. The
  shared frontier policy and bounded world/zone/projection loops are stronger,
  but the scheduler is not yet the single runtime propagation source of truth.

Local verification for this debt refresh:

- Direct GCC compile probes passed for `thread_pool_usage.c`,
  `transpiler_thread_pool.c`, `llvm_pipeline.c`, `mir.c`, `transpiler.c`, and
  `pgy_runtime_lib.c`.
- Shell gates passed:
  `build_source_inventory_smoke.sh`, `cfg_body_dataflow_smoke.sh`,
  `runtime_intent_observability_contract_smoke.sh`,
  `mir_declaration_inventory_smoke.sh`,
  `type_resolution_resolver_inventory_smoke.sh`, `source_utf8_smoke.sh`, and
  `test_inc_size_smoke.sh`.
- `tests/type_resolution_dag_smoke.sh` was not run locally because it requires
  `SEMANTIC_TEST_BIN`. Full `make` regression was not run in this environment.

## Progress - 2026-05-02 - Thread Pool Usage Fact Tightening

- Shared C/LLVM thread-pool usage detection now treats `await` and
  `task-group` as direct runtime-thread-pool surfaces instead of relying on
  nested fallback traversal.
- MIR instruction scanning now checks `inst->ast`, `inst->expr0`, and
  `inst->expr1` only. Source-only MIR block arrays are no longer consulted for
  thread-pool usage decisions.
- `tests/parallel_core_contract_smoke.sh` now gates the shared owner and both
  backend consumers so C/LLVM cannot reintroduce duplicate thread-pool usage
  walkers.
- The structural AST walk for thread-pool surfaces now lives in
  `src/parser/ast_analysis.c` as `ast_uses_thread_pool_surface(...)`.
  `src/codegen/thread_pool_usage.c` is now an adapter over MIR instruction
  provenance.
- C and LLVM backend entry points no longer re-scan the synthetic
  `__pgy_top_level_exec` AST body. Top-level executable code must appear in the
  MIR routine inventory, so thread-pool detection now has one backend entry
  contract: iterate MIR routines and consume `pgy_mir_routine_uses_thread_pool`.
- Closed in this slice: the source-only block fallback was removed from
  `thread_pool_usage.c`. If a future construct needs the runtime thread pool,
  it must be materialized as MIR instruction-carried provenance.

## Progress - 2026-05-02 - Intent Zone-Authority Compression Slice

- Intent compression now covers the first authority slice: when an
  authority-sensitive step has exactly one `who` participant and that
  participant resolves unambiguously to the current zone's authority subject
  slot, semantic analysis derives `authorized by: <who>` instead of forcing
  duplicate syntax.
- The canonical owner remains the zone/resource layer. The intent step only
  records `derived_authorized_by_from_zone` provenance and then consumes the
  same authorized-by validation path as explicit syntax.
- Pure local-zone declarations still require explicit approval, so toy
  declarative steps keep the existing fix-oriented diagnostic. Derivation is
  limited to authority-sensitive surfaces such as secure helpers, transfers,
  causes/effects, and action-derived authority flows.
- Gate: `intent_compression_contract_smoke.sh` now checks the provenance flag,
  AST print wording, contract-summary wording, authority derivation owner, and
  semantic regression names.

## Progress - 2026-05-02 - AIR Authority Provenance Lift

- The zone-derived `authorized by` provenance now flows through
  `DIRIntentStep.authorized_by_derived_from_zone` into
  `AIRBoundaryNode.authority_from_zone`.
- AIR text/JSON dumps now expose `authority_from_zone`, and strict AIR
  provenance messages include `authority_provenance=zone-derived|explicit|none`
  so LSP/CI consumers can distinguish explicit approval from compressed
  zone-owner inference.
- AIR validation rejects impossible shapes where zone-derived authority is set
  on a non-authority boundary or outside zone/world boundaries.
- AIR cleanup evidence accounting was repaired in the same slice:
  `AIR_EVIDENCE_MIR_CLEANUP` now consumes MIR CFG cleanup successors first,
  while boundary-specific pin evidence stays under
  `AIR_EVIDENCE_MIR_PIN_CLEANUP`. Gate: `make test-air` (`62/0`).

## Progress - 2026-05-02 - DAG Intent Annotation Seam Shrink

- `type_checker_intent_participants.c` now reads intent participant type
  annotations through `semantic_type_resolution_lookup_annotation_or_unknown`,
  not the metadata-first materializer helper.
- `type_checker_intent_transfer.c` now uses the same annotation-only path for
  transfer source/target bindings and step `where` type reads.
- `type_checker_intent_action_contract.c` now uses annotation-only reads for
  inherited action parameter type matching.
- `type_checker_zone_decl_authority.c` now uses annotation-only reads for
  authority subject-slot type validation, closing the zone authority
  subject-slot seam from the active materializer allowlist.
- `type_checker_ability_decl.c` now uses annotation-only reads for abstract
  ability method signature validation. This removes one more signature-only
  owner from the materializer allowlist.
- `type_resolution_resolver_inventory_smoke.sh` now caps materializer helper
  owners at `18` instead of `25`. This is a stricter gate: seven semantic
  owners were removed from the materializer allowlist. `type_checker_projection_path.c`
  now consumes annotation facts for projection field-path type reads, so
  projection diagnostics no longer create type metadata as a side effect.
  `type_checker_ownership_destructure.c` also consumes annotation facts for
  destructuring ownership type reads, keeping that body-safety path read-only
  with respect to DAG metadata creation.
  `type_checker_intent_decl.c`
  remains on the allowlist for now because intent header binding symbols are
  installed before all annotation metadata is safe to consume in the current
  stage order.
- Rechecked the next obvious candidates and kept them on the allowlist:
  `type_checker_world_helpers.c`, `type_checker_func_action_contract.c`, and
  `type_checker_intent_role_fields.c` all still require the materializer seam.
  Converting them to annotation-only reads regresses semantic/DAG smoke because
  effect/action/compressed-intent checks can run before the relevant metadata is
  guaranteed to be materialized. The next DAG closure step is therefore a
  stage-order/materialization prepass fix, not another local helper rename.
- `type_checker_ability_where.c` no longer owns that materializer seam:
  effective ability generic actuals now materialize through
  `collect_effective_generic_arg_types(...)` in
  `type_checker_generic_effective_args.c`.
  Ability where validation consumes the resulting type evidence and otherwise
  stays annotation-only.
- DAG stage signature now installs generic parameter scope while staging class
  and ability signatures. This does not remove another allowlist owner by
  itself, but it closes a real stage-order gap: staged field/method signature
  materialization now sees the same generic parameter vocabulary that the full
  semantic checker later installs.
- Local gates: `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, and `make test-semantic`.

## Progress - 2026-05-02 - Intent Single-Subject Who Inference Slice

- Intent compression now has its first fail-closed `who` inference rule:
  if an intent step omits `who`, no action/default already supplied it, and the
  enclosing intent has exactly one subject participant, semantic analysis
  derives `who` from that single subject participant.
- The rule deliberately does not infer across multiple subject participants.
  Ambiguous participant sets keep the existing explicit-`who` requirement and
  are covered by a negative semantic regression.
- Provenance is carried end-to-end:
  `ASTIntentStepData.derived_who_from_single_participant` flows through AST
  print/contract summary diagnostics, DIR, AIR, and `pgy.air.graph.v1` JSON as
  `who_from_single_participant`.
- This is not full Intent-Compress. Remaining compressed-default work still
  includes receiver/enclosing-subject `who`, broader `where`, requires/guard
  inference policy, explicit-vs-inferred conflict diagnostics, and example
  migration.
- Test owner cleanup: AIR evidence tests were split at function boundaries into
  `test_air_evidence_part_b.cases.h` and
  `test_air_observability_pin_part_g.cases.h`, restoring the test-case size
  gate without changing AIR behavior.
- Local gates: `make test-semantic` (`2430/0`), `make test-air` (`65/0`),
  `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`, and
  `make source-utf8-test-smoke`.

## Progress - 2026-05-02 - Intent On-Receiver Who Inference Slice

- Intent compression now has a second fail-closed `who` inference rule:
  if a step omits `who` and carries `on: receiver.Action(...)`, semantic
  analysis derives `who` from the receiver only when the receiver is an intent
  subject participant and that subject declares the referenced action.
- Multiple distinct matching receivers do not infer. The step remains explicit
  and existing participant/action diagnostics own the failure. This prevents
  compressed syntax from becoming an authority or effect owner.
- Provenance is carried end-to-end:
  `ASTIntentStepData.derived_who_from_on_receiver` flows through AST print,
  semantic contract summary, DIR, AIR, and `pgy.air.graph.v1` JSON as
  `who_from_on_receiver`.
- The owner is responsibility-named, not size-split:
  `type_checker_intent_on_inference.c` owns on-clause inference, while
  `type_checker_intent_action_contract.c` stays focused on inherited action
  contracts. This keeps all production owners below the 600 LOC review gate.
- This is still not full Intent-Compress. `where`, `using`, `requires`, and
  `authorized by` inference remain separate closure work with explicit
  conflict diagnostics.
- Local gates: `make test-semantic` (`2443/0`), `make test-air` (`65/0`),
  `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`,
  `make source-utf8-test-smoke`, and `make documentation-quality-test-smoke`.

## Progress - 2026-05-02 - Intent On-Receiver Where/Using Inference Slice

- `on: receiver.Action(...)` now supplies a narrow `where` derivation when the
  resolved receiver action declares `within <Zone>` and the step has no
  explicit/local zone. This works even when the step name differs from the
  action name, making the `on` clause a real evidence source instead of only a
  redundant execution clause.
- The existing unique-zone-binding rule then derives `using` from the inferred
  zone when the intent has exactly one binding of that zone type. No new public
  syntax or keyword was added.
- The rule remains fail-closed: conflicting `within` zones across multiple
  `on` calls do not infer, and explicit `where` continues to win.
- `type_checker_intent_on_inference.c` owns the receiver/action evidence path;
  shared subject-action lookup moved to `type_checker_intent_helpers.c` so the
  action-contract owner stays below the 600 LOC review threshold.
- This still does not infer `requires` or `authorized by` from arbitrary `on`
  clauses. `authorized by` remains owned by action/zone authority validation,
  not by intent compression.
- Local gate: `make test-semantic` (`2454/0`).

## Progress - 2026-05-02 - Intent On-Receiver Action Contract Slice

- A step with exactly one resolved `on: receiver.Action(...)` now inherits the
  action contract's `requires` and `causes` clauses when the step has not
  declared those clauses locally.
- `authorized by self` is mapped to the on-call receiver alias and recorded as
  `inherited_authorized_by_from_action`.
- The same evidence path now also maps `authorized by <action-param>` to the
  corresponding single `on` call argument when that argument is a declared
  intent participant identifier. Non-identifier arguments, missing parameter
  bindings, and multiple `on` calls remain explicit.
- Action-derived zone, ability, effect, and authority provenance now reaches DIR/AIR as
  `where_inherited_from_action` / `source_from_action` and
  `requires_inherited_from_action` / `requires_from_action`,
  `causes_inherited_from_action` / `causes_from_action`, and
  `authorized_by_inherited_from_action` / `authority_from_action`. AIR
  diagnostics report the authority side as `authority_provenance=action-inherited`
  instead of flattening it into `explicit`.
- Action-derived authority is now checked through RIR evidence in the parsed
  on-receiver AIR regression: `authority_from_action` must pair with matching
  `AIR_EVIDENCE_RIR_AUTHORITY` / `rir_authority_evidence_name` rather than
  remaining a boundary flag only.
- Multiple or conflicting `on` actions remain fail-closed. The implementation
  does not union ability requirements or merge effect clauses across actions.
- Shared ability-clone append logic moved to `type_checker_intent_helpers.c`,
  keeping `type_checker_intent_on_inference.c` focused on on-clause evidence
  and `type_checker_intent_action_contract.c` focused on step-name action
  inheritance.
- Local gates: `make test-semantic` (`2490/0`), `make test-air` (`66/0`),
  `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`,
  `git diff --check`, `make source-utf8-test-smoke`, and
  `make documentation-quality-test-smoke`.

## Progress Log - 2026-05-03 CFG Loop Snapshot Lifetime Closure

- `for` / `while` flow now restores merged resource state before destroying
  the loop scope. This prevents loop-local snapshot symbols from writing
  through freed scope storage during MIR/transpile lowering.
- `parallel` task flow now restores the entry ownership snapshot before
  destroying each task scope, keeping task-local symbols out of post-scope
  writes while preserving joined conflict analysis.
- Function signature metadata misses now fail closed to `TYPE_UNKNOWN` instead
  of reaching `type_create_function(...)` as `NULL`.
- C backend MIR block lookup now prefers exact `source_statement_index`
  metadata over block-AST name search for preserved let statements.
- AIR boundary evidence now rejects fact-count drift: each boundary evidence
  node must carry exactly one boundary fact. This prevents hand-built or
  JSON-fed evidence from widening HIR/RIR/MIR boundary proofs into ambiguous
  multi-fact claims.
- Local native MinGW gates: `test-semantic` (`2500/0`) and `test-transpile`
  (`710/0`), plus `test-air` (`68/0`). The POSIX Makefile path is still
  blocked locally by Git Bash `Win32 error 5`, so these were run through direct
  object rebuild/link.
