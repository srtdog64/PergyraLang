# 비동기·동시성 모델과 최근 연구의 비교

Delivery base: `d0fa49ea`.
Status: 읽기 전용 탐색 증거다. 컴파일러 의미론의 권위가 아니다. 의미 계약은
`docs/05_async_concurrency.md`, `docs/113_memory_concurrency_model.md`,
`docs/114_async_model_positioning.md`, `docs/146_sea_execution_lanes.md`,
`docs/178_parallel_boundary_evidence.md`, `docs/181_parallel_surface_full_design.md`,
`docs/204_concurrency_direction_pscc_review.md`와 그 실행 게이트가 소유한다.
이 문서는 후속 구현 rung을 열지 않는다. §4의 권고는 결정 후보일 뿐이다.

연구 쪽 인용은 2026-10-05에 웹에서 제목·저자·학회·연도·URL을 확인했다. arXiv에만
있는 것은 "preprint"로 적었다. "확인 못 함"은 찾아봤지만 존재를 확인하지 못했다는
뜻이다. "평가"라고 적은 문장은 인용 저자의 주장이 아니라 이 문서의 해석이다.

## 0. 결론 먼저

**큰 방향은 맞다.** 2025–2026년 연구와 주류 언어가 수렴한 지점 여섯 개 중 넷에서
Pergyra는 같은 쪽에 서 있다. 그중 둘은 주류 언어보다 한 걸음 앞에 있다.

| 합의 지점 | Pergyra | 판정 |
|---|---|---|
| 구조적 생성이 기본, 비구조적 생성은 이름 붙은 탈출구 | `parallel`은 join-before-continue. 이름 있는 `spawn`은 모든 경로에서 소비해야 하는 Future. 단, `async { }`는 **기본이 분리(detached)** | 부분 일치 |
| 취소는 협조적이고 scope 트리를 따라 내려간다 | 협조적, `PgyCancelNode` 부모 사슬로 후손 전파 | 일치. deadline scope는 없음 |
| 타입별 표시 + 흐름 민감한 이동 분석 | 사용자용 Send/Sync는 없음. 경계마다 Copy/Channel/Exclusivity/Disjointness 증거 중 하나를 요구 | 일치(방식은 다름) |
| 사용성이 기본값을 정한다 | "하나의 건전한 기본값", lane을 증거로 유도 | 일치 |
| stackful/stackless는 런타임 선택이다 | lane facade 뒤에 스레드 풀·ucontext/Fiber 코루틴·M:N을 숨김 | 일치. 그런데 표면에는 `async` 색이 남아 있음 |
| 교착 자유는 제한된 구조에서만 증명된다 | 결정적 join 부분집합은 증명 모델이 있음. 채널은 분석 없음 | 부분 일치 |

**앞서 있는 두 곳**

1. **반드시 소비되는 Future.** Rust의 scoped async 트릴레마는 "값을 누수시켜도
   안전하다"는 전제에서 나온다. Pergyra는 이 전제를 받아들이지 않는다. `Future<T>`는
   모든 경로에서 `await`되거나 `own` 매개변수로 넘겨져야 한다(`PGY_SEM_TASK_LIFECYCLE`).
   그래서 트릴레마가 막는 조합(동시 + 병렬 + 부모 빌림)을 원리상 열 수 있는 자리에 있다.
2. **증거로 유도하는 실행 lane.** Zig 0.16 `Io`, Java virtual threads, OCaml 5는
   실행 모델을 인터페이스 뒤로 숨긴다. Pergyra SEA는 한 걸음 더 간다. lane을
   사용자가 고르지 않고 경계 증거에서 유도하며(`src/compiler/execution_lane.c`),
   그 판단이 IR 사실로 남아 검사할 수 있다.

**어긋난 세 곳**

1. **`async` 색.** 런타임은 stackful이다(스레드, ucontext, Win32 Fiber). 그런데
   표면은 `await`를 `async func` 안에서만 허용한다. 색의 사용성 비용은 내면서,
   stackless 상태 기계가 주는 이득(작은 프레임, 런타임 없는 환경)은 받지 못한다.
   사용자 가이드 `docs/05` §2의 예제도 이 규칙을 어긴다(§3.1).
2. **취소에 deadline과 실패 정책이 없다.** Trio의 cancel scope, Java의 `Joiner`,
   Swift의 `async let`처럼 "이 범위는 언제 끝나야 하는가"와 "자식 하나가 실패하면
   형제는 어떻게 되는가"를 말할 방법이 없다. `await`에 타임아웃도 없다.
3. **채널과 `select`가 뒤처져 있다.** `select`는 비차단 폴링 한 번뿐이고 송신·타임아웃
   case가 없다. 교착 분석도 없다. `Channel<T>`는 복사할 수 없는 값이라 이름 있는
   작업 사이에서 공유할 수 없다.

**즉시 고쳐야 할 결함 둘**(§3.2): 분리된 `async { }` 블록이 `await`로 멈추면 남은
일이 아무 신호 없이 사라진다. 같은 블록 안의 `spawn`은 C 백엔드에서 컴파일조차
되지 않는다.

## 1. 현재 모델 (코드 기준)

| 구문 | 의미 | 근거 |
|---|---|---|
| `async func` | `is_async_decl` 표시가 붙은 일반 함수. CPS나 상태 기계 변환은 없다. generic/`where`는 미지원 | `src/parser/ast_async_constructors.c:11`, `docs/grammar/01_syntax.md:273` |
| `await e` | async 문맥에서만 허용. `Future<T>`는 `T`, `RemoteFuture<T>`는 `Result<T>`를 낸다. 핸들을 소비하므로 두 번 await할 수 없다. "완료 합류일 뿐, 수명·취소·실패 분류·병렬 구조를 소유하지 않는다" | `src/semantic/type_checker_expr.c:326`, `docs/113` |
| `spawn F(args)` | affine `Future<T>`를 낸다. 불변 바인딩의 직접 초기화식이나 `await`의 직접 피연산자로만 허용. 넘길 수 있는 길은 `own Future<T>` 매개변수뿐 | `docs/113`, `type_checker_future_lifecycle.c` |
| `spawn blocking F()` | AIR `has_declared_blocking_evidence`를 거쳐 BlockingPool lane | `src/parser/parser_async.c:277`, `docs/146` |
| `spawn async () {…}` | 파싱은 되지만 "beta-out-of-scope"로 거부 | `type_checker_async_channel.c:288` |
| `async { … }` | **분리(detached)**. 지역 변수를 포인터로 캡처할 수 없다(`PGY_SEM_BORROW_ESCAPE`). C는 `pgy_lane_spawn_dispatch(LOCAL_ASYNC)` 뒤에 `pgy_lane_detach`, LLVM은 `pgy_async_detach_export` | `type_checker_async_decl.c:42`, `transpiler_async_parallel_emit.c:521` |
| `parallel { … }` | 이어가기 전에 모든 arm을 합류 | `parser_parallel.c:148` |
| `parallel (x in xs) join with m` | 닫힌 모드 집합: `all`(인덱스 순서 `Array<R>`), `sum/product/min/max`(인덱스 순서 고정 left fold, checked 산술), `any`(첫 `give` 승리, 명시적 비결정) | `docs/181` §1, `parser_parallel.c:79` |
| `give e;` | join 본문 마지막 문장으로 정확히 한 번 | `docs/181` R2 |
| `parallel on (lane) { every / continuous }` | 비전 표면. 파서가 fail-closed로 거부 | `parser_parallel.c:183` |
| `Channel<T>`, `ch <- v`, `<-ch` | ring buffer + mutex + condvar 두 개로 된 값 구조체. "복사하면 안 된다". 차단 send/recv만 소유권을 옮긴다. `TryRecv`/`RecvTimeout` 등은 복사 전용 | `pgy_runtime_channel_inline.h:55`, `docs/177` §8 |
| `select` | 수신 case와 `default`만 있다. 비차단 폴링 한 번, 사이트별 atomic round-robin 시작점. `default`가 없으면 준비된 채널이 없을 때 그냥 지나간다 | `parser_async.c:322`, `transpiler_select.c:244`, `llvm_stmt_select.c:238` |
| `Cancel(f)` / `IsCancelled()` | 요청일 뿐 합류도 해제도 하지 않는다. 안전점은 작업 진입, 채널 대기의 10 ms 주기, `any` 래퍼 안 루프 백엣지. 선점 취소는 beta 밖 | `pgy_parallel.h:63`, `docs/181` R3, `docs/05` §6 |
| 예산 | `PGY_BUDGET_SPAWN_COUNT`, `PGY_BUDGET_CHANNEL_COUNT`, 프로세스 전체 `PGY_BUDGET_WALL_MS` 감시자(중단) | `pgy_runtime_budget.h:36` |
| `intent` | 절 안에 await/spawn/async/parallel/select/send/recv 금지. spawn·send는 호출 요약으로 프로시저 간 검사. 런타임은 같은 subject의 활성 intent 충돌을 거부 | `type_checker_intent_control.c:77`, `pgy_runtime_intent_trace_inline.h:411` |
| `zone` / `world` | zone은 `pthread_rwlock_t`와 atomic generation을 가진다. zone identity는 spawn 인자가 될 수 없다. AIR에서 zone은 SYNC/PinnedZone, world 경계는 ASYNC/LocalAsync | `pgy_runtime_zone_sync_abi.h`, `air_boundary.c:102` |

**런타임 lane**(`src/runtime/pgy_lane_scheduler.h:60`)

- WorkerPool: pthread 1:1 풀. 워커당 mutex FIFO shard, round-robin push, 자기 shard
  먼저 훔치기. `await`는 help-first(대기 중 큐의 일을 대신 돌리고 비면 park).
- LocalAsync: stackful 코루틴(POSIX ucontext, Win32 Fiber, 128 KB 스택). 준비 큐는
  스레드 지역이고 **이벤트 루프가 없다**. 누군가 await·cancel·detach를 부를 때만
  `pgy_async_progress_one`으로 진행한다.
- BlockingPool: 따로 늘어나는 스레드 풀.
- Movable(M:N): `src/runtime/async/`에 있지만 "fiber context core는 실행된 적이 없다"
  (`docs/194`). 이 lane을 만드는 소스가 없다. `AsyncScope*` API 호출자는 0개다.

**정적 검사**: Future 수명(`type_checker_future_lifecycle.c`), parallel 캡처와 slot
경합(`type_checker_flow_parallel.c`, `parallel_capture_*_reach.c`, 메서드 수신자 쓰기
깊이 8까지, 모르면 쓰기로 침), boundary witness(`boundary_witness.c`,
`WitnessDataRace.v`와 대응), spawn 인자와 채널 전송(`type_checker_async_channel.c`),
await를 넘는 pin(`PGY_SEM_PIN_AWAIT_BOUNDARY`), intent 안 동시성. **교착 검사는 없다.**
채널 순환 검사도, 잠금 순서 검사도 없다.

**형식 모델**(`docs/semantics/proofs/`): `AsyncLifecycleCore`, `AsyncContextCore`,
`AsyncScopeCore`(`run_no_orphan`, `cancel_reaches_descendants`,
`background_only_via_detach`), `SuspensionRevalidationCore`(`stale_never_resolves`),
`DeterministicSubsetCore`, `ParallelReductionCore`(`join_schedule_invariant`),
`ParallelSchedulingCore`(`help_first_progress`, `park_only_deadlocks`),
`WitnessDataRace`, `IntentConflict`, `ZoneCrossingCore`. 모두 경계가 정해진 모델이다.
구현 증명이 아니며, adequacy smoke가 모델을 소스 줄에 묶는다. 공정성·종료·C11
happens-before는 증명하지 않는다("Neither core proves termination, scheduler
fairness, C11 happens-before", `AsyncModelCores.md`). 채널·select 모델은 없다.

**self-host**: `select`는 `surface_not_covered`, `parallel`과 채널 송신은
`statement_native_pipeline_only`, `ChannelClose`·`TrySend` 등은
`builtin_native_pipeline_only`로 거부된다. 기본 경로가 받는 것은 인자 2개 이하의
Int/String spawn·await뿐이다(`codegen/runtime_abi/spawn_runtime_owner.pgy`).
비동기·동시성은 현재 self-host 치환 rung 위에 있지 않다.

## 2. 주제별 비교

### 2.1 구조적 동시성

**연구와 실무의 합의.** 작업은 scope 객체(nursery, task group, `CoroutineScope`,
`StructuredTaskScope`) 안에서만 만든다. scope 블록은 자식이 모두 끝나기 전에는 빠져나갈
수 없다. 자식 하나가 실패하면 형제를 보통 취소하고 오류를 부모에게 올린다. Trio(2018)에서
Kotlin(2018), Swift 5.5(SE-0304, SE-0317), Python 3.11(`TaskGroup`, PEP 654),
C++26(`async_scope`, P3149)으로 퍼졌다. Java `StructuredTaskScope`는 JDK 25의 JEP 505에서
`open()` 팩토리와 교체 가능한 `Joiner`로 바뀌었고, JEP 543이 JDK 28 확정을 제안 중이다.
비구조적 생성(Kotlin `GlobalScope`, Swift `Task.detached`)은 모든 주류 언어에 남아
있지만 **이름 붙은 탈출구**이지 기본값이 아니다. 형식 이론으로는 async/finish 계산법
Featherweight X10(PPoPP 2010)이 가장 가깝다. nursery·task group의 취소까지 포함한
2022–2026년 동료 심사 계산법은 확인 못 함. Gray·Krishnamurthi·Crichton(OOPSLA 2026)은
async/await 설계 공간을 9차원으로 나눴고, 일곱 개 언어·런타임 조합(JS, C#, Swift,
asyncio, Trio, Tokio, Smol) 중 어느 둘도 실행 존재와 순서에서 일치하지 않음을 보였다.

**Pergyra.** `parallel`은 그대로 구조적이다. 이름 있는 `spawn`은 scope 객체 없이
**타입 규칙**으로 구조를 근사한다. Future를 모든 경로에서 소비하게 강제하면 고아가
생길 수 없다(`AsyncScopeCore.run_no_orphan`). 반면 `async { }`는 분리가 기본이고
이름 붙은 탈출구가 아니다.

**차이와 평가.**
- 선형 Future는 scope 객체보다 가볍다. 한 함수 안에서는 Trio nursery와 같은 보장을
  준다. 다만 scope 객체가 주는 두 가지를 아직 못 준다. "실패 시 형제 취소"와
  "scope 단위 deadline"이다(§2.2).
- 합의는 "분리는 이름 붙은 탈출구"다. `async { }`가 기본 분리인 것은 이 합의와 반대다.
  `docs/204` §2.5가 이미 `spawn background` + `PGY_CAP_DETACH`를 채택 방향으로 적었다.
  방향은 맞고, 착지하지 않았을 뿐이다.
- Pergyra에는 예외가 없다. 그래서 "실패"는 panic(프로세스 중단)이거나 `Result` 값이다.
  Java `Joiner`에 해당하는 것, 즉 `Result`를 내는 형제 중 첫 `Err`가 나머지를 취소하는
  join 모드는 없다. `any`(첫 성공)만 있다. `docs/204`가 적은 first-success/quorum도
  착지 전이다.

### 2.2 취소

**합의.** 협조적 취소가 표준이다(Swift, Kotlin, Java interrupt, Trio, C++ stop token,
Zig 0.16 `error.Canceled`). Rust의 drop 기반 취소는 예외이고, 그 커뮤니티도 이제
cancel-correctness를 알려진 위험으로 다룬다(Rain 2025). Trio cancel scope는 deadline을
`with` 블록에 붙이고 **level-triggered**로 동작한다. 한번 취소된 scope 안에서는 이후
모든 안전점이 취소를 낸다. Zig는 취소를 오류 집합에 넣어 타입에 드러낸다. 실증 연구:
Sethi 등(OSDI 2022)은 Java·C#·Go 13개 시스템에서 취소 관련 버그 156개를 찾았다.

**Pergyra.** 협조적이다. `Cancel`은 요청이고 합류가 아니다. 후손은 부모 사슬로 상속한다.
안전점은 작업 진입, 채널 대기의 10 ms 주기, `any` 래퍼 안 루프 백엣지다. 프로세스
전체 wall-clock 예산은 있다. 작업별 deadline, `await` 타임아웃, scope deadline은 없다.

**평가.** 협조적 + 후손 전파는 합의와 같다. `Cancel`이 핸들을 은퇴시키지 않는 규칙
(`AsyncLifecycleCore`)은 Rust의 "drop = 취소 = 데이터 유실" 문제를 원천에서 피한다.
비어 있는 것은 **취소의 원인**이다. 시간과 실패 정책이 취소를 일으킬 방법이 없다.
Pergyra에는 예산(budget)이라는 개념이 이미 있다. Trio식 deadline scope는 예산 개념의
작업 단위 버전으로 자연스럽게 들어갈 자리가 있다.

### 2.3 타입으로 얻는 데이터 레이스 자유

**합의.** 타입별 표시(Rust `Send`/`Sync`, Swift `Sendable`)를 바닥에 깔고, 흐름에
민감한 이동 분석으로 표현력을 되찾는다. Swift SE-0414 region-based isolation과
SE-0430 `sending`은 Milano·Turcotti·Myers(PLDI 2022)를 기반으로 한다. Verona BoC
(OOPSLA 2023), OCaml의 contention/portability mode(DRFCaml, POPL 2025), Scala의
separation checker(Degrees of Separation, OOPSLA 2024)가 같은 흐름이다. Swift 6.0에서
6.2로 가는 과정(SE-0466 기본 `@MainActor`, SE-0461 caller-actor 기본)은 엄격한 검사가
기본값을 고친 뒤에야 쓸 만해졌음을 보여준다.

**Pergyra.** 사용자용 Send/Sync는 없다(`docs/113`, `docs/114` §8에서 명시적으로 거부).
경계를 넘을 때마다 네 증거 중 하나를 요구한다. Copy, Channel, Exclusivity,
Disjointness다(`docs/178`). 증거가 없으면 거부한다. 성장하는 컨테이너는 원시 포인터로
워커 경계를 넘을 수 없다(`worker_boundary_storage_policy.c`). slot 경합은 boundary
witness로 검사한다.

**평가.** 사용자 표시 없이 경계별 증거를 요구하는 방식은 Swift가 6.2에서 도달한
"기본값이 덜 동시적이어야 쓸 만하다"는 교훈과 같은 쪽이다. 사용자에게 증명 전략을
드러내지 말라는 저장소 원칙과도 맞는다. 약점은 **구성성**이다. 증거 규칙이 여러 검사기
파일(`type_checker_flow_parallel.c`, `type_checker_async_channel.c`,
`parallel_capture_*_reach.c`)에 흩어져 있다. 함수 경계를 넘는 요약은 수신자 쓰기 깊이
8까지 근사한다. 연구 쪽 region 시스템은 이 요약을 타입이나 region 사실 하나로 들고
다닌다. `docs/178`이 캡처 처리 행을 이미 MIR로 옮겼다. 경계 증거 전체를 SoT 레지스트리의
fact family 하나로 등록하는 것이 연구 쪽 구조에 가장 가깝다.

### 2.4 효과와 함수 색

**합의와 쟁점.** 색 문제는 Nystrom(2015)이 이름 붙였다. 해법은 세 갈래다.
(1) stackless + 색 유지: Rust, C++20, Kotlin, Swift. (2) stackful + 색 제거: Go, Java
virtual threads(JEP 444), OCaml 5 effect handler(PLDI 2021)와 Eio. (3) 능력 전달:
Effekt(OOPSLA 2020), Scala capabilities, Zig 0.16 `Io`(2026-04). Zig는 색을 키워드에서
매개변수로 옮겼고, `io.async`(실패 없음)와 `io.concurrent`(진짜 동시성이 필요함,
`error.ConcurrencyUnavailable`로 실패 가능)를 나눴다. 능력 전달이 `async` 표시보다
실제로 가벼운지는 열린 문제다.

**Pergyra.** `docs/114`는 이를 "coloring decomposition"이라 부른다. 생성(spawn), 합류
(await), 구조(parallel), 스트리밍(Channel), 수신 분기(select), 원격 합류
(RemoteFuture), 취소(Cancel)를 각각 다른 소유자에게 준다. 그런데 `async` 색 자체는
남아 있다. `await`는 `async func` 안에서만 허용된다.

**평가.** 분해 자체는 좋다. Zig의 async/concurrent 분리와 같은 종류의 생각이다.
문제는 남은 색이다.
- 런타임은 stackful이다. 비코루틴 스레드에서도 help-first로 합류할 수 있다
  (`pgy_parallel_task_ops.h:95`). 따라서 "async 문맥 필수"는 구현 제약이 아니라
  순수한 표면 규칙이다.
- 그 규칙이 지키는 실제 의미는 **"이 호출은 멈출 수 있다"는 정보**다. pin이 await를
  넘지 못한다는 규칙과 resume 후 zone generation 재검증(`SuspensionRevalidationCore`)이
  이 정보를 쓴다. 색을 그냥 없애면 피호출자의 멈춤이 호출자의 pin을 몰래 넘게 된다.
- 연구가 주는 답은 색을 **사용자 표시**에서 **추론된 효과**로 바꾸는 것이다.
  Pergyra에는 이미 재료가 있다. intent 검사는 spawn·send에 대해 호출 요약으로
  프로시저 간 검사를 한다(`type_checker_intent_control.c`). `docs/effect_system_design.md`는
  `async` 효과 멤버를 제안해 두었다. "멈출 수 있음"을 추론된 요약으로 만들면, pin·intent
  검사는 그 요약을 소비하고 사용자는 `async`를 쓰지 않아도 된다.
- 다만 이것은 큰 결정이다. 사용자가 보는 의미가 바뀐다. 즉시 착지할 일이 아니라
  결정 기록이 먼저다(§4 R1).

### 2.5 suspension을 넘는 수명과 빌림

**합의와 공백.** Rust는 `.await`를 넘는 빌림을 위해 `Pin`이 필요하다. scoped async는
누수가 안전하다는 전제 때문에 건전하지 않다(Boats 2023 "trilemma"). Swift는 `sending`과
`~Escapable`(SE-0446) 1단계에 있다. suspension 지점을 넘는 안전한 빌림에 대한 2023–2026
동료 심사 타입 시스템이나 형식 모델은 확인 못 함. PinChecker(preprint 2025)는 버그
탐지기다.

**Pergyra.** 활성 pin view는 `await`, `spawn`, `parallel`, 채널, `defer`를 넘지 못한다.
resume 후 zone 사실은 generation으로 재검증한다. 모델 `SuspensionRevalidationCore`는
`stale_never_resolves`를 증명한다.

**평가.** 연구에 아직 답이 없는 자리에서 "넘기지 않는다 + 다시 검증한다"를 고른 것은
보수적이고 옳다. `Pin`의 복잡성을 사용자에게 넘기지 않는다. 남은 틈 하나: 모델은
무한 `nat` generation을 쓰지만 런타임 generation은 `_Atomic uint32_t`다. wrap-around가
모델 밖에 있다. 실무 위험은 낮지만 문서화할 가치는 있다.

### 2.6 교착과 채널

**합의.** 기다림 그래프의 모양을 제한해 순환을 막는다. 채널에는 session type,
참조에는 connectivity graph(POPL 2022), 잠금에는 순서, 작업에는 finish/join 규율이다.
Rust MPST 라이브러리(Rumpsteak PPoPP 2022, Ferrite/MultiCrusty ECOOP 2022, 타임아웃이 있는
MultiCrusty^T ECOOP 2024)가 있다. 교착·누수 자유를 증명한 separation logic(LinearActris,
POPL 2024)도 있다. 하지만 어느 것도 주류 범용 언어에 들어가지 않았다. 비동기 mixed choice
(곧 `select`)는 아직 연구 중이다(Bocchi 등, preprint 2026). Go의 실증(Tu 등, ASPLOS 2019):
차단 버그는 공유 메모리(42%)보다 메시지 전달(58%)에서 더 많았다. 주류의 실제 답은 동적
탐지다. Go는 GC 도달성으로 누수를 찾는 `goroutineleak` 프로파일을 1.27(2026-08)에서
정식으로 내놓았다(GOLF, ASPLOS 2025).

**Pergyra.** 채널은 mutex/condvar 값이다. `select`는 비차단 폴링이고 송신·타임아웃
case가 없다. 정적 교착 분석은 없다. 관련 설계 근거는 하나뿐이다. join 원소는 채널
의존을 가질 수 없어서 풀 고갈 교착이 생기지 않는다(`docs/181` §1.3). 풀 교착은 런타임
help-first await와 보상 스레드로 다룬다(`ParallelSchedulingCore`). 열린 버그로 LLVM
경로의 간헐적 blocked-send hang(task_863abddf)이 있다.

**평가.** 이 축이 가장 뒤처져 있다. 연구 수준(session type)과 실무 수준(Go/CML의 차단
`select`, Go의 동적 누수 탐지) 모두에 못 미친다. 다만 주류 언어도 정적 교착 자유는
못 가졌다. 지금 session type으로 뛰는 것은 저장소의 rung 규율에도 맞지 않는다. 현실적인
순서는 다음과 같다. 불투명 채널 핸들(`docs/178`의 지렛대). 그다음 차단 `select`와
타임아웃 case. 그다음 "모든 워커가 채널에 park됐고 실행 가능한 일이 없음"을 진단으로
바꾸는 런타임 탐지기(조용한 hang을 fail-closed 결과로). intent의 참여자·step 구조는
나중에 multiparty protocol 골격이 될 수 있다. 하지만 지금은 아이디어일 뿐이다.

### 2.7 결정적 병렬성

**합의.** 결정성을 구성으로 얻는다. LVars(FHPC 2013, POPL 2014), Deterministic
Parallel Java(OOPSLA 2009), MaPLe의 disentanglement(ICFP 2022, 정적 TypeDis POPL 2026),
schedule-independent safety(POPL 2026: 내부 결정적 프로그램은 한 interleaving만 검증하면
된다). 대가는 임의의 공유 가변 상태를 포기하는 것이다.

**Pergyra.** `join with all/sum/product/min/max`는 인덱스 순서 고정 fold이고 checked
산술이다. 워커 수와 무관하게 같은 결과를 낸다(`ParallelReductionCore.join_schedule_invariant`,
`join_chunk_count_invariant`). `any`는 명시적으로 비결정이다. `DeterministicSubsetCore`는
결정적 부분집합을 증명하고 반례(`write_conflict_is_schedule_dependent`)도 둔다.

**평가.** 연구 흐름과 가장 잘 맞는 축이다. 비결정을 이름(`any`)으로 드러내는 것도
LVars의 quasi-determinism 같은 "비결정은 명시적으로" 원칙과 같다. 빠진 것은 증명이
아니라 **실행 증거**다. workers 1/2/4/8에서 바이트가 같은지 보는 게이트가 착지하지
않았다(`docs/204`). 싸고 강한 게이트다.

### 2.8 검증

**연구.** Iris/separation logic 계열이 런타임 부품까지 내려왔다. 검증된 OCaml 5 병렬
스케줄러(PLDI 2026), Zoo(POPL 2026), BWoS(OSDI 2023), 취소 가능한 동기화 CQS(PLDI 2023).
취소를 포함한 구조적 동시성 런타임 전체의 기계 검증은 확인 못 함.

**Pergyra.** Rocq 모델은 경계가 정해진 의미 모델이고, adequacy smoke가 소스 줄에 묶는다.
공정성, 종료, C11 happens-before, 채널·select는 범위 밖이다.

**평가.** 언어 규칙을 모델로 고정하고 게이트로 묶는 방식은 이 단계 언어에 맞는 투자다.
가장 큰 공백은 채널·select다. 차단 `select`를 넣기 전에 그 모델을 먼저 두는 것이
fail-closed 순서다.

### 2.9 실행 모델

**합의.** stackful 대 stackless는 이제 런타임 선택이다(Java virtual threads, OCaml 5,
Zig `Io`). Java JEP 491(JDK 24)은 `synchronized`가 carrier를 pin하던 문제를 고쳤다.
io_uring 같은 완료 기반 I/O는 취소 시 버퍼 소유권을 요구한다. 빌린 버퍼는 drop 취소에서
건전하지 않으므로 API가 소유 버퍼를 넘겨야 한다.

**Pergyra.** lane facade가 실행자 불변성을 계약으로 한다. 하지만 실제로 도는 것은
1:1 풀, 이벤트 루프 없는 LocalAsync, BlockingPool이다. M:N은 실행된 적이 없다.

**평가.** facade + 증거 기반 lane은 합의의 앞쪽에 있다. 실행 lane을 이 facade 뒤로 숨기고
"사용자 선택"으로 드러내지 않는 것은 저장소의 개발자 경험 원칙과도 맞는다. 다만 이벤트
루프가 없는 LocalAsync는 "분리된 일은 결국 진행된다"는 가정을 만족하지 못한다(§3.2). M:N은
Movable lane을 만드는 소스가 생기기 전에는 넓히지 않는 것이 맞다(`docs/194`의 판단과 같다).
I/O stdlib을 넓힐 때는 소유 버퍼를 기본으로 해야 한다. 차단 send/recv만 소유권을 옮기는
현재 채널 규칙과 같은 쪽이다.

### 2.10 intent, 보상, 워크플로 (이 문서의 해석)

연구 조사 범위 밖이지만 Pergyra 고유 축이라 적는다. intent는 step, `compensate`,
`rollback: full|current|none`을 가진다. 이것은 saga(Garcia-Molina·Salem, SIGMOD 1987)와
같은 모양이다. intent 절 안에 동시성 구문을 금지하는 규칙(`docs/53` §6)은 durable
workflow 엔진들이 "workflow 코드는 결정적이어야 하고 I/O는 activity로 밀어낸다"고
요구하는 것과 같은 결정성 조건이다. **평가**: intent를 결정적 조정 계층으로, `spawn`·
`parallel`을 그 아래 실행 계층으로 두는 현재 분리는 좋은 방향이다. 이 해석은 후속
연구로 확인할 가치가 있다.

## 3. 직접 확인한 결함과 문서 불일치

검증 실행 파일: `bin/pgy.exe`, `d0fa49ea`에서 빌드(2026-10-05, Windows UCRT64). 같은 결과가
`583aaf04` 빌드에서도 나왔다. 프로브 소스는 아래에 그대로 적었다.

### 3.1 문서 불일치

- `docs/05_async_concurrency.md` §2의 예제는 일반 `func Main` 안에서 `await`를 쓴다.
  체커는 `[ERROR] 7:20 - 'await' used outside of async function`으로 거부한다. 테스트
  픽스처는 모두 `async func Main`을 쓴다. 문서 쪽이 틀렸거나, §2.4의 결정에 따라 규칙
  쪽이 바뀌어야 한다.
- `docs/semantics/05_parallel_execution.md:43`은 read/write slot overlap이 "warning-level"
  이라고 한다. `docs/113:78-80`은 semantic error라고 한다. `docs/113`이 정본이다.
- `docs/146`은 self-host가 "async를 lower하지 않는다"고 한다. 기본 경로는 인자 2개 이하의
  Int/String spawn·await를 받는다(`spawn_runtime_owner.pgy`). 일부가 낡았다.

### 3.2 분리된 `async { }` 블록의 두 결함

```pergyra
async func Slow(x: Int) -> Int {
    return x + 1;
}

async func Main() -> Void {
    async {
        let p: Future<Int> = spawn Slow(1);
        let v: Int = await p;
        Log("detached done " + ToString(v));
    }
    Log("main end");
}
```

- LLVM 경로(`--native-pipeline`): 컴파일 성공, 출력은 `main end`뿐이고 종료 코드 0이다.
  블록 안의 `await` 이후 일은 **아무 진단 없이 사라진다**. `pgy_lane_detach`는 진행
  단계를 한 번만 돌린다(`pgy_parallel_coroutine.h:428-433`). 런타임에는 종료 시 남은
  코루틴을 비우는 경로가 보이지 않는다. 멈추지 않는 본문(`Log`만 있는 블록)은 양쪽
  백엔드에서 정상 출력한다.
- C 경로(`--native-pipeline --backend=c`): 생성된 C가 컴파일되지 않는다.
  `error: invalid storage class for function 'pgy_spawn_wrapper_1'`. spawn 래퍼가 async
  블록 함수 안에 중첩되어 나온다.

첫째는 "숨은 제어 흐름 금지"와 "편의가 실패를 런타임까지 숨기면 안 된다"는 저장소
원칙과 정면으로 부딪힌다. 둘째는 C/LLVM parity 결함이다. 둘 다 `docs/204`의
`spawn background` + `PGY_CAP_DETACH` 결정과 함께 닫는 것이 자연스럽다. 그 전이라도
분리된 블록 안의 suspension을 거부하거나, 종료 시 미완료 분리 작업을 구조화된 진단으로
보고해야 한다.

## 4. 이 방향이 맞는가

### 4.1 유지할 것

1. **`parallel`을 실행 원시로 두는 층위**(`docs/53` §4). 구조적 생성이 기본이라는 합의와
   같다.
2. **반드시 소비되는 Future.** 트릴레마를 피할 수 있는 드문 위치다. 약화하지 말아야 한다.
3. **경계별 증거 4종, 사용자 Send/Sync 없음.** 사용자 표시 없이 흐름 증거로 판단하는
   쪽이 Swift 6.2가 도달한 교훈과 같다.
4. **join 모드의 닫힌 집합과 인덱스 순서 fold.** 결정성 연구와 가장 잘 맞는다.
5. **협조적 취소, `Cancel` ≠ 합류.** Rust의 drop 취소 문제를 원천에서 피한다.
6. **suspension을 넘는 pin 금지와 generation 재검증.** 연구에 답이 없는 자리에서 보수적인
   선택이다.
7. **intent 안 동시성 금지.** 조정 계층의 결정성 조건이다.

### 4.2 고칠 것 (결정 후보, 우선순위 순)

| # | 무엇 | 왜 | 최소 단위 | 게이트 후보 |
|---|---|---|---|---|
| R0 | 분리된 `async { }` 결함 둘(§3.2) | 조용한 유실은 원칙 위반, C는 parity 결함 | 분리 블록 안 suspension을 fail-closed로 거부하거나, 종료 시 미완료 분리 작업을 진단으로 보고. C spawn 래퍼를 파일 범위로 끌어올리기 | 양 백엔드 음성 픽스처 + C/LLVM 출력 비교 |
| R1 | `async` 색의 결정 기록 | 런타임은 stackful인데 표면만 색을 강제. `docs/05` 예제도 어긋남 | 결정 기록 하나: (a) 색 유지 + 문서 수정, (b) "멈출 수 있음"을 추론 효과로 바꾸고 pin·intent 검사가 그 요약을 소비. 이 문서는 (b)를 권한다 | (a)면 문서 예제 컴파일 게이트, (b)면 요약 소유자 음성 게이트 |
| R2 | deadline scope와 실패 정책 | 취소를 일으킬 원인이 없다. Result 형제의 첫 실패를 다룰 join이 없다 | `PgyCancelNode`와 예산을 재사용하는 scope deadline(level-triggered). `await`의 타임아웃은 `Result`로. Result 원소용 join 모드 후보 하나 | 가상 클록 결정 목격자(`PGY_VIRTUAL_CLOCK`) |
| R3 | 채널 | 이름 있는 작업 사이 공유 불가, 비차단 select뿐 | 불투명 채널 핸들 → 차단 `select` + 타임아웃 case → "모두 park" 탐지기 | 채널·select Rocq 모델 먼저, 그다음 양 백엔드 목격자 |
| R4 | 결정성 실행 증거 | 증명은 있고 실행 증거가 없다 | workers 1/2/4/8 바이트 동일 게이트 | `PGY_WORKERS` 매트릭스 smoke |
| R5 | 경계 증거를 fact family 하나로 | 증거 규칙이 검사기 여러 곳에 흩어져 구성성이 약하다 | `docs/178` 증거를 SoT 레지스트리 행 하나로 등록하고 소비자 이전 | 레지스트리 행 + 음성 래칫 |
| R6 | 문서 정리 | §3.1의 불일치 셋 | 문서만 | 문서 예제 컴파일 smoke |

R0과 R6은 작고 즉시 할 수 있다. R1은 사용자 의미를 바꾸므로 결정이 먼저다. R2와 R3는
`docs/204`의 채택 방향과 겹친다. 그 문서의 순서를 따르는 것이 맞다. 모두 현재 self-host
치환 rung 밖이다. 저장소 규율상 진행 중인 rung을 밀어내지 않는다.

### 4.3 하지 말 것

- 사용자용 Send/Sync 표시 도입. 저장소가 이미 거부했고, 연구 흐름도 흐름 증거 쪽이다.
- 지금 session type 도입. 주류 언어 어느 곳도 못 했다. 채널 기본기(R3)가 먼저다.
- Movable lane을 만드는 소스 없이 M:N 확장. 측정 대상이 없는 아키텍처 작업이다.
- 실행 lane을 사용자 문법으로 드러내기. `docs/146`의 증거 유도가 강점이다.

### 4.4 열린 질문

- 추론된 "멈출 수 있음" 효과가 분리 컴파일·extern 경계에서 어떻게 보수적으로 남는가.
- Result 원소 join 모드의 이름과 의미(첫 `Err` 승리인가, 모두 모으는가).
- intent의 참여자·step 구조를 protocol 골격으로 쓸 수 있는가.
- uint32 generation wrap-around를 모델에 넣을 것인가, 런타임 불변식으로 막을 것인가.

## 5. 출처 (2026-10-05 확인)

구조적 동시성
- N. J. Smith, "Notes on structured concurrency, or: Go statement considered harmful", 2018. https://vorpus.org/blog/notes-on-structured-concurrency-or-go-statement-considered-harmful/
- R. Elizarov et al., "Kotlin coroutines: design and implementation", Onward! 2021. https://doi.org/10.1145/3486607.3486751
- SE-0304 https://github.com/swiftlang/swift-evolution/blob/main/proposals/0304-structured-concurrency.md · SE-0317 https://github.com/swiftlang/swift-evolution/blob/main/proposals/0317-async-let.md
- JEP 505 https://openjdk.org/jeps/505 · JEP 533 https://openjdk.org/jeps/533 · JEP 543 https://openjdk.org/jeps/543
- PEP 654 https://peps.python.org/pep-0654/
- J. K. Lee, J. Palsberg, "Featherweight X10", PPoPP 2010. https://doi.org/10.1145/1693453.1693459
- G. Gray, S. Krishnamurthi, W. Crichton, "A Design Space Exploration of Async/Await", OOPSLA 2026. https://doi.org/10.1145/3839519

취소
- N. J. Smith, "Timeouts and cancellation for humans", 2018. https://vorpus.org/blog/timeouts-and-cancellation-for-humans/
- Rain, "Cancelling async Rust", 2025. https://sunshowers.io/posts/cancelling-async-rust/
- S. Marlow et al., "Asynchronous exceptions in Haskell", PLDI 2001. https://doi.org/10.1145/378795.378858
- U. Sethi et al., "Cancellation in Systems", OSDI 2022. https://www.usenix.org/conference/osdi22/presentation/sethi
- N. Koval, D. Khalanskiy, D. Alistarh, "CQS", PLDI 2023. https://arxiv.org/abs/2111.12682

데이터 레이스 자유
- R. Jung et al., "RustBelt", POPL 2018. https://doi.org/10.1145/3158154
- SE-0414 https://github.com/swiftlang/swift-evolution/blob/main/proposals/0414-region-based-isolation.md · SE-0430 https://github.com/swiftlang/swift-evolution/blob/main/proposals/0430-transferring-parameters-and-results.md · SE-0461 https://github.com/swiftlang/swift-evolution/blob/main/proposals/0461-async-function-isolation.md
- M. Milano, J. Turcotti, A. C. Myers, "A Flexible Type System for Fearless Concurrency", PLDI 2022. https://doi.org/10.1145/3519939.3523443
- A. L. Georges et al., "Data Race Freedom à la Mode", POPL 2025. https://doi.org/10.1145/3704859
- Y. Xu, A. Boruch-Gruszecki, M. Odersky, "Degrees of Separation", OOPSLA 2024. https://doi.org/10.1145/3649853
- L. Cheeseman et al., "When Concurrency Matters: Behaviour-Oriented Concurrency", OOPSLA 2023. https://doi.org/10.1145/3622852

효과와 색
- B. Nystrom, "What Color is Your Function?", 2015. https://journal.stuffwithstuff.com/2015/02/01/what-color-is-your-function/
- D. Leijen, "Structured Asynchrony with Algebraic Effects", TyDe 2017. https://doi.org/10.1145/3122975.3122977
- KC Sivaramakrishnan et al., "Retrofitting Effect Handlers onto OCaml", PLDI 2021. https://doi.org/10.1145/3453483.3454039
- J. I. Brachthäuser, P. Schuster, K. Ostermann, "Effects as Capabilities", OOPSLA 2020. https://doi.org/10.1145/3428194
- D. Ahman, M. Pretnar, "Asynchronous Effects", POPL 2021. https://doi.org/10.1145/3434305
- A. Kelley, "Zig's New Async I/O", 2025. https://andrewkelley.me/post/zig-new-async-io-text-version.html · Zig 0.16.0 notes https://ziglang.org/download/0.16.0/release-notes.html

수명과 빌림
- Without Boats, "The Scoped Task trilemma", 2023. https://without.boats/blog/the-scoped-task-trilemma/
- SE-0446 https://github.com/swiftlang/swift-evolution/blob/main/proposals/0446-non-escapable.md
- Y. Dai, Y. Feng, "PinChecker", preprint 2025. https://arxiv.org/abs/2504.14500

교착과 채널
- K. Honda, N. Yoshida, M. Carbone, "Multiparty Asynchronous Session Types", JACM 2016. https://doi.org/10.1145/2827695
- Z. Cutner, N. Yoshida, M. Vassor, Rumpsteak, PPoPP 2022. https://doi.org/10.1145/3503221.3508404
- P. Hou, N. Lagaillardie, N. Yoshida, "Fearless Asynchronous Communications with Timed Multiparty Session Protocols", ECOOP 2024. https://doi.org/10.4230/LIPIcs.ECOOP.2024.19
- L. Bocchi et al., "Mixed Choice in Asynchronous Multiparty Session Types", preprint 2026. https://arxiv.org/abs/2602.23927
- J. Jacobs, S. Balzer, R. Krebbers, "Connectivity Graphs", POPL 2022. https://doi.org/10.1145/3498662
- J. Jacobs, J. K. Hinrichsen, R. Krebbers, "Deadlock-Free Separation Logic", POPL 2024. https://doi.org/10.1145/3632889
- J. H. Reppy, "CML", PLDI 1991. https://doi.org/10.1145/113445.113470
- T. Tu et al., "Understanding Real-World Concurrency Bugs in Go", ASPLOS 2019. https://doi.org/10.1145/3297858.3304069
- G.-V. Saioc et al., "Dynamic Partial Deadlock Detection and Recovery via Garbage Collection", ASPLOS 2025. https://doi.org/10.1145/3676641.3715990 · Go 1.27 notes https://go.dev/doc/go1.27

결정성과 검증
- L. Kuper, R. R. Newton, "LVars", FHPC 2013. https://doi.org/10.1145/2502323.2502326
- R. L. Bocchino Jr. et al., "Deterministic Parallel Java", OOPSLA 2009. https://doi.org/10.1145/1640089.1640097
- A. Moine, S. Westrick, J. Tassarotti, "All for One and One for All", POPL 2026. https://arxiv.org/abs/2511.23283
- C. Allain, G. Scherer, "A Verified Parallel Scheduler for OCaml 5", PLDI 2026. https://doi.org/10.1145/3808337
- J. Wang et al., "BWoS", OSDI 2023. https://www.usenix.org/conference/osdi23/presentation/wang-jiawei

실행 모델
- JEP 444 https://openjdk.org/jeps/444 · JEP 491 https://openjdk.org/jeps/491
- R. D. Blumofe, C. E. Leiserson, "Scheduling multithreaded computations by work stealing", JACM 1999. https://doi.org/10.1145/324133.324234
- Without Boats, "Notes on io-uring", 2020. https://without.boats/blog/io-uring/

워크플로(§2.10)
- H. Garcia-Molina, K. Salem, "Sagas", SIGMOD 1987. https://doi.org/10.1145/38713.38742
