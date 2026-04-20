# `.inc` Split Roadmap

마지막 업데이트: 2026-04-20

`type_checker.c` 및 transpiler/LLVM의 일부 `.inc` 파일은 “모듈화”가 아니라 **파일이 분할된 단일 translation unit**에 가깝다. 이 문서는 P1 (TODO.md 폐인 포인트 보드) 항목을 단계적으로 닫기 위한 axis-by-axis 절단 계획.

---

## 현 상태 (2026-04-20)

`src/semantic/`:
- `.c` 7개, `.h` 12개, `.inc` **53개**
- 분리된 internal header 7개 (`type_checker_*_internal.h`) — 인터페이스만 분리, 구현은 여전히 .inc

`type_checker.c` include chain (depth 4):
```
type_checker.c
├─ type_checker_helpers.inc → core / host / late
├─ type_checker_visibility.inc
├─ type_checker_module_contracts.inc
├─ type_checker_resolution.inc → graph / stage
├─ type_checker_expr.inc → resolve / operator_expr
├─ type_checker_ownership_boundaries.inc
├─ type_checker_ownership_param_summary.inc
├─ type_checker_decls.inc → a / b (5 추가 .inc)
├─ type_checker_async_channel.inc
└─ type_checker_program.inc
```

가장 큰 .inc 파일 Top 5 (LOC):
1. `type_checker_resolution_graph_inventory.inc` — 2,167
2. `type_checker_decls_a.inc` — 2,088
3. `type_checker_decls_domain_helpers.inc` — 1,542
4. `type_checker_resolution_stage.inc` — 1,468
5. `type_checker_program.inc` — 1,206

---

## 절단의 어려움 — Static Cascade

단순히 `.inc` 하나를 `.c` 로 옮기는 것은 위험하다. 예: ownership classifier (`semantic_classify_ownership_type`)를 `type_checker_ownership_boundaries.inc` 에서 새 `.c` 로 옮기려면:

```
semantic_classify_ownership_type
└─ calls type_is_subject_type   ← static in type_checker_helpers_host.inc
   └─ helper에서 caller로 cascade 필요
```

`type_is_subject_type`은 `type_checker_helpers_host.inc` 에 **static**으로 존재. .c 분리 시 cascade promote 필요.

**경험칙**: 한 axis 절단은 평균 3~5개의 cascade promote를 동반.

---

## Axis 절단 우선순위

축 단위로 `.c+.h` 절단. 의존성이 가장 낮은 → 높은 순:

### 1. **diagnostic helpers** (P0 — 다음 sprint 시작점)
- 대상: `type_checker_helpers_context.inc` (emit_diagnostic_full 등)
- 종속: `Type`, `SemanticContext`, `ASTNode` 만 사용 — leaf
- 출력: `type_checker_diag.c` + `type_checker_diag.h`
- 예상 cascade: 0개

### 2. **ownership classifier + labels**
- 대상: `type_checker_ownership_boundaries.inc:14-82` (5개 함수)
- 종속: `type_is_*` predicate 4개 (그 중 `type_is_subject_type`이 static)
- 출력: `type_checker_ownership_classify.c` + 기존 `type_checker_ownership_internal.h` 갱신
- 예상 cascade: 1개 (`type_is_subject_type` promote to extern in `type_checker_internal.h`)

### 3. **channel transport validator**
- 대상: `type_checker_async_channel.inc:11-217` (validator + reporters)
- 종속: ownership classifier (위 axis 2 선행 필요), `OwnershipConsumerKind`
- 출력: `type_checker_channel_transport.c` + 기존 `type_checker_channel_transport_internal.h` 갱신
- 예상 cascade: 0개 (axis 2 완료 가정)

### 4. **generic contract diagnostics**
- 대상: `type_checker_helpers_late.inc:40-79` 외 generic mismatch helpers
- 종속: `Type`, `SemanticContext`, `GenericParams`, ability metadata
- 출력: `type_checker_generic_diag.c` + 기존 `type_checker_generic_diag_internal.h` 갱신
- 예상 cascade: 2~3개

### 5. **ownership consumers (escape diagnostics)**
- 대상: `type_checker_ownership_diag_internal.h` 기반 helper family (10-param 진단)
- 종속: ownership classifier, `OwnershipConsumerKind`
- 출력: `type_checker_ownership_diag.c`
- 예상 cascade: 1~2개

### 6. **module contract / authority consumer**
- 대상: `type_checker_module_contracts.inc` 일부
- 종속: ability metadata, generic params
- 출력: `type_checker_module_contract.c`
- 예상 cascade: 5+ (가장 무거움)

### 7. **resolution graph / stage** (마지막 — 가장 큰 .inc 두 개)
- 대상: `type_checker_resolution_graph_inventory.inc` (2,167 LOC), `type_checker_resolution_stage.inc` (1,468 LOC)
- 종속: 거의 모든 type/context 인프라
- 예상 cascade: 10+ — full audit 필요

---

## 절단 워크플로 (axis 1개 기준)

각 axis는 다음 6단계로 진행:

1. **Pre-cut audit**
   - 대상 .inc의 모든 함수에 대해 in/out 의존성 grep
   - static helper cascade list 작성
   - 테스트 baseline 캡처 (`test_semantic` count)

2. **Header 갱신**
   - 기존 `*_internal.h`에 extern 선언 추가
   - cascade되는 static helper도 동시에 extern 승격 (별도 commit으로 분리)

3. **`.c` 신설**
   - `.inc` 본문 → 새 `.c` 로 이동
   - include chain 정리 (header만, 다른 .inc 직접 include 금지)

4. **`type_checker.c` 갱신**
   - 해당 `.inc` include 제거
   - 새 헤더 include 추가
   - Makefile에 `.c` 등록

5. **빌드 + 회귀**
   - `make rebuild` → object 빌드 확인
   - `test_semantic` count 동일 확인
   - `test_transpile` count 동일 확인
   - smoke test 8/8 OK

6. **Cascade fix-up**
   - cascade promote된 static helper들의 다른 사용처가 깨지지 않는지 확인
   - 필요시 fwd decl 정리

---

## 진행 트래킹

| Axis | 상태 | 비고 |
|---|---|---|
| 1. diagnostic helpers | TODO | 다음 sprint 첫 항목 |
| 2. ownership classifier | TODO | cascade 1개 (`type_is_subject_type`) |
| 3. channel transport validator | TODO | axis 2 선행 필요 |
| 4. generic contract diagnostics | TODO | cascade 2~3개 |
| 5. ownership consumers | TODO | axis 2 선행 필요 |
| 6. module contract consumer | TODO | 무거움, 별 sprint 권장 |
| 7. resolution graph / stage | TODO | 마지막, full audit |

총 7개 axis 완료 시 `.inc` 카운트 53 → 약 35-40으로 감소 예상. 남은 .inc는 정말 단일 hot path 분할용으로만 유지.

---

## 참고

- [TODO.md P1 (구조/운영 폐인 포인트)](../TODO.md)
- [`docs/91_build_troubleshooting.md`](91_build_troubleshooting.md) — `make rebuild` 작동 메커니즘
- [`src/semantic/type_checker_internal.h`](../src/semantic/type_checker_internal.h) — 현재 extern 인터페이스
