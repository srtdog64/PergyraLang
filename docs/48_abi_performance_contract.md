# ABI Performance Contract

마지막 업데이트: 2026-10-08 (추후 체크 항목 추가; 기존 샘플 관찰일은 2026-04-08)

이 문서는 ABI 테스트가 성능을 어떻게 재고, 그 수치를 어떻게 해석해야 하는지 고정한다.

## 1. 목적

ABI 테스트는 두 층을 분리해서 본다.

- `make test-abi`
  - CI용
  - correctness + hard upper bound
- `make test-abi-perf`
  - 로컬 benchmark용
  - comparative metrics + phase breakdown

즉 CI는 “느리게 망가졌는지”를 잡고,
perf harness는 “어느 단계가 느린지”를 본다.

## 2. 현재 계측 항목

`driver_run_pipeline_timed(...)`는 아래 phase를 기록한다.

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

backend는 추가로 내부 세 구간으로 나뉜다.

- `backend_codegen`
  - C backend: transpile emit
  - LLVM backend: LLVM object emission
- `backend_native_compile`
  - C backend: generated C -> object compile
  - LLVM backend: runtime library C -> object compile
- `backend_link`
  - object -> runnable binary link

## 3. 해석 기준

현재 작은 ABI 샘플에서는 frontend 비용보다 backend 고정비가 훨씬 크다.

관찰 포인트:

- `module_load` ~ `mir_validate`는 대체로 `0.x ms`
- 총 compile 시간 대부분은 `backend`에 몰린다
- runtime은 대체로 `2ms ~ 3ms` 수준이다

즉 지금 성능 병목의 1차 원인은:

- frontend IR 단계가 아니라
- backend codegen / toolchain compile / link 고정비다

## 4. 2026-04-08 로컬 샘플

`make test-abi`에서 직접 관찰한 작은 샘플 기준:

- C backend compile: 대략 `0.330s ~ 0.451s`
- LLVM backend compile: 대략 `1.159s ~ 1.226s`
- runtime: 대략 `0.002s ~ 0.003s`

이 수치는 baseline이 아니라 참고치다.
기계, compiler cache, toolchain 상태에 따라 달라질 수 있다.

같은 샘플에서 backend subphase를 보면 다음이 드러난다.

- C backend
  - `codegen`: 대략 `0.1ms ~ 0.4ms`
  - `native_compile`: 대략 `219ms ~ 338ms`
  - `link`: 대략 `110ms ~ 115ms`
- LLVM backend
  - `codegen`: 대략 `3.3ms ~ 5.7ms`
  - `native_compile`: 대략 `987ms ~ 1023ms`
  - `link`: 대략 `166ms ~ 199ms`

즉 현재 작은 ABI 샘플에서:

- C는 generated C 자체보다 native compile이 대부분을 먹고
- LLVM은 LLVM codegen보다 runtime library compile 비중이 더 크다
- 양쪽 모두 link 비용도 무시할 정도는 아니다

## 5. 현재 의미

현재 수치가 말해주는 것은 명확하다.

- `object borrow-first`나 projection dirty 최적화는 이미 들어갔지만
- 작은 프로그램에서는 아직 backend/toolchain 고정비가 지배적이다
- 특히 LLVM 경로는 LLVM IR emission 자체보다 runtime library compile 비용이 더 크다
- 그래서 hand-written C와의 체감 차이는 lowering 품질뿐 아니라 toolchain 고정비에서도 온다

즉 다음 최적화 우선순위는:

1. LLVM HIR fallback 제거
2. transpiler의 ABI metadata 실사용 강화
3. LLVM domain projection dirty/invalidation 정렬
4. larger workload benchmark baseline 축적

## 6. 문서 계약

성능 문서에서 지켜야 할 규칙:

- CI 문서에는 hard upper bound만 적는다
- benchmark 문서에는 comparative metric과 phase breakdown을 적는다
- correctness와 performance regression을 같은 PASS/FAIL 문장으로 섞지 않는다

## 7. 추후 체크: 정수 폭 · ISA · 배포 호환성 (OPEN)

이 절은 추후 검토 메모다. 기본 `Int`의 64비트 전환, ABI 변경, 최소 ISA
상향을 결정하거나 현재 활성 작업과 별도의 최적화 트랙을 시작하지 않는다.

- 지원 대상별 최소 ISA와 OS/ABI를 확인한다. x86-64와 AArch64에서의 정수
  연산 지원만으로 기본 정수 폭이나 모든 저장소의 폭을 결정하지 않는다.
- 배포용 최소 ISA와 빌드 PC 전용 최적화를 구분한다. 2026-10-08 소스에서
  `src/codegen/llvm_target_machine.c`의 `llvm_create_host_machine`은 호스트 CPU
  이름과 기능을 사용하고, `src/compiler/compiler_llvm.c`의 비-Windows release
  링크 경로에는 `-march=native`가 있다. 타 CPU 배포 호환성을 확인할 대상이며,
  실제 실행 실패나 모든 산출물의 비호환을 입증한 결과는 아니다.
- 32/64비트 비교는 양쪽에서 같은 의미를 갖는 고정 입력과 알고리즘으로 한다.
  소스 revision, 실행 파일, CPU/OS, 도구 버전, 최적화 및 ISA 옵션을 기록하고,
  C/LLVM의 일반 계산·대량 배열·SIMD 비용을 구분한다. 컴파일 시간, 실행 시간,
  최대 메모리와 실제 생성 명령을 확인한다. 이 비교는 아직 실행하지 않았다.
- 기본 계산형, 배열 저장 밀도, FFI·직렬화 형식을 구분한다. 자동 폭 축소를
  검토한다면 값 범위와 오버플로 의미, 외부 ABI가 보존되는지 먼저 확인한다.
- 기본 폭 변경 여부는 위 증거를 본 뒤 결정한다. 변경 시 산술 의미·런타임
  ABI·C/LLVM·self-host 소비자와 경계/오버플로 음성 검증을 함께 검토한다.
  64비트라는 이유만으로 더 빠르거나 식별자 재사용이 안전하다고 주장하지 않는다.

참고: [GCC x86 대상 ISA와 native 옵션](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html),
[Arm AArch64 레지스터 설명](https://developer.arm.com/community/arm-community-blogs/b/operating-systems-blog/posts/arm-neon-programming-quick-reference).
