# 187. 병렬 트랙 BDFL 결정 메모 2건 — B4(fiber 통합), Form B(every/continuous) (2026-07-17)

WO-RT-4 종결(docs/186 진행 2) 시점에 남은 두 착수-금지 항목의 **결정
준비 자료**다. 이 문서는 결정하지 않는다 — 선택지·증거·권고(표기)만
고정하고, 판정은 BDFL 몫. 등록 근거: docs/186 P-C1의 첫 단계가 "결정
메모 초안 작성"이고, B4는 "B1~B3 측정치가 나온 뒤 BDFL 결정"이었다 —
그 측정치가 이제 있다.

## 메모 1. B4 — 잠자는 2호 스케줄러(fiber)의 LOCAL_ASYNC 편입 여부

**질문**: `src/runtime/async/scheduler.c`(per-worker 큐 + steal + fiber
+ epoll IO, 완성돼 있으나 호출자 0)를 LOCAL_ASYNC lane의 실행기로
채택할 것인가.

**B1~B3가 바꾼 증거 지형** (전부 실측, benchmarks/PARALLEL_RESULTS.md):

- **compute 축에서 fiber의 성능 논거는 소멸했다.** fine-grain 64x
  열위의 진범은 스케줄러 품질이 아니라 producer 임계경로(태스크당
  부기 × N)였고, B3 auto-chunk가 34x로 닫았다(2,174–2,596→64–71ms).
  큐 경합 가설(B2 shard로 검증)·park/wake 가설(spin 실험)은 반증.
  중첩 fork-join은 help-first await로 OpenMP-parity. 즉 **"스케줄러를
  fiber로 바꾸면 빨라진다"는 워크로드를 현재 갖고 있지 않다.**
- **남는 잠재 수혜 축은 IO-동시성이다**: 수천 태스크가 채널/blocked
  send로 동시에 파킹하는 형태(던전크롤러 서버형). THREAD 모델은
  파킹당 OS 스레드를 소비하지 않지만(태스크는 큐에 있고 worker만
  파킹), **채널 대기 중인 태스크는 worker를 점유**한다 — worker 수를
  넘는 동시 채널-파킹이 정당한 워크로드로 등장하면 fiber가 실제
  해법이 된다. 오늘 그 워크로드의 실측 표본은 없다.

**선택지**:
- **O1. 현상 유지 (evidence-gated 보류)** — LOCAL_ASYNC는 현
  coroutine 스캐폴드, scheduler.c는 잠자는 채로. 비용 0, 리스크 0.
  단 "완성돼 있으나 미배선" 코드가 계속 오진 유인으로 남는다
  (docs/186 §1이 이미 경고 주석으로 완화).
- **O2. lane-국소 채택** — scheduler.c를 LOCAL_ASYNC 실행기로 배선
  (M:N은 evidence-gated 한 lane이라는 교리 그대로). 선행 조건 제안:
  ① IO-파킹 witness 워크로드(예: worker수 << 동시 채널-파킹 태스크
  수) RED 실증, ② SEA 관측-동등 계약(실행기 교체가 결과를 못 바꿈)
  게이트, ③ cancel/budget/capability 통합 비용 견적. Windows fiber
  경로(SwitchToFiber)는 이미 코루틴에서 사용 중이라 신규 아님.
- **O3. 제거** — scheduler.c를 지워 이중-실행기 표면 자체를 없앰.
  IO-동시성 축을 나중에 다시 지어야 하는 비용과 맞바꾼다.

**권고(구속력 없음)**: O1 유지 + O2의 선행 조건 ①(IO-파킹 witness)만
먼저 지어 두기 — witness가 RED면 O2 착수 근거가 생기고, GREEN이면
fiber 없이도 충분하다는 증거가 된다. 어느 쪽이든 결정이 측정 위에
선다. (O3는 IO 축 실증 전엔 정보 손실.)

**★선행 조건 ① 충족 (2026-07-17, 메모 작성 당일)**:
`tests/channel_pool_starvation_probe.sh`가 착지했고 **양 백엔드 RED**다
— PGY_WORKERS=2에서 수신 arm 7개가 송신 arm 7개보다 먼저 큐에 서면
worker 2 + help로 들어간 main까지 전부 채널 recv에 파킹, 송신자는
영원히 큐에 남는다(10s timeout, control은 GREEN). help-first await가
이 클래스를 못 닫는 이유도 실증됨: help는 큐 태스크를 이 스레드에서
**완주**시키므로, 수신자를 help하면 helper도 함께 파킹한다. 즉 O2의
착수 근거가 생겼다 — 단, 대안 후보 "채널 대기 안 help"는 fork-join과
달리 채널 의존이 비순환이 아니라 help 중첩 깊이가 큐 깊이까지 자랄
수 있음(스택 성장)을 함께 검토할 것. probe는 상태-단언형(고쳐지면
exit 1로 "게이트로 승격하라"를 요구)이라 fix가 무음으로 지나가지
못한다. 보드 WO-RT-5로 등록.

## 메모 2. Form B — `parallel on (lane) { every / continuous }` 판정 3건

docs/182 §3 원문 그대로, 판정 없이 착수 금지인 3건:

**판정 1. 시작 의미론** — 블록 도달=암시 시작 vs 명시 시작 API.
- 암시(도달=시작): 표면 최단, Flash-류 콘텐츠 루프와 정합. 단
  "선언인가 실행인가"의 구두점-register 원칙(cheatsheet §1)과 긴장 —
  `parallel every`가 문장이면 실행, 세계-층이면 선언이어야 일관.
- 명시(`let t = ...; t.Start()` 류): 시작 시점이 코드에 보여
  fail-close·budget 심사 지점이 명확. 표면 1줄 손해.
- 판정에 걸린 것: 반응형 블록이 **code-층 문장**인지 **world-층
  선언**인지의 언어 정체성 — 구두점 원칙과 함께 판정해야 함.

**판정 2. 취소 표면** — 핸들 반환(`let t = parallel every…`) vs 스코프
종속(둘러싼 zone/스코프 종료 시 자동 stop).
- 핸들: 명시적 stop/cancel, 이미 있는 태스크 cancel 사다리 재사용.
  누수 위험(핸들 버리면 영원 루프) — budget이 backstop.
- 스코프 종속: 구조적 동시성(누수 불가능), zone 의미론과 정합.
  장수 루프(게임 메인 루프)는 최상위 스코프에 묶는 관용구 필요.
- 판정에 걸린 것: 던전크롤러 use-case의 기본형 — 콘텐츠 sandbox에선
  "스코프 밖으로 살아남는 루프 없음"이 신뢰 스토리에 더 부합.

**판정 3. 단일 lane 규약** — worker-pool 고정(§2.5 R2)인지, role
소유(R4)까지 보류인지.
- worker-pool 고정: 즉시 구현 가능, lane fact는 상수. 단 every 루프가
  worker 하나를 상시 점유(위 B4 메모의 IO-파킹 논점과 동일 뿌리).
- role 소유 대기: 표면 재작업 없이 lane 증거 모델로 확장 가능하나
  R4 착지까지 Form B 전체가 밀린다.

**교차 제약(이미 고정된 것)**: 실행 모양은 §3 원문(가상모드
advance-브로드캐스트, compare 게이트는 가상모드만), Duration 산술이
선행 잔여(P-C2), §2 취소 사다리는 착지됨. B3의 chunk 정책은 §6
선고정 제약("fold 모양=(n, 선언된 정책)의 함수, 스케줄-적응형 금지")을
준수했음을 기록해 둔다 — Form B 실행기도 같은 제약 아래 설계할 것.

**권고(구속력 없음)**: 판정 1은 구두점-register 원칙과 묶어 "world-층
선언 + 명시 시작"이 언어 정체성에 정합, 판정 2는 킬러 유즈케이스
기준 "스코프 종속 기본 + 핸들은 opt-in", 판정 3은 "worker-pool 고정
후 role은 R4에서 재방문"이 최소-후회 경로로 보인다. 셋 다 반례가
있으면 뒤집는 게 맞고, 이 문서는 그 논쟁의 좌표만 고정한다.

## Related

docs/186(계획·종결 기록) · docs/182 §3/§6(원문 제약) · docs/181(표면
설계) · benchmarks/PARALLEL_RESULTS.md(측정) · TODO 보드 WO-RT-4 잔여.
