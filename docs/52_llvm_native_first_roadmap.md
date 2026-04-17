# LLVM / Native-First Migration Roadmap

마지막 업데이트: 2026-04-09

이 문서는 Pergyra를 `MIR-first, LLVM/native-first` 시스템 언어 경로로 옮기기 위한 실행 로드맵이다.

핵심 방향:

- frontend truth: `HIR -> DIR -> RIR -> MIR`
- backend truth: `MIR -> LLVM/native`
- C backend: reference/bootstrap/debug

## 1. 목표 상태

최종 목표는 아래다.

```text
.pgy
  -> Lexer
  -> Parser
  -> Semantic
  -> HIR
  -> DIR
  -> RIR
  -> MIR
  -> LLVM IR / object
  -> native link
  -> binary
```

그리고 이 상태에서:

- backend가 HIR를 직접 codegen 입력으로 쓰지 않는다
- ABI/type layout/calling contract는 MIR/ABI table을 단일 원천으로 쓴다
- runtime은 가능한 한 prebuilt/native object로 소비한다

## 2. 현재 상태

이미 된 것:

1. `HIR -> DIR -> RIR -> MIR` driver 고정
2. C/LLVM 둘 다 MIR entrypoint 수용
3. ordinary non-async function 대부분 MIR emission
4. intent는 step/check/eval/dispatch/zone-meta carrier를 MIR에서 소비
5. ABI spec / pipeline / perf harness 존재
6. backend timing이 `codegen / native_compile / link`로 분해됨

아직 남은 것:

1. async function fallback
2. class method 잔여 fallback
3. domain/world/zone/relation/effect emission의 HIR direct path
4. main wrapper / top-level executable orchestration의 HIR direct path
5. intent symbol resolution의 HIR 의존
6. ABI metadata를 backend가 끝까지 강하게 소비하지 못하는 문제
7. linker fixed cost

## 3. 단계별 전환 계획

### Phase 1. MIR carrier 완성

목표:

- intent와 exceptional CFG처럼 backend가 AST 구조를 직접 해석하던 부분을 MIR carrier로 옮긴다.

현재 상태:

- 대부분 진행됨
- 남은 건 symbol lookup 성격의 의존

완료 조건:

- intent orchestration의 실행 정보가 MIR에서 모두 운반됨
- backend는 AST field보다 MIR metadata를 우선 사용함

### Phase 2. LLVM fallback 제거

목표:

- LLVM backend에서 `silent fallback`을 없애고, MIR가 없으면 hard error로 실패하게 만든다.

우선순위:

1. intent symbol-resolution 의존 축소
2. class method fallback 제거
3. async function fallback 제거
4. main wrapper/top-level executable metadata의 MIR 이동
5. domain declaration emission의 MIR/native path 도입

완료 조건:

- `llvm_emit_program_from_mir(...)`가 HIR direct emission 없이 동작
- `llvm_codegen_with_mir(...)`가 이름만이 아니라 실제 계약도 MIR-only

### Phase 3. ABI-first backend

목표:

- type/layout/runtime function 판단을 backend의 ad-hoc 문자열 추정이 아니라 MIR ABI metadata에서 읽는다.

우선순위:

1. `type_layout` 실사용 강화
2. resource/runtime helper selection의 ABI table화
3. C/LLVM backend의 layout 판단 로직 공통 축소

완료 조건:

- backend가 타입 구조를 “추측”하지 않는다
- ABI contract 변경 시 backend가 문서/테이블 소비만으로 따라간다

### Phase 4. Native runtime path 경량화

목표:

- LLVM/native-first 경로의 체감 병목인 compile/link 고정비를 줄인다.

우선순위:

1. prebuilt runtime object 기본 경로화
2. 환경별 runtime object cache 안정화
3. link driver 최적화 (`lld` opt-in에서 더 나아간 기본화 검토)
4. 불필요한 subsystem init 제거

완료 조건:

- 작은 샘플에서도 compile 시간의 주 병목이 과도한 runtime rebuild가 아니게 된다
- link 병목이 계측/문서/CI에서 관리 가능해진다

### Phase 5. C backend 축소

목표:

- C backend를 primary path에서 reference path로 완전히 내린다.

우선순위:

1. 문서/CLI/CI 기본값을 LLVM/native-first로 고정
2. C backend를 reference/debug suite로 재배치
3. C 전용 debt의 우선순위 하향

완료 조건:

- release/default 사용자 경험이 LLVM/native-first
- C backend는 reference test path로만 충분

## 4. 테스트 전략

전환 중 테스트는 이렇게 나눈다.

1. MIR correctness
- `test-mir`
- CFG/phi/cleanup/rollback/invalidation 검증

2. End-to-end ABI
- `test-abi`
- `test-abi-perf`

3. LLVM runtime behavior
- `llvm-test-smoke`

4. C reference parity
- `test-transpile`

원칙:

- LLVM/native 경로가 primary gate다
- C는 parity/reference gate다

## 5. 속도 전략

현재 계측상 LLVM의 주 병목은 대체로 `link`다.

따라서 속도 전략은 순서가 중요하다.

1. prebuilt runtime object
2. runtime object cache
3. linker 개선
4. 그 다음 lowering micro-optimization

즉 지금은 “LLVM codegen이 느리다”보다 “native toolchain fixed cost가 크다”를 먼저 푸는 게 맞다.

## 6. 현재 우선순위

지금 바로 진행할 우선순위:

1. intent symbol lookup의 HIR 의존 축소
2. class method fallback 제거
3. main wrapper metadata의 MIR 이동
4. domain emission의 MIR/native-first 경로 도입
5. ABI-first backend 강화
6. linker/runtime cache 기본화

## 7. 성공 기준

이 로드맵이 성공했다고 보려면 다음이 만족돼야 한다.

1. LLVM backend가 실질적으로 MIR-only
2. C backend가 reference 역할로 안정화
3. ABI contract가 단일 진실 원천으로 backend에 소비됨
4. perf harness에서 LLVM/native-first가 기본 경로로 관리됨
5. 문서와 코드의 backend 역할 정의가 더 이상 충돌하지 않음
