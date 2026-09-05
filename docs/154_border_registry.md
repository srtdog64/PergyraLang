# 154. BORDER 등기부 — src/ 구획면 (Codebase Compartment Faces)

Status: `registry, gate-locked`. BDFL 프레임(2026-07-04): 코드베이스를
프로세스(파이프라인 단계)가 아니라 **구획면(border)**으로 조직한다 —
docs/151 §4 간선 등록부의 코드베이스 판. `border-registry-test-smoke`
(tests/border_registry_smoke.sh)가 이 등기부의 실행형이다: **등재되지
않은 경계 교차 include = FAIL**. 순수 텍스트 검사라 CI에서 항상
load-bearing (twin-parity 게이트 계보).

## 0. 원칙

- **조약 먼저, 이사는 rung.** census(2026-07-04)가 경계 ~95% 청정을
  실측했으므로 big-bang 폴더 이동은 하지 않는다 — 등기부가 현행을
  잠그면 폴더 구획(B-2)은 언제든 안전한 기계 작업이 된다.
- **면(face)은 명시적으로 사인한다.** 경계를 넘는 include는 아래
  등기된 face 목록에 있어야 하고, 새 교차가 필요하면 등기부와 smoke를
  같은 커밋에서 갱신한다(§4 간선 등록부와 동일 규율: 미등재 교차 =
  드리프트 신호).
- 착지점 원칙: 게이트/문서에만 있는 경계는 무착지다 — 이름(접두사)과
  폴더가 경계의 물리 착지점이며, B-1이 이름을, B-2가 폴더를 준다.

## 1. 경계와 사인된 face (smoke가 잠금)

| 경계 | 규칙 | 사인된 face (예외) |
|---|---|---|
| backend (transpiler_* ↔ llvm_*) | 상호 include 0 | 공유 유틸은 `codegen_*` 중립 접두사로만 |
| semantic → codegen | 0 | — |
| parser → codegen | 0 | — |
| parser → semantic | 등재 face만 | `diag_codes.h`(전-스테이지 진단 registry), `type_system.h`(effect mask 어휘 — 死include 오판 이력 §2), `callable_contract_vocabulary.h`(caps/effects membership·canonical spelling의 기존 owner face) |
| codegen → semantic | 등재 face만 | `diag_codes.h`, `builtin_kind.h`, `lifecycle_state.h` |
| runtime → parser/semantic/codegen | 0 | — |
| 런타임 twin (inline ↔ extern) | 상호 include 0 | 공유는 `pgy_runtime_budget.h`류 공용 decl 헤더로만 (docs/137 R6 이력) |
| codegen → AIR | 0 (verification-only) | — |

## 2. B-1 수술 기록 (2026-07-04)

- **무명 공유 face 3헤더 개명** — llvm_*이 transpiler_* 이름의 헤더를
  include해 경계가 이름으로 검사 불가능했던 것을, 기존 중립 접두사
  관례(`codegen_channel_runtime_abi.h` 등)로 정렬:
  `transpiler_type_mapping.h` → `codegen_type_mapping.h`(includer 58),
  `transpiler_builtin_type_table.h` → `codegen_builtin_type_table.h`,
  `transpiler_mir_resource_name_helpers.h` →
  `codegen_mir_resource_name_helpers.h`. 결과: llvm_*→transpiler_*
  include **6 → 0**.
- **쌍 이름 분기(B-2 잔여)**: `transpiler_type_mapping.c`는 Makefile
  명시 소스 목록(동시세션 편집 중)을 건드리지 않기 위해 이름 유지 —
  `.c`↔`.h` 쌍 개명은 트리 조용할 때 B-2에서.
- **parser→type_system은 死include가 아니었다.** 심볼 grep(Type/
  type_*)이 0이라 제거했으나 빌드가 `EFFECT_NONE`/`EFFECT_AUTHORITY`
  미선언으로 실패 — effect-clause 파서가 effect mask **어휘**를 소비
  한다. dead-include 검증 규율(좁은 grep 금지, 빌드 테스트 필수)의
  재확인 사례. face로 등재하고 include에 사유 주석.
- 게이트 경로 결합 갱신: perf_contract(×4)·memory_concurrency(×1)·
  build_source_inventory(exemption 패턴) — 전부 텍스트 계약이라 경로
  치환으로 완결, 6/6 재실행 green.

## 3. Rung 사다리

2026-09-05 등기 정합성 보정: `parser_decl_clause.c`와 `ast_print.c`는 이미
`callable_contract_vocabulary.h`를 통해 caps/effects 어휘와 정전 순서를 소비한다.
[Callable Contract Vocabulary](semantics/callable_contract_vocabulary.md)가
소유한 이 경계가 등기부와 smoke에서 누락되어 있었다. 해당 헤더만 명시적으로
등재한다. parser가 별도 어휘 표를 만들거나 semantic 내부 구현 헤더를 임의로
읽는 허용은 아니며, 미등재 include 거절은 유지한다.

같은 재검증에서 twin 검사가 주석의 파일명까지 include로 오인하고, 역방향은
이미 이름이 바뀐 파일을 검사하다 누락을 숨기는 문제도 확인했다. 현재 owner인
`pgy_runtime_lib_authority_file_core.h`와 inline 입력을 필수로 확인하고 실제
`#include` 지시문을 검사한다. `border_registry_checker_smoke.sh`는 주석만 있는
대조군, 양방향 실제 교차 include, 양쪽 입력 누락을 실제 검사 함수로 검증한다.

| rung | 내용 | 상태 |
|---|---|---|
| B-0 | 등기부 + include-edge smoke | **landed** (이 문서 + tests/border_registry_smoke.sh) |
| B-1 | 무명 face 정정 (3헤더 개명, 死include 판정, face 등재) | **landed** (§2) |
| B-2 | 폴더 구획 + `.c`/`.h` 쌍 정렬 — 경계별 opportunistic, big-bang 금지. 이사 시 갱신 목록: Makefile 소스 목록·MIR allowlist(2곳)·twin parity 목록·cache deps·게이트 경로 | planned |

## Related

docs/151 §4(간선 등록부 — 방법의 원본) · docs/42+AxisOwnership.v(소유
규율) · tests/capability/run_budget_twin_parity.sh(순수-텍스트 경계
게이트 선례) · TODO 보드 A-14 · docs/137 R6(twin 경계의 사고 이력)
