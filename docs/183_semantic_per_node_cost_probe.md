# 183. 네트워크-이론 축소 체크 → semantic 노드당 비용 프로브 (2026-07-13)

Status: `measured` → `executed` (1라운드 §2.5 + 2라운드 §2.6 완료,
2026-07-13; 잔여는 교리-게이트 §3)

BDFL 질문(2026-07-13): "파서에서 구문트리 만들 때 네트워크 이론을
접목해 커널처럼/sort3처럼 줄일 수 있는 게 있는지 체크할 수 있나?"
— sort3 = 3-원소 정렬 네트워크의 증명된 최소 비교기 수, 커널 =
인스턴스를 작은 핵으로 줄이는 kernelization. 이 문서는 그 체크의
실측 기록이고, 체크가 찾아낸 진짜 과녁(파서가 아니라 semantic의
노드당 비용)의 실행 주문서다.

## §1. sort3 비유의 대응물 3개 — 실측 판정

### §1.1 파서 우선순위 사다리 (sort3의 정확한 유사물) — 축소 가능, 지금 무가치

C 파서는 11층 cascade다: expression→assignment→pipe→logical_or→
logical_and→equality→comparison→addition→cast→multiplication→
unary→primary (`parser_expr.c`). 원자 하나(`42`) 파싱이 11층을
통과-호출한다. self-host 파서(`expr_precedence_owner.pgy`)도 같은
사다리(문자-레벨 cascade). 이는 "여분 비교기가 있는 정렬 네트워크"의
정확한 유사물이고, Pratt/precedence-climbing(우선순위 표 1개 + 루프
1개)으로 **의미-동일·증명-가능한 최소 네트워크** 치환이 성립한다.

**판정: 보류.** 실측(§2)에서 parse는 파이프라인의 1~2%(0.20초/21초).
줄여도 관측 불가. C-core 동결 신호와도 일치. self-host 파서를 훗날
재작성할 때 Pratt를 기본형으로 삼는 것만 설계 메모로 남긴다.

### §1.2 트리→DAG 커널화 (hash-consing) — 노드 56% 제거 가능하나 시간 범인 아님

`driver_rung2_owner.pgy`의 AST 덤프(34,665 subtree)를 구조 해시로
census: 중복 인스턴스 34.6%, DAG-공유 시 노드 56.2% 제거 가능.
그러나 상위 중복이 전부 `Returns: String`×604, `Parameters:`×429,
`Then:`/`Block:` 껍데기 같은 1~3노드 보일러플레이트다. 깊은 Concat
체인은 선형 중첩(매 층이 다름)이라 공유되지 않는다.

**판정: 메모리 커널은 되지만 시간 병목이 아니다.** 필요 실증 후
rung(메모리 압박이 실측될 때).

### §1.3 진짜 발견 — 그래프가 아니라 그래프 위의 보행 비용

단계 분해(`--tokens` / `--ast` / `--mir-json`, 2026-07-13, 게임 부하
하의 상대값):

| 파일 | 소스줄 | AST 노드(덤프 줄) | lex | parse | semantic+MIR |
|---|---|---|---|---|---|
| driver_rung2_owner | 327 | **34,666** | 0.05s | 0.20s | **21.0s** |
| routine_lower_owner | 557 | 13,411 | — | — | 6.4s |
| stmt_owner | 517 | 3,671 | — | — | 2.5s |

두 겹의 병리:

1. **노드 폭발 106:1** — 소스 327줄이 AST 34k 노드. self-host
   text-munging 스타일(중첩 Concat 체인)의 대가. 이건 typed-AST
   이주(자기-인식 linchpin)의 기존 논거를 정량화한 부수 증거.
2. **노드당 비용 ~0.3→0.6ms, 규모와 함께 상승** — 정상 semantic
   패스의 100배+. 노드 수가 커질수록 노드당 비용이 오르는 모양은
   전형적인 "노드마다 side-table 선형 스캔"(O(n·m)) 또는 "노드마다
   subtree 재보행"(선형 체인에서 O(n²)) 시그니처다. side-map
   hash화(3707c01a, 17.8×)로 잡았던 그 클래스.

**이 노드당 비용이 self-host 치환 velocity(docs/self_hosted/16)의
실병목이다** — .pgy 파일 하나가 semantic까지 21초면 반복 루프가
죽는다. compare 게이트 수술(b06e0d3c)이 케이스당 상수를 잡은 것과
같은 계보의, 컴파일러 안쪽 판.

## §2. 실행 주문 (WO-SEMPERF-1)

측정-먼저 규율로 범인을 격리한 뒤에만 수술한다.

1. **단계 이분**: `--ast` → `--hir` → `--dir` → `--rir` → `--mir`
   각 플래그의 wallclock으로 21초가 사는 단계를 특정 (플래그가
   파이프라인을 그 IR까지 실행하므로 계측 코드 불요).
2. **합성 스케일링 프로브**: 선형 Concat 체인 깊이 N =
   100/200/400/800 파일을 생성해 특정된 단계의 시간을 적합 —
   기울기 ~4×/배증이면 O(n²) 확정, ~2×면 살찐-상수 O(n·m).
3. **코드 범인 특정**: 후보 클래스 — ① 노드당 subtree 재보행
   (`infer_expression_type_name`류가 이진 노드마다 재귀 —
   선형 체인에서 O(depth²)) ② 심볼/typed-entry 선형 배열 lookup
   ③ source-shape/fact 행 선형 결합. 프로브 결과가 지목하는 것만.
4. **수술 원칙**: 조회를 O(1)로(해시/메모), 보행을 1회로(캐시된
   타입 주석). 알고리즘 치환이지 캐시-계층 발명이 아님 — §8
   cosmetic abstraction 금지 준수. 트윈 영향 없음(semantic 단일
   소유, 양 백엔드 공유).
5. **목격자**: 위 3파일 표의 재측정(목표: driver_rung2 21s → 한
   자릿수 초) + 배터리 무회귀(semantic 2794 / transpile 918 /
   parser) + 기존 게이트 전부.

## §2.5 실행 결과 (2026-07-13 당일) — 1라운드: mir_lower의 7.6초

격리 사슬(전부 실측, 가설은 두 번 기각):

1. 단계 이분(`PGY_DEBUG_PIPELINE_STAGE` 타임스탬프): semantic 3.1s +
   **mir_lower 6.8s**, 나머지 단계 전부 ~0.07s.
2. 신설 `PGY_DEBUG_MIR_TIMING`(mir_lower 13 하위-슬롯 + 루틴별 티커,
   영구 env-게이트 관측성): **build_blocks 3.5s + stmt_instructions
   4.0s** = 95%. 단일 괴물 루틴 없음 — 블록당 ~1ms 상수가 전면.
3. 기각 1: 합성 스케일링(깊이/폭/문장/스코프) 전부 선형 — 합성이
   실물 미재현(--hir로 재서 mir_lower 미측정이었던 것도 자백).
4. 기각 2: 명령당 surface-usage AST 워크 4회 — skip-프로브로 무죄
   (끄고도 불변).
5. **체포**: `bb/phi_terminator` 3.59s → terminator의
   `mir_instruction_capture_source_provenance` →
   **`ast_capture_inline`이 호출마다 디스크 `tmpfile()` 생성 + stdout
   fd dup2 왕복 + flush 4회**. Windows에서 tmpfile ≈ 1ms — 명령 수천
   개 × 1ms = 그 7.5초. skip-프로브로 확정(끄니 build_blocks 0.075s /
   stmt 0.042s).

수술: 인라인 프린터를 **파일-정적 싱크**(printf→emitf, stdout 모드는
같은 포맷 문자열로 byte-동일, 캡처 모드는 malloc 버퍼)로 전환 —
프린터 이원화 없이 tmpfile/fd 서커스 제거. 부수: 타 파일에 살던
`print_generic_params_inline`/`print_where_clause_inline`이 캡처 중
진짜 stdout으로 새는 회귀를 **8.2MB mir-json byte-비교가 적발** →
싱크 파일로 이주(ast_print_generics.c 소멸).

결과: build_blocks 3.62→**0.028s**, stmt_instructions 4.17→**0.015s**
(합 ~150×). driver_rung2 `--mir` 11.7→7.7s, routine_lower 6.4→2.1s.
파리티: mir-json 8.2MB + basic + `--ast` 덤프 전부 byte-동일. 배터리:
MIR 132/0 · semantic 2794/0 · transpile 918/0 · AIR 141/0 · parser ·
compare 3/3.

**교훈 2건**: ① 최적화 중 하나 넣은 recompute-once 가드가 MIR 테스트
2건을 깨뜨림 — stmt population이 append 후 expr0을 배정하고 **최종
재기록 패스에 의존**하는 계약이었다(가드 철회, 계약을 함수 주석으로
명문화; 프리-수술 상태 pathspec-stash 이분으로 내 회귀임을 확정).
② "본질이 싼 AST 워크"와 "syscall 낀 캡처"를 프로파일 없이 구별하는
유일한 도구는 skip-프로브 이분이었다 — gdb는 Windows에서 3연패.

잔여(2라운드): **semantic 3.1s** — `ast_capture_inline` 미호출 확인,
별도 기전. 같은 방법론(단계 이분→하위 슬롯→skip-프로브)으로.

## §2.6 실행 결과 (2026-07-13 당일) — 2라운드: semantic의 분기-스냅샷 O(분기×심볼)

격리 사슬(관측성을 한 층씩 신설하며 하강; 코퍼스는 동시 스트림이
진화시키는 중이라 src/self_hosted 전체를 동결 사본으로 고정):

1. `PGY_DEBUG_PIPELINE_TIMING` 신설(기존 `DriverPhaseTimings`를
   env-게이트 stderr로; AIR 2블록이 무계측 사각 + **316행 고아
   phase_start**였던 것도 배선): semantic 3.2s가 파이프라인의 75~80%.
   부수 적발 — **Windows NUL 리다이렉트 아티팩트**: `--mir 1>nul`
   13.7s vs 파일 4.8s. 덤프 wallclock은 파일 기준만 유효(§1.3의
   원계측 21s에도 콘솔/채널 오염이 섞여 있었다).
2. `PGY_DEBUG_SEMANTIC_TIMING` 신설, 3층 하강: semantic 3.2 =
   type_check_program 3.0 → 하위슬롯 pass2_full_check 2.4s (precollect
   0.3 / topo 0.1 / worklist 0.03) → 문장 티커 >100ms **단 1건** =
   병리적 소수 함수가 아니라 고른 노드당 상수.
3. 식 census(방문수+kind별 inclusive; **리프 kind는 inclusive==self**
   성질로 재귀 시간귀속을 우회, QPC — clock()은 1ms 해상도라 불가):
   방문 97k ≈ 노드수(재보행 아님), **식 전체 inclusive 합 0.4s → 식
   검사 무죄**. 범인은 문장-레벨 기계.
4. 문장 census: **IF 4,707방문 inclusive 5.19s**(중첩 이중계상 —
   else-if 사다리가 깊이만큼 재누적), WHILE 656방문 1.98s.
5. **체포**: if/match/loop flow가 분기마다 `snapshot_resource_states`
   최대 3회 — 각 스냅샷이 **scope 체인 전체**(프로그램 스코프 ~1천+
   심볼 포함)를 돌며 심볼마다 `semantic_classify_ownership_type`
   cascade(subject/boundary 판정 = host-decl 조회)를 재실행.
   O(분기×심볼) — §1.3이 예측한 "side-table 선형 스캔 O(n·m)"의
   실체, 위치만 식 검사가 아니라 분기 스냅샷. skip-프로브로 확정
   (classify를 끄니 pass2 2.38→0.32s).

수술: **Symbol에 type-포인터-키 memo**(`flow_tracks_memo`) — 분류는
(심볼, 타입포인터)당 1회, 재해석이 포인터를 갈면 자연 무효화.
무음-drift 공포는 **검증 모드**(`PGY_SEMPERF_VERIFY_CLASSIFY_MEMO=1`:
memo 히트마다 재계산 대조, 불일치 시 fail-loud abort)로 봉인 —
동결 코퍼스 + semantic 배터리 2794케이스 전체를 검증 모드로 통과
(divergence 0).

결과(동결 driver_rung2, 파일-리다이렉트, min-of-3):

| | 2라운드 전 | 후 |
|---|---|---|
| pass2_full_check | 2.38s | **0.30s (~8×)** |
| semantic 전체 | 3.2s | **0.77s** |
| `--mir` end-to-end | 4.3~4.8s | **1.76s** |
| `--mir-json` end-to-end | ~4.5s | **1.63s** |

§2 목표 "한 자릿수 초" 초과 달성(원계측 21s 대비 ~12×; 1라운드
tempfile 제거와 합산). 파리티: mir 덤프 11.5MB + mir-json 8.26MB
**byte-동일**. 배터리: semantic 2794/0(검증 모드 on) · MIR 132/0 ·
transpile 918/0 · AIR 141/0 · parser · compare 3/3.

교훈 3건: ① 덤프 벤치는 반드시 파일-리다이렉트로(NUL/콘솔이 3배
부풀림). ② 재귀 검사기의 시간귀속은 (a) 방문 census로 재보행/노드당
비용을 가르고 (b) 리프-kind inclusive==self로 층을 가른다 — 프로파일러
없이 결정 가능. ③ 분류-memo처럼 "무음 divergence가 무서운" 캐시는
검증 모드(재계산 대조 + fail-loud)를 같이 지어 배터리로 실증하면
공포가 게이트로 바뀐다.

신설 관측성(영구, env-게이트): `PGY_DEBUG_PIPELINE_TIMING`(드라이버
단계 테이블+AIR 2슬롯), `PGY_DEBUG_SEMANTIC_TIMING`(패스 3층 +
>100ms 문장 티커 + 식/문장 kind census), `PGY_SEMPERF_VERIFY_CLASSIFY_MEMO`
(memo 무결성 게이트).

잔여: semantic 0.77s의 최대 슬롯은 precollect ~0.3s — 측정-먼저
교리상 여기서 중단(velocity 병목 해소됨). routine_lower_owner.pgy는
동시 스트림 개편으로 현 트리에 부재 — driver_rung2 단일 코퍼스
전후로 판정.

## §3. 잔여·비주문 (교리-게이트)

- Pratt 사다리 치환: self-host 파서 재작성 시 기본형 (§1.1).
- AST hash-consing: 메모리 실증 후 (§1.2).
- 노드 폭발 자체(106:1)의 근치는 typed-AST 이주 — 별도 트랙,
  이 주문은 그 전에도 velocity를 사는 독립 수술.
- docs/INDEX.md 등록: 동시 스트림 잠금 해제 후.
