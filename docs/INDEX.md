# PergyraLang 전체 문서 인덱스

마지막 업데이트: 2026-04-08

## 문서 분류

| 분류 | 문서 | 목적 |
|------|------|------|
| **비전** | [`docs/00_vision.md`](docs/00_vision.md) | 언어 철학, Slot = 자원 소유, v2 계획(양자 연산) |
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
| **리뷰** | [`review/abi_unification_log.md`](review/abi_unification_log.md) | ABI 통일 실행 로그 |
| **리뷰** | [`review/channel_ownership_tier.md`](review/channel_ownership_tier.md) | Channel Zone/World 이중 소유 모델 |
| **리뷰** | [`review/object_vs_tobject_semantics.md`](review/object_vs_tobject_semantics.md) | object vs tobject 의미론 |
| **레퍼런스** | [`README.md`](README.md) | 언어 소개, 시작 가이드 |
| **레퍼런스** | [`TODO.md`](TODO.md) | 상세 TODO |

## 최신 변경 사항 (2026-04-08)

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

### v2 계획: 양자 연산

- **현 상태**: `PgyQubit` struct는 시뮬레이션 스케줄톤일 뿐, 양자 시맨틱스 미지원
- **v2 계획**: Linear 타입, Measure 후 상태 붕괴 추적, 얽힘 관계 검증
- **문서**: `docs/00_vision.md`, `docs/18_language_status.md` 업데이트

## 문서 업데이트 가이드

문서를 수정할 때는 다음 사항을 확인하십시오:

1. **버전 정보**: 변경 사항이 v1(현재)인지 v2(계획)인지 명시
2. **ABI 문서**: ABI spec 변경 시 `pgy_abi_spec.h`, `test_abi_spec.c`, 관련 문서 동시 업데이트
3. **컴파일러 계약**: 키워드/IR 변경 시 `docs/37_compiler_contracts.md` 필수 업데이트
4. **리뷰 문서**: 설계 결정 시 `review/` 폴더에 문서화

## TODO

- [ ] Step 6: 전체 빌드 + 회귀 테스트 (Linux 환경)
- [ ] stdlib 인프라 구현 (fsm, pool, timer, math, string utils)
- [ ] LSP 완성 (diagnostics, go-to-definition, autocomplete)
- [ ] 패키지 매니저 구현
- [ ] v2 양자 연산 설계 착수
