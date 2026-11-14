# 190. 레드팀 리뷰 3차 — inline→extern 이후 링키지/상태-소유권 렌즈 (2026-07-18)

docs/189(C 컴파일러 개발자 렌즈)의 후속. **inline→extern 워크스트림**
(`fb8778c5`/`13553d91`/`c16459d4`/`997c319f`, 보드 WO-RED2 ✅CLOSED "잔여 없음")
직후, 그 워크스트림이 만들어 낸 표면을 **cancel-probe CI 탈출**(`8685c3c1`)의
렌즈로 다시 본다. 질문 하나: **"cancel-probe 는 형제 없는 1회성 사고였나,
클래스였나."**

3축 병렬 감사(드라이버/오브젝트-캐시 견고성 / 신규 링키지 모드 × `.bc` 트윈
interplay / 게이트·CI 구성 맹점) + 오케스트레이터 직접 검증(방출 바이너리 nm,
커밋된 `.bc` 실물 llvm-nm)으로 작성했다.

**방법론 정직 기록**: 감사 중 오케스트레이터가 세운 초기 가설 1건은 코드
직독에서 **기각**됐다 — "ALLOC_BYTES 가 인라인 컬렉션(프로그램)과 extern 할당자
(오브젝트) 양쪽에서 과금되어 env 로도 2× 우회". 허위다. `pgy_budget_charge_alloc`
는 `PGY_RT_DECL`(extern, `pgy_runtime_allocator_inline.h:221`)이라 컬렉션 트윈도
이 extern 퍼널로 오브젝트 사본에 과금하며, env 한도는 두 사본에 미러되므로
**env-구동 예산은 안전**하다(§Tier A-3). 실제 결함은 그보다 좁은 **프로그램적
setter 경로**다. 아래 ★는 오케스트레이터가 바이너리/`.bc` 실물로 직접 재확인한
발견, 나머지는 감사 에이전트가 file:line/심볼 매핑으로 실증한 발견이다.

## 종합 판정 (한 줄)

"cancel-probe 는 **클래스였다**. extern 오브젝트가 C 백엔드를 처음으로 2-TU 세계로
바꾸면서, **미변환 상태로 남은 능력/예산/취소 싱글턴**이 방출 TU 와 런타임
오브젝트에 각각 인스턴스화된다. 지금은 **env 채널이 두 사본에 미러**되고
**`.bc` 가 stale 이라 freshness 가 bc-ON 을 자동 차단**하는 두 우연이 급소를
가리고 있을 뿐, 셋 다 cancel-probe 와 정확히 같은 병형이다. 그리고 워크스트림의
마감('잔여 없음')은 이 클래스에 대해 **조기**였다 — 마감 근거로 인용한
'backend-compare 6샤드'는 join-any/취소 목격자(샤드 16/17/18 mod 20)를 한 번도
돌리지 않았다."

---

## Tier A — 상태 소유권 (cancel-probe 클래스의 형제들)

### ★A1. HIGH-클래스(잠재) — C-extern: 능력 마스크 분열, 변환된 가족이 미변환 게이트를 오브젝트-측에서 호출

- **증거**: 능력 게이트 `pgy_cap_require_export`/슬롯 `pgy_cap_granted_slot`은
  **미변환**(plain `static inline` + 함수-로컬 `static uint32_t granted =
  PGY_CAP_ALL`, `pgy_runtime_panic_checked_inline.h:180-194,217`). 그런데
  **변환된**(`PGY_RT_DECL`, extern→오브젝트) 가족의 본문이 그 게이트를 호출한다:
  `pgy_runtime_scalar_std_inline.h:228-243`(`Random`/`SeedRandom` 내
  `pgy_cap_require_export(PGY_CAP_RANDOM,...)`), `pgy_runtime_process_args_inline.h:33-42`
  (`pgy_args` 내 PGY_CAP_ENV). 이 본문들은 cext 오브젝트 안에서 컴파일되므로
  **오브젝트의 `granted` 사본**(env 미설정 시 PGY_CAP_ALL)을 본다. 방출 TU 에서
  부른 `pgy_cap_set_manifest_export`(`:197`, static inline)는 **방출 TU 사본만**
  갱신한다.
- **★실측**: 채널 probe 를 C-백엔드로 링크한 `probe.exe` 를 objdump 하면 budget
  상태 인스턴스가 2개(`st.0`@0x40 프로그램 TU + `st.2`@0x240 오브젝트, 서로 다른
  주소·TU). 능력 슬롯도 동형.
- **공격**: 샌드박스 로더가 `pgy_cap_set_manifest_export` 로 RANDOM/ENV 를 회수한
  뒤 콘텐츠가 `Random()`/`Args()` 를 호출 → 오브젝트 사본이 PGY_CAP_ALL → 게이트
  통과 = **매니페스트 기반 능력 샌드박스 우회**(docs/15 킬러비전). 역방향도:
  env 제한 후 `grant_all` 해도 오브젝트 사본은 제한 유지 → 예기치 못한 panic.
- **왜 지금 안 터지나(정직)**: setter 는 **Pergyra surface 미노출**(parser/semantic
  grep 0). 현 런타임 능력 채널은 **env `PGY_CAP_GRANT`**(`pgy_runtime_capability.h:39-76`)
  뿐이고, 두 사본이 **각자 같은 env 를 lazy latch → 미러**되므로 env 경로는 양쪽
  다 동작한다. 즉 **오늘 배포해도 env 로는 안 뚫린다.** 프로그램적 매니페스트를
  거는 C-백엔드 로더 shim 이 배선되는 순간 라이브가 된다. **cancel-probe(라이브로
  CI hang 냄)와 동일 클래스의 두 번째 사례.**
- **판정**: VERIFIED-in-code + 바이너리 실측. **오버클레임 금지**: "샌드박스 뚫림"
  아님 — "단일-인스턴스 불변식 위반, env-경로 우연 안전, 프로그램적-경로 잠재 우회".

### ★A2. HIGH(bc-ON, 재생성 즉시 라이브) — `.bc` strip 구멍: `pgy_task_*`/`pgy_async_*` 미strip → 취소 TLS 이중 인스턴스

- **증거(커밋된 `.bc` 실물 llvm-nm)**: `src/runtime/pgy_runtime_lib.bc` 에서
  `pgy_task_cancel_export`/`pgy_task_is_cancelled_export`/`pgy_async_detach_export`
  = `T`(external, 정의됨, **미strip**), `g_pgy_coro`/`g_pgy_thread_current` =
  `d`(로컬 static). strip predicate `llvm_fn_is_stateful_runtime`
  (`src/codegen/llvm_runtime_attrs.c:169-177`)의 9개 prefix 에 **`pgy_task_`/
  `pgy_async_` 없음**(`pgy_spawn`/`pgy_await`/`pgy_channel_`/`pgy_lane_`는 있음 —
  같은 TLS 의 다른 접촉자는 strip). 방출 IR 호출처: `llvm_expr_task_calls.c:115,129`,
  `llvm_stmt_parallel_join.c:507`(**join-any 패자 취소가 이 심볼**),
  `llvm_stmt_parallel_async.c:540-546`.
- **공격**: bc-ON 에서 spawn/await/워커루프는 오브젝트의 TLS 를 스탬프하는데,
  `task.is_cancelled()`/`async_detach`는 **inline 된 .bc 본문**이 들고 온
  프로그램-모듈 사본(제로 TLS)을 읽는다 → `is_cancelled()` 항상 false → **협조적
  취소 사망, join-any 패자 취소 no-op**. docs/182 취소 계약이 bc-ON leg 에서 조용히
  깨진다.
- **경감**: bc-ON 은 opt-in 이고 현재 `.bc` 는 **stale**(mtime 11:32 < pgy_parallel.h
  12:01)이라 freshness 스캔이 자동 차단 중. 단 **다음 재생성 즉시 재무장**되고
  게이트는 이를 전혀 안 본다. **워크스트림이 만든 게 아니라 기존 구멍**(변환과
  무관하게 존재)이나, cancel-probe 와 같은 클래스라 이 렌즈가 처음 포착.
- **판정**: VERIFIED-in-artifact(오케스트레이터 재확인: `T`/`d` 심볼 + predicate 부재).

### A3. MED — C-extern: 예산 상태 분열, 프로그램적 한도·introspection 무효 (env 는 안전)

- **증거**: alloc/spawn 과금은 **변환됨**→오브젝트 `st.2`(`allocator_inline.h:221-228`,
  `pgy_parallel.h:166,516`, `pgy_parallel_blocking.h:140`, `coroutine.h:263`).
  channel 과금(`pgy_runtime_channel_inline.h:80-81`, `:540`
  `PGY_CHANNEL_DEFINE(Int,...,static inline)`)·`set_limit`/`reset`/`used`/
  `is_imposed`(`panic_checked_inline.h:241-271`)는 **미변환**→방출 TU `st.0`.
- **harm**: env(`PGY_BUDGET_*`)는 사본별 lazy latch(`pgy_runtime_budget.h:80-95`)로
  양쪽 적용 → kind별 홈이 단일이라 **env bound 유지**. 깨지는 것: 로더 API
  `pgy_budget_set_limit_export` 로 건 ALLOC/SPAWN 한도가 과금 사본(오브젝트)에 안
  닿아 **무효**; `pgy_budget_used_export` 가 ALLOC/SPAWN 에 0 보고(관측성).
  `imposed` 부작용: 프로그램 사본에만 set → 오브젝트 fast-path 가 과금 자체 SKIP.
- **판정**: VERIFIED-in-code.

### A4. MED(parity) — env vs manifest 우선순위가 백엔드마다 다름 (기존 버그, 이 감사서 발견)

- **증거**: C twin 은 latch 가 slot accessor 안(`panic_checked_inline.h:183-193`)이라
  `set_manifest` 후 slot 접근 시 env 를 먼저 latch → **manifest 가 최종**. LLVM
  오브젝트 twin 은 `set_manifest` 가 직접 대입(`authority_file_core.h:304-308`,
  latch 없음)하고 첫 `require` 의 latch 가 `g_pgy_cap_granted = env_mask`(대입,
  `:300-301`) → **env 가 manifest 를 덮어씀**. env·manifest 둘 다 다른 값으로
  설정되면 **두 백엔드의 grant 결과가 상이**.
- **판정**: VERIFIED-in-code. 능력 정책의 백엔드 divergence — parity 오라클이
  같은-출력이면 놓친다.

---

## Tier B — 드라이버 / 오브젝트-캐시 견고성 (워크스트림의 신규 오케스트레이션)

### ★B1. HIGH — cext TU 상대경로 → 비-루트 CWD·cold 캐시에서 C 백엔드 하드페일

- **증거**: `compiler_runtime_cache.c:291` `argv[..]=PGY_RUNTIME_CEXT_LIB_C` =
  원경로 `"src/runtime/pgy_runtime_cext_lib.c"`(`compiler_internal.h:23` 기본값,
  **어디서도 -D 로 override 안 됨**). 형제 3개(PGY_SRC_DIR/RUNTIME_DIR/
  RUNTIME_LIB_C)는 Makefile 에서 `$(PROJECT_ROOT)/` **절대**(`Makefile:177-179`).
  `-I`(289)는 절대인데 `-c` TU(291)만 상대.
- **공격**: 임의 디렉토리에서 `pgy foo.pgy --backend=c` + cold 캐시 → cc 가
  `-c src/runtime/...` 를 그 CWD 기준 해석 → "No such file" → "Runtime object
  compilation failed". **컴파일러가 아무데서나 안 돎.** 게이트는 전부 `cd $ROOT`
  라 은폐. (이건 워크스트림/내 fb8778c5 가 만든 회귀.)
- **수리**: CEXT 도 절대경로(형제와 동일). **1줄.**
- **판정**: VERIFIED-in-code.

### B2. MED — `abi-perf-runtime` 이 드라이버 LLVM 캐시경로를 오염(UB-플래그 회귀)

- **증거**: `Makefile:1622` `ABI_PERF_RUNTIME_RELEASE_OBS0=$(TMPDIR)/
  pgy_runtime_cache_release_obs0.o` = `compiler_runtime_cache_object_path` 가
  만드는 바로 그 이름. 레시피(`:1999`) `$(CC) $(CFLAGS) -O3 -DPGY_LLVM_ENABLED`
  — CFLAGS(`:105`)에 `-fwrapv`/`-fno-strict-aliasing` **없음**(드라이버는
  `:282-283`/`cache.c:282-283` 에서 명시 강제). → `make abi-perf-runtime` 후
  일반 LLVM 릴리스 빌드가 이 fresh 오브젝트를 링크 → checked-arith UB 회귀
  (docs/189 봉인한 트윈-플래그 클래스 캐시 경유 재발).
- **판정**: VERIFIED. 스코프: perf 타겟 실행자 한정 → MED.

### B3–B7 (MED–LOW, 에이전트 실증, 오케스트레이터 미직접검증)

- **B3** concurrent-build race: 두 레그 모두 최종 캐시경로에 직접 `-o`,
  lock/atomic-rename 부재, 로컬 기본 `PGY_BACKEND_COMPARE_JOBS=auto`(≤8) + cc
  중도사망 시 torn 오브젝트가 newer-mtime 로 **영속**(remove 는 cc 비0 때만).
  CI 는 `JOBS=1` 이라 재현 불가(= cancel-probe 와 같은 "로컬만 터짐" 형).
- **B4** 캐시키 불완전: `{opt,obs}` 만. repo identity(worktree 공유 TMP 이름
  충돌)·컴파일러 identity(CC=clang/gcc 전환·gcc 업그레이드 시 재빌드 안 됨)
  무키.
- **B5** `/tmp` world-shared 예측가능 오브젝트 이름 — 방출 `.c` 는 private-0700
  dir(`c_runner.c:35-52`)인데 **모든 바이너리에 링크되는** 오브젝트는 공유 root.
- **B6** freshness 비재귀: `compiler_runtime_dir_has_newer_source` 가 단일 dir
  flat readdir → **`async/` 서브디렉토리 맹점** + 삭제파일 맹목(newer 만 탐지).
- **B7** cross-fs mtime strict `<` + 1초 granularity → 동초 편집·재빌드 시 stale
  링크.

---

## Tier C — 게이트/CI 구성 맹점 (탈출이 통과한 이유 + hang 예산)

**탈출 재구성(핵심)**: cancel-probe 목격자는 게이트 전체에서 딱 둘 — (a)
backend-compare corpus 케이스 `parallel_join_any`/`_blocked`/`_spinloop`
(`compare_backends.sh:576-578`, 샤딩 `idx % total`, TOTAL=20 → **샤드 16/17/18**),
(b) `parallel_join_smoke.sh`(CI step 만, **어느 로컬 aggregate 에도 없음**,
**timeout 전무**). 마감이 인용한 "backend-compare 6샤드"(0-5)는 셋 다 스킵했고,
같이 돌린 나머지 게이트(starvation=크로스-경계 *함수콜*이라 링크 해소되어 무영향,
core-contract=grep, warning-clean=compile-only)는 **per-TU 상태 split 에 구조적
맹목**이었다.

### ★C1. HIGH — hang 예산 없음 (CI 건강 직결)

- **증거**: `.github/workflows/ci.yml` 의 `timeout-minutes` 는 self-host 3잡
  (40/40/30)뿐 — `build-linux`/`backend-compare-linux`/`build-windows`/
  `build-macos-c-only`/`formal-proofs-rocq9` = **무제한(GitHub 기본 360분)**.
  `parallel_join_smoke.sh:130-154` 의 `expect_runs` 는 **per-test timeout 무**
  (형제 게이트 starvation 10s/backpressure 3s/nested 30s/fuzz 60·30s/compare
  30s 와 대조). 이번엔 fix push(19분 후)의 `cancel-in-progress` 가 거둬서 ~15-35분
  소각에 그쳤을 뿐.
- **수리**: 5잡에 `timeout-minutes` + join smoke 에 `timeout` 래퍼. **저비용.**

### C2. HIGH — 취소/join-any 행동 목격자가 모든 로컬 aggregate 밖

- `parallel-join-test-smoke`(Makefile:2911)는 `test-all`/`parallel-production-
  contract`/`redteam-repair-contract` 어디에도 없음 → 개발자가 병렬 작업을 로컬
  검증해도 취소 fixture 를 안 돌림 → **다음 상태-split 도 로컬 green.** 수리:
  redteam-repair aggregate 에 편입.

### C3–C7 (MED)

- **C3** `PGY_RUNTIME_INLINE=1` opt-out 을 게이트 0개가 exercise(config-rot,
  docs/189 C5-② 클래스). 기본이 extern 인데 default 도 pin 안 됨.
- **C4** backend-compare 가 both-legs-hang 을 PASS(rc 124==124 + 빈출력 동일).
- **C5** asan 이 방출 프로그램을 한 번도 실행 안 함(compile 파이프라인만).
- **C6** 플랫폼 비대칭: Windows/macOS 가 backend-compare 미실행(차분 오라클이 이
  버그가 살던 축에서 안 돎).
- **C7** 양 게이트(cext-contract/bc-contract) 100% grep, **행동 테스트 0**.
  cext-contract 는 "변환 가족에서 도달 가능한 미변환 함수-로컬 static 상태"
  (A1/A3 = cancel-probe 클래스)에 대한 핀/테스트 전무. bc-contract 는 predicate
  **존재**만 핀하고 실제 `.bc` 심볼 대비 **커버리지**(A2)는 안 봄.

---

## 강한 곳 (반증거 — 이번 감사가 공격 실패한 축)

- **cancel-probe 수리(8685c3c1) 건전**: C-extern 은 `PGY_RT_GLOBAL` extern
  단일-홈, bc-ON 은 `pgy_cancel_probe_*` 접촉자 전원 strip — 양쪽 다 실물/소스
  확인. 수리 자체는 옳았다(다만 형제들을 안 데려갔을 뿐).
- **C7 수리(g_pgy_cap_granted/g_pgy_budget LLVM 단일화) 유효**: `.bc` 에서 둘 다
  `U`(extern), budget `PGY_BUDGET_NOINLINE` 이 실제로 인라인 사본 0 달성 확인.
- **두 오브젝트 메커니즘(cext vs exports) 공존 불가**: compiler.c:257 은 cext 만,
  compiler_llvm.c 는 exports obj 만 링크, 캐시경로 분리(`cache.c:176-201`) →
  상호 clobber·심볼 중복 불가.
- **오브젝트-빌드 실패 시 무음 inline fallback 없음**: 하드 에러 전파(§1.1 준수),
  유일한 무음 경로는 의도된 env opt-out(C3).
- **기본 모드 토큰 중립성**: linkage.h 기본 확장이 정확히 `static inline`/`static`,
  변환 diff 스팟체크에서 pre-conversion 토큰 복원·initializer 가드 극성 전부 정상
  → 현재 `.bc` 재생성해도 기본-모드 내용 불변(A2 는 strip 누락이지 토큰 drift 아님).
- **linkage 모드 상호배제**: DECLS_ONLY×EXTERN_DEFS `#error`(`:52-54`) 정상.

---

## 즉시 수리 가능 Top 5 (각 저비용, 게이트 포함)

1. **B1**: cext TU 를 절대경로로(형제 3개와 동일 패턴). 1줄. — C 백엔드 out-of-tree
   복구.
2. **A2**: strip predicate 에 `pgy_task_`/`pgy_async_` 추가(`llvm_runtime_attrs.c:169-177`)
   + bc-contract 에 "predicate vs `.bc` 심볼 인벤토리" 대조 단계.
3. **A1/A3**: cap/budget 게이트 가족을 `PGY_RT_GLOBAL`/`PGY_RT_DECL` 로 단일-홈화
   (cancel-probe `8685c3c1` 과 동일 수술) — 클래스 통째로 닫아 미래 로더 shim 안전
   확보. 최소안: extern-mode 행동 fixture(manifest set → `Args`/`Random` panic 기대).
4. **C1**: CI 5잡 `timeout-minutes` + `parallel_join_smoke` per-test `timeout`.
5. **C2**: `parallel-join-test-smoke` 를 `redteam-repair-contract` aggregate 에 편입
   (다음 상태-split 을 로컬에서 fail-close).

## BDFL 판정 필요 2건

1. **A4**: env vs manifest 우선순위를 백엔드 간 통일할 것인가, 통일한다면 어느 쪽
   (manifest-우선=C 현재 / env-우선=LLVM 현재)이 정책적으로 옳은가. 능력 정책의
   결정 순서는 언어 의미론 결정이지 단순 버그가 아님.
2. **워크스트림 마감 재개방**: WO-RED2 의 "동시성 클러스터 extern 완료, 잔여 없음"
   은 상태-소유권 클래스에 대해 조기였다(A1/A3 미변환 잔여 + 마감 근거 '6샤드'가
   목격자 미실행). 이 문서의 A/C 를 WO-RED3 으로 열 것인가, 아니면 A2(bc-ON,
   기존 구멍)만 분리하고 A1/A3(env 우연-안전)은 "미래 로더 shim 선행조건"으로
   deferral 할 것인가.

## Related

docs/189(레드팀 2차 — C 컴파일러 렌즈, 이 문서가 그 C13 cancel-probe 수리의
**형제 탐색**) · docs/188(레드팀 1차 — 골든/SoT) · docs/137(레드팀 계약 — R6
budget/capability 샌드박스, 이 문서가 그 C-백엔드 재개봉을 보고) · docs/182(취소
계약 — A2 가 그 bc-ON 위반) · docs/15(capability 샌드박스 — A1 이 그 C-extern
매니페스트 경로) · project_compile_speed_extern_workstream(워크스트림 원장 —
마감 재개방) · project_redteam_review_2026_06(R6 계보).
