# PergyraLang 전체 문서 인덱스

마지막 업데이트: 2026-04-12

## 문서 분류

| 분류 | 문서 | 목적 |
|------|------|------|
| **비전** | [`docs/00_vision.md`](docs/00_vision.md) | 언어 철학, Slot = 자원 소유, v2 계획(양자 연산) |
| **비전** | [`docs/01_intent_first_design.md`](docs/01_intent_first_design.md) | Intent-First 설계 철학, 좋은 Intent 정의 방법 |
| **비전** | [`docs/19_design_philosophy.md`](docs/19_design_philosophy.md) | 설계 철학 |
| **현황** | [`docs/18_language_status.md`](docs/18_language_status.md) | 현재 구현 상태, v2 양자 연산 계획 |
| **현황** | [`CHANGELOG.md`](CHANGELOG.md) | 버전 히스토리 |
| **문법** | [`docs/grammar/01_syntax.md`](docs/grammar/01_syntax.md) | 최소 문법 요약 |
| **문법** | [`docs/grammar/02_grammar.md`](docs/grammar/02_grammar.md) | 현재 구현 문법 레퍼런스 |
| **문법** | [`docs/grammar/03_naming.md`](docs/grammar/03_naming.md) | 네이밍 규칙 |
| **컴파일러** | [`docs/20_compiler_pipeline_guide.md`](docs/20_compiler_pipeline_guide.md) | 파이프라인 기여자 가이드 |
| **컴파일러** | [`docs/36_ir_pipeline_architecture.md`](docs/36_ir_pipeline_architecture.md) | IR 계층 설계 |
| **컴파일러** | [`docs/37_compiler_contracts.md`](docs/37_compiler_contracts.md) | 컴파일러 계약 고정안 |
| **컴파일러** | [`docs/38_c_macro_deception_and_abi.md`](docs/38_c_macro_deception_and_abi.md) | C 매크로의 기만과 ABI 본질 |
| **컴파일러** | [`docs/39_test_driven_abi_and_explicit_lowering.md`](docs/39_test_driven_abi_and_explicit_lowering.md) | Test-Driven ABI + Explicit Lowering 전략 |
| **컴파일러** | [`docs/40_lowering_rules.md`](docs/40_lowering_rules.md) | RIR→MIR 매핑 규칙 17개 |
| **소유권** | [`docs/21_slot_relation_model.md`](docs/21_slot_relation_model.md) | Slot 관계 모델 |
| **소유권** | [`docs/22_ownership_model.md`](docs/22_ownership_model.md) | own/ref 소유권 모델 |
| **소유권** | [`docs/56_tobject_boundary_snapshot_policy.md`](docs/56_tobject_boundary_snapshot_policy.md) | `tobject` 전송 계약과 telemetry snapshot 분리 정책 |
| **현황** | [`docs/63_feature_depth_matrix.md`](docs/63_feature_depth_matrix.md) | 기능별 depth 매트릭스와 상태 변경 기록 (파싱/시맨틱/MIR/C/LLVM/런타임/테스트) |
| **현황** | [`docs/64_depth_filling_roadmap.md`](docs/64_depth_filling_roadmap.md) | empty cell 제거 중심의 depth filling 로드맵과 진행 기록 |
| **현황** | [`docs/65_stable_example_surface_board.md`](docs/65_stable_example_surface_board.md) | stable example / design sketch 예제 경계와 source of truth |
| **현황** | [`docs/66_semantic_implementation_map.md`](docs/66_semantic_implementation_map.md) | 의미론 기준의 현재 구현 지도와 stable/partial 분류 |
| **현황** | [`docs/68_pain_point_report.md`](docs/68_pain_point_report.md) | 실제 사용 pain point 보고 (컴파일/런타임 버그 포함) |
| **라이브러리** | [`docs/29_stdlib_design.md`](docs/29_stdlib_design.md) | stdlib/common/domain kit 계층과 모듈 정책 |
| **라이브러리** | [`docs/67_layered_stdlib_and_domain_kits.md`](docs/67_layered_stdlib_and_domain_kits.md) | 코어 추가 금지, common stdlib vs domain kit 분리 정책 |
| **사용성** | [`docs/58_keyword_authorship_pain_points.md`](docs/58_keyword_authorship_pain_points.md) | 키워드/작성 UX pain point 정리 |
| **사용성** | [`docs/59_authoring_surface_compression_plan.md`](docs/59_authoring_surface_compression_plan.md) | 작성 경로 압축과 P0/P1/P2 설계 방향 |
| **사용성** | [`docs/60_zone_context_and_transfer_inference.md`](docs/60_zone_context_and_transfer_inference.md) | lexical zone context와 `using/transfer` 유도 규칙 |
| **사용성** | [`docs/61_surface_compression_examples.md`](docs/61_surface_compression_examples.md) | surface compression의 구현 예제와 목표 예제 |
| **리뷰** | [`review/abi_unification_log.md`](review/abi_unification_log.md) | ABI 통일 실행 로그 |
| **리뷰** | [`review/channel_ownership_tier.md`](review/channel_ownership_tier.md) | Channel Zone/World 이중 소유 모델 |
| **리뷰** | [`review/object_vs_tobject_semantics.md`](review/object_vs_tobject_semantics.md) | object vs tobject 의미론 |
| **레퍼런스** | [`README.md`](README.md) | 언어 소개, 시작 가이드 |
| **레퍼런스** | [`TODO.md`](TODO.md) | 상세 TODO |

## 최신 변경 사항 (2026-04-12)

### 계층형 stdlib / domain kit 정리

1. **코어 확장 대신 라이브러리 계층 확정**
   - `docs/67_layered_stdlib_and_domain_kits.md` 추가
   - 코어는 `authority / boundary / orchestration / ownership`에 집중
   - common stdlib와 domain kit로 금융/IoT/컴플라이언스 재료를 분리

2. **새 common stdlib / domain kit 모듈 추가**
   - common: `money`, `datetime(Duration/Instant)`, `timer`, `versioning`
   - domain: `ledger`, `obligation`, `device_adapter`

3. **도메인별 probe 예제 추가**
   - `examples/finance_ledger_probe/main.pgy`
   - `examples/compliance_obligation_probe/main.pgy`
   - `examples/iot_device_adapter_probe/main.pgy`

### 작성 surface / diagnostics 정리

1. **Contract provenance를 local / inherited / derived로 정리**
   - intent step diagnostics와 AST/debug가 `locally declared`, `inherited from matching action contract`, `derived from transfer target`를 같은 용어로 드러내도록 정렬
   - dense action/step surface에서 왜 실패했는지 한 번에 읽을 수 있도록 provenance summary 강화

2. **Dense clause diagnostic 개선**
   - `with remote` 같은 오용에 대해 `use 'with effects ...'` 수정 힌트 추가
   - `authorized self` 같은 오용에 대해 `use 'authorized by <subject>'` 수정 힌트 추가

3. **Contract compression reference pair 고정**
   - `docs/65_stable_example_surface_board.md`에 canonical pair 사용 원칙 명시
   - `intent/authority/transfer` 압축 예제를 stable source of truth와 분리된 reference tier로 명확화

## 최신 변경 사항 (2026-04-11)

### 컴파일러 아키텍처

1. **ABI 통일 작업 시작** (Step 1-7 완료)
   - `src/runtime/pgy_abi_spec.h` — 15개 타입, 40+ static_assert
   - `src/test_abi_spec.c` — 28 PASS, 0 FAIL
   - `src/compiler/rir_public.inc` — `rir_dump_json()` 추가
   - `src/compiler/mir.h` — `MIRTypeLayout`, `MIRFieldLayout` 구조체 추가
   - `src/compiler/mir_public.inc` — `mir_abi_table_init()`, `mir_abi_lookup()` (28개 타입)
   - `src/codegen/transpiler_helpers.inc` — `transpiler_emit_mir_resource_op()` Visitor
   - Channel: 전체 struct → opaque handle (`uint32_t`) + Zone Arena ownership
   - docs: 38, 39, 40 번 문서 생성

2. **Windows CI 빌드 수정**
   - `TOKEN_TYPE` → `PGY_TOKEN_TYPE` (winnt.h 충돌 해결)
   - `TokenType` → `PgyTokenType` (~20개 파일, 43개 참조)
   - Unused function warning 처리 (`__attribute__((unused))`)

3. **키워드 분류 명확화**
   - `where`: 예약 키워드(TOKEN_WHERE), intent 절에서 재사용
   - `object` vs `tobject`: 별개 nominal kind, 경계 통과 여부로 구분

4. **LLVM ABI 정렬 회귀 수정**
   - `src/codegen/llvm_mir_emit.c` — 메서드 파라미터 인덱스 매핑을 hidden `self`와 정확히 정렬
   - `src/codegen/llvm_mir_locals.inc` — pointer-self host 타입(`zone` 등)의 MIR param binding을 함수 시그니처와 동일하게 정렬
   - `tests/cases/backend_compare/host_method_class_return` / `zone_param_mutation` / `zone_host_method_abi_combo`로 LLVM/C 실행 결과 회귀 고정

5. **Windows CI 이식성/문서 정렬**
   - `src/test_abi_pipeline.c` — CRLF stdout false negative 제거
   - `src/compiler/fmt.c` — formatter temp output을 binary mode로 써 line-ending 차이 제거
   - `src/runtime/pgy_runtime.h` — inline list push growth 경로를 안전화해 Windows GCC warning 오염 제거
   - `docs/17_development_status.md`, `docs/18_language_status.md`, `docs/63_feature_depth_matrix.md`, `docs/testdoc/spray_device_probe.md` — 현재 tooling/depth/CI 상태로 문서 정렬

### 양자 표면 / v2 경계

- **현 상태**: `QubitSlot`, `ClaimQubit`, `Measure`, `Entangle` 표면은 존재하지만 전체 quantum resource semantics는 아직 미완료이며 베타 대상이 아니라 `v2 / experimental`로 분리 추적한다
- **v2 계획**: Linear 타입, Measure 후 상태 붕괴 추적, 얽힘 관계 검증
- **문서**: `docs/00_vision.md`, `docs/18_language_status.md` 업데이트

## 문서 업데이트 가이드

문서를 수정할 때는 다음 사항을 확인하십시오:

1. **버전 정보**: 변경 사항이 v1(현재)인지 v2(계획)인지 명시
2. **ABI 문서**: ABI spec 변경 시 `pgy_abi_spec.h`, `test_abi_spec.c`, 관련 문서 동시 업데이트
3. **컴파일러 계약**: 키워드/IR 변경 시 `docs/37_compiler_contracts.md` 필수 업데이트
4. **리뷰 문서**: 설계 결정 시 `review/` 폴더에 문서화

## TODO

- [ ] 전체 빌드 + 회귀 테스트 baseline을 Linux/Windows 공통 문서로 재정리
- [ ] stdlib 인프라 구현 (fsm, pool, timer, math, string utils)
- [ ] LSP 고도화 (semantic symbols, richer diagnostics, references/rename 품질 개선)
- [ ] 패키지 매니저 구현
- [ ] v2 양자 연산 설계 착수
