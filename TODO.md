# Pergyra TODO (배포 준비)

## 완료 (P0 — 즉시 수정)

- [x] **`system()` 명령 주입 제거** — `_spawnvp`/`execvp`로 교체, 경로 검증 추가 (`pgy_path_is_safe`)
- [x] **AES-256 실구현** — XOR 가짜 암호를 FIPS 197 AES-256-CTR + HMAC-SHA256 인증으로 교체 (외부 의존성 없음)
- [x] **`auto __tmp` 제거** — `PGY_RESULT_TRY` 매크로에서 GCC 확장 `auto` 제거, C11 호환 (명시적 타입 파라미터)
- [x] **REPL 고정 파일명** — `_pgy_repl_tmp.*` → `TMPDIR/pgy_repl_{pid}.*` (PID 기반 유니크 경로)

## P1 — 다음 단계

- [ ] **CI (GitHub Actions) 구축** — Ubuntu + Windows 빌드 매트릭스, `make all && make test-all`, AddressSanitizer
- [ ] **CodeQL + secret scanning 활성화** — C/C++ 분석 모드, push protection
- [ ] **CHANGELOG.md + 버전 정책 수립** — SemVer, 릴리스 태깅 규칙
- [ ] **SECURITY.md** — 보안 취약점 제보 채널, 책임 있는 공개 정책

## P1.5 — 언어/컴파일러 보강

- [ ] **ability 기반 연산자 dispatch 고도화** — 현재는 `role/impl ability` 메서드에서 `operator_<suffix>_<Type>` alias를 합성해 C/LLVM이 정적으로 호출하는 방식. 장기적으로는 ability/vtable 기반의 직접 dispatch와 더 정교한 overload 우선순위 규칙이 필요
- [ ] **LLVM 연산자 오버로드 회귀 테스트 확장** — 현재 스모크는 `role IntMath for Int` 1건 중심. 비교 연산, 포함된 role, enum/custom type, namespace 경로까지 자동 테스트 확대

## P1.55 — 언어 기능 확장

### 기반 타입 시스템
- [x] **태그드 유니언 (enum with data)** — `enum Shape { Circle(Int), Rect(Int, Int) }` 데이터를 가진 enum
  - 완료: variant payload 파싱, variant 생성자 타입 추론, C tagged union / LLVM discriminated struct, LLVM tagged-union regression 및 예제 실행
- [ ] **Option<T> / None** — "상자가 비어있을 수 있다"를 타입으로 표현. `-1` sentinel 제거
  - `Option<T>` = 태그드 유니언 `{ Some(T), None }`
  - 패턴 매칭과 연동: `match opt { case Some(v): ... case None: ... }`
- [ ] **디스트럭처링** — `let (slot, token) = ClaimSecureSlot<Int>()` 등 패턴 기반 바인딩 확장
- [ ] **sealed ability** — 구현 가능한 role을 제한 (`sealed ability Combatable` → 같은 모듈 내 role만 impl 가능)

### 에러 처리
- [x] **`?` 연산자** — `Result<T>` 에러 자동 전파. `let val = riskyFunc()?;` → 에러 시 즉시 반환
  - 완료: 시맨틱 검증, C early-return lowering, LLVM `Result<T>` 레이아웃/unwrap/early-return lowering, `pipe_and_try.pgy` C/LLVM 실행 검증

### 편의 문법
- [x] **문자열 보간** — `"값은 ${x}"` → `StringConcat(...)` 계열로 lowering
  - 완료: 렉서/파서/코드젠 경로 동작, `pipe_and_try.pgy` 실행 검증
- [x] **파이프 연산자** — `data |> Transform |> Validate |> Persist` 단방향 데이터 흐름
  - 완료: 파서 변환(`a |> f` → `f(a)`, `a |> f(b)` → `f(a, b)`), C/LLVM 실행 검증
- [x] **defer** — `defer Release(s)` 스코프 종료 시 자동 실행
  - 완료: C scope-exit cleanup, LLVM defer stack + return/break/continue/scope-exit 실행, smoke regression 추가
- [ ] **`let` 타입 추론** — `let s: Slot<Int> = 42` → `let s = 42`로 축약

### Slot 소유권 모델 결정 (P0 — 근본 설계 결정)
- [ ] **`a = b` 의미론 확정** — move를 기본으로 할 것인가?
  - 현재: struct 값 복사 (C 기본) → double free 가능, Slot 정체성 약화
  - 제안: **move 기본** — `let b = a` 후 a 무효, 컴파일 에러
  - QubitSlot은 이미 move 강제 (consume_qubit_value) — 이 패턴을 Slot 전체로 확장
  - 명시적 복사: `let c = Clone(a)` — 새 Slot 할당 + 값 복사
  - **미결정: 함수 인수의 기본 전달 방식**
    - 안 A: 함수 인수도 move 기본 → Rust와 동일, 엄격하지만 명확
    - 안 B: 함수 인수는 암묵적 borrow (ReadView) → 편리하지만 규칙 이중화
    - 안 C: 함수 시그니처에 명시 — `func F(own s: Slot<Int>)` vs `func F(ref s: Slot<Int>)`
  - **영향 범위 큼 — 기존 예제 50개 + 시맨틱 체커 + 양쪽 코드젠 전부 수정 필요**
  - 결정 후 구현, 결정 전 구현 금지

### Slot 표면 문법 개선 (P0 우선순위)
- [ ] **암묵적 Read + 대입 기반 Write** — Slot의 기본 사용 표면을 일반 변수처럼
  - 읽기: `Slot<T>`가 `T` 문맥에 오면 자동 `Read` (암묵적 역참조)
  - 쓰기: `slot = expr` → 자동 `Write(slot, expr)` lowering
  - 해제: `Release(slot)` 항상 명시적 유지
  - `Read(slot)` / `Write(slot, v)`는 명시성 필요 시 직접 호출 가능 (의미론적 primitive)
  - `.value`는 보조 표면으로만 (메인 표면 아님 — "상자 안의 값" 느낌이 정체성 약화)
  - 구현: 시맨틱 체커에서 타입 자동 unwrap + 코드젠에서 Read/Write 호출 삽입

### Slot 최적화 (P0 우선순위)
- [ ] **스택 할당 최적화** — 스코프를 벗어나지 않는 Slot은 malloc 대신 alloca
  - 시맨틱 분석에서 escape 분석: "이 Slot이 함수를 벗어나는가?"
  - 벗어나지 않으면 LLVM alloca로 내림 → 힙 할당 제거
  - `with` 블록 내 Slot은 무조건 스택 후보

### View 범위 부여 (리뷰 필요 — 미결정)
- [ ] **View에 바이트/인덱스 범위 부여** — 실제 사용 사례 만들어보고 결정
  - 안 A: Slice 기반 — `SliceOf(buf, 0, 1024)` → Slot의 "창문"
    - 장점: 배열/버퍼에 자연스러움, 기존 Slice 인프라 재활용
    - 단점: 바이트 수준 아님, 구조체 필드 접근에 안 맞음
  - 안 B: View에 범위 부여 — `ViewRead(buf, offset, length)`
    - 장점: View 의미론과 일관 (권한 + 범위), 네트워크/파일 I/O에 적합
    - 단점: View가 복잡해짐, 타입 시스템 확장 필요
  - 결정 기준: "상자의 일부를 본다" vs "상자에서 조각을 꺼낸다"
  - **미결정 — 파일 I/O, 네트워크 버퍼, GPU 텍스처 사례를 만들어보고 결정**

### 병렬/채널
- [x] **select 실체화** — 여러 채널 중 먼저 준비된 것을 처리 (런타임 `pgy_channel_ready` 활용)
  - 완료: C/LLVM readiness 기반 lowering, recv binding 지원, LLVM smoke 추가

### 이터레이터
- [ ] **이터레이터 프로토콜** — ability 기반 `Iterable<T>` + `for item in collection { }`
  - `ability Iterable<T> { func HasNext(self) -> Bool; func Next(self) -> T; }`
  - for-in이 ability dispatch로 변환

### ability 차별화
- [ ] **ability ≠ interface 문서화** — ability는 "협업 프로토콜의 자격 조건"이며 슬롯에 부착됨. 인터페이스는 타입에 부착됨. 이 차이를 설계 문서에 명시
  - 인터페이스: "이 타입이 무엇을 할 수 있는가" (타입 → 메서드)
  - ability: "이 슬롯에 앉으려면 어떤 역할을 이행해야 하는가" (슬롯 → 자격 → role → 메서드)
  - runtime bind로 교체 가능한 이유: ability가 타입이 아닌 슬롯에 붙기 때문
  - 1단계: 리터럴 추론 (`42` → Int, `"hello"` → String, `true` → Bool, `[1,2,3]` → Array<Int>)
  - 2단계: 함수 반환 타입 추론 (`let s = someFunc()` → 반환 타입 따라감, `infer_expression_type` 활용)
  - 3단계: View 추론 (`let v = ViewRead(s)` → `ReadView<T>` 자동, 소스 슬롯의 inner type에서 T 추출)
  - 목표: 제네릭이 기본 축이지만 입문자가 `<T>` 없이 시작할 수 있게

## P1.6 — 자원/오케스트레이션 방향 고정

- [x] **Slot Protocol 고정** — 모든 슬롯이 공유하는 불변 계약(`Claim / Access / Mutate / Transfer / Release`)을 정의하고, `Slot<T> / SecureSlot<T> / QubitSlot`이 어디까지 지원하는지 현재 구현과 목표 구현을 명시
- [x] **Slot/View 계층 마감** — `Slot<T>`를 최소 소유 셀로 고정하고 `ReadView<T> / WriteView<T> / MoveToken<T>`를 권한 축소/이전 계층으로 정리
  - ~~현재 2단계~~: **완료** — `ReadView<T>` / `WriteView<T>` semantic type, non-owning release 금지, read/write 제한, `SecureSlot` source tracking, `MoveToken<T>` 재바인딩 transfer semantics, C lowering (pointer alias), LLVM lowering (pointer alias for view, structural copy for move)
  - 검증: semantic 158 passed, transpile 127 passed, 47 examples passed
  - 추가 완료: `SecureSlot` view의 LLVM 토큰 전파까지 구현 완료
- [ ] **슬롯을 추상 자원 핸들로 일반화** — `Slot<T>`를 메모리 저장 상자보다 넓은 자원 핸들 개념으로 재정의. 장기적으로 `MemorySlot`, `DeviceSlot`, `SessionSlot`, `NetworkSlot`, `QubitSlot` 같은 자원 클래스로 확장 가능한 계약 정리
- [ ] **채널 의미론 강화** — 컨테이너/디바이스/원격 작업 간 비동기 제출, 대기, 수거, 후처리 흐름을 표현할 수 있도록 채널, 작업 그래프, 메시지 패싱 규칙 보강
- [x] **`Future<T>`를 transfer boundary로 고정** — `await Future<QubitSlot>`을 채널 `recv`와 같은 ownership 경계로 취급하고, anchored handle 금지 / fresh binding 권장 / inline use 제한 규칙 정리
- [ ] **effect/resource capability 표기 도입** — `local cpu`, `secure device`, `remote quantum backend`, `nondeterministic`, `collapse` 같은 제약을 타입 또는 효과 시스템으로 드러내는 설계 초안과 최소 구현
  - 현재 1단계: 함수 타입 메타데이터에 inferred effect mask(`secure`, `nondeterministic`, `collapse`) 저장 및 call graph 전파
  - 현재 1.5단계: `spawn/await/channel`에서 `remote`를 orchestration boundary effect로 추론, structured comment `@effects` metadata 병합, `RemoteFuture<T>`/`DeviceSlot<T>` semantic family 도입, source-level `/// @effects` parser attachment
  - 다음 단계: `DeviceSlot`/원격 자원과 연결된 `remote` 정밀화, 선언적 annotation 문법, effect mismatch 진단
- [ ] **성능 목표를 orchestration overhead 중심으로 재정의** — 산술 microbenchmark보다 `submit`, `serialize`, `transfer`, `sync`, `retry/recovery` 비용을 줄이는 방향으로 벤치마크와 언어 목표 재정렬

## P1.7 — 의미 통일 언어로서의 다음 단계

### 비용 모델 / effect
- [ ] **비용 모델 표면화** — 의미는 통일하되 비용은 숨기지 않도록 `local / secure / remote / device` 자원군의 비용 차이를 언어 표면이나 문서 계약에 드러내기
  - `await`, `sync`, `serialize`, `copy`, `submit`, `transfer` 같은 orchestration 비용성 구분
  - "semantic unity, visible cost" 원칙 명문화
- [ ] **effect system 2단계** — inferred effect를 넘어서 선언적 effect 표기, mismatch 진단, backend/resource boundary effect 계약까지 확장

### slot 권한 / 자원군 확장
- [ ] **slot 권한 모델 고도화** — `ReadView / WriteView / MoveToken` 다음 단계로 공유 읽기 vs 독점 쓰기, capability narrowing, 함수 경계의 view 규칙 정교화
- [ ] **실제 자원군 확장** — `SessionSlot`, `ChannelSlot`, `RemoteJob/RemoteFuture`, `DeviceSlot` 고도화처럼 메모리 밖 자원군을 더 실제화해서 “의미 통일”을 증명
- [~] **class/object model 구현 정렬** — 문서상 `class = ability를 수행하는 identity-bearing object type`으로 고정했으므로, 현재의 `struct-like lowering`에서 self-cell / copy / role-binding 의미론을 점진적으로 맞추기
  - 완료: class direct copy 금지, C backend method `self*` lowering, `obj.Method()` → self-cell call, bare field access의 C/LLVM self-cell 해석, role이 `struct` 값 타입에 바인딩될 때 경고
  - 남음: role/party가 class object semantics를 더 직접 사용하도록 정렬, plain function/class 전달 규칙의 장기 모델 확정, Box/class/Slot 저장 모델 명문화

### orchestration 완성도
- [ ] **오케스트레이션 모델 강화** — `select` 공정성, timeout, cancellation, backpressure, submit/collect 규칙 등 병렬/비동기 제어를 언어 계약으로 더 고정
- [x] **async/await runtime 고도화** — 순차 실행이 아니라 실제 task runtime / coroutine 경로로 승격
  - 완료: POSIX `ucontext` + Windows Fiber 기반 coroutine runtime, `spawn/await/async` 언어 기능화
- [ ] **Windows coroutine 검증/고정** — Fiber 기반 coroutine runtime이 Windows CI와 실제 실행에서 안정적으로 동작하는지 검증하고 필요한 보강 추가

### 툴링 / 표준면
- [ ] **stable stdlib surface 재고정** — arrays, strings, file I/O, channel/future, slot/view/move, logging 중 무엇이 stable/experimental인지 다시 명시
- [ ] **툴링 단계 진입** — formatter, LSP 진단 품질, 예제 실행 UX, backend unsupported message 개선

### 대표 프로그램
- [ ] **대표 애플리케이션 3종** — 배열 알고리즘 예제를 넘어서 언어 정체성을 증명하는 end-to-end 프로그램 추가
  - 이종 자원 파이프라인 예제
  - secure + device + channel 예제
  - slot/orchestration 철학이 드러나는 실제 프로그램

## P1.8 — 멀티 타겟

- [ ] **JavaScript 백엔드** — `.pgy → JS` 변환으로 브라우저/Node.js 실행 지원
  - Slot → JS 객체 (WeakRef 활용 가능), Channel → async generator / MessageChannel
  - parallel → Web Worker / Promise.all, defer → try-finally
  - 1단계: emit_expression/emit_statement를 JS 문법으로 출력 (C 백엔드 구조 재사용)
  - 2단계: Slot 런타임을 JS 클래스로 포팅 (ClaimSlot, Read, Write, Release)
  - 3단계: Party vtable dispatch를 JS 프로토타입 체인 또는 객체 딕셔너리로 매핑
  - 목표: "하나의 .pgy 파일이 네이티브 + 웹 양쪽에서 동작"
- [ ] **WebAssembly 타겟** — LLVM wasm32 backend 활용, 브라우저에서 네이티브 성능

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치, "지원/비지원" 명문화
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리
