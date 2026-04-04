# Pergyra TODO (배포 준비)

## 완료 (P0 — 즉시 수정)

- [x] **`system()` 명령 주입 제거** — `_spawnvp`/`execvp`로 교체, 경로 검증 추가 (`pgy_path_is_safe`)
- [x] **AES-256 실구현** — XOR 가짜 암호를 FIPS 197 AES-256-CTR + HMAC-SHA256 인증으로 교체 (외부 의존성 없음)
- [x] **`auto __tmp` 제거** — `PGY_RESULT_TRY` 매크로에서 GCC 확장 `auto` 제거, C11 호환 (명시적 타입 파라미터)
- [x] **REPL 고정 파일명** — `_pgy_repl_tmp.*` → `TMPDIR/pgy_repl_{pid}.*` (PID 기반 유니크 경로)

## P1 — 다음 단계

- [ ] **CI 하드닝** — Ubuntu + Windows 빌드 매트릭스 유지, AddressSanitizer/UBSan, 더 촘촘한 smoke coverage
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
- [x] **Option<T> / None** — "상자가 비어있을 수 있다"를 타입으로 표현. `-1` sentinel 제거
  - 완료: `Option<T>` constructed type, `Some/None`, `IsSome/IsNone/UnwrapOption`, C/LLVM lowering
  - 완료: `match opt { case Some(v): ... case None: ... }` destructuring
- [ ] **디스트럭처링** — `let (slot, token) = ClaimSecureSlot<Int>()` 등 패턴 기반 바인딩 확장
- [ ] **sealed ability** — 구현 가능한 role을 제한 (`sealed ability Combatable` → 같은 모듈 내 role만 impl 가능)

### 에러 처리
- [x] **`?` 연산자** — `Result<T>` 에러 자동 전파. `let val = riskyFunc()?;` → 에러 시 즉시 반환
  - 완료: 시맨틱 검증, C early-return lowering, LLVM `Result<T>` 레이아웃/unwrap/early-return lowering, `pipe_and_try.pgy` C/LLVM 실행 검증

### 편의 문법
- [x] **문자열 보간** — `"값은 ${x}"` → `StringConcat(...)` 계열로 lowering
- [x] **파이프 연산자** — `data |> Transform |> Validate |> Persist` 단방향 데이터 흐름
- [x] **defer** — `defer Release(s)` 스코프 종료 시 자동 실행
- [x] **`let` 타입 추론** — initializer 기반 기본 추론은 현재 구현됨
  - 완료: annotation이 없을 때 initializer 타입으로 추론
  - 남음: 문서/표면 예시를 더 공격적으로 inference 중심으로 정리할지 결정

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
- [ ] **스택 할당 최적화** — 스코프를 벗어나지 않는 Slot은 malloc 대신 alloca
  - 시맨틱 분석에서 escape 분석: "이 Slot이 함수를 벗어나는가?"
  - 벗어나지 않으면 LLVM alloca로 내림 → 힙 할당 제거

### View 범위 부여 (리뷰 필요 — 미결정)
- [ ] **View에 바이트/인덱스 범위 부여** — 실제 사용 사례 만들어보고 결정
  - 안 A: Slice 기반 — `SliceOf(buf, 0, 1024)` → Slot의 "창문"
  - 안 B: View에 범위 부여 — `ViewRead(buf, offset, length)`
  - **미결정 — 파일 I/O, 네트워크 버퍼, GPU 텍스처 사례를 만들어보고 결정**

### 병렬/채널
- [x] **select 실체화** — 여러 채널 중 먼저 준비된 것을 처리

### 이터레이터
- [ ] **이터레이터 프로토콜** — ability 기반 `Iterable<T>` + `for item in collection { }`

### ability 차별화
- [ ] **ability ≠ interface 문서화** — ability는 "협업 프로토콜의 자격 조건"이며 슬롯에 부착됨

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
- [ ] **World 경계 = 실패 도메인 경계** — 크로스 World 통신은 Channel만
  - World 시맨틱 체커 구현 (현재 파싱만 완료)
  - World 코드젠 (C 백엔드 우선)
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
  - 현재: inferred effect mask + spawn/await/channel에서 remote 추론
  - 현재: `/// @effects ...` 선언이 있으면 body inferred effect와 mismatch 진단
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
- [~] **최종 문맥 계층 고정** — `ability -> role -> party -> relation -> effect -> zone -> world`
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
  - 완료: `dto` keyword를 `struct` 호환 projection alias로 추가
  - 완료: `ToObject(TargetStruct, subjectBinding)` 최소 passive projection surface를 semantic/C backend에 연결
  - 완료: `ToDto(TargetDto, subjectBinding)` 최소 projection surface를 semantic/C backend에 연결
  - 완료: `relation/effect/zone`의 domain slot에 optional initializer를 연결해 `object slot view: View = ToObject(View, subject)` 같은 projection wiring을 직접 표현 가능하게 함
  - 완료: `zone`의 `refresh objectSlot from subjectSlot` surface로 projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right` surface로 지속 lifecycle rule을 parser/semantic에 연결
  - 완료: `maintain` duplicate/conflict warning (`maintain` + `detach/unlink`) 추가
  - 완료: `zone`의 `authority subjectSlot` surface와 optional `by subjectSlot` authority annotation을 parser/semantic에 연결
  - 완료: `zone`의 `state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/link/detach/unlink/maintain stateName` shorthand를 parser/semantic에 연결
  - 완료: `HasState(stateName)` zone query builtin을 parser/semantic/transpiler placeholder surface에 연결
  - 완료: `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)` slot-aware state query를 semantic에 연결
  - 현재 구현: `ability/role/party/relation/effect/zone/systemic/world`
  - 남음: `relation`, 구조적 `effect`, `zone`의 깊은 계층 의미론, runtime/codegen surface, inter-layer composition

### 존재론 모델
- [~] **subject-first 존재론 고정** — `struct` vs `subject`
  - 완료: `subject = 상태와 identity를 가진 주체 타입`으로 문서화
  - 완료: 현재 `subject`와 `class`가 같은 subject surface라는 점 문서화
  - 완료: `actor`를 독립 존재론 계층이 아니라 subject의 실행 profile/sugar로 정리
  - 완료: `entity`는 코어 언어 존재론에 넣지 않고 프레임워크/도메인 용어로 남긴다고 문서화
  - 완료: `object`는 별도 코어 타입이 아니라 subject의 수동 해석 모드라고 문서화
  - 완료: `dto`는 object의 외부 경계용 축약 투영이라고 문서화
  - 완료: `subject` keyword alias를 parser surface에 반영
  - 남음: `class`와의 장기 alias/deprecation 전략, actor surface 재배치, subject/object view surface 고정

### slot 권한 / 자원군 확장
- [ ] **slot 권한 모델 고도화** — 공유 읽기 vs 독점 쓰기, capability narrowing
- [ ] **실제 자원군 확장** — SessionSlot, ChannelSlot, RemoteJob 고도화
- [~] **class/object model 구현 정렬** — class = ability를 수행하는 identity-bearing object type
  - 완료: class direct copy 금지, C/LLVM self-cell lowering, positional constructor
  - 부분 완료: `Box<class>` explicit handle surface (`Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`)
  - 남음: inheritance, `Slot<class>` object-handle cell 승격

### orchestration 완성도
- [ ] **오케스트레이션 모델 강화** — select 공정성, timeout, cancellation, backpressure
  - 부분 완료: `TryRecv/RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`
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

### 대표 프로그램
- [ ] **대표 애플리케이션 3종** — 이종 자원 파이프라인, secure+device+channel, slot/orchestration 철학 증명

## P1.8 — 멀티 타겟

- [ ] **JavaScript 백엔드** — `.pgy → JS` 변환으로 브라우저/Node.js 실행 지원
- [ ] **WebAssembly 타겟** — LLVM wasm32 backend 활용

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리
