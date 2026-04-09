# Invariant Optimization Progress

마지막 업데이트: 2026-04-09

이 문서는 최근 진행한 ABI 정렬, backend 성능 계측, AlphaDev식 invariant 최적화 작업이 어디까지 왔는지 추적한다.

핵심 원칙은 하나다.

> 로컬에서 중간 정리나 복사를 반복하지 말고, 앞 단계가 이미 보장한 불변식을 이용해 일반 경로를 줄인다.

## 1. 이미 완료한 것

### 1.1 ABI / 테스트 / 계측

- `test_abi_spec`와 `test_abi_pipeline`를 통해 ABI spec과 실제 compiler->binary 경로를 함께 검증한다.
- `test-abi-perf`가 추가되어 medium workload benchmark와 phase timing을 함께 본다.
- `test-abi-perf`는 이제 release runtime object prebuild를 실제로 사용한다.
- `PGY_PREBUILT_RUNTIME_OBJ_RELEASE_OBS0/OBS1`가 bench target에 연결돼 LLVM runtime 재컴파일 고정비를 피한다.
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

### 1.5 nominal constructor parity

- `party`와 `roster` nominal constructor는 이제 C/LLVM 양쪽에서 같은 field order와 shared initializer contract를 가진다.
- 즉 `party slot ...` 또는 `shared x: T = ...`가 섞여 있어도 constructor materialization이 backend마다 다르게 깨지지 않는다.
- 이 수정으로 `party/shared initializer`, `roster shared + nested party shared` 샘플이 다시 양 backend에서 같은 값을 낸다.

## 2. 지금 실제로 측정된 상태

작은 ABI 샘플 기준으로 frontend IR 단계는 거의 비용이 없다.
현재 병목은 대부분 backend/toolchain 고정비다.

### 2.1 C backend

- 총 compile: 대략 `0.330s ~ 0.451s`
- `backend_codegen`: 대략 `0.1ms ~ 0.4ms`
- `backend_native_compile`: 대략 `219ms ~ 338ms`
- `backend_link`: 대략 `110ms ~ 115ms`

### 2.2 LLVM backend

- 이전 small ABI 기준 총 compile: 대략 `1.159s ~ 1.226s`
- 현재 prebuilt runtime bench 기준 총 compile: 대략 `0.160s ~ 0.168s`
- `backend_codegen`: 대략 `3ms ~ 7ms`
- `backend_native_compile`: 대략 `3ms ~ 4ms`
- `backend_link`: 대략 `150ms ~ 176ms`

### 2.3 의미

- C backend는 transpile 자체보다 native compile/link 비용이 지배적이다.
- LLVM backend도 LLVM IR/object emission보다 runtime library compile 비용이 더 크다.
- prebuilt runtime object를 bench target에 연결한 뒤, LLVM compile 총합은 크게 줄었고 남은 주 병목은 거의 전부 link다.
- non-Windows에서는 `PGY_USE_LLD`를 명시하지 않아도 `ld.lld`가 설치돼 있으면 기본적으로 `lld`를 사용한다.
- 따라서 현재 “느리다”는 체감은 IR 단계 자체보다 toolchain fixed cost 영향이 크다.

## 3. AlphaDev식 invariant 최적화 관점에서 이미 찾은 핵심 후보

### 3.1 높은 우선순위

1. `intent` observability no-trace fast path
- 현재는 trace/history builtin을 실제로 안 써도 registry/trace/history bookkeeping이 항상 돈다.
- 프로그램이 `IntentLast*` / `IntentHistory*`를 안 쓰는 경우 이 경로를 꺼야 한다.
- 다만 `rollback: full` 같은 compensation 경로 때문에 step bookkeeping 자체를 완전히 제거할 수는 없다.
- 여기서는 bookkeeping을 둘로 나눠야 한다.
  - 필수 bookkeeping:
    - "어느 step까지 성공했는가?"
    - rollback 시 역순 compensation 대상을 결정하기 위한 최소 상태다.
    - 구현은 `last completed step index` 같은 단순 integer watermark 하나로 충분하다.
  - 비필수 bookkeeping:
    - 각 step의 상세 상태 전이
    - 각 step의 소요 시간
    - trace/history 문자열
    - 디버그용 세부 reason / profiling payload
- 즉 release fast path의 목표는 bookkeeping을 전부 없애는 것이 아니라, compensation correctness에 필요한 최소 상태만 남기고 나머지 observability 비용을 compile flag / profile mode 뒤로 보내는 것이다.

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
- bench target에는 이미 연결됐다.
- 다음 남은 일은 CI/local default path까지 어디까지 확장할지 결정하는 것이다.

5. projection redundancy elimination 확대
- C backend relation/effect/zone에는 dirty 모델이 들어갔다.
- 이를 LLVM/domain/world까지 더 넓혀야 한다.

## 3.3 보류한 후보

### persistent vector / slice view for intent snapshots

아이디어:

- 완전 복사 대신 구조 공유
- 기존 배열은 불변으로 취급
- 새 step/snapshot은 view만 가짐
- 변경 시 path-copy 또는 spill

이 후보를 버린 것은 아니다. 다만 현재 순서에서는 의도적으로 뒤로 미뤘다.

이유:

- 현재 intent hot path의 큰 비용은 `subject pointer array` 복사보다
  - trace 문자열 누적
  - history step 문자열 복사
  - exit 시 last-history 복제
  쪽이 더 크다
- 현재 subject registry는 exclusivity/conflict 판정에 실제로 쓰인다
- subject count는 대체로 작아서, 작은 N에서는 persistent 구조의 indirection/allocator 비용이 더 비쌀 수 있다
- 지금 단계에서 persistent vector를 먼저 넣으면 runtime ownership/lifetime/ABI surface가 커진다

즉 현재 판단은:

- `persistent vector`는 어려워서 미룬 것이 아니다
- `비용 대비 효과`가 지금은 낮아서 미룬 것이다

현재 권장 순서:

1. `intent no-trace fast path`
2. subject list `small inline buffer`
3. runtime library compile cache
4. 그 다음에 snapshot-heavy workload가 확인되면 `persistent vector / slice view`

다시 꺼낼 조건:

- intent participant 수가 큰 workload가 실제로 반복된다
- conflict registry snapshot/branching이 많다
- trace/history fast path 이후에도 subject-list copy가 상위 병목으로 남는다
- lifetime/ownership 규칙을 ABI 표면을 깨지 않고 닫을 수 있다

## 4. 아직 남아 있는 큰 부채

### 4.1 MIR-only backend migration

- LLVM backend에는 아직 HIR fallback이 남아 있다.
- 대표 항목:
  - async function fallback
  - intent step 내부 AST 직접 해석
  - class method fallback
  - main wrapper / top-level executable fallback

즉 현재 상태는 `MIR 주도 + 마지막 AST 직접 해석 잔여` 하이브리드다.

### 4.2 ABI-first transpiler 미완료

- transpiler는 MIR entrypoint를 받지만, type/layout/runtime_fn 판단에서 ABI metadata 실사용이 아직 약하다.

### 4.3 backend 간 projection 최적화 비대칭

- C는 dirty invalidation/incremental rebuild가 앞서 있다.
- LLVM은 object borrow/materialize 분리는 들어갔고 `party/roster` constructor parity도 맞췄다.
- 추가로 domain sync helper 선택은 이제 class registry metadata(`domain_kind`, `sync_function_name`)를 우선 사용한다.
- 하지만 domain projection dirty 모델과 declaration emission 본체는 아직 HIR 의존이 남아 있다.

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
- intent step semantic lowering (MIR carrier는 `participant/step/check/on/subintent/compensate/default-dispatch/zone-meta`까지 들어갔고, 남은 건 bootstrap/compatibility 층임)
- class method MIR migration (현재 plain class method는 MIR routine generation이 닫히지 않아 LLVM fallback을 유지)
- class/actor hidden method HIR routine 수집과 owner-aware MIR/RIR matching은 들어갔고, plain class method는 LLVM MIR direct path를 타기 시작했다
- empty method body도 valid MIR routine로 취급하게 바뀌었고, 그 결과 subject method는 이제 plain class처럼 LLVM MIR direct path를 탄다
- actor method도 MIR direct path 우선은 들어갔지만, 아직 hard-require fallback 제거까지는 가지 않았다
- domain 쪽은 sync helper 선택에 이어 world zone class lookup 하나를 registry metadata/field type 역조회로 옮겼다

### next

1. intent bootstrap metadata의 MIR/native symbol table화
2. transpiler의 `type_layout` / ABI table 실사용 강화
3. LLVM true PHI lowering
4. main wrapper / top-level executable MIR metadata화
5. class/domain 잔여 HIR fallback 제거

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
