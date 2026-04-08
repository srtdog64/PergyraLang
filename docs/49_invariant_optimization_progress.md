# Invariant Optimization Progress

마지막 업데이트: 2026-04-08

이 문서는 최근 진행한 ABI 정렬, backend 성능 계측, AlphaDev식 invariant 최적화 작업이 어디까지 왔는지 추적한다.

핵심 원칙은 하나다.

> 로컬에서 중간 정리나 복사를 반복하지 말고, 앞 단계가 이미 보장한 불변식을 이용해 일반 경로를 줄인다.

## 1. 이미 완료한 것

### 1.1 ABI / 테스트 / 계측

- `test_abi_spec`와 `test_abi_pipeline`를 통해 ABI spec과 실제 compiler->binary 경로를 함께 검증한다.
- `test-abi-perf`가 추가되어 medium workload benchmark와 phase timing을 함께 본다.
- `driver_run_pipeline_timed(...)`가 다음 phase를 기록한다.
  - `module_load`
  - `semantic`
  - `hir_lower`
  - `dir_lower`
  - `dir_validate`
  - `rir_lower`
  - `rir_enrich`
  - `rir_validate`
  - `rir_dir_validate`
  - `mir_lower`
  - `mir_validate`
  - `backend`
  - `total`
- backend timing은 다시 다음으로 분해된다.
  - `backend_codegen`
  - `backend_native_compile`
  - `backend_link`

### 1.2 object / tobject lowering

- C backend와 LLVM backend 모두 `object borrow-first / tobject materialize-first` 첫 단계를 가진다.
- non-escaping `ToObject(...)` local binding은 borrowed projection alias로 다룬다.
- `ToTObject(...)`는 boundary transfer contract로 유지되며 materialized value를 만든다.

### 1.3 projection sync 최적화

- relation/effect/zone projection runtime은 `__projection_ready_*`와 `__projection_dirty_*`를 함께 가진다.
- dirty projection target만 다시 build한다.
- source slot assignment는 matching projection slot을 invalidate한다.
- 즉 “source가 안 바뀌었으면 projection rebuild를 다시 하지 않는다”는 invariant 최적화가 C backend에 들어갔다.

### 1.4 MIR debug 노이즈 제거

- normal compile path에서는 `[MIR LOWER] ...` 출력이 기본 비활성화다.
- 필요할 때만 `PGY_DEBUG_MIR_LOWER=1`로 켠다.

## 2. 지금 실제로 측정된 상태

작은 ABI 샘플 기준으로 frontend IR 단계는 거의 비용이 없다.
현재 병목은 대부분 backend/toolchain 고정비다.

### 2.1 C backend

- 총 compile: 대략 `0.330s ~ 0.451s`
- `backend_codegen`: 대략 `0.1ms ~ 0.4ms`
- `backend_native_compile`: 대략 `219ms ~ 338ms`
- `backend_link`: 대략 `110ms ~ 115ms`

### 2.2 LLVM backend

- 총 compile: 대략 `1.159s ~ 1.226s`
- `backend_codegen`: 대략 `3.3ms ~ 5.7ms`
- `backend_native_compile`: 대략 `987ms ~ 1023ms`
- `backend_link`: 대략 `166ms ~ 199ms`

### 2.3 의미

- C backend는 transpile 자체보다 native compile/link 비용이 지배적이다.
- LLVM backend도 LLVM IR/object emission보다 runtime library compile 비용이 더 크다.
- 따라서 현재 “느리다”는 체감은 IR 단계 자체보다 toolchain fixed cost 영향이 크다.

## 3. AlphaDev식 invariant 최적화 관점에서 이미 찾은 핵심 후보

### 3.1 높은 우선순위

1. `intent` observability no-trace fast path
- 현재는 trace/history builtin을 실제로 안 써도 registry/trace/history bookkeeping이 항상 돈다.
- 프로그램이 `IntentLast*` / `IntentHistory*`를 안 쓰는 경우 이 경로를 꺼야 한다.

2. ABI `type_layout` 실사용 강화
- MIR에는 ABI table과 `type_layout`가 이미 있다.
- 하지만 transpiler는 아직 이를 단일 진실 원천으로 끝까지 사용하지 않는다.
- 문자열 추론/fallback을 줄이고 ABI-first emit로 옮겨야 한다.

3. LLVM true PHI lowering
- MIR validator는 이미 phi incoming과 predecessor 가용성을 검증한다.
- 그런데 LLVM lowering은 아직 alloca/store/load 스타일의 보수적 중간 경로가 많다.
- true PHI로 직접 내리면 중간 복사를 줄일 수 있다.

### 3.2 다음 우선순위

4. runtime library object cache
- 현재 LLVM small workload 비용의 대부분은 runtime library compile이다.
- “runtime lib는 매번 다시 컴파일할 필요가 없다”는 불변식을 이용해 prebuilt object/cache로 줄일 수 있다.

5. projection redundancy elimination 확대
- C backend relation/effect/zone에는 dirty 모델이 들어갔다.
- 이를 LLVM/domain/world까지 더 넓혀야 한다.

## 4. 아직 남아 있는 큰 부채

### 4.1 MIR-only backend migration

- LLVM backend에는 아직 HIR fallback이 남아 있다.
- 대표 항목:
  - ordinary function fallback
  - intent emission fallback
  - class method fallback
  - main wrapper / top-level executable fallback

즉 현재 상태는 `MIR 주도 + HIR 보조` 하이브리드다.

### 4.2 ABI-first transpiler 미완료

- transpiler는 MIR entrypoint를 받지만, type/layout/runtime_fn 판단에서 ABI metadata 실사용이 아직 약하다.

### 4.3 backend 간 projection 최적화 비대칭

- C는 dirty invalidation/incremental rebuild가 앞서 있다.
- LLVM은 object borrow/materialize 분리는 들어갔지만 domain projection dirty 모델은 아직 덜 정렬됐다.

## 5. 지금 기준 체크리스트

### done

- ABI spec + pipeline test
- ABI perf harness
- phase timing + backend split timing
- object borrow-first
- tobject materialize-first
- relation/effect/zone dirty projection sync
- MIR debug print 기본 비활성화
- 상태/ABI/audit 문서 갱신

### in progress

- MIR-only backend migration
- ABI-first transpiler
- LLVM projection optimization parity

### next

1. `intent` no-trace fast path
2. LLVM runtime library compile cache
3. transpiler의 `type_layout` / ABI table 실사용 강화
4. LLVM true PHI lowering
5. LLVM HIR fallback 제거

## 6. 현재 판단

현재 구현은 다음 수준까지 왔다.

- correctness/ABI 검증은 닫혀 있다
- object/tobject lowering 분리는 첫 단계가 들어갔다
- projection sync는 C backend에서 invariant 기반 최적화가 실제로 동작한다
- 성능 계측은 이제 “전체 느림”이 아니라 “어느 하위 구간이 느린가”까지 보인다

하지만 아직 다음 단계 전이다.

- backend 전체가 ABI-first는 아니다
- backend 전체가 MIR-only는 아니다
- LLVM은 C만큼 invariant 최적화를 먹지 못했다

한 줄로 정리하면:

현재는 “불변식을 활용할 준비와 첫 최적화는 들어갔고, 이제 LLVM/HIR fallback/toolchain fixed cost를 줄이는 단계”다.
