# Semantic Declaration TU 분리 템플릿

마지막 업데이트: 2026-04-24

이 문서는 `src/semantic/type_checker_decls_*.inc` 안에 섞여 있는 declaration body 를 **독립 Translation Unit (`.c`)** 로 승격시키는 표준 절차를 정의한다. 목표는 `.inc` 크기 감축 자체가 아니라 **owner boundary 와 dependency direction 고정** 이다 — 그 결과로 이후 "다른 지향 / 새 keyword / 새 paradigm" 이 나올 때 **이 문서만 따라가면 plug-in 된다**.

## 1. 왜 이 템플릿인가

[docs/100_beta_readiness_checklist.md](100_beta_readiness_checklist.md) §1 의 의도:

> 장기 모듈화는 `.inc` 제거율 자체가 아니라 owner boundary 와 dependency direction 을 명확히 한다. 신규 core 변경이 multi-thousand-line include fragment 를 직접 수정하지 않아도 된다.

그리고 [docs/99_language_module_taxonomy.md](99_language_module_taxonomy.md) 의 layering:

```
pgy.foundation -> pgy.core -> pgy.execution
                            -> pgy.compat.oop
                            -> pgy.compat.fp
                            -> pgy.kit.*
```

컴파일러 TU 구조는 이 layering 을 **반영** 해야 한다. core 가 compat.oop / compat.fp TU 에 역의존하지 않으며, compat 모듈을 "쓰지 않는" 사용자에게는 해당 모듈의 semantic 체크 코드가 core 흐름에 개입하지 않는 것이 궁극적 방향.

## 2. 두 가지 템플릿

declaration kind `X` 를 분리할 때 두 경우가 있다. `X` 가 type graph 에 연결될 필요가 있는지가 분기점.

### 템플릿 A — Body-only split (간단 경우)

조건:
- `X` 가 type resolution DAG 에 별도 precollect 노드를 등록할 필요 **없음** (현재 또는 당분간)
- 기존 `type_checker_decls_X.inc` 에 `type_check_X_decl(ASTNode*, SemanticContext*)` body 만 들어있음
- forward declaration 이 이미 `type_checker.h` 에 존재

**예시**: [relation](../src/semantic/type_checker_relation_decl.c), [effect](../src/semantic/type_checker_effect_decl.c) — 1차 slice 에서 분리됨

### 템플릿 B — DAG-linked split (type graph 연결)

조건:
- `X` 의 field/param/method type 이 type resolution graph 에서 노드로 등록되어야 함
- `semantic_type_resolution_precollect_X_inventory(ASTNode*, SemanticContext*)` 함수가 필요

**예시**: [enum](../src/semantic/type_checker_resolution_graph_decl.c), event, role, class, ability — 모두 [src/semantic/type_checker_resolution_graph_decl.c](../src/semantic/type_checker_resolution_graph_decl.c) 에 공존

> 현재 convention: DAG precollect 함수들은 **kind-per-file 로 쪼개지 않고** `resolution_graph_decl.c` 에 집중. 이 파일은 "DAG 인벤토리 레이어" 의 단일 TU. 본격적 kind-per-file DAG 분리는 별도 slice 로 미래 결정.

## 3. 템플릿 A 실행 절차 (5 step)

### Step 1 — 신규 TU 생성

`src/semantic/type_checker_X_decl.c`:

```c
#include "type_checker_internal.h"

bool
type_check_X_decl(ASTNode *node, SemanticContext *ctx)
{
    /* .inc 에서 그대로 이동. 로직 변경 금지 — 순수 이동 slice 는 diff review 가능성 유지를 위해 별도 */
}
```

**규칙**:
- 선언 순서 / 공백 / 주석 **전부 원본 유지**. 로직 수정은 이 slice 와 섞지 말 것.
- `type_checker_internal.h` 가 필요한 helper 와 type 을 모두 export 하고 있다. 새 include 추가 거의 불필요.
- 새로운 header 파일 **만들지 않는다** — forward declaration 은 이미 `type_checker.h` 에 있어야 한다. 없으면 그쪽에 추가.

### Step 2 — 기존 `.inc` include 제거

대상 `.inc` 가 어느 aggregator 에 include 되는지 확인:

```bash
grep -rn 'type_checker_decls_X\.inc' src/semantic/
```

상위 aggregator (예: `type_checker_decls_b.inc`) 에서 해당 라인 제거.

### Step 3 — Makefile `SEMANTIC_SOURCES` 에 등록

[Makefile](../Makefile) 의 `SEMANTIC_SOURCES` 목록에 알파벳 순 혹은 관련 TU 옆에 추가:

```makefile
                   $(SEMANTIC_DIR)/type_checker_X_decl.c \
```

### Step 4 — 기존 `.inc` 파일 삭제

```bash
rm src/semantic/type_checker_decls_X.inc
```

### Step 5 — 회귀 확인

```bash
mingw32-make rebuild                     # 새 TU 포함 full build
mingw32-make test-semantic               # baseline 유지
mingw32-make test-transpile              # baseline 유지
mingw32-make module-taxonomy-test-smoke  # owner boundary drift
mingw32-make llvm-test-backend-compare   # C/LLVM parity 회귀
```

## 4. 템플릿 B 실행 절차 (DAG-linked)

템플릿 A 의 5 step 을 그대로 따르되, 추가:

### Step 1.5 — DAG precollect 함수 추가

`src/semantic/type_checker_resolution_graph_decl.c` 에 함수 append (enum/event/role/class/ability 옆에):

```c
void
semantic_type_resolution_precollect_X_inventory(ASTNode *x_decl,
                                                SemanticContext *ctx)
{
    if (x_decl == NULL || x_decl->type != AST_X_DECL || ctx == NULL)
        return;

    /* field / method / param type 들을 semantic_type_resolution_collect_type_refs 로 등록 */
}
```

### Step 1.6 — DAG inventory dispatch 에 호출 추가

`src/semantic/type_checker_resolution_graph_inventory.inc` 안의 kind switch 에 X 케이스 추가:

```c
case AST_X_DECL:
    semantic_type_resolution_precollect_X_inventory(stmt, ctx);
    break;
```

## 5. 새 kind 추가 시 체크리스트

새 declaration kind `zeta` 를 언어에 추가할 때:

- [ ] `src/parser/` 에 AST 정의 + parse 로직 (이 문서 범위 밖)
- [ ] `src/semantic/type_checker.h` 에 `bool type_check_zeta_decl(ASTNode*, SemanticContext*);` forward 추가
- [ ] `src/semantic/type_checker.c` dispatcher (statement 검사 중앙) 에 `case AST_ZETA_DECL: return type_check_zeta_decl(node, ctx);` 추가
- [ ] `src/semantic/type_checker_zeta_decl.c` 생성 (템플릿 A 또는 B)
- [ ] type graph 연결 필요 시 템플릿 B 추가 작업
- [ ] Makefile `SEMANTIC_SOURCES` 등록
- [ ] [docs/99_language_module_taxonomy.md](99_language_module_taxonomy.md) 의 적절한 layer (pgy.core / pgy.compat.oop / pgy.compat.fp / pgy.kit.*) 에 zeta 항목 추가
- [ ] 회귀 4 개 (위 Step 5 명령)

**건드리지 않는 것**: 다른 declaration kind 의 `.c` 파일, 기존 `.inc` aggregator, `type_checker_internal.h` (신규 helper 가 꼭 필요할 때만)

## 6. 새 paradigm 추가 시

core 모델(`intent/subject/world/zone/slot/vessel` 등) 외의 새 paradigm 지원 (예: 새로운 effect system, 논리프로그래밍 모듈, 새 concurrency model) 이 오면:

1. 해당 paradigm 의 TU 를 **별도 파일** 로 생성 (core TU 에 섞지 않는다)
2. docs/99 에 `pgy.compat.<paradigm>` 또는 `pgy.kit.<paradigm>` 신설
3. core TU 가 paradigm TU 에 역의존하지 않는지 승인 gate 통과 (include graph 검사)
4. paradigm 을 "쓰지 않는" 코드에서 해당 semantic 체크 경로가 진입하지 않는지 확인

참고 memory: [project_paradigm_modularity.md](../memory/project_paradigm_modularity.md) (private) — core 가 OOP/FP/DOP 에 역의존하지 않는 설계 목표의 근거.

## 7. 알려진 미적용 경계

이 템플릿이 **아직** 커버하지 않는 영역 (후속 slice 대상):

- `src/semantic/type_checker_decls_a.inc` (2124 LOC) — ability + role + party + roster 혼재. 4 회 템플릿 A/B 반복 예정
- `src/semantic/type_checker_decls_domain_helpers.inc` (1557 LOC) — helper-axis 분리 (kind-axis 아님, 별도 전략)
- ~~`src/semantic/type_checker_decls_zone.inc` (1076 LOC)~~ — **2 차 slice 에서 템플릿 A 로 완료** ([type_checker_zone_decl.c](../src/semantic/type_checker_zone_decl.c))
- ~~`src/semantic/type_checker_decls_intent.inc` + `decls_world.inc`~~ — **4 차 / 4-B 에서 완료** ([type_checker_intent_decl.c](../src/semantic/type_checker_intent_decl.c), [type_checker_world_decl.c](../src/semantic/type_checker_world_decl.c))

codegen / runtime 의 1000+ LOC `.inc` 는 semantic 템플릿이 validated 된 뒤 **패턴 transfer** 로 적용. semantic 과 codegen/runtime 는 TU boundary 기준이 다르므로 그대로 복사하지 않는다.

## 8. Slice 레퍼런스

| Slice | 대상 | 파일 | 템플릿 | LOC 이동 |
|---|---|---|---|---|
| 1 차 | relation | [type_checker_relation_decl.c](../src/semantic/type_checker_relation_decl.c) | A | 94 |
| 1 차 | effect | [type_checker_effect_decl.c](../src/semantic/type_checker_effect_decl.c) | A | 66 |
| 2 차 | zone | [type_checker_zone_decl.c](../src/semantic/type_checker_zone_decl.c) | A | 1076 |
| 3 차 | ability | [type_checker_ability_decl.c](../src/semantic/type_checker_ability_decl.c) | A | 80 (type_checker.c 에서 추출) |
| 4 차 | world | [type_checker_world_decl.c](../src/semantic/type_checker_world_decl.c) | A (내부 static helpers 포함) | 856 |
| 4-B | intent | [type_checker_intent_decl.c](../src/semantic/type_checker_intent_decl.c) | A + **helper externalization** ([type_checker_intent_helpers_internal.h](../src/semantic/type_checker_intent_helpers_internal.h)) | 919 |
| 3-B | role | [type_checker_role_decl.c](../src/semantic/type_checker_role_decl.c) | A + externalization ([type_checker_decls_a_helpers_internal.h](../src/semantic/type_checker_decls_a_helpers_internal.h), type_checker_internal.h 확장) | 383 |
| 3-B | party | [type_checker_party_decl.c](../src/semantic/type_checker_party_decl.c) | A (3-B 공유 header) | 147 |
| 3-B | roster | [type_checker_roster_decl.c](../src/semantic/type_checker_roster_decl.c) | A (3-B 공유 header) | 80 |
| 5-F | async | [type_checker_async_decl.c](../src/semantic/type_checker_async_decl.c) | A (helper 없음) | 72 |
| 5-D | builtins stdlib dispatch | [type_checker_builtins_stdlib_body.c](../src/semantic/type_checker_builtins_stdlib_body.c) | A + externalization ([type_checker_builtins_internal.h](../src/semantic/type_checker_builtins_internal.h)) | 1131 |
| 5-A | decls_domain_helpers 전체 | [type_checker_decls_domain_helpers.c](../src/semantic/type_checker_decls_domain_helpers.c) | A + externalization (type_checker_internal.h 로 5 helper 승격) | 1559 |
| 이전 | enum | [resolution_graph_decl.c](../src/semantic/type_checker_resolution_graph_decl.c) | B | — |
| 이전 | event | [resolution_graph_decl.c](../src/semantic/type_checker_resolution_graph_decl.c) | B | — |
| 이전 | role / class / ability (precollect) | [resolution_graph_decl.c](../src/semantic/type_checker_resolution_graph_decl.c) | B (precollect only) | — |

**4-B 에서 확립된 helper externalization 패턴**:

decls_intent.inc 가 decls_a.inc 의 11 개 `static` 함수를 cross-.inc 로 호출하던 의존이 있었음. 해결책:

1. `src/semantic/type_checker_intent_helpers_internal.h` — 11 개 함수의 **external linkage** 선언 (헤더)
2. decls_a.inc 및 decls_domain_helpers.inc 의 해당 함수 정의에서 `static` 키워드 제거 (총 12 개 site — 11 정의 + 1 forward decl)
3. 새 TU 가 내부 헤더를 include 해서 link-time 에 해결

승격된 11 함수: `intent_clause_invokes_authority_sensitive_call`, `intent_step_warn_redundant_action_contract`, `intent_step_format_contract_source_summary`, `intent_condition_is_bool`, `intent_clause_rejects_control_transfer`, `intent_involves_is_subject_host`, `subject_decl_has_action_named`, `intent_step_derive_who_from_action`, `intent_step_inherit_action_contract`, `intent_step_derive_transfer_context`, `intent_step_derive_zone_binding_context`.

**3-B 에서 추가로 externalize 된 5 helper**:

- `any_subject_role_has_ability` (def: decls_domain_helpers.inc)
- `any_subject_role_find_base_ability_impl` (def: decls_domain_helpers.inc)
- `validate_ability_require_fields_for_role` (def: decls_a.inc)
- `find_generic_param_index` (def: generic_contracts.inc, decl: type_checker_internal.h 로 승격)
- `concrete_type_satisfies_bound` (def: generic_contracts.inc, decl: type_checker_internal.h 로 승격)

총 static 제거 site: 7 개 (forward decls + definitions + 기존 type_checker.c forward decls).

**5-D 에서 externalize 된 helper (builtins chain untangling)**:

- `type_is_future_like` (builtins.c 내 static) → `type_checker_builtins_internal.h`
- `type_check_channel_send_builtin` / `type_check_channel_recv_builtin` (builtins_query_channel.inc)
- `type_check_claim_device_slot` / `type_check_device_handle_arg` (builtins_slotops.inc)
- `reject_borrowed_boundary_container_store` (builtins_query.inc)

이 slice 가 특수한 이유: `builtins_stdlib_body.inc` 는 .inc 체인의 dangling `static Type *` / `Type *` 접두사가 _다음_ .inc 의 함수 signature 와 concatenate 되는 preprocessor-macro 스타일 chain 안에 있었음. 제거 후 `nominal.inc` 의 첫 함수에 `static Type *\n` 접두사 명시, `slotops.inc` 끝의 dangling `Type *` 제거하여 chain 해체.

**5-A 에서 externalize 된 helper (domain_helpers TU 승격)**:

- `type_name_or_unknown`, `resolve_named_type` (helpers_resolution.inc)
- `decl_is_projection_source` (helpers_effects.inc)
- `projection_refresh_source_field_name`, `projection_target_decl_has_field` (decls_a.inc)

전부 `type_checker_internal.h` 로 승격. `decls_b.inc` 는 empty stub 이 되어 삭제, `decls.inc` 는 `decls_a.inc` 만 include 로 단순화.

**§1 semantic 축 마감 상태 (2026-04-24 sprint 종료 시점)**:

모든 `src/semantic/*.inc` 가 **800 LOC 미만**. 현재 largest:
- `helpers_late.inc` (773)
- `builtins_query.inc` (769)
- `expr.inc` (758)

이들은 helper / dispatch / expr-visitor 축이라 declaration-kind 분리 template 에 해당하지 않음. 추가 감축은 helper axis 재배치 slice 가 될 것이나, **§1 의 semantic 조건은 이 시점에 충족**.

**남은 deferred 작업 (별도 sprint)**:

- `decls_a.inc` (1509 LOC) — 800 초과 marker 남아있음. ability/class/subject 본체 분리 필요 (5-B 잔여). 현재 해당 decls 는 이미 별도 TU 로 승격됨 — 이 .inc 에 남은 것은 미분리 subject/class helper 군.
- codegen/runtime 축 대형 `.inc` (4731, 4334, 4154 LOC) — §1 의 전체 조건을 위해 향후 sprint 필요.

---

이 문서는 **single source of truth** 이며, 새 slice 마다 §8 Slice 레퍼런스 표에 추가하여 추적한다.
