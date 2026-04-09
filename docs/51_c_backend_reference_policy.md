# C Backend Reference Policy

마지막 업데이트: 2026-04-09

이 문서는 Pergyra에서 C backend를 어떻게 취급할지 고정한다.

결론은 간단하다.

- C backend는 더 이상 장기적인 주 실행 경로가 아니다.
- C backend는 `reference backend`, `bootstrap backend`, `debug backend`로 재정의한다.
- 주 실행 경로는 `MIR -> LLVM/native`로 옮긴다.

## 1. 왜 C backend를 남기나

C backend를 유지하는 이유는 철학 때문이 아니라 운영 때문이다.

현재 C backend는 다음 역할에 가치가 있다.

1. Bootstrap
- LLVM 경로가 깨졌을 때 최소한의 실행 경로를 유지한다.
- 새 플랫폼 bring-up에서 초기 생존 경로가 된다.

2. Reference semantics
- LLVM/backend 최적화 회귀가 생겼을 때 비교 기준이 된다.
- MIR lowering이 의미를 잃지 않았는지 확인하는 기준 구현이 된다.

3. Debuggability
- generated C를 읽어 lowering 결과를 빠르게 추적할 수 있다.
- ABI/runtime 상호작용을 사람 눈으로 확인하기 쉽다.

4. Toolchain fallback
- LLVM이 없는 환경이나 제한된 환경에서 최소 기능을 유지한다.

## 2. C backend가 더 이상 담당하지 않을 것

다음은 C backend의 목표에서 제외한다.

1. 최고 성능 backend
- 고성능은 LLVM/native-first 경로가 담당한다.

2. 최종 배포 backend
- release 품질 배포 기본값은 LLVM/native 경로로 둔다.

3. 새 언어 기능의 우선 구현 대상
- 새 기능은 MIR/LLVM 계약에 먼저 닫는다.
- 필요할 때만 C backend에 reference lowering을 추가한다.

4. 아키텍처의 진실 원천
- 타입/layout/calling/runtime 의미의 단일 진실 원천은 MIR/ABI 계약이다.
- C backend는 그 계약의 한 consumer다.

## 3. C backend의 공식 역할

앞으로 C backend의 공식 역할은 아래 4개다.

1. Reference backend
- MIR lowering 결과를 검증하는 의미 보존 backend

2. Bootstrap backend
- LLVM 경로가 불완전한 기능을 임시로 돌리는 생존 backend

3. Differential backend
- LLVM과 출력/동작을 비교하는 회귀 backend

4. Introspection backend
- generated code inspection과 backend debugging용 backend

## 4. 정책

정책은 다음과 같이 고정한다.

1. Driver 기본값
- 기본 backend는 LLVM/native다.
- C backend는 명시적 선택 또는 fallback/debug 선택지다.

2. 기능 우선순위
- 새 lowering 기능은 `HIR -> DIR -> RIR -> MIR -> LLVM` 순으로 먼저 닫는다.
- C backend는 reference parity가 필요할 때 뒤따라온다.

3. 테스트 우선순위
- correctness 핵심 회귀는 MIR/LLVM 기준으로 닫는다.
- C backend는 differential/reference 회귀를 유지한다.

4. 문서 용어
- C backend를 `primary backend`라고 부르지 않는다.
- 항상 `reference`, `bootstrap`, `fallback`, `debug` 중 하나로 명시한다.

## 5. C backend가 유지해야 하는 최소 품질

reference backend로 남기려면 다음은 유지해야 한다.

1. MIR 입력 수용
- backend entrypoint는 MIR 기반이어야 한다.

2. 핵심 의미 보존
- function CFG
- SSA/local 의미
- cleanup/rollback/invalidation
- projection/object/tobject
- ABI contract

3. 회귀 가능성
- `test-transpile`
- `test-abi`
- 필요 시 LLVM과의 differential 비교

## 6. 허용되는 부채와 허용되지 않는 부채

허용되는 것:

- LLVM보다 단순한 lowering
- 덜 공격적인 최적화
- debug-friendly한 generated code

허용되지 않는 것:

- MIR contract를 무시하는 독자 의미
- 언어 semantics의 별도 구현
- ABI 단일 진실 원천을 우회하는 문자열 추론
- 새 기능이 C에만 있고 LLVM에는 없는 상태의 장기 방치

## 7. 종료 조건

아래 상태가 되면 C backend는 완전히 optional component가 된다.

1. LLVM backend가 `intent`, `class method`, `main wrapper`, domain emission까지 MIR/native-first로 닫힘
2. ABI contract가 LLVM/native 경로에서 단일 원천으로 소비됨
3. runtime/toolchain fixed cost가 실사용 기준에서 허용 가능 수준으로 내려감
4. CI에서 C backend 없이도 핵심 언어 회귀를 충분히 커버할 수 있음

그 이후 C backend는 제거 대상이 아니라, 명시적 reference implementation으로 축소 유지한다.
