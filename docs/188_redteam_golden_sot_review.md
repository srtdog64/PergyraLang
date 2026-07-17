# 188. 레드팀 입장서 — 골든테스트 공백과 SoT 갈래 감사 (2026-07-17)

BDFL 지시 "골든테스트 볼만한 지점이랑 SoT 여러개로 갈린 부분들 레드팀
입장에서" 의 답. main 단일 브랜치에서 읽기-감사만 수행(수리 없음), 모든
판정은 게이트 실행·재현 바이너리·grep 증거 기반. 감사 시점 트리:
`41c17e84` + 미커밋 23파일(사용자 WIP — macOS sysctl 이식, transpile
골든 갱신 중, mir_hir_block_projection 신규).

먼저 GREEN 확인(이번 감사에서 실행): channel-pool-starvation 게이트
c/llvm 4/4(보상 스레드 설계, `8c001799` 흡수판), backpressure 64/64×2
(inline-help 반증 재발 없음), nested 4/4, worker-invariance(1/2/3/16
byte-동일). 런타임 봉합 자체는 건전하다.

## 발견 (심각도순)

### R1. [HIGH · silent-wrong · 현재 main에서 재현] Channel<T> 함수-param 무음 디스크립터 복사 — 게이트 0

직렬 코드 재현(현 main, 이 감사에서 재실행):

```pgy
func RecvOne(ch: Channel<Int>) -> Int { let v: Int = <- ch; return v; }
// ch에 1,2,3,4 send 후: RecvOne(ch) + RecvOne(ch) == 2   // 기대 3
```

채널을 함수 파라미터로 넘기면 디스크립터(buf 포인터+head/count)가
값-복사되어 호출마다 자기 복사본 head로 공유 buf를 읽는다 — **중복
수신, 경합 무관**. 등록 상태는 칩(task_b4b2f972)뿐, 거절도 게이트도
없다. 이 클래스는 "테스트는 지나가고 프로덕션에서 조용히 틀리는"
프로젝트 최악 분류다.

**권고**: ① 1차 rung = semantic fail-close(Channel-typed param 거절,
힌트: 캡처-현장 직접 사용) ② 거절 fixture + 직접-사용 회귀를 같은
커밋에 ③ reference 의미론 승격은 그 뒤 별도 판정.

### R2. [현재 RED 게이트] chunk 정책 owner pin — 분가·리네임 2축 드리프트를 정확히 물었으나 main이 빨간 상태

`tests/selfhost_parallel_chunk_policy_smoke.sh`가 지금 RED다:
owner(`chunk_policy_owner.pgy`)의 required-term이
`src/runtime/pgy_parallel.h`를 가리키는데, 머지 리팩터가 chunk 코드를
`src/runtime/pgy_parallel_chunk.h`로 **분가**시켰고, 분할 산술 철자도
`base * k + (k < rem ? k : rem)` → `base * index + (index < remainder
? index : remainder)`로 **리네임**됐다(의미 동일 — 이 감사에서 대조
확인, factor=4·count 로직·경계식 전부 등가).

레드팀 판정 두 겹:
- **드리프트 감지기는 설계대로 작동했다** — owner 모르게 투영이
  움직이면 빨개진다는 계약 그대로. 이건 성공 사례다.
- 동시에 **exact-string pin의 취약 클래스**가 실증됐다: 무해한
  리팩터(파일 분가, 변수 리네임)가 false-positive 드리프트로 잡힌다.
  진짜 의미 불변량은 이미 실행 골든(테이블+cover 불변식+C==LLVM leg)이
  잡고 있으므로, 문자열 pin은 안정 식별자만 물어야 한다.

**권고**: ① owner의 pin을 안정 식별자로 교체(`pgy_parallel_chunk_count`,
`#define PGY_PARALLEL_CHUNK_FACTOR 4`, `pgy_parallel_spawn_chunk_at`,
export명, emitter 호출명 — 산술 철자 pin 폐기) + 경로를
`pgy_parallel_chunk.h`로 갱신 ② require 행이 골든 파일에도 내장돼
있으므로(3중 lockstep: owner+golden+투영) 골든 재생성 동반 ③ 근본
수리는 **등록 3점**(OWNERS.md·component contract smoke·artifact-kind)
— 등록됐더라면 리팩터 시점에 contract smoke가 owner 동반-수정을 강제
했다. 미등록이 이 사건의 1원인.

### R3. [의미 드리프트 · 판정 필요] SPAWN_COUNT 예산이 auto-chunk 이후 다른 것을 세고 있다

B3 전: `parallel (i in 0..N) join`은 태스크 N개 = SPAWN_COUNT N charge
→ R6 sandbox의 스폰 ceiling이 사실상 반복수 상한이었다. B3 후: charge는
`chunk_count(n) = min(n, workers×4)`개뿐 — **같은 프로그램의 예산
의미가 조용히 바뀌었다**. 반복 바디 자체는 이제 어떤 축에도 안 걸린다
(ALLOC_BYTES가 ctx 배열 메모리로 간접 방어할 뿐, 시간/반복 축 부재 —
현 축: ALLOC_BYTES/ALLOC_COUNT/SPAWN_COUNT/CHANNEL_COUNT).

**권고**: BDFL 판정 1건 — (a) "SPAWN_COUNT = 실제 OS-태스크 생성 수"로
재정의·문서화, 반복-폭탄은 별도 축(ITER/TIME)의 미래 작업으로 등록,
또는 (b) join fan-out은 N을 계속 charge. **어느 쪽이든 charge 수를
pin하는 골든이 현재 0개** — 결정 후 `budget charge 횟수` 골든 필수
(작은 fixture + 예산 한도 경계값 2개면 충분).

### R4. [골든 공백] parallel join 방출 형태의 유닛-레벨 골든 0

`src/tests/`에 `_pj_`/`pjoin` 문자열이 한 건도 없다. join lowering
(이제 chunk 호출 포함)은 행동 fixture와 결정성 게이트로만 방어되는데,
**형태 회귀가 행동을 안 바꾸는 클래스**가 있다: cancel back-edge 제거
(any-join 지연으로만 관측), `_pj_cc_` free 누락(leak — 게이트 없음),
await 루프 bound 실수(n vs nch — 초과 await는 panic이지만 미달은
조용히 지나갈 수 있는 조합 존재).

**권고**: transpile 유닛 1개 — 소형 join AST를 emit해
EXPECT_STR_CONTAINS로 5줄 pin: `pgy_parallel_chunk_count(_pj_n_`,
`pgy_parallel_spawn_chunk_at(_pj_cc_`, `_pj_k < _pj_nch_`,
`free(_pj_cc_`, cancel-경유 await 순서. (R2의 안정-식별자 원칙과 동일
낮이 — 산술이 아니라 호출·경계 식별자만.)

### R5. [골든 공백] F1 "무음 직렬화 금지" warn이 어떤 게이트에도 안 물려 있다

`pgy_parallel_warn("spawn", "worker pool inactive; task runs inline
(serial)...")`은 docs/177 F1의 관측가능성 계약 그 자체인데, tests/에서
이 텍스트를 확인하는 곳이 없다. 누가 warn을 지우거나 문구를 바꿔도
아무것도 빨개지지 않는다 — "관측 가능해야 한다"는 계약이 자기 자신은
관측 안 되는 상태.

**권고**: 풀 없이 spawn을 강제하는 witness(런타임 유닛 또는 fixture)
+ stderr에 해당 warn 존재 assert. PGY_WORKERS 무효값 warn도 같은
witness에 동승 가능.

### R6. [SoT 이중화 · 소형] BLOCKED_TICK 가드 매크로가 두 채널 헤더에 중복 + 선점-정의에 열림

`PGY_CHANNEL_BLOCKED_TICK`의 `#ifndef` 가드 블록이
`pgy_runtime_channel_inline.h`와 `pgy_runtime_channel_string_inline.h`
두 곳에 산다(문구 드리프트 가능). 그리고 `#ifndef` 패턴 특성상 어떤
TU가 **먼저** `((void)0)`으로 정의하면 보상이 무음 비활성된다 — 현재는
`pgy_runtime.h`의 include-순서 주석+하드-요구로 방어되지만, 우회 include
경로가 생기면 조용히 뚫린다.

**권고**: 가드를 공용 헤더 1곳(`pgy_runtime_channel_status.h`)으로
단일화, 재정의는 `#error`로 fail-close.

### R7. [잔차 기록 검증 · 이상 없음] 보상 스레드 설계의 알려진 한계 3개는 전부 관측 가능

이 감사에서 재확인한 degrade 경로: ① spare cap(worker×4) 초과 →
quantum-park 강등(보드 기록, fiber=후보) ② spare pthread_create 실패 →
warn 후 park ③ tick의 sleepers/pending 검사 레이스 → 10ms 양자
재평가로 자기치유. 셋 다 무음 아님. 단 ①의 "cap 초과 형태" 문서화-RED
witness는 없다 — 선택 항목으로만 등재(만들면 fiber 판정의 정량 근거가
하나 더 생긴다).

### R8. [프로세스 SoT] 병렬 측정 서사가 5곳에 산다 — canon 포인터 1줄 부재

같은 숫자·판정이 `benchmarks/PARALLEL_RESULTS.md`(자칭 최신),
`benchmarks/BN_RESULTS.md`, docs/186 진행절, TODO 보드 WO-RT-4/5,
세션 메모리에 복제돼 있다. 지금은 서로 정합하지만 다음 재측정 때
5곳 동시 갱신을 사람이 기억해야 한다. docs/185(SoT 게이트 카탈로그)에
"병렬 성능 원장 canonical = PARALLEL_RESULTS.md, 나머지는 파생" 1행
등재를 권고 — 이 문서 부류의 기존 해법 그대로.

### R9. [골든 refresh 규율] 골든 갱신이 잦아지는 국면 — 갱신 사유 동반 규칙 제안

현재 dirty에 transpile 골든 7파일+`expected/clean.json`이 갱신 중이고,
직전 이력에 "ci: refresh production C size golden"이 있다. 골든은
드리프트 감지기라서, **refresh 커밋이 사유 없이 지나가면 감지기를
끄는 것과 같다**. 권고: 골든-갱신 커밋 메시지에 "무엇이 왜 바뀌어
골든이 따라가는가" 1문장 의무화(docs/185에 규칙 1행).

### R10. [배선 공백] 신설 게이트 4종이 Makefile/test-all 밖에 있다

nested witness · worker-invariance · channel-starvation ·
selfhost-chunk-policy 4종 스모크는 전부 standalone(당시 Makefile
동시-소유로 보류). Makefile이 이제 열렸으므로 타겟+test-all 배선이
가능해졌다 — 배선 전까지 이 게이트들은 "아는 사람만 돌리는" 상태고,
R2가 보여줬듯 아무도 안 돌리는 사이 main이 빨개질 수 있다.

## 요약 표

| # | 분류 | 상태 | 한 줄 |
|---|---|---|---|
| R1 | silent-wrong | **LIVE** | 채널 param 복사 — 거절도 게이트도 없음 |
| R2 | SoT pin | **RED now** | owner pin이 분가·리네임에 발화, 등록 3점이 근본 수리 |
| R3 | 의미 드리프트 | 판정 대기 | SPAWN 예산이 N→chunk수로 조용히 변경, charge 골든 0 |
| R4 | 골든 공백 | open | join 방출 형태 유닛 골든 0 |
| R5 | 골든 공백 | open | F1 warn 텍스트 미핀 |
| R6 | SoT 중복 | 소형 | BLOCKED_TICK 가드 2벌 + 선점-정의 창 |
| R7 | 잔차 검증 | 이상 없음 | 보상 degrade 3경로 전부 관측 가능 |
| R8 | 프로세스 SoT | 소형 | 측정 서사 5곳, canon 1줄 부재 |
| R9 | 규율 | 제안 | 골든 refresh 사유 의무화 |
| R10 | 배선 | open | 신설 게이트 4종 test-all 밖 |

## Related

docs/185(SoT 게이트 카탈로그) · docs/186/187(병렬 계획·판정 메모) ·
docs/security/02(위협 모델) · TODO 보드 WO-RT-4/5 · 칩 task_b4b2f972.
