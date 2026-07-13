# 183. 네트워크-이론 축소 체크 → semantic 노드당 비용 프로브 (2026-07-13)

Status: `measured` → `executed` (1라운드 §2.5 + 2라운드 §2.6 완료,
2026-07-13; 잔여는 교리-게이트 §3)

BDFL 질문(2026-07-13): "파서에서 구문트리 만들 때 네트워크 이론을
접목해 커널처럼/sort3처럼 줄일 수 있는 게 있는지 체크할 수 있나?"
— sort3 = 3-원소 정렬 네트워크의 증명된 최소 비교기 수, 커널 =
인스턴스를 작은 핵으로 줄이는 kernelization. 이 문서는 그 체크의
실측 기록이고, 체크가 찾아낸 진짜 과녁(파서가 아니라 semantic의
노드당 비용)의 실행 주문서다.

## §0. 물리 판정: 현재 parser 산출물은 network/DAG가 아니라 ownership tree

- C `ASTNode`는 자식 포인터를 재귀적으로 `ast_destroy`한다. 공유 노드용
  refcount, visited set, interner가 없으므로 physical AST의 합법적 소유
  형태는 단일 부모 tree다.
- self-host expression arena는 여러 root와 edge를 표현할 수 있지만,
  현 parser producer는 binary/unary/call 조립 때 자식 arena를 병합
  복사한다. 따라서 현 producer 산출물도 구조 공유 없는 tree/forest다.
- 결론적으로 **네트워크 이론 기반 parser 최적화가 구현된 상태는
  아니다.** 이 문서가 실행한 것은 비유 후보의 비용 검증이며, 실제
  착지한 최적화는 MIR printer I/O 제거와 semantic 분류 memo다.

## §1. sort3 비유의 대응물 3개 — 실측 판정

### §1.1 파서 우선순위 사다리 (sort3 비유 후보) — 축소 가능, 최소성 증명 없음

C 파서는 11층 cascade다: expression→assignment→pipe→logical_or→
logical_and→equality→comparison→addition→cast→multiplication→
unary→primary (`parser_expr.c`). 원자 하나(`42`) 파싱이 11층을
통과-호출한다. self-host 파서(`expr_precedence_owner.pgy`)도 같은
사다리(문자-레벨 cascade). Pratt/precedence-climbing(우선순위 표와
루프 하나)은 이 고정 호출층을 줄일 수 있다. 다만 입력과 무관하게
비교기를 실행하는 sorting network와 입력 의존 루프인 Pratt parser는
동일한 계산 모델이 아니며, 현재 비교기 하한이나 최소성 증명도 없다.
따라서 이것은 설계 비유이지 "증명된 최소 네트워크"가 아니다.

**판정: 보류.** 2026-07-13 raw-file 재측정은 token 0.046~0.054초,
AST dump 0.197~0.227초였다. 전체가 1.95초대로 줄어든 뒤에는 parse가
약 10%까지 올라왔지만 절대비용은 여전히 약 0.2초다. self-host parser를
재작성할 때 Pratt를 후보로 삼되, parity와 실제 wallclock 이득이 먼저다.

### §1.2 dump tree 구조 interning — 이전 56.2% 수치 철회

`c724eb87`의 `driver_rung2_owner.pgy` AST dump를 ordered
`(label, child subtree ids)`로 bottom-up intern해 재측정했다.

```text
nodes=34958 edges=34957 roots=1 unique_subtrees=21664
hash_cons_removed=13294 hash_cons_removed_pct=38.0
```

기존 `56.2%`는 겹치는 duplicate subtree의 크기를 중복 합산한 값으로
재현되지 않아 철회한다. `34.6%`도 duplicate-instance 비율이 아니라
대소문자 구분 전 단순 label 집계와 섞인 값이었다. 재현 명령은
`scripts/parser_tree_network_census.sh <pgy --ast output>`이다.

또한 이 값은 printer가 만든 `Parameters:`/`Returns:`/`Then:` 같은
합성 행까지 포함한 **dump tree 압축 상한**이다. physical `ASTNode`
메모리 절감률이 아니다. 실제 AST hash-consing은 현 재귀 파괴 소유권을
refcount/interner 또는 immutable arena로 바꾸어야 하므로 침습적이다.

**판정: 구현하지 않음.** parser 시간 병목이 아니고, physical memory
census도 없다. 메모리 압박이 parser AST로 귀속된 실측이 생기기 전에는
hash-consing을 주문하지 않는다. 이것은 parameterized complexity의
kernelization도 아니며, 정확한 명칭은 structural interning이다.

### §1.3 진짜 발견 — 그래프가 아니라 그래프 위의 보행 비용

단계 분해(`--tokens` / `--ast` / `--mir-json`, 2026-07-13, 게임 부하
하의 상대값):

| 파일 | 소스줄 | AST 노드(덤프 줄) | lex | parse | semantic+MIR |
|---|---|---|---|---|---|
| driver_rung2_owner | 327 | **34,666** | 0.05s | 0.20s | **21.0s** |
| routine_lower_owner | 557 | 13,411 | — | — | 6.4s |
| stmt_owner | 517 | 3,671 | — | — | 2.5s |

위 표는 최초 탐색 당시의 역사적 관측이며 `21.0s`는 이후 확인된
console/NUL 출력 오염을 포함한다. 현재 같은 driver의 raw-file 경계는
334 source lines, 35,367 AST dump rows, token 0.046~0.054s, AST
0.197~0.227s, `--mir` 1.947~2.129s다. 서로 다른 출력 경계의 수치를
speedup ratio로 나누지 않는다.

두 겹의 병리:

1. **dump 행 폭발 106:1** — 소스 327줄이 AST dump 34k 행. self-host
   text-munging 스타일(중첩 Concat 체인)의 대가. 이건 typed-AST
   이주(자기-인식 linchpin)의 기존 논거를 정량화한 부수 증거.
2. **최초 관측의 노드당 비용 상승** — 당시 정상 semantic
   패스보다 비정상적으로 컸다. dump 행 수가 커질수록 비용이 오르는 모양은
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

## §2.6 실행 결과 (2026-07-13 당일) — 2라운드: 분기-스냅샷의 반복 분류 비용

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
   expensive classification이 O(분기×심볼)으로 반복됐다. 위치는 식
   검사가 아니라 분기 스냅샷이었다. skip-프로브로 확정
   (classify를 끄니 pass2 2.38→0.32s).

수술: **Symbol에 type-포인터-키 memo**(`flow_tracks_memo`) — 분류는
(심볼, 타입포인터)당 1회, 재해석이 포인터를 갈면 자연 무효화.
무음-drift 공포는 **검증 모드**(`PGY_SEMPERF_VERIFY_CLASSIFY_MEMO=1`:
memo 히트마다 재계산 대조, 불일치 시 fail-loud abort)로 봉인 —
동결 코퍼스 + semantic 배터리 2794케이스 전체를 검증 모드로 통과
(divergence 0). `test-semantic`은 이 env를 기본으로 켜므로 동일 검증이
기존 semantic CI 경계에 흡수된다.

정확한 복잡도 판정: memo는 expensive classifier 호출을
O(분기×심볼)에서 O(unique (symbol,type))로 줄였지만, snapshot이 scope
체인의 모든 symbol을 순회하는 O(분기×심볼) 자체는 남아 있다. 이번
수술은 알고리즘 차수를 완전히 바꾼 것이 아니라 비싼 내부 연산을
amortize한 것이다.

결과(동결 driver_rung2, 파일-리다이렉트, min-of-3):

| | 2라운드 전 | 후 |
|---|---|---|
| pass2_full_check | 2.38s | **0.30s (~8×)** |
| semantic 전체 | 3.2s | **0.77s** |
| `--mir` end-to-end | 4.3~4.8s | **1.76s** |
| `--mir-json` end-to-end | ~4.5s | **1.63s** |

§2 목표 "한 자릿수 초"는 달성했다. 통제된 2라운드 비교는
4.3~4.8s→1.76s(약 2.4~2.7×)다. 최초 21s는 console/NUL 출력 오염을
포함하므로 `21/1.76 = 12×`를 알고리즘 speedup으로 보고하지 않는다.
1라운드의 별도 통제 비교는 11.7s→7.7s였다. 파리티: mir 덤프 11.5MB + mir-json 8.26MB
**byte-동일**. 배터리: semantic 2794/0(검증 모드 on) · MIR 132/0 ·
transpile 918/0 · AIR 141/0 · parser · compare 3/3.

현재 HEAD 재검증(진화한 corpus, 같은 raw-file 경계)은 `--mir`
1.947~2.129s, output 12,220,267 bytes였다. pipeline timing 1회는
semantic 1.192s / MIR lower 0.271s / total 1.941s였고, memo 검증 모드는
3.972s에 같은 MIR bytes를 냈다. 원 기록의 1.76s/0.77s와 완전히 같은
고정 corpus·machine 표본은 아니므로 회귀 판정이 아니라 효과의 현재
재현으로만 취급한다.

2026-07-14 계측 감사에서 두 사각을 추가로 닫았다. 기존 pipeline
`total`은 teardown 직전에 기록되어 MIR/HIR/AIR/AST 파괴와 stdout flush를
제외했고, semantic 하위표는 stable-ID 부여와 host-decl index 구축을
제외했다. 이제 debug `total`은 stdout 파일 경계 flush와 compiler teardown
뒤에 기록하고, semantic 표는 `identity_host_index` 슬롯을 별도로 낸다.
따라서 위 2026-07-13 수치는 동결 코퍼스의 역사적 전후 비교로 유효하지만,
당시 pipeline `total`을 프로세스 end-to-end와 동일시하지 않는다. wallclock
회귀 판정은 계속 외부 파일-리다이렉트 측정을 기준으로 한다.

교훈 3건: ① 덤프 벤치는 반드시 파일-리다이렉트로(NUL/콘솔이 3배
부풀림). ② 재귀 검사기의 시간귀속은 (a) 방문 census로 재보행/노드당
비용을 가르고 (b) 리프-kind inclusive==self로 층을 가른다 — 프로파일러
없이 결정 가능. ③ 분류-memo처럼 "무음 divergence가 무서운" 캐시는
검증 모드(재계산 대조 + fail-loud)를 같이 지어 배터리로 실증하면
공포가 게이트로 바뀐다. 여기서는 기존 `test-semantic`이 검증 모드를
켜고 `perf-contract`가 그 wiring과 census 계산기를 ratchet한다.

신설 관측성(영구, env-게이트): `PGY_DEBUG_PIPELINE_TIMING`(드라이버
단계 테이블+AIR 2슬롯), `PGY_DEBUG_SEMANTIC_TIMING`(패스 3층 +
>100ms 문장 티커 + 식/문장 kind census), `PGY_SEMPERF_VERIFY_CLASSIFY_MEMO`
(memo 무결성 검증 모드; `test-semantic`에서 상시 활성).

잔여: semantic의 다음 후보는 precollect였지만 현재 HEAD에서 다시
분해하기 전 수치 고정은 금지한다. 또한 memo key는 type pointer만
포함하지만 classifier는 type의 kind/name/nominal flavor와 host-decl
index도 읽는다. 현 lifecycle은 host index를 snapshot 전에 만들고 symbol
type 재해석은 포인터 교체로 수행하며 verify mode가 현재 corpus의
불변성을 확인했다. 향후 Type을 같은 주소에서 mutate하거나 host index를
재구축하는 패스가 생기면 semantic classification epoch를 key에 추가해야
한다. scope 전수 순회를 없애는 별도 tracked-symbol inventory는 실제
병목이 다시 측정될 때만 주문한다.

## §3. 잔여·비주문 (교리-게이트)

- Pratt 사다리 치환: self-host 파서 재작성 시 기본형 (§1.1).
- AST hash-consing: 메모리 실증 후 (§1.2).
- 노드 폭발 자체(106:1)의 근치는 typed-AST 이주 — 별도 트랙,
  이 주문은 그 전에도 velocity를 사는 독립 수술.
- docs/INDEX.md 등록: 동시 스트림 잠금 해제 후.
