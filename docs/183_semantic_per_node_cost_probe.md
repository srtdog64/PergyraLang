# 183. 네트워크-이론 축소 체크 → semantic 노드당 비용 프로브 (2026-07-13)

Status: `measured` → `work-order` (§4 실행 중)

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

## §3. 잔여·비주문 (교리-게이트)

- Pratt 사다리 치환: self-host 파서 재작성 시 기본형 (§1.1).
- AST hash-consing: 메모리 실증 후 (§1.2).
- 노드 폭발 자체(106:1)의 근치는 typed-AST 이주 — 별도 트랙,
  이 주문은 그 전에도 velocity를 사는 독립 수술.
- docs/INDEX.md 등록: 동시 스트림 잠금 해제 후.
