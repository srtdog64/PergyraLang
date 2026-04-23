# AST 타입 디스패치 Partition

마지막 업데이트: 2026-04-23

## 목적

codegen 백엔드 (C transpiler, LLVM) 는 수많은 switch-on-`ast->type` 디스패처를 가진다. 그 중 많은 AST 타입이 특정 switch 에서 **한 번도 도달하지 않는다** — 하지만 "도달 불가" 는 C 언어 수준의 guarantee 가 아니라 **파서 invariant** 다.

이 문서는 다음 질문에 대한 단일 근거를 제공한다.

1. 왜 특정 AST 타입이 특정 switch 에서 "도달 불가" 로 판정되는가
2. 어떤 경우 case label 을 **추가해야** 하고 어떤 경우 **추가하면 안 되는가**
3. safety net 으로 유지할 가치가 있는 AST 타입은 무엇인가

## 구조적 Partition

파서는 AST 노드를 **생성되는 syntactic position 에 따라** 분리 생성한다. 각 노드 타입은 **부모 노드의 특정 필드에만** 저장된다. 그래서 전체 AST 타입 집합은 4 개 disjoint 카테고리로 쪼개진다.

### 카테고리 1 — Type Annotation Nodes

**소속 AST 타입 (현재):**

- `AST_TYPE`
- `AST_CHANNEL_TYPE`
- `AST_EVENT_HANDLER_TYPE`
- `AST_FUTURE_TYPE`

**생성 경로:** `parse_type()` 만 생성. 호출처는 오직 type-annotation syntactic position:

- `:` 뒤 (let/param/field annotation)
- `->` 뒤 (return type)
- generic arg
- type alias target

**저장 위치:** 부모 노드의 **type field** 에만 저장.

- `let_decl.type`
- `func_decl.return_type`
- `param.type`
- `generic_args.params[i]`
- `class_decl.fields[i].type` (`ClassField` 내부)

**소비 경로:** `ast_type_to_llvm(ctx, type_node)` 같은 **전용 type-dispatcher**. type field 에서 꺼내 LLVMTypeRef 로 변환한다.

**도달 불가 이유:** expression / statement switch 의 입력은 value/statement position 의 노드인데, 파서가 이 타입들을 해당 position 에 넣지 않는다. AST 트리 구조상 접근 경로가 type field 뿐이고, type field 는 type-dispatcher 만 읽는다.

**case label 추가 여부:** **추가 금지**. 의미 없는 noise. type 은 value 가 아니라 switch 에서 "값을 만들라" 는 의미 자체가 말이 안 됨.

### 카테고리 2 — Declaration Sub-metadata

**소속 AST 타입 (현재 28개):**

- Domain children: `AST_DOMAIN_SLOT`, `AST_ZONE_APPLY`, `AST_ZONE_LINK`, `AST_ZONE_DETACH`, `AST_ZONE_UNLINK`, `AST_ZONE_REFRESH`, `AST_ZONE_AUTHORITY`, `AST_ZONE_STATE`, `AST_ZONE_LAYER_SLOT`, `AST_ZONE_MAINTAIN_EFFECT`, `AST_ZONE_MAINTAIN_RELATION`, `AST_ZONE_MAINTAIN_STATE`
- World children: `AST_WORLD_ACTIVATE`, `AST_WORLD_DEACTIVATE`, `AST_WORLD_MAINTAIN`, `AST_WORLD_STATE`, `AST_WORLD_SYSTEMIC`, `AST_WORLD_ZONE`
- Intent children: `AST_INTENT_INVOLVES`, `AST_INTENT_STEP`, `AST_INTENT_VALUE`
- Role/Party children: `AST_ROLE_SLOT`, `AST_SYSTEMIC_SLOT`, `AST_PARTY_METHOD`, `AST_PARTY_SHARED`, `AST_REQUIRE_FIELD`, `AST_OVERRIDE_FUNC`
- Match children: `AST_MATCH_CASE`

**생성 경로:** 특정 부모 decl 을 파싱하는 함수 내부에서만 생성.

- `parser_domain.c` — world/zone/relation/effect/party/roster decl body 파싱
- `parser_intent.c` — intent decl body 파싱
- `parser_decl.c` — match case, field 파싱

**저장 위치:** 부모 decl 의 dedicated array field. 예:

```c
zone_decl.applies[]
zone_decl.links[]
zone_decl.refreshes[]
world_decl.activations[]
world_decl.deactivations[]
intent_decl.steps[]
party_decl.methods[]
match_stmt.cases[]
```

**소비 경로:** 부모 decl 의 전용 handler 가 해당 array 를 직접 index 해서 **필드 꺼내쓰기** 로 처리. 재귀적 `emit_expression`/`emit_statement` 호출 없음.

예시 (`llvm_domain.c:270`):

```c
for (size_t i = 0; i < zone->data.zone_decl.apply_count; i++) {
    ASTNode *apply = zone->data.zone_decl.applies[i];
    const char *effect_name = apply->data.zone_apply.effect_slot_name;
    const char *target_name = apply->data.zone_apply.target_slot_name;
    /* LLVM IR 직접 emit, emit_expression 호출 없음 */
}
```

**도달 불가 이유:**

1. 파서가 이 타입을 program statements 나 function body 에 넣지 않는다 (다른 파서 경로에서만 생성)
2. 전용 handler 가 field 접근으로 끝내고 재귀하지 않는다

**case label 추가 여부:** **원칙적으로 추가 금지** — 현재 도달 경로가 없으므로 dead code. 단, **미래 문법 확장 가능성** 이 있는 일부는 예외 (아래 "Safety net 판단 기준" 참조).

### 카테고리 3 — Top-level Declarations

**소속 AST 타입 (현재):**

- 타입 선언: `AST_FUNC_DECL`, `AST_CLASS_DECL`, `AST_ABILITY_DECL`, `AST_ROLE_DECL`, `AST_PARTY_DECL`, `AST_ROSTER_DECL`, `AST_WORLD_DECL`, `AST_RELATION_DECL`, `AST_EFFECT_DECL`, `AST_ZONE_DECL`, `AST_EVENT_DECL`, `AST_ENUM_DECL`, `AST_INTENT_DECL`
- 모듈/import: `AST_IMPORT_DECL`, `AST_USE_DECL`, `AST_NAMESPACE_DECL`, `AST_INCLUDE_STMT`, `AST_EXTERN_BLOCK`
- 별칭: `AST_TYPE_ALIAS`
- Role 내부 impl block: `AST_IMPL_ABILITY` (role decl child 지만 top-level 유사 처리)

**생성 경로:** `parser.c` 의 top-level loop. 파싱이 시작되면 이 함수가 program statements 를 순차 수집한다.

**저장 위치:** `program->data.program.statements[]`.

**소비 경로:** HIR → MIR 변환 시 타입별로 분리되어 MIR 의 **partitioned arrays** 에 들어간다.

```c
mir->functions[]
mir->intents[]
mir->zones[]
mir->worlds[]
mir->relations[]
mir->effects[]
mir->events[]
/* etc. */
```

LLVM 백엔드는 이 partitioned arrays 를 각 전용 handler 가 소비:

- `llvm_register_nominal_decl` — class/enum/ability/role
- `llvm_emit_domain` — zone/world/relation/effect/roster
- `llvm_emit_intent_decl` — intent
- `llvm_register_extern_block` — extern
- etc.

**도달 불가 이유:** LLVM 백엔드는 MIR-based emission 이라 `program->statements[]` 를 직접 iterate 하지 않는다. `llvm_emit_statement` 는 function body 문장 전용이고, function body 에는 파서가 top-level decl 을 허용하지 않는다.

**case label 추가 여부:** **skip list 에 추가 권장**. 이유:

- 이들은 다른 디스패처 (program pass, HIR builder, MIR builder) 가 소비하는 **정당한 위치가 존재**
- 미래 dispatch 구조 변경으로 `llvm_emit_statement(top_level_decl, ctx)` 가 실수로 호출될 가능성 non-zero
- 해당 실수가 일어나도 조용히 skip 되어야 (ICE 나 warning 대신) graceful 함
- 현재 `llvm_stmt.c` 의 `case AST_FUNC_DECL: ... case AST_IMPL_ABILITY: break;` 블록이 바로 이 safety net

### 카테고리 4 — Root

**소속 AST 타입:** `AST_PROGRAM`

**생성 경로:** 파싱 시작 시 1회.

**저장 위치:** 없음. AST tree 의 root 이며 다른 노드의 field 가 아니다.

**소비 경로:** 최상위 entrypoint (`semantic_analyze`, `hir_build`, `llvm_codegen_from_mir`). 진입 직후 children 을 iterate 하고 root 자체는 더 이상 전달되지 않는다.

**도달 불가 이유:** AST tree 에서 재귀적으로 만날 수 있는 노드가 아니다. 어떤 노드의 child 도 PROGRAM 일 수 없다.

**case label 추가 여부:** **추가 금지**.

## Safety Net 판단 기준

"도달 불가" 판정은 모두 동일하지만, **미래 reachable 로 바뀔 가능성** 에 따라 safety net 여부를 정한다.

| 조건 | 판단 |
|---|---|
| 카테고리 1 (type annotation) | safety net 불필요. 문법 확장으로 type 이 value position 에 올 일은 없음 |
| 카테고리 2, 미래 확장 가능성 있음 | safety net 유지. 예: zone/world runtime verb (`activate`, `apply`, `link` 등) — 이름이 "동작" 이라 미래에 function body 문법으로 확장될 유혹이 있음 |
| 카테고리 2, 미래 확장 가능성 없음 | safety net 불필요. 예: `AST_MATCH_CASE`, `AST_INTENT_STEP`, `AST_ZONE_LAYER_SLOT` — 구조상 부모 container 안에서만 의미가 있음 |
| 카테고리 3 (top-level decl) | safety net 유지. dispatcher 구조 변경 시 실수 라우팅 흡수 |
| 카테고리 4 (root) | safety net 불필요. 재귀 경로 없음 |

## 현재 구현된 Safety Net

### `llvm_emit_statement` skip 리스트

**목적:** 카테고리 3 (top-level decl) 이 실수로 statement context 로 라우팅될 때 조용히 skip.

**위치:** `src/codegen/llvm_stmt.c` 의 대형 switch 블록.

```c
case AST_FUNC_DECL:
case AST_CLASS_DECL:
case AST_ABILITY_DECL:
case AST_ROLE_DECL:
case AST_PARTY_DECL:
case AST_ROSTER_DECL:
case AST_WORLD_DECL:
case AST_RELATION_DECL:
case AST_EFFECT_DECL:
case AST_ZONE_DECL:
case AST_EVENT_DECL:
case AST_INTENT_DECL:
case AST_IMPORT_DECL:
case AST_NAMESPACE_DECL:
case AST_TYPE_ALIAS:
case AST_USE_DECL:
case AST_INCLUDE_STMT:
case AST_IMPL_ABILITY:
    /* Top-level declarations: handled by the program pass ... */
    break;
```

### `llvm_emit_statement` + `llvm_emit_expression` 의 World/Zone forward

**목적:** 카테고리 2 중 미래 확장 가능성이 있는 11종 runtime verb 를 statement context 에서 받으면 expression 쪽으로 forward 해서 **explicit diagnostic** 으로 노출.

**statement side (`llvm_stmt.c`):**

```c
case AST_WORLD_ACTIVATE:
case AST_WORLD_DEACTIVATE:
case AST_WORLD_MAINTAIN:
case AST_WORLD_STATE:
case AST_ZONE_APPLY:
case AST_ZONE_LINK:
case AST_ZONE_DETACH:
case AST_ZONE_UNLINK:
case AST_ZONE_REFRESH:
case AST_ZONE_AUTHORITY:
case AST_ZONE_STATE:
    /* Safety net — see docs/95_ast_dispatch_partition.md */
    llvm_emit_expression(node, ctx);
    ...
    break;
```

**expression side (`llvm_expr.c`):**

```c
case AST_WORLD_ACTIVATE:
...
case AST_ZONE_STATE:
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM domain AST node %d reached expression emission; "
        "domain operations must lower through MIR/domain emitters, "
        "not silent expression fallback",
        (int)node->type);
    return LLVMConstInt(ctx->type_i32, 0, 0);
```

**의도:** 현재 도달 경로 없음. 미래 문법 확장으로 이 AST 가 statement/expression context 로 오면 즉시 `PGY_CODE_LLVM_TYPE_UNSUPPORTED` diagnostic 발생 — 조용히 no-op 되지 않음.

## 추가 금지 리스트

다음 AST 타입은 expression / statement switch 에 case label **추가 금지**. 추가해도 의미 없이 noise 만 늘어남.

**카테고리 1 (type annotation):**

- `AST_TYPE`, `AST_CHANNEL_TYPE`, `AST_EVENT_HANDLER_TYPE`, `AST_FUTURE_TYPE`

**카테고리 2, 확장 가능성 없음 (metadata only):**

- `AST_DOMAIN_SLOT`, `AST_ROLE_SLOT`, `AST_SYSTEMIC_SLOT`, `AST_PARTY_METHOD`, `AST_PARTY_SHARED`, `AST_REQUIRE_FIELD`, `AST_OVERRIDE_FUNC`
- `AST_WORLD_SYSTEMIC`, `AST_WORLD_ZONE`
- `AST_ZONE_LAYER_SLOT`, `AST_ZONE_MAINTAIN_EFFECT`, `AST_ZONE_MAINTAIN_RELATION`, `AST_ZONE_MAINTAIN_STATE`
- `AST_INTENT_INVOLVES`, `AST_INTENT_STEP`, `AST_INTENT_VALUE`
- `AST_MATCH_CASE`

**카테고리 4:** `AST_PROGRAM`

이 타입들이 실제 switch 에 도달하면 **파서 또는 dispatcher 의 구조적 버그**. case label 로 흡수하지 말고 default warning 으로 바로 노출해서 버그가 드러나게 해야 한다.

## 새 AST 타입을 추가할 때 체크리스트

`ast.h` 에 새 `AST_*` 상수를 추가할 때 다음을 결정한다.

1. **카테고리 판정**
   - type annotation position 에서 생성? → 카테고리 1
   - 특정 parent decl body 안에서만 생성? → 카테고리 2
   - program-level 에서 생성? → 카테고리 3
   - root? → 카테고리 4 (새 root 는 거의 없음)

2. **저장 위치 지정**
   - 카테고리 1: 부모의 type field 로
   - 카테고리 2: 부모 decl 의 전용 array field 로
   - 카테고리 3: `program.statements[]` 로
   - 카테고리 4: 없음

3. **소비 경로 지정**
   - 카테고리 1: `ast_type_to_llvm` 등 type-dispatcher 확장
   - 카테고리 2: 부모 decl handler 에서 array iterate + field 접근
   - 카테고리 3: program pass + HIR/MIR partitioning 반영
   - 카테고리 4: 최상위 entrypoint

4. **switch case 추가 여부**
   - 카테고리 1: 추가 금지
   - 카테고리 2: 원칙적 금지. 단 미래 확장 가능성 있으면 safety net 추가 (expression 쪽 explicit error)
   - 카테고리 3: `llvm_emit_statement` skip 리스트에 추가
   - 카테고리 4: 추가 금지

5. **docs/95 업데이트**
   - 이 문서의 카테고리 리스트에 새 타입 등재

## cross-link

- `docs/70_beta_closure_master_board.md` — surface trust docs 트랙
- `docs/72_diagnostic_codes.md` — `PGY_CODE_LLVM_TYPE_UNSUPPORTED` 등
- `docs/92_inc_split_roadmap.md` — dispatcher 모듈화 로드맵
- `src/codegen/llvm_stmt.c` — statement switch (skip 리스트 + safety net)
- `src/codegen/llvm_expr.c` — expression switch (safety net explicit error)
- `src/parser/ast.h` — AST 타입 정의 source of truth
- `src/parser/parser_domain.c` — 카테고리 2 생성 경로
- `src/parser/parser_intent.c` — 카테고리 2 생성 경로 (intent)
- `src/parser/parser.c` — 카테고리 3 생성 경로 (program level)
