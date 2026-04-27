# Codegen Zero/Default-Init Idiom Audit

마지막 업데이트: 2026-04-21

## 목적

C 백엔드(transpiler)와 LLVM 백엔드가 동일 Pergyra 소스에 대해 `zero/default` 값을 어떤 idiom으로 emit하는지 실측 감사. 동일 의미론이어야 할 두 경로가 실제로 동일한가, 혹은 silent divergence가 있는가를 기록한다.

맥락: `docs/70_beta_closure_master_board.md`는 C/LLVM parity를 `81%`로 보고. 본 감사는 그 잔여 19%가 어디에 있는지 `init idiom` 축에서 해부한다.

x86 instruction 수준의 선택 — 예컨대 `xor eax, eax` vs `sub eax, eax` — 은 Pergyra 레이어가 아니다. Pergyra는 C source와 LLVM IR을 emit하고 최종 x86은 gcc/LLVM이 결정한다. 본 문서는 우리 레이어에서의 동일 질문 — "두 백엔드가 같은 IR-level idiom을 쓰는가" — 에 답한다.

## 감사 대상

| Case | Pergyra surface |
|---|---|
| 1 | uninit local `let x: Int;` (annotation 있고 init 없음) |
| 2 | struct/class default construction |
| 3 | Slot default (claim) |
| 4 | array literal |
| 5 | Option/Result None |
| 6 | pointer NULL |

## Case × Backend 매트릭스 (수정 후 기준)

| Case | C 백엔드 idiom | LLVM 백엔드 idiom | Divergence |
|---|---|---|---|
| 1. uninit local | **semantic에서 사전 차단** (PGY_SEM_UNINIT_LOCAL) | **semantic에서 사전 차단** | RESOLVED |
| 2. struct default (constructor, 필드 없음) | `T x = {0};` (L785) / aggregate fallback `T x = (T){0};` (L815 guard) | `LLVMConstNull + store` + 필드별 GEP store | LOW (의미 동일) |
| 3. Slot default (ClaimSlot) | `pgy_claim_T()` 런타임 콜 | `LLVMConstNull + store` + 필드별 store (런타임 콜 없음) | **MEDIUM — 의도된 비대칭으로 확정 (아래 "Case 3 contract" 참조)** |
| 4. array literal | statement-expr: `({PgyArray_T a = pgy_array_new_T(n); pgy_array_push_T(&a, ...); a;})` | `LLVMConstNull + store` + `pgy_array_push_T` 콜 | LOW (의미 동일) |
| 5. Option/Result None | `None_T()` 런타임 콜 | 전용 default 없음, 일반 struct 경로 | LOW (명시 생성자 강제) |
| 6. pointer NULL | `= 0` (pointer context) | `LLVMConstPointerNull` | LOW |

### 인용 위치 (확인용)

- C 백엔드
  - `src/codegen/transpiler_emitters_base_a.inc:196-197, 229` — Slot claim 런타임 콜
  - `src/codegen/transpiler_emitters_base_a.inc:748-763` — positional constructor → designated init
  - `src/codegen/transpiler_emitters_base_a.inc:785` — constructor-call without fields → `{0}`
  - `src/codegen/transpiler_emitters_base_a.inc:805` — callable (function pointer) → `= 0` (pointer NULL)
  - `src/codegen/transpiler_emitters_base_a.inc:815-824` — aggregate-aware fallback (scalar `= 0` / aggregate `= (T){0}`)
  - `src/codegen/transpiler_expr_emitters.inc:4164-4172` — array literal statement-expression
  - `src/codegen/transpiler_emitters_mir_inventory_ssa.inc:1033-1046` — `transpiler_c_type_uses_scalar_zero` helper
- LLVM 백엔드
  - `src/codegen/llvm_stmt.c:1769-1831` — `llvm_emit_let_decl` (init present 경로만 store)
  - `src/codegen/llvm_expr.c:79-93` — array literal `LLVMConstNull`
  - `src/codegen/llvm_expr.c:160-185` — party/class instance default
  - `src/codegen/llvm_stmt.c:1200-1252` — slot claim IR (direct struct init)
  - `src/codegen/llvm_domain.c:2232`, `src/codegen/llvm_stmt.c:3033` — global 초기값
- Semantic guard
  - `src/semantic/type_checker_ownership_let_infer.inc` — 함수-바디 uninit local 거부
  - `src/semantic/diag_codes.h` — `PGY_CODE_SEM_UNINIT_LOCAL`, `PGY_CAUSE_UNINIT_LOCAL`, `PGY_FIX_INITIALIZE_AT_BINDING`

## 이번 sprint 수정 (완료)

### 1. Semantic 차단 — `let x: T;` (no init) 거부

`type_checker_ownership_let_infer.inc` 의 `ann != NULL && init == NULL` 경로에 explicit 거부 추가. 에러 코드 `PGY_CODE_SEM_UNINIT_LOCAL`, cause `semantic:let:uninit_local_binding`, fix action `initialize-at-binding`.

메시지:

```
Local binding '<name>' has a type annotation but no initializer.
Reason:
- function-body lets must be initialized at the binding site
- the backends diverge on uninitialized reads
Fix:
- provide an initializer directly: 'let <name>: <T> = ...'
- or use a conditional expression as the initializer
```

**파서 구조 확인**: class/subject field는 `ClassField` 경로로 parser_decl.c:925-944 에서 처리되어 `AST_LET_DECL` 노드가 생기지 않는다. function parameter는 `AST_LET_DECL`을 쓰지만 `type_check_let_decl`이 아닌 dedicated param validation을 통과한다. 따라서 본 거부는 **함수-바디 지역 변수 한정**으로 정확히 fire된다.

### 2. C 백엔드 L815 aggregate-aware fallback

원래:

```c
codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
```

수정 후:

```c
} else if (transpiler_c_type_uses_scalar_zero(c_type)) {
    codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
} else {
    codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, name, c_type);
}
```

semantic 차단으로 이 경로가 실제로 fire되지는 않지만, defense in depth. `transpiler_c_type_uses_scalar_zero` 는 이미 `transpiler_emitters_base_b.inc:607` 에서 쓰이던 기존 helper라 별도 정의 불필요.

### 3. Case 3 (Slot default) — 의도된 비대칭으로 확정

C 백엔드는 `pgy_claim_T()` 런타임 helper를 호출한다. LLVM 백엔드는 struct를 직접 zero-init하고 `claimed=true` 플래그 필드를 쓴다 (`llvm_stmt.c:1200-1252`). 이 비대칭은 **설계 의도**로 받아들인다:

- 런타임 helper 측 side-effect (slot pool 카운터, profiling hook 등)은 현재 관측되지 않는다
- LLVM 경로가 LLVM smoke test와 `compare_backends.sh` 35 case에서 녹색으로 동작 중이다
- 두 경로를 단일 helper로 통합하려면 LLVM에서 `pgy_claim_T` 선언/링크 경로 + 반환 struct 복사 + slot registry hook 재설계가 필요하다. 본 감사 범위 밖

향후 runtime observability가 slot pool 상태를 실측 노출하기 시작하면 (docs/70 의 `runtime observability 76%` 항목이 확장되면) 재감사.

## 내부 inconsistency (parity와 별개, 수정 안 함)

### C 백엔드

- `transpiler_emitters_base_a.inc:782` 의 `init_expr != NULL ? init_expr : "0"` 폴백은 domain-type (zone/world/relation/effect/party/roster) constructor-call 경로 안에서 `emit_expression`이 NULL을 반환할 때의 cosmetic fallback이다. 실제로 fire되는지 test coverage로 확인 불가. **zero-init site가 아님** — init은 non-NULL 전제인 경로의 안전 장치. 변경 안 함.

### LLVM 백엔드

- 프리미티브 global → `LLVMConstInt(ctx->type_i32, 0, 0)` (`llvm_stmt.c:3033`) — select round-robin 카운터용. "numeric zero" 의도가 명시적이라 `LLVMConstNull` 로 바꿀 이유 없음. 드리프트 아님.
- aggregate global → `LLVMConstNull(struct_type)` (`llvm_domain.c:2232`) — 적절한 의도 표현.

### 양쪽 공통

- `llvm.memset` intrinsic 사용 0건. 큰 aggregate에 대해서도 필드별 `store`로 처리. O0 pathological case에서 코드 크기 영향 가능하지만 LLVM O1+ 에서는 mem2reg/SROA가 흡수. 실측 perf 문제 보고된 바 없음.

## Parity 테스트 현황

- `tests/compare_backends.sh` (414 LOC) — exit code + stdout + stderr만 비교. IR 비교 없음
- `tests/diagnostics_json_smoke.sh` — 에러 reporting 포맷만 검증, codegen 무관
- `.c.txt` / `.llvm.txt` 분기 인프라 존재하나 `examples/biome_simulator/` 1개만 사용 (그마저 내용 동일)
- 35개 backend_compare case 중 uninit-local / struct-default / slot-fresh-alloc / array-fresh-zero를 **명시적으로 exercise하는 case는 없음**

Case 1은 이제 semantic 레벨에서 차단되어 backend-compare 에서 exercise할 필요가 없다. Case 3 MEDIUM은 backend-compare에서 관측 가능한 side effect가 없어 현재 exercise할 입력 설계가 어렵다 — 해당 테스트 확장은 runtime observability 확장 작업의 일부로 다뤄진다.

## Divergence risk 판정 (수정 후)

### Case 1 — RESOLVED

이번 sprint에 semantic 차단 도입. 두 백엔드 모두 이 경로를 evaluate하지 않는다.

### Case 3 — MEDIUM, 의도된 비대칭

위 "Case 3 contract" 참조. 운영 중 관측 가능한 문제는 없음. runtime observability 확장 시 재감사.

### Case 2, 4, 5, 6 — LOW

의미 동일, 스타일만 다름.

## 회귀 커버리지

본 sprint에 추가된 semantic 테스트 (`src/tests/semantic/test_semantic_misc_a_part_{a,b}.cases.h`):

1. `function-body let with annotation and no initializer is rejected` — `let x: Int;` 거부 확인
2. `function-body let with aggregate annotation and no initializer is rejected` — `let b: Box<Int>;` 거부 확인
3. `subject field let with no initializer does not trigger the uninit-local guard` — subject field path는 guard 미발동 확인 (negative test로 명시)

## 다음 sprint 후보

우선순위 순:

1. **Slot claim contract 재정렬** — runtime observability 확장이 완료되면 LLVM도 claim helper 경유로 수렴시키고 양쪽이 같은 slot registry hook을 trigger하도록. 현재는 "의도된 비대칭"으로 동결.
2. **parity test harness 확장** — init idiom 축 외의 runtime side effect parity를 backend-compare에서 직접 exercise할 수 있는 harness 추가. 현재는 stdout 중심이라 side effect 비대칭 관측 어려움.
3. **LLVM 백엔드 uninit defense in depth** — 현재는 semantic 차단에 의존. 미래에 semantic이 버그로 uninit을 놓쳐도 LLVM이 `store zeroinitializer` 안전망을 emit하도록 보강 (선택).
4. **C 백엔드 L782 dead path 정리** — domain-type constructor 경로 안 `"0"` fallback의 실제 fire 가능 여부 확인 후 제거 또는 명시 error로 승격.

## cross-link

- `docs/70_beta_closure_master_board.md` — C/LLVM parity 81% 베이스라인
- `docs/72_diagnostic_codes.md` — `PGY_CODE_SEM_UNINIT_LOCAL` 등 진단 코드 (본 sprint에 추가)
- `docs/73_diagnostic_vocabulary.md` — canonical 용어 (slot handle 등)
- `docs/91_build_troubleshooting.md` — 빌드 재현성
- `docs/92_inc_split_roadmap.md` — semantic .inc 분할 로드맵 (본 감사와 독립)
