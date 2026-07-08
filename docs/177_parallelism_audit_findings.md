# 177. 병렬성 실측 감사 — "직렬성의 착시" 발견

Status: `audit-findings` (수정 0 — 발견·근인·수정 순서 제약만 고정. 수정은
태스크 #1/#2). 작성 2026-07-06. 계기: BDFL — "FP가 병렬에 유리한 건 불변이라서
인데 우리도 구현했나? 병렬성 체크해서 이치에 맞게 값 나오는지 보자. 여러 문제점
체크해야 함." 방법: 프로브 8종(.tmp/par_probe/) × 양 백엔드 × 10회 반복 +
codegen/런타임 소스 추적. 상위: docs/113(메모리·동시성 모델), docs/146(SEA),
docs/114(async 포지셔닝).

---

## 0. 한 장 요약

- **개념 교정**: FP의 병렬 우위는 스택 배치가 아니라 **공유 가변 상태의 부재**
  (불변성)다. 그 기준으로 감사한 결과 —
- **교리는 절반 구현돼 있다**: copy-only async 표면(docs/113), 가변 컬렉션
  캡처 fail-closed 거절(실측), Channel/Slot 스레드-안전 런타임(실측), spawn
  인자의 Channel 수송 거절(실측). 게이트 3종(worker-boundary-ub / memory-
  concurrency-model / parallel-core-contract) 전부 green.
- **★F1 (major)**: 그러나 **parallel/spawn은 현재 전부 무음 직렬 실행**이다.
  10/10 결정론은 병렬이 이치에 맞아서가 아니라 **병렬이 없어서**였다 —
  직렬성의 착시.
- **★F2 (major, F1이 가림)**: 캡처된 가변 **스칼라는 포인터로 공유**되고
  아무 가드가 없다. 풀이 켜지는 순간 실 데이터레이스(UB)로 승격 →
  **수정 순서 제약: F2 정책 없이 F1을 고치면 안 된다.**

## 1. 프로브 매트릭스 (전부 양 백엔드 × 10회)

| 프로브 | 내용 | 결과 |
|---|---|---|
| p1 | 채널 4-send parallel 후 합산 | 10/10 = "10", 백엔드 일치 |
| p2 | `let mut x=1; parallel { x=x+1; } Log(x)` | **2** — 캡처 돌연변이가 join 후 보임(복사 아님) |
| p3 | 두 arm이 같은 x에 `+1`/`+10` | 10/10 = "11" 결정론 |
| p4 | spawn×2 + await | 10/10 = 42/25 |
| p5 | 서로소 슬롯 2개 병렬 쓰기 | 10/10 = "10" |
| p6 | 두 arm이 같은 x에 각 10만 증분(레이스 압력) | 10/10 = **200000, 손실 0** |
| p7 | 교대 필수 핑퐁(직렬이면 어떤 순서로도 deadlock) | 블록-arm **파싱 불가**; spawn판은 Channel 인자 수송 **거절** |
| p8 | **backpressure 목격자**: 용량 8 채널에 10만 send + 동시 소비 arm | **HANG 양 백엔드** — arm이 겹치지 않음 |

p1~p6의 "이치에 맞는 값"은 전부 직렬 실행과 일치하는 값이다. p8이 판별자였다.

## 2. 근인 사슬 (F1 — 소스 추적, 전 고리 실측)

1. 방출은 올바르다: LLVM parallel 방출(llvm_stmt_parallel_async.c:369-392)은
   전부-spawn 후 전부-await(fan-out/join), `pgy_lane_spawn_dispatch(WORKER_POOL)`.
   런타임도 올바르다: lane dispatch→`pgy_spawn`(풀 제출), `pgy_parallel_run`
   (제출 전체 후 조인), pthread 워커 풀(기본 4).
2. **그러나 생성된 main에 `pgy_pool_init`이 없다**: transpiler.c:440은
   `transpiler_requires_thread_pool()`일 때만 방출 → thread_pool_usage.c
   ("Shared C/LLVM") → `pgy_mir_program_uses_thread_pool` → **MIR surface-usage
   fact(`uses_thread_pool_surface`)가 parallel/spawn 프로그램에서 참이 되지
   않는다**(p1/p4/p8 방출 C에서 pool_init 0회 실측 — dispatch 호출은 있음).
3. 풀 비활성 → `pgy_spawn`이 **`pgy_spawn_inline_completed`로 폴백**
   (pgy_parallel.h:455)하는데, 이 함수는 **경고 없이** 태스크를 첫 호출 안에서
   완주 실행 → 직렬. (정정: 4번째 인자 `false`는 warn이 아니라
   `charge_spawn_budget` 플래그다. 무음성은 이 폴백 경로에 진단이 아예 없다는
   것이지 플래그가 아니다.) p8에선 생산자가 9번째 send에서 영원히 대기.
4. **왜 아무 게이트도 못 잡았나**: 기존 corpus(producer_consumer, async_demo,
   backend_compare fixture)는 전부 buffer-fitting/overlap-불요 워크로드 —
   직렬 실행과 관찰 동치. **동시성 목격자 fixture가 코퍼스에 없다.**
   docs/146의 SEA 계약("executor must not change the program's meaning")은
   이 예제들에 대해선 공허하게 성립해왔다.

무음 폴백 자체가 CLAUDE §1.1(hidden control flow) 위반이라는 점이 이중 결함:
fact가 고쳐져도, 풀 초기화 실패 시 같은 무음 직렬화가 재발한다. 폴백은 warn
또는 fail이어야 한다.

## 3. F2 — 스칼라 캡처는 포인터 공유 (교리 갭, F1이 가림)

- llvm_stmt_parallel_async.c:256 — 캡처 ctx struct에 **값이 아니라 alloca
  포인터**를 저장. 모든 arm이 부모 스택 슬롯 공유.
- 가드는 가변 **컬렉션**만 거절("cannot share mutable collection by pointer;
  use a channel/result boundary or copy before spawning") — **스칼라 무가드.**
- 현재는 F1(직렬)이 가려서 p3/p6이 결정론으로 보인다. 풀이 켜지면 같은 프로브가
  unsynchronized RMW = C-수준 UB. docs/113 교리("worker-local/lock/atomic/
  immutable-snapshot 아니면 rejected or unsafe")를 스칼라가 빠져나가는 구멍.
- 비대칭 노트: **클로저는 copy-capture(Stage A), parallel은 포인터-캡처** —
  두 경계의 캡처 교리가 갈라져 있다. 단일-arm 쓰기+join-후 읽기(p2)는 구조적
  동기화로 건전할 수 있으나(Rust scoped-thread 유사), 다중-arm 쓰기(p3)는
  정책이 필요하다 — IntentConflict.v `sep_when_active`의 문장-수준 미시판.
- **★수정 순서 제약: F2 정책(거절/copy-in/명시 마커) 착지 전에 F1(풀 활성화)을
  먼저 고치면 안 된다.** 태스크 #1에 명기.

## 4. F3 — 표면 갭 (minor)

- **bare 블록이 parallel arm으로 파싱 불가** → 다중-문장 arm은 while-트릭
  강제. 핑퐁 같은 실 프로토콜 arm 표현 제약.
- **spawn 인자 Channel 수송 거절 vs parallel 캡처 Channel 허용** 비대칭 —
  copy-only 인자 교리로는 정합이나, 결과적으로 "채널로 통신하는 두 named task"
  를 spawn으로 표현할 수 없다(parallel 블록 캡처만 가능한데 F3-1로 arm 표현이
  제한). 동시성 표현의 실질 통로가 좁다.

## 5. F4 — 긍정 실측 (지킬 것)

Channel/Slot/Future 값 흐름 10/10 결정론·백엔드 일치, 가변 컬렉션 거절 발화,
게이트 3종 green, spawn 인자 copy-only 강제. **불변-경계 교리의 절반은 실물**
이다 — 나머지 절반(스칼라, 그리고 진짜 병렬)이 태스크 #1/#2.

## 6. 등록

- 태스크 #1 — F1: fact 수리 + 무음 폴백 제거 + **p8-형 backpressure 목격자
  fixture를 게이트로 승격**(재발 방지의 본체).
- 태스크 #2 — F2: 스칼라 캡처 정책(권고: 다중-작성자 거절 + copy-in 기본,
  클로저 교리와 정렬). #1보다 선행 또는 동시.
- F3은 표면 결정(BDFL)이라 WO 미등록 — 이 문서가 결정 입력.
- WitnessDataRace.v의 well-typed 전제가 스칼라 캡처를 모델하는지 확인해 정리
  범위를 명시할 것(#2에 포함) — 정리가 틀린 게 아니라 전제 밖일 것으로 추정
  되나 실측 전 단정 금지.

## 7. 수정 착지 (2026-07-06) — F1/F2 완료, F3는 표면 결정으로 보류

순서 제약대로 **F2 → F1을 함께 빌드**(pool이 켜질 때 정책이 이미 존재).

- **F2 완료** — `type_checker_flow_parallel.c`에 `parallel_reject_scalar_write_race`:
  docs/178 Exclusivity 증거 판정. 외부 스칼라에 대해 arm별 writer/참조 수를
  세어 **≥2 writer(write-write) 또는 1 writer+타 arm 참조(read-write)를 거절**,
  단일 writer 무참조·전부 read는 허용. collection(별도 거절)·Channel/Slot/
  Future(런타임 동기화)는 제외. 기존 line 196의 *경고*를 실 *거절*로 승격.
  실측: p3/p6 거절, p1/p2/p5 통과, 기존 병렬 예제(producer_consumer/async_demo/
  concurrency_demo/channel_test) 무회귀.
- **F1 완료** — 근인은 `ast_uses_thread_pool_surface`가 **함수 본문으로 안
  내려간 것**(inventory rollup이 함수 decl 노드를 스캔하는데 `AST_FUNC_DECL`
  케이스 부재 → default:false). `ast_thread_pool_analysis.c`에 함수-본문 하강
  케이스 1개 추가(`ast_func_body`가 async/sync 분기 처리). 실측: p1/p4/p8이
  이제 pool_init 방출(C+LLVM), **p8 backpressure가 양 백엔드 100000 완주 = 진짜
  병렬**. 무음 폴백에는 `pgy_parallel.h`에 warn 추가(§1.1 준수).
- **목격자 게이트 완료** — `tests/cases/backend_compare/parallel_backpressure_
  witness/`(용량 4 채널에 100 send + 동시 소비). 직렬 회귀 시 deadlock→run
  timeout→**RED**, 병렬이면 C==LLVM=100 green. compare_backends.sh 목록 등록.
  실측: 드라이버 통과(1/1). buffer-fitting corpus가 못 준 재발 방지의 본체.
- **회귀 판별** — test-transpile이 897/19지만, 내 3파일을 stash한 baseline도
  897/19(동일) → **19 fail은 전부 동시 세션의 slot ABI 라우팅 리팩터, 내 변경
  기여 0**. test-concurrency green, 게이트 3종 green.

**F3는 코드 수정 대상이 아님(보류, BDFL 결정 입력)**: (a) bare 블록을 parallel
arm으로 파싱 = **문법 확장**이라 표면 결정(임의 문법 변경은 canon 위반 위험),
(b) spawn 인자 Channel 거절 = **의도적 copy-only 교리**라 "고칠" 대상이 아니라
교리 변경 후보. 둘 다 이 문서가 결정 입력이고, WO 미등록 유지. "채널로 통신하는
named task 쌍" 수요가 실측되면 (b)를 재론.

.tmp/par_probe/(프로브 8종+로그 — 미커밋 실험물) · docs/113(교리 — F2가 집행
갭) · docs/146(SEA — lane 계약은 옳고 실행이 미완) · docs/114 · pgy_parallel.h
:455(무음 폴백) · llvm_stmt_parallel_async.c:256(포인터 캡처)/:369(fan-out
join 올바름) · thread_pool_usage.c(Shared C/LLVM 탐지 — fact 미설정 근인) ·
transpiler.c:440(조건부 pool_init) · IntentConflict.v(다중-작성자 정책의 이론
착지점) · 클로저 Stage A(copy-capture 선례 — 교리 정렬 대상).

## 8. copy-only 교리 판정 + 후속 착지 (2026-07-08/09, BDFL 비준)

"copy-only 교리를 깨야 하나"에 대한 소스-추적 판정과 그 후속 구현의 기록.

- **판정: 깨지 않는다 — 교리는 load-bearing이고 옳다.** F3(b)의 spawn
  Channel-인자 거절은 교리 결함이 아니라 **lowering 사실**의 귀결이다:
  현행 `Channel<T>`는 `pthread_mutex_t`+condvar 2개를 struct 안에 inline으로
  박은 값 구조체(pgy_runtime_channel_inline.h)이고, ABI 스펙이 명시적으로
  "must not be copied"를 박아뒀다(pgy_abi_spec.h §6). copy-only인 spawn
  인자로 넘기면 mutex 비트복사 = POSIX UB — 거절이 정확하다. 반대로
  parallel 캡처는 포인터 공유(모든 arm이 *같은* mutex 객체)라 안전하고,
  p8 backpressure 목격자가 이를 실증한다. **비대칭은 실재하지만 그 원인은
  "Channel이 아직 복사 가능한 값이 아니다"라는 표현 문제다.**
- **깨면 안 되는 이유 2중**: (a) task 경계 최초의 by-ref escape가 되어
  클로저 Stage C 거절 canon과 충돌하고 F2 구멍을 handle-보유 타입 전체로
  재개방, (b) by-ref task escape는 수명 추론을 강제하는 형태 — lifetime
  주석 하드-금지(docs/118 §2.1)와 정면 충돌.
- **올바른 미래 lever**: opaque handle lowering. ABI가 이미 타깃을 깔아뒀다
  (ZoneChannel/WorldChannel = uint32_t handle, pgy_abi_spec.h). 일반
  Channel<T>가 handle로 lowering되면 `spawn Worker(ch)` = uint32_t 복사 =
  **copy-only를 만족하면서** 채널 공유가 열린다. ABI+양 백엔드+arena 수명
  workstream이고 수요("채널로 통신하는 named task 쌍") 실측 전 착수 금지.
- **후속 착지 1 — slice 쓰기 표면(`9bf946d7`)**: `view[i] = v`를
  `pgy_slice_set`으로 양 백엔드 완결(semantic은 원래 read/write 대칭 수용,
  codegen이 반열림이었다). 부산물: slice_get + raw list/queue/map 접근자의
  inline OOB panic 본문이 재생성된 `.bc` 재최적화에서 access violation으로
  mis-lower하는 잠재 패밀리를 runtime_panic_codegen_smoke로 적발,
  `llvm_fn_is_bounds_checked_accessor`에 패밀리 단위 등재로 폐쇄.
- **후속 착지 2 — Disjointness admission(`c994f39b`, docs/178 WO-DOP-1
  rung 0)**: `base.Slice(0,B)`/`base.Slice(B,LEN)` 분할 쌍을 parallel
  캡처에서 증거로 인정. [0,B)∩[B,B+LEN)=∅는 B/LEN 값과 무관한 정리이므로
  값 분석 없이 구성 사실만으로 성립(판정이 아니라 선언). 조건: 불변 view
  바인딩 + 불변 Int 경계(리터럴 or 비-param 로컬) + 같은 base의 캡처된
  fact-slice가 정확히 이 쌍 + 절반당 정확히 1개 arm 참조 + base는 어떤
  arm에도 미참조. 그 외 전부 기존 fail-closed 거절 유지.
  `parallel-disjoint-test-smoke`(admit 110 양 백엔드 + negative 4종:
  동일-slice 이중 쓰기/base 침범/mut 경계/임의 겹침 view) + backend_compare
  `parallel_disjoint_split_write`가 상설 게이트. **DOP 지향의 병렬 통로
  0→1** — docs/178 §2 표의 "통로 부재"가 이 시점부로 stale.
- **evidence lifetime 접점**(docs/semantics/09의 압축 예산으로 읽기):
  Disjointness 증거 = 마지막 소비자가 semantic admission이고 이후 **erase**
  (런타임 객체 0) · Channel 증거 = 런타임 **retain**(mutex/backpressure
  상태) · spawn-Channel = **reject**. 이 arc의 세 결정이 정확히 세 budget의
  인스턴스다.
- 남은 표면 결정(코드 아님, 이 문서가 결정 입력): F3(a) bare-block arm
  (문법 확장), F2 copy-in 기본화(관찰 의미 변경 — 부모가 join 후 변화를
  못 보게 됨).
