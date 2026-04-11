# ABI Alignment Audit

마지막 업데이트: 2026-04-11

이 문서는 현재 구현이 ABI 계약, backend 계약, 상태 문서와 얼마나 정렬되어 있는지 점검한 결과를 기록한다.
목적은 두 가지다.

- 이미 맞는 계약과 아직 안 맞는 계약을 분리한다
- 문서가 구현을 뒤처지게 설명하는 지점을 추적한다

## 1. 현재 맞는 것

- ABI spec 단일 문서는 [`src/runtime/pgy_abi_spec.h`](../src/runtime/pgy_abi_spec.h)에 있고, [`src/test_abi_spec.c`](../src/test_abi_spec.c)와 [`src/test_abi_pipeline.c`](../src/test_abi_pipeline.c)로 회귀된다.
- `make test-abi`는 spec-level check와 end-to-end pipeline/binary check를 함께 돈다.
- `tests/compare_backends.sh`는 LLVM MIR ABI 매핑 회귀를 실제 실행 결과 비교로 잡는다. 현재 `host_method_class_return`, `zone_param_mutation`, `zone_host_method_abi_combo`가 메서드 인자 바인딩/zone pointer-self host ABI 정렬을 고정한다.
- `make test-abi-perf`는 같은 pipeline 위에서 phase timing과 medium workload benchmark를 돈다.
- driver는 `driver_run_pipeline_timed(...)`를 통해 `module_load`, `semantic`, `HIR/DIR/RIR/MIR`, `backend`, `total` timing을 직접 제공한다.
- `object` lowering은 이제 C/LLVM 양쪽에서 borrow-first다. non-escaping `ToObject(...)` local binding은 source subject를 직접 읽는 projection alias로 다뤄진다.
- `tobject` lowering은 여전히 materialize-first다. `ToTObject(...)`는 boundary transfer value를 실제로 만든다.
- nominal constructor의 shared initializer 적용도 C/LLVM 양쪽에서 다시 맞춰졌다. `party`와 `roster`는 constructor argument 뒤의 shared field default를 동일하게 materialize한다.
- relation/effect/zone projection sync는 C backend에서 `__projection_ready_*` + `__projection_dirty_*` 기반 incremental rebuild를 사용한다.
- normal compile path에서 `[MIR LOWER] ...` debug 출력은 기본 비활성화되어 있다. 필요할 때만 `PGY_DEBUG_MIR_LOWER=1`로 켠다.

## 2. 아직 안 맞는 것

### 2.1 MIR-only backend 이행은 아직 완료가 아니다

- LLVM backend는 여전히 HIR fallback을 가진다.
- 현재 남아 있는 대표 debt:
  - async ordinary function fallback
  - intent emission fallback
  - class method fallback
  - main wrapper / top-level executable fallback
- 따라서 “backend는 MIR만 읽는다”는 최종 계약은 아직 완료되지 않았다.

### 2.1.1 최근 닫힌 LLVM ABI 매핑 결함

- 2026-04-11 기준으로 LLVM MIR 경로의 두 가지 실제 오동작은 닫혔다.
  - 메서드 lowering에서 hidden `self` 처리 뒤에 일반 파라미터 인덱스가 한 칸 더 밀리던 문제
  - `zone` 같은 pointer-self host 타입이 함수 선언 시그니처와 MIR local alloca binding에서 다르게 잡히던 문제
- 이 결함은 한쪽 backend만 잘못된 값을 내는 형태로 드러났고, 현재는 backend compare 회귀로 고정됐다.

### 2.2 transpiler의 ABI metadata 실사용은 아직 약하다

- transpiler는 MIR entrypoint를 받지만, 타입/레이아웃 판단의 단일 진실 원천으로 `MIRInstruction.type_layout`와 ABI table을 강하게 쓰는 단계까지는 가지 못했다.
- 즉 현재 ABI 검증은 spec/test 쪽은 닫혔지만, transpiler 내부 의사결정은 아직 완전한 ABI-first가 아니다.

### 2.3 projection dirty model은 backend 간 비대칭이 남아 있다

- C backend는 relation/effect/zone projection dirty invalidation과 incremental rebuild를 직접 가진다.
- LLVM 쪽은 object borrow/materialize 분리는 들어갔지만, 같은 수준의 domain projection dirty runtime 최적화는 아직 C만큼 정렬되지 않았다.

### 2.4 linker/toolchain 고정비는 여전히 크다

- ABI perf 기준 병목은 아직 `codegen`보다 `link`다.
- 대신 지금은 두 개의 우회 경로가 생겼다.
  - `PGY_PREBUILT_RUNTIME_OBJ`
  - `PGY_PREBUILT_RUNTIME_OBJ_<DEV|RELEASE>_<OBS0|OBS1>`
    : LLVM runtime object를 외부에서 미리 빌드해 재사용
  - `PGY_USE_LLD=1`
    : non-Windows link에서 `-fuse-ld=lld` opt-in
- 이건 완전 해결이 아니라, 고정비를 줄일 수 있는 cache/prebuilt 선택지를 추가한 상태다.

## 3. 이번에 갱신한 문서

- [`docs/17_development_status.md`](17_development_status.md)
  - `MIR 주도 + HIR 보조` 하이브리드 상태로 정정
  - phase timing / ABI perf harness 추가
  - `object borrow-first / tobject materialize-first` 추가
  - `__projection_dirty_*` runtime 모델 추가
- [`docs/20_compiler_pipeline_guide.md`](20_compiler_pipeline_guide.md)
  - `driver_run_pipeline_timed()`와 ABI benchmark harness 추가
  - C backend projection lowering 최신 상태 반영
- [`docs/13_world_roster_architecture.md`](13_world_roster_architecture.md)
  - relation/effect/zone projection dirty model 반영
  - `tobject`를 boundary projection contract로 명시
- [`docs/22_class_object_model.md`](22_class_object_model.md)
  - `object`와 `tobject`가 syntax를 공유하지만 같은 계약이 아님을 명시
  - borrow/materialize lowering 차이를 반영
- [`docs/48_abi_performance_contract.md`](48_abi_performance_contract.md)
  - ABI benchmark의 phase/subphase 정의와 현재 관찰 수치를 기록

## 4. 남은 문서-구현 불균형

- [`docs/38_mir_only_backend_migration.md`](38_mir_only_backend_migration.md)는 계획 문서로는 맞지만, 상태 보고 문서가 아니다. 완료/미완료를 이 문서에 섞지 말고 status/audit 문서에서만 다뤄야 한다.
- `ABI-first transpiler`에 대한 별도 구현 상태 문서는 아직 없다. 지금은 status와 audit 문서에만 흩어져 있다.
- LLVM 쪽 domain projection dirty optimization 상태를 별도로 기록한 문서는 아직 없다.

## 5. 다음 작업 우선순위

1. LLVM HIR fallback 제거
2. transpiler의 `type_layout` / ABI table 실사용 강화
3. LLVM domain projection dirty/invalidation 모델 정렬
4. ABI benchmark 결과의 저장형 baseline 도입

한 줄로 정리하면:

현재 구현은 “ABI spec/test는 닫혔고, lowering/runtime 최적화도 일부 들어갔지만, backend 전체가 ABI-first / MIR-only라고 말하기에는 아직 이르다.”
