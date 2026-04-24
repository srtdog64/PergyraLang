# Pergyra TODO (배포 준비)

마지막 업데이트: 2026-04-24

## 현재 상태 냉정 평가 (2026-04-12 재정렬)

### 종합 판단: Late-Stage Alpha

- 베타 readiness 추정: 약 `70%`
- 현재 표현: `late-stage alpha / beta-closure sprint`
- 보정 이유:
  - 기능 표면만 보면 core/foundation 구현은 넓지만, beta는 기능 개수가 아니라 end-to-end 신뢰도다
  - Type-resolution DAG가 아직 semantic source-of-truth가 아니므로 declaration order / module contract / generic consumer path drift 위험이 남아 있다
  - 장기 모듈화 stop condition도 아직 멀다. semantic에는 800 LOC 초과 `.inc`가 남아 있고, codegen/runtime에는 1,000 LOC를 크게 넘는 `.inc`가 남아 있다
  - 따라서 공식 진행률은 “기능 표면 성숙도”가 아니라 “베타 신뢰도 readiness” 기준으로 약 70%로 본다

## Beta taxonomy freeze: core / foundation / style

베타 기준은 이제 기능 나열이 아니라 언어 정체성 기준으로 나눈다.

- Core language: `intent`, `world`, `zone`, `subject`, `relation`, `effect`, `projection`, `authority`, `handoff`, runtime observability, anchored ownership boundary, generic contract system, module visibility/export contract, `parallel`.
- Generic contract는 core다. exact/ability/multi-bound/default type arg actual resolution은 FP/OOP 편의가 아니라 domain contract를 표현하는 타입 언어다.
- Foundation layer: primitive values, `func`, `let`, control flow, callable/lambda baseline, `Option`/`Result`, stable collections, core 실행에 필요한 runtime ABI.
- Style / compatibility surface: OOP convenience, FP combinator libraries, app infra, richer async helpers.
- Execution family split: `parallel`은 core execution primitive이고, `spawn`/`async`/`await`/`select`/`channel`/cancel은 그 아래 execution family다. fiber/coroutine은 language core가 아니라 runtime scheduling/suspension mechanism이다.

실행 규칙:

- B0 blocker는 `core + foundation stable subset`에만 붙인다.
- `pgy.fp`식 Functor/HKT 추상화, class-heavy OOP 확장, coroutine/fiber 고도화는 beta identity blocker가 아니다.
- 단, `parallel`은 core이므로 slot/resource/effect conflict, cancellation/fairness, C/LLVM lowering parity는 beta 품질 기준으로 계속 관리한다.
- Source of truth: `docs/99_language_module_taxonomy.md`
- Machine-readable manifest: `docs/language_module_manifest.json`
- Representative case tags: `docs/language_module_cases.json`
- Drift gate: `make module-taxonomy-test-smoke`
- Operational beta checklist: `docs/100_beta_readiness_checklist.md`

## 구조/운영 폐인 포인트 보드 (2026-04-20)

이 섹션은 기능 backlog가 아니라, 실제 작업 효율과 베타 신뢰도를 계속 깎는 구조 debt / 운영 pain point를 고정한다.

우선순위 제안:
- `P1`: `.inc` 분할을 실제 `.c`/`.h` 모듈로 전환
- `P2`: hint namespace (`code` / `cause_ir` / `fix_source`)를 레지스트리 기반으로 고정
- `P3`: type-category vocabulary를 2-3층으로 압축
- `P4`: 빌드/샌드박스/중간-stage JSON/artifact 문제를 공식 경로 기준으로 정리
- `P9`: arena 패턴을 scratch/result lifetime 기준으로 명시 도입
- `P10`: 모듈화/전파 고도화의 compile/runtime 속도 회귀를 별도 baseline으로 추적

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
  - 진행: zone refresh projection field-map DAG collector는 `type_checker_resolution_graph_zone.c`로 이동해 inventory `.inc`를 737 LOC까지 축소하고, semantic 800 LOC stop condition 대상에서 graph inventory를 제외
  - 진행: world/zone local-contract stage replay는 `type_checker_resolution_stage_domain.c`로 이동해 `type_checker_resolution_stage.inc`를 969 LOC까지 축소
  - 진행: `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`는 standalone TU로 빌드되며 hidden include-order helper 의존을 internal/header 계약으로 승격
  - 진행: `type_checker_intent_decl.c` standalone TU 승격 중 드러난 implicit helper dependency를 internal/header 계약으로 승격하고, `-Werror=implicit-function-declaration -Werror=implicit-int`를 기본 CFLAGS로 고정해 같은 종류의 C 모듈화 버그를 빌드 단계에서 차단
  - 진행: `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`도 hard implicit-declaration CFLAGS 아래에서 빌드되도록 helper/header 의존을 명시
  - 진행: `type_checker_decls_a.inc -> type_checker_decls_domain_helpers.inc`, `type_checker_decls_intent.inc -> type_checker_world_decl.c`, `type_checker_helpers_effects.inc -> type_checker_helpers_host.inc` 사이 dangling return-type seams 제거
  - 진행: `type_checker_resolution_graph_core.inc` → inventory include 경계의 dangling `static void` seam 2개를 명시 return type으로 정리
  - 진행: `generic_params_required_count`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격
  - 완료: required ability resolver와 action required-ability validator는 `type_checker_module_contract.c`로 실제 TU 분리 완료
  - 완료: `type_checker_module_contracts.inc` 제거. module contract include-order 구조 debt는 닫힘
  - [ ] `.inc` 내부 static helper 중 교차 참조 심한 심볼 목록 작성
  - [x] include-order에 의존하는 implicit declaration 경로 제거를 빌드 계약으로 승격 (`-Werror=implicit-function-declaration`, `-Werror=implicit-int`)

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
  - [ ] diagnostic literal registry 초안 추가
  - [ ] `cause_ir` / `fix_source` 네이밍 규칙 문서화
  - [ ] free-form 문자열 신규 추가 지점에 TODO/grep gate 마련

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
- 기본 방침:
  - 단일 공식 빌드 경로를 정하고 나머지는 document-only 또는 best-effort로 내린다
  - stale artifact 회피를 위해 강제 재빌드 경로를 공식화
- 준비 작업:
  - [x] 공식 Windows 빌드 경로 1개로 문서화
    - 기준: GitHub Actions `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime
    - plain Linux-hosted `gcc`는 `ci-windows` acceptance line이 아님
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
- 강한 남은 과제: full bounded fixpoint / transitive frontier scheduler는 **명시적 beta blocker**로 유지. 다만 남은 debt는 zone/world frontier loop의 부재가 아니라 remaining authority/failure handoff family와 더 넓은 world-zone propagation family를 같은 source-of-truth로 일반화하는 일이다
- 추가 closure: relation/effect/zone projection sync도 bounded transitive recompute loop로 올라왔고 declaration order에 기대지 않는다
- 추가 회귀: `projection_chain_abi`가 C/LLVM ABI smoke, `make test-all`, `make llvm-test-backend-compare`에서 잠겼다
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
  - `mir.c:mir_apply_ssa_rename` outer 3배열 → function-local `PgyArena`
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
- [x] **P0-6: defer 변수 스코프 버그 수정** — `type_checker_flow.c`에서 defer body 처리 전/후 slot 상태 저장/복원
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
  - 진행: intent emitter는 MIR `IntentAuthorizedBy` metadata를 C/LLVM 양쪽에서 수집하고, step-local approval을 `pgy_zone_authority_validate_flags_export(...)`로 검증해 `authority:<step>` recoverable intent failure와 runtime authority snapshot을 같은 경로로 남긴다
  - 진행: intent `authorized by`는 concrete zone subject slot으로 해석되며, 같은 타입의 non-authority slot 또는 ambiguous same-type slot mapping은 semantic hard error로 닫혔다
  - 회귀: `intent authorized participant must resolve to authority slot`, `intent authorized participant reports ambiguous authority slot`
  - 회귀: `intent_authority_snapshot_abi`, `intent_authority_snapshot`
  - 남음: queryable failure reason/state surface는 여전히 beta blocker다
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

현재 source-of-truth:
- stable example
  - `examples/projection_bind_group_minimal.pgy`
  - `examples/projection_refresh_publish_group_minimal.pgy`
- semantic regression
  - `src/test_semantic.c:test_projection_contract_diagnostics`

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
    - graph validator cycle과 legacy alias-resolution cycle이 모두 `Contract source:` / `Reason:` / `Fix:` 구조로 정렬됨
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
    - 진행: zone projection field-map collector를 `type_checker_resolution_graph_zone.c`로 분리해 inventory `.inc`를 737 LOC까지 축소
    - 진행: world/zone local-contract stage replay를 `type_checker_resolution_stage_domain.c`로 분리해 stage `.inc`를 969 LOC까지 축소
    - 진행: standalone TU 승격 중 드러난 dangling return-type seams와 implicit helper dependency를 제거해 `make test-all`, `make llvm-test-backend-compare` 회귀 통과
    - 진행: implicit declaration / implicit int는 기본 CFLAGS에서 에러로 고정되어 이후 DAG/semantic split 중 hidden helper dependency가 즉시 실패하도록 정렬
    - 진행: `type_resolution_intern_node` / `type_resolution_add_edge` / `type_resolution_find_path` / `type_resolution_format_cycle`는 include-order static helper에서 `type_checker_internal.h` internal API로 승격
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
  - World 내부의 Slot은 로컬 (zero-cost), World 간은 Channel (명시적 비용)

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
  - 완료: `src/compiler/dir.h`, `src/compiler/dir.c`, `pgy --dir`, `test-dir`
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
