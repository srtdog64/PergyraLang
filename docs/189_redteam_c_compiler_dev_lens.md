# 189. 레드팀 리뷰 2차 — "베테랑 C 컴파일러 개발자" 렌즈 (2026-07-18)

docs/188(골든/SoT 감사)의 후속. 이번 렌즈는 **GCC/Clang을 만들어 본 C
컴파일러 개발자가 Pergyra 구현을 뜯어보면 어디를 먼저 찌르는가**다.
6축 병렬 감사(방출 UB 카탈로그 / 런타임 메모리모델 / 컴파일러 프로그램
견고성 / ABI·백엔드 drift / 컴파일 확장성 / 테스트 갭) + 직접 검증으로
작성했다.

**방법론 정직 기록**: 감사 후보 1건은 착지 전 직접 검증에서 **기각**됐다
— "LLVM 백엔드가 `&&`/`||`를 eager 평가한다"는 주장은 허위 양성이다.
`llvm_emit_binary`가 TOKEN_AND/OR을 전용 단락평가 경로(조건분기+phi,
`llvm_expr_scalar_core.c:327-359`)로 먼저 처리하므로 552행의 eager
`LLVMBuildAnd`는 **도달 불가한 죽은 switch arm**이다(잔여는 C3에 흡수).
아래 표의 ★는 이 세션에서 코드 라인을 직접 재확인한 발견, 나머지는
감사 에이전트가 file:line 스니펫으로 실증한 발견이다.

**범위 주의**: working tree 기준이며, 유저의 CI 작업으로 dirty한 파일
(Makefile, pgy_parallel.h, pgy_parallel_pool_lifecycle.h,
perf_contract_smoke.sh 등)은 as-is로 읽었다. docs/179 원장에 이미 등록된
약점(CI red, 행동 목격자 얇음, 증분 컴파일 부재, 자기적용 갭, 사용자
실증 0)은 재발견하지 않았고, 이 문서는 **원장에 없는 새 지점**만 싣는다.

## 종합 판정 (C 컴파일러 개발자의 한 줄)

"코어는 생각보다 훨씬 단단하다 — seq_cst eventcount는 ARM에서도 정식으로
옳고, realloc 오용 0, 성장 경로 전부 SIZE_MAX 가드, argv-배열 exec,
strict-aliasing은 구조적으로 닫혀 있다. 그런데 **fail-closed를 팔면서
Float→Int가 무가드**고, **LLVM 레그의 `.bc` 인라인 구성은 로컬에서만
켜지고 CI는 그 구성을 한 번도 검증하지 않으며**, **semantic 검사기는
파서가 가진 재귀 가드조차 없다**. 자기 교리를 자기 코드에 다 적용하지
않은 지점들이 급소다."

---

## Tier A — 방출 코드 의미론 (fail-closed 서사와의 모순)

### C1. ★ Float→Int 변환 무가드 — 양 백엔드 UB/poison

- **증거**: C 백엔드 `AST_CAST`가 `((int32_t)(operand))` 무가드 방출
  (`transpiler_expr_dispatch_emit.c:350-365`). LLVM은 bare
  `LLVMBuildFPToSI`(`llvm_expr_aggregate.c:579,588`,
  `llvm_expr_scalar_core.c:221`, store-coercion 다수 사이트).
- **공격**: 범위 밖 float→int는 C11 6.3.1.4 **하드 UB**(-fwrapv/-fno-
  strict-aliasing 무관), LLVM에선 **poison**. NaN 캐스트 한 줄이면 재현.
  div/mod가 세운 fail-closed 서사와 정면 모순이고, 산술 UB 2-레이어
  모델(docs 산술 원장)이 "닫았다"고 기록한 표면 옆의 열린 문이다.
- **수리**: `pgy_checked_fptosi_i32/i64` helper를 checked_div와 동일
  패턴으로 양 백엔드에 (strip-목록 등재 포함), CheckedArith.v에 정리
  추가, `runtime_panic_codegen_smoke`에 NaN/±Inf/1e30 케이스.

### C2. 정수 `+`/`-`/`*` overflow = 무음 wrap — 의도된 결정이지만 3중 잔여

- **증거**: C는 raw 연산자 + 드라이버 `-fwrapv`(`compiler.c:210`, 의도
  주석 206-209), LLVM은 nsw 없는 plain add/sub/mul
  (`llvm_expr_scalar_core.c:509-520`, 트리 전체에 `BuildNSW*` 0). 즉 양
  백엔드 정합 wrap이고 divergence는 없다. checked add/mul은 **opt-in
  빌트인**(`checkedAdd`/`checkedMul`)로만 존재. CheckedArith.v 증명
  범위는 **div/mod 2개뿐**.
- **공격**: ① **산출물 이식성** — 방출된 `.c`는 `-fwrapv` 없이 plain
  `gcc -O2`로 재컴파일하는 순간 signed-overflow UB로 전락한다. 안전이
  방출 소스의 속성이 아니라 **드라이버 플래그의 숨은 속성**이다. ②
  **도메인 정합** — 결제/도메인-정확성을 파는 언어에서 `Int` 곱셈
  overflow가 무음 wrap인 기본값은 C# 아버지(`checked` 블록)와도, 이
  레포의 fail-closed 교리와도 어긋나는 지점이라 재판정 가치가 있다. ③
  **서사** — "산술 UB 닫음"이 div/mod 한정임이 원장에 명시돼야 한다.
- **수리(판정 필요)**: 최소=방출 `.c` 상단에 `#if !defined(__GNUC__) ||
  !wrap-보장` 컴파일 방벽 주석/검사 + 원장 서사 정정. 최대=+,-,*를
  checked로 승격(성능 실측 후 BDFL 판정 — 기존 checked helper와 게이트
  인프라가 이미 있어 증분 비용은 낮다).

### C3. ★ C 배열접근 fail-open 폴백 — 무검사 `arr[i]` else

- **증거**: Array/Slice 분기는 메타데이터 없으면 fail-closed 에러인데,
  그 외 수신자 타입은 최종 else가 **무검사 raw 인덱싱**을 방출
  (`transpiler_expr_array_access_emit.c:101-103` — `strdup_fmt("%s[%s]")`).
- **공격**: 타입추론 갭으로 실 인덱싱 가능 타입이 이 분기에 흘러들면
  경계검사 없는 OOB. 도달 가능성은 미확정이나, **이웃 분기 전부가
  fail-closed인데 이 분기만 무음 통과**인 형태 자체가 no-hidden-flow
  교리 위반(backend_fail_closed_smoke가 잡는 클래스와 동류).
- **수리**: else를 `PGY_CODE_C_TYPE_UNSUPPORTED` 백엔드 에러로 전환
  (이웃과 동일 형태). 동류 정리: LLVM의 도달불가 TOKEN_AND/OR eager
  arm(`llvm_expr_scalar_core.c:551-554`)도 error case로 전환.

---

## Tier B — `.bc` 트윈 클러스터 (증폭기: 로컬은 켜고 CI는 끈다)

**공통 전제**: 런타임 비트코드 인라인은 수동 `make runtime-bc`로만
생성되고 **CI는 한 번도 만들지 않는다**(ci.yml에 runtime-bc 참조 0).
반면 로컬(BDFL 머신)은 twin-lockstep 규율로 `.bc`를 상시 재생성한다.
즉 **로컬 LLVM 레그와 CI LLVM 레그는 다른 구성을 테스트 중**이고,
아래 4건은 전부 "위험한 쪽 구성이 무감시"라는 공통 증폭기를 갖는다.

### C4. ★ `.bc` 컴파일 플래그 divergence — `-fwrapv`/`-fno-strict-aliasing` 부재

- **증거**: `scripts/build_runtime_bc.sh:80-83` — clang `-O2 -std=c11` +
  define 미러만 있고 **두 `-f` 플래그가 없다**. 직전 주석(78-79)은
  "ABI-identical" 주장. 네이티브 런타임 오브젝트와 방출 C는 둘 다 두
  플래그를 받는다(`compiler_llvm.c:249-250,278-279`, `compiler.c:210-213`).
- **공격**: `.bc`에 인라인되는 미스트립 primitive(문자열/인덱스 산술
  등)만 signed-overflow UB + TBAA 활성 상태로 clang -O2 최적화를 통과
  → **같은 소스가 LLVM(bc-on) 레그에서만 다른 결과**를 낼 수 있는
  백엔드-선택적 무음 divergence.
- **수리**: 빌드 스크립트에 두 플래그 추가(1줄) + "define만이 아니라
  codegen 플래그도 미러" 주석 정정.

### C5. ★ 가드-폴딩 strip 목록 갭 + 생존 게이트 부재

- **증거**: strip 술어(`llvm_runtime_attrs.c`)는 checked-arith/경계검사
  접근자/lifecycle 2종/capability 4종/budget 6종/panic. **zone_authority
  ·clock 계열은 무매치**(grep 0). 그런데 `pgy_zone_authority_check_export`
  류는 fail-closed 경로에 `PGY_RUNTIME_PANIC`(inline fprintf+abort,
  `pgy_runtime_lib_authority_file_core.h:219-235`)을 갖고 있다 — attrs.c
  주석이 "mis-lower + access violation"이라 기록한 바로 그 폴딩 클래스.
- **공격**: `.bc` 링크 시 authority 검사의 abort 본문이 인라인
  최적화를 통과한다. 과거 slice_get/list_get_oob 회귀와 같은 클래스인데
  현재 무방비. 그리고 **가드 생존을 단언하는 게이트 자체가 없다** —
  기존 panic 스모크/backend-compare는 전부 `.bc` 없는 경로만 돈다.
- **수리**: ① zone_authority/clock export를 strip 술어에 등재. ② CI에
  `.bc`-on 잡 1개(backend-compare 샤드 하나라도 `make runtime-bc` 후
  실행) — 이거 하나로 Tier B 전체가 무감시에서 벗어난다. ③ 신규 가드
  등재 누락을 잡는 구조 게이트(런타임 헤더의 `PGY_RUNTIME_PANIC` 사용
  export 목록 ⊆ strip 술어 목록 grep 게이트).

### C6. ★ `.bc` freshness가 불완전 dep 목록 기반 — stale-but-fresh

- **증거**: `llvm_runtime_bitcode_freshness.c:42-70` — 하드코딩 deps[]
  ~26개 vs `src/runtime` 헤더 122개. **`pgy_parallel_chunk.h`·
  `pgy_parallel_pool_lifecycle.h`(이번 병렬 캠페인의 신설 파일)도
  미등재** — chunk 정책만 고치면 `.bc`는 "fresh" 판정으로 옛 chunk
  코드를 인라인한다.
- **공격**: dev-pain 원장의 "stale `.bc`가 UB를 무음 재개방" 사건의
  기제 확정판. mtime 검사가 통과하는 stale이라 기존 규율(재생성 습관)
  로도 못 잡는 각도가 있다.
- **수리**: deps[] 나열 대신 `src/runtime` 디렉토리 글롭 전체 mtime
  비교(또는 clang -MM 의존성 파일). 나열 유지 시 신설 헤더 등재를 잡는
  grep 게이트 필요.

### C7. 단일-인스턴스 가드 미확장 — clock/zone-TLS/pool flags

- **증거**: cap/budget은 `PGY_RUNTIME_BC_BUILD` 가드로 단일 인스턴스화
  완료(`authority_file_core.h:279-283,341-345` — R6 정비의 결과). 그런데
  같은 클래스인 **가상 clock**(`static atomic_llong`,
  `pgy_runtime_lib_clock_core.h:6-7` — "single-instance state lives
  export-side" 주석과 자기모순), **zone-authority TLS last-error 5종**
  (`authority_file_core.h:59-64`), **pool active/shutting 플래그**
  (`pgy_parallel.h:305-306`)는 무가드로 `.bc`에 딸려 들어가 이중
  인스턴스가 된다(해당 함수들 미스트립).
- **공격**: bc-on 구성에서 clock advance와 now가 서로 다른 복사본을
  칠 수 있음(가상시간 desync — Form B 가상모드 compare 게이트의 기반
  침식). cap/budget에만 적용하고 멈춘 절반 수리.
- **수리**: 동일 가드 패턴 확장 + "런타임 파일-스코프 가변 전역 전수
  → 가드 or 스트립 or 명시 예외" census 1회(WO-RT-1 census와 병합 가능).

---

## Tier C — 컴파일러 '프로그램' 견고성

### C8. semantic flow 검사기에 종료 계약이 없다 (파서 400 vs semantic 0)

- **증거**: `type_check_block_flow` → dispatch(AST_BLOCK/IF/MATCH/WHILE)
  → 상호재귀(`type_checker_flow.c:150-262`)에 depth 가드 부재. 파서는
  400캡(`parser.c:19`)+연산자 4096캡을 갖췄다. census는 **가드가
  아니라 `PGY_DEBUG_SEMANTIC_TIMING` env-게이트 print-only 계기**임이
  확정(`type_checker_flow.c:49-74`) — 릴리즈 기본값에선 semantic에
  어떤 방벽도 없다.
- **맥락 정합**: 2026-07-14~15의 semantic hang 사건(원인=지수적
  escape-분석 재귀, gdb 확정)은 동시 세션 refactor로 **해소됐지만**,
  그 사건이 바로 "semantic 종료 보장 없음" 클래스의 실증이었고 **구조
  갭 자체는 그대로 남아 있다**. 다음 사건의 구조적 후보: ① 캡 없는
  상호재귀의 스택 고갈(깊은 중첩 = crash — 파서 400캡이 semantic엔
  없음), ② 분석-상태 결합 finder 루프(`semantic_find_next_*`의
  past_after 커서 — host_decl_index가 재진입 중 변형되면 진행성 붕괴
  가능, `type_checker_domain_role_lookup.c:58`).
- **수리**: 파서와 동일한 enter/leave depth 가드(진단으로 fail-close)
  + statement-flow 전역 fuel(census를 print에서 budget-abort로 승격 —
  hang 사건 9라운드가 증명했듯 사후 격리 비용이 막대하므로 사전
  방벽이 정답). 게이트: C13의 adversarial corpus가 이 가드를 실행한다.

### C9. LSP 생존성 — 컴파일러의 exit/abort가 서버를 통째로 죽인다

- **증거**: `ast_create_node`의 예산 초과 `exit(1)`/OOM `abort()`
  (`ast_constructors.c:87,93`)가 LSP의 요청-내 파싱에서 그대로 실행됨.
  LSP는 영속 프로세스(`pgy_lsp.c:210` while(1))이고 요청별 teardown은
  깔끔하지만 in-process 격리가 없다. 추가로 didChange마다 디바운스
  없이 풀 재분석 + advisory ON(`semantic_analyze_ex(ast,true)` —
  배치보다 비싼 경로, `pgy_lsp_diagnostics.c:182`).
- **공격**: 병리적 편집 버퍼(>1M 노드) 하나로 언어 서버 전멸. 배치
  컴파일러엔 옳은 bounded-refusal이 영속 서버엔 DoS.
- **수리**: 예산 초과를 파서 에러 경로(진단)로 강등하고 exit는 배치
  전용 래퍼로; LSP에 디바운스/코얼레싱. (semantic-squiggle HUD 축의
  기반 안정성 항목이기도 함.)

### C10. 파서 단일-에러 모델 + OOM 미검사 18사이트 + NUL 무음 절단

- **증거**: `parser_error`가 sticky `has_error`로 **파일당 진단 1개**
  (`parser.c:160-163`, 리셋 없음). 중앙 `ast_create_node`는 검사되지만
  **raw `calloc(1,sizeof(ASTNode))` 18사이트가 검사·예산 둘 다 우회**
  (`parser.c:500`, `parser_decl.c:164`, `parser_statement_dispatch.c:142,
  371,387` 등 — OOM 시 즉시 deref). 렉서는 길이 무보관 NUL-종료라
  **소스 내 embedded `\0` 이후 코드가 무음으로 사라진다**(`lexer.c:88`).
- **공격**: ① 에러 UX가 C 컴파일러 표준(다중 진단) 미달 — 진단 품질을
  파는 언어의 역설. ② NUL 절단은 리뷰/공급망 각도(디스크엔 있는데
  컴파일러엔 안 보이는 후행 코드)까지 있는 정확성 결함.
- **수리**: ① sync 지점에서 has_error 리셋 + max-errors 캡(전형 20).
  ② 18사이트를 `ast_create_node`로 수렴(grep 게이트로 재유입 차단).
  ③ 렉서에 길이 전달, NUL 발견 시 진단.

---

## Tier D — 방출 위생·표면 계약

### C11. C-키워드/식별자 escape 부재 — self-host와 역-parity

- **증거**: bootstrap C 백엔드 식별자 폴스루가 raw 방출
  (`transpiler_expr_dispatch_emit.c:255`). `restrict`/`register`/`union`
  등은 Pergyra 키워드가 아니라서 합법 식별자 → 방출 C가 깨진다.
  **self-hosted 컴파일러는 이미 escape한다**
  (`symbol_table_owner.pgy:97-142`, `pgy_` prefix) — 치환-대상이
  치환-원본보다 옳은 역-parity. 유저 식별자가 `_pgy_`/`_pj_` 예약
  prefix를 쓸 때의 충돌도 무거절.
- **수리**: bootstrap에 동일 예약어 escape + 예약 prefix 거절(또는
  전 유저 심볼 일괄 prefix). parity fixture 1개(변수명 `register`).

### C12. ★ Channel 복사면 미완 — `let c2 = ch`가 열려 있다 (WO-RT-6 확장)

- **증거**: Channel의 ownership 분류가 fallthrough **COPY_ONLY**
  (`type_checker_ownership_classify.c:25`) → 로컬 복사 합법. 복사면
  전수: param **열림**(WO-RT-6 등록), **let-복사 열림·미등록**(param과
  동일한 무음 중복수신), return **미판정**(owner-handle만 검사,
  `type_checker_func_param_contract.c:6-17`), field **닫힘**
  (`type_checker_call_constructor.c:24-30`), 클로저 캡처 **닫힘**
  (Stage A 허용목록, `type_checker_lambda_capture.c:133`).
- **공격**: WO-RT-6(param 거절)만 착지하면 `let c2 = ch;` 한 줄로 같은
  버그가 그대로 재생산된다 — 면 단위 패치가 아니라 **표현(representation)
  판정**이 필요하다.
- **수리(판정 필요)**: A안=TextBuilder 선례처럼 4면 동시 계약(owner-let
  +param+return+rebind 거절). B안=디스크립터를 heap 핸들로 승격 — 복사
  =별칭이 되어 클래스 자체가 소멸(유저 기대 의미론과도 일치)하되 수명
  스토리(해제 시점) 필요. C 컴파일러 개발자 권고는 B(값-타입 동기화
  객체는 표현 오류)나, fail-close 교리와의 절충은 BDFL 몫.

---

## Tier E — 검증 인프라 (컴파일러 팀 표준 대비)

### C13. 동시성 검증망 부재가 최대 리스크-집중 지점

- **증거**: **TSan 참조 레포 전체 0**. `make test-asan`(ASan+UBSan,
  누수검출, 120s 행 가드)은 존재하나 **CI 미배선**(ci.yml 참조 0),
  valgrind 타겟도 미사용. 병렬/채널 게이트는 backpressure 64회 반복
  외 대부분 단일 결정 실행.
- **공격**: 가장 활발히 바뀌는 서브시스템(병렬 런타임)에 레이스
  검출망이 가장 얇다. 이번 감사가 잡은 **표본 2건**이 그 증거: ①
  `g_pgy_cancel_probe` 평문 함수포인터 = 형식적 C11 데이터 레이스
  (`pgy_runtime_cancel_probe.h:21`, 쓰기 `pgy_parallel.h:237,534` vs
  채널 대기 스레드 읽기 — x86에서 benign이나 표준상 UB) ② **blocking-
  pool 보상 오귀속**: `g_pgy_pool_task_depth` TLS가 두 풀 공용인데
  tick은 무조건 `g_pgy_pool`에 스폰(`pgy_parallel_pool_lifecycle.h:
  220-236`) — blocking-pool 태스크가 채널로 상호 의존하면 보상이
  엉뚱한 풀로 가서 기아 미해소(WO-RT-5 fix의 사각).
- **수리**: ① Linux CI에 test-asan 잡 1개(타겟 이미 존재 — 배선만).
  ② `PGY_SANITIZERS=thread` 변형 1개를 병렬/채널 게이트 8종+
  backpressure 루프에. ③ cancel_probe `_Atomic`화(1줄). ④ tick에
  풀-소속 인지(depth TLS를 풀별로 or task에 풀 태그).
- **반증거(기록)**: 코어 eventcount는 4연산 전부 seq_cst로 **ARM 포함
  정식 무결**(lost-wakeup 불가 증명 구조), 락 순서 전역 무순환, teardown
  게이트 정확, panic-under-lock 부재 — 런타임 코어 자체는 이 레포에서
  가장 잘 지어진 축에 속한다.

### C14. 측정·캠페인 갭 — 컴파일 속도는 무측정, fuzz는 미가동

- **증거**: ① `tests/perf_contract_smoke.sh`(HEAD 기준; 현재 유저
  편집 중)는 **하드코딩 가짜 시각**(`compile=0.500s`)을 로그에 쓰고
  summary **포맷만** grep — 실제 가치는 fail-open 폴백 grep이고 컴파일
  속도 임계는 0개. 단계별 타이밍 계기(`PGY_DEBUG_PIPELINE_TIMING`,
  14단계)는 있으나 무게이트. ② fuzz: `fuzz_parity.py`(수동 전용) +
  self-host 생성기 oracle-on matrix 타겟(`Makefile:2793-2802`)이
  존재하나 **CI는 생성기-안정성 스모크 8케이스만** — 랜덤 프로그램을
  실제 양 백엔드로 돌려 diff하는 캠페인은 미가동. ③ 방출 C 경고
  게이트 부재: emitted 코드는 `-std=c11 -Wall`만(gcc/clang -Wextra
  -Werror clean 게이트 없음 — 기존 -Werror 게이트들은 손코딩 probe만
  컴파일). ④ C 백엔드는 런타임 **14,098줄을 매 빌드 인라인 재컴파일**
  (런타임 오브젝트 캐시가 `#ifdef PGY_LLVM_ENABLED` 전용,
  `compiler_runtime_cache.c:31`) — 릴리즈에선 그 14k줄이 매번 -O3.
  ⑤ C 방출부 변수 조회 **O(locals²)**(선형 스캔 8사이트,
  `transpiler_symbols.c:119-413`) + `MAX_SLOT_VARS=4096` **하드 캡
  절벽**(초과=컴파일 거부). ⑥ 타입 interning 부재(모든 `Slot<Int>`
  언급이 새 malloc + 구조 비교).
- **공격**: parity 오라클은 **양 백엔드가 같이 틀리는 클래스에 맹목**
  이다(front-end 미컴파일이 동일하게 방출되면 910케이스가 전부 통과) —
  dual-backend 강점을 end-to-end 정확성으로 오독하지 말 것. 랜덤
  차분 캠페인은 그 보완재인데 오라클을 지어놓고 안 돌리는 상태.
- **수리**: ① CI에 fuzz matrix 1줄 배선 + seed를 run-number로 회전.
  ② emitted-C `-Wall -Wextra -Werror` gcc+clang 게이트(911 corpus
  재활용). ③ 런타임 오브젝트 캐시의 C-백엔드 확장. ④ 타이밍 계기의
  ratchet화(컴파일 속도 회귀 게이트). ⑤ adversarial corpus
  디렉토리(깊은 중첩·10MB·NUL·랜덤 바이트, timeout 래퍼) — C8 가드의
  실행 게이트를 겸한다.

---

## 강한 곳 (반증거 — 이번 감사가 공격 실패한 축)

- **런타임 코어 동기화**: seq_cst eventcount 정식 무결(ARM 포함), 락
  순서 무순환, teardown/spare-스폰 경합 닫힘, SPSC 교과서적.
- **할당 위생**: `p=realloc(p,…)` 오용 **0**, 성장 경로 전부
  SIZE_MAX/wrap 가드, 64MiB 파일 캡, assert 0 + abort/exit 총 3사이트.
- **strict aliasing**: memcpy/typed-storage 구조 + 양 레그
  `-fno-strict-aliasing` 백스톱으로 원천 봉쇄(TBAA 위반 사이트 0).
- **프로세스 실행**: argv-배열 `_spawnvp`/execvp — 셸 무경유라 경로
  공백/인용 버그 클래스 자체가 없음.
- **평가 순서**: `++`/`--`/복합대입 표면 부재 + 문장식 시퀀싱으로
  unsequenced UB 방출 불가.
- **shift UB**: 표면에 시프트 연산자 자체가 없어 미도달.
- **MIR 메모리 형상**: per-node 포인터 그래프가 아닌 연속 배열(옳은
  선택), 심볼 테이블 해시(로드팩터 <0.5).
- **sret 급소**: 0-인자 struct 반환은 하드 컴파일 에러로 가드됨, 손수
  박은 byval/sret/align 속성 **0**(ABI 전부 LLVM 타깃 위임 — SysV/Win64
  분기 은닉처 없음).

## 즉시 수리 가능 Top 5 (각 1시간 이하, 게이트 포함)

1. **C4**: build_runtime_bc.sh에 `-fwrapv -fno-strict-aliasing` (1줄).
2. **C5-①**: zone_authority/clock export를 strip 술어에 등재.
3. **C3**: 배열접근 fail-open else → 백엔드 에러 (+LLVM 죽은 arm).
4. **C13-①③**: test-asan CI 배선(잡 1개) + cancel_probe `_Atomic`화.
5. **C6**: freshness를 runtime 디렉토리 글롭 mtime으로 교체.

## BDFL 판정 필요 3건

1. **C2**: `Int` 산술 overflow의 표면 의미론 — wrap 공식화(+원장 서사
   정정+산출물 방벽)인가, checked 승격(성능 실측 후)인가.
2. **C12**: Channel 표현 판정 — 4면 fail-close 계약(A) vs heap 핸들
   승격(B). WO-RT-6은 이 판정의 부분집합이 된다.
3. **C5-②**: CI에 `.bc`-on 레그를 넣을 것인가(로컬-CI 구성 비대칭
   해소) — 넣지 않는다면 `.bc` 경로를 experimental로 격하 명시가 정직.

## Related

docs/188(레드팀 1차 — 골든/SoT) · docs/179(강함 원장 — 이 문서의 신규
발견은 원장 축4/축2 부족분의 세목) · docs/137(레드팀 계약) ·
project_arithmetic_ub_closure(2-레이어 모델 — C1/C2가 그 옆문) ·
project_semantic_termination_gap(해소된 hang 사건 — 그 클래스의 구조 갭이 C8) · TODO 보드 WO-RT-6
(C12가 확장).
