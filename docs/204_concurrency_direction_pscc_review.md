# 204. 동시성 모델 방향 판정 — PSCC 제안 심사와 정전(canon) 대응표

Updated: 2026-09-03 (Asia/Seoul)

Status: **DECISION RECORDED — 방향 결정**. 의미론 권위는 여전히
`docs/53`(parallel 코어 정책) · `docs/113`(동결 계약) · `docs/114`(근거) ·
`docs/146`(SEA lane) · `docs/178`(경계 증거)에 있다. 이 문서는 그 위에
**동시성 전체 방향**을 고정하고, 외부 제안(PSCC v0.1, 부록 A)을 정전에
대조해 무엇을 채택·정정·유보하는지 기록한다. 이 문서 자체는 구현 게이트가
아니다. 문장 하나가 실행 계약이 되려면 `docs/113`에 들어가고 독립 실행
게이트가 붙어야 한다. 2026-09-03에는 §3.5의 첫 rung이 그 절차로 착지했다.

---

## 0. 결론

제안의 **골격은 맞다.** 그리고 그 골격의 대부분은 이미 Pergyra 정전이다 —
이름만 다르다. 제안이 "PSCC"라고 부른 것의 핵심 문장들은 다음 문서에서
이미 결정된 것들이다:

| 제안의 문장 | 정전에서 이미 결정된 곳 |
|---|---|
| `parallel`은 증명 의무다 | `docs/53` §1(코어 primitive), `docs/113` §Happens-Before(충돌은 에러), `docs/178` §0("경계를 건너는 접근은 증거가 필요하고 증거 없으면 거절") |
| 무음 순차 폴백 금지 | `docs/177` F1, `docs/186` §3("무음 직렬화 금지"), `docs/168` `ProjectionFallbackFact` |
| 키워드 하나에 여러 의미를 싣지 않는다 | `docs/114` §0-§1 "coloring decomposition", "Each concern has an owner", "Widening one cell must not silently widen the others" |
| lane/스케줄은 언어가 아니라 증거에서 파생된 materialization이다 | `docs/146` §4, `src/runtime/pgy_lane_scheduler.h:12-16` 관측 동등 계약 |
| 봉인 후 풍부한 분석은 지운다 | `docs/178` §"evidence lifetime" — "Evidence-carrying compiler, not evidence-hoarding runtime" |
| 결정적 reduce는 고정 트리다 | `docs/181` R4 인덱스-순서 left fold, "스케줄-적응형 금지"; `docs/186` §3 |

따라서 이 문서의 가치는 새 모델의 발명이 아니다. (1) 제안의 어휘를 정전
어휘로 **번역**하고, (2) 정전과 **충돌하는 지점을 정정**하며, (3) 제안에서
**진짜 새로운 조각**을 골라 기존 구멍 목록(`docs/186` §2, `docs/182`)에
꽂는 것이다.

채택 조건은 여섯이다. 하나라도 빠지면 제안을 그대로 문서화해서는 안 된다.

1. `async`는 **lifetime 구조체가 아니다**. suspension 규율만 뜻한다(§2.1).
2. `ConcurrencyPlan`은 **봉인된 MIR fact**다. AIR는 검증만 하고 어떤
   IR로도 lowering되지 않는다(§2.2).
3. `parallel for`/`reduce`/`unordered`는 새 키워드가 아니라 **이미 있는
   join 표면**이다(§2.3).
4. share/split/lend/move는 **증거 4종 + 캐리지**의 별명이다. 키워드로
   만들지 않는다(§2.4).
5. task `detach`는 **zone `detach` 절과 이름이 충돌**한다. 다른 이름을
   쓰고 capability로 막는다(§2.5).
6. 결정성은 **허용 부분집합의 정의**이지 헤드라인 기능이 아니다.
   우선순위는 `docs/198`을 따른다(§2.6).

인용된 외부 연구(부록 A의 각주)는 **이 저장소에서 검증되지 않았다**.
방향의 근거는 위 정전 문서와 코드이지 인용이 아니며, 외부 주장에 인용을
권위로 쓰지 않는다(`docs/118`·`119`·`120`의 anti-hype 규칙).

---

## 1. 판정 요약표

판정 어휘: **정전** = 이미 결정·착지됨 · **채택** = 방향으로 채택, 미착지 ·
**정정** = 정전과 충돌, 고쳐서 채택 · **유보** = 베타 이후로 미룸.

| 제안 § | 내용 | 판정 | Pergyra 앵커 |
|---|---|---|---|
| 1 | 축 분리(parallel≠thread, async≠thread, Zone≠lock, Intent≠effect, Capability≠mutex) | 정전 | `docs/114` §1 표; `docs/53` §5 |
| 2 | `parallel` = 병렬 가능성 증명; 안전 미증명→순차 폴백 금지; 수익성 순차는 허용 | 정전 | `docs/113` "cannot prove … must stay rejected"; `src/runtime/pgy_parallel_spawn.h:31-38`(풀 부재 시 inline 실행은 stderr 경고와 함께 — 안전이 아니라 가용성 폴백) |
| 3 | AccessSummary(R/W/atomic/reduction/transfer/kill/host effect) | 채택 | 현재 실물: `src/semantic/boundary_witness.{h,c}` OpAcqR/OpAcqW/OpRel + `type_checker_flow_parallel.c`; `docs/168` `ReductionFact`·`ElementalPureFact`(부분 착지) |
| 4 | AccessPath = zone·resource·slot·field·index 도메인; 서로소 증명 | 채택 | `docs/178` Disjointness rung 0(slice 분할 쌍); index/field 도메인 일반화는 미착지 |
| 5 | 정적 미증명 시 **진입 전 1회** 런타임 가드, 실패=`ParallelAdmissionError`, 롤백 없음 | 채택(신규 증거 종) | `docs/178` 증거 4종에 없음 → §3.2 "Dynamic Disjointness" |
| 6 | Capability 분류 트리(Memory/Host/Control/Affinity) | 정정 | 현재 비트: `PGY_CAP_IO_READ/IO_WRITE/NETWORK/CLOCK/RANDOM/ENV/RENDER/AUDIO/INPUT` (`src/runtime/pgy_runtime_capability.h:22-32`); Affinity는 capability가 아니라 **lane**(`docs/146`) — §2.7 |
| 7 | mode를 표면에 노출하지 않고 파생 fact로 | 정전 | `docs/146` §4 "lane the evidence unlocked"; `docs/114` §8 "no user-selectable scheduler" |
| 8 | `async` ≠ parallel | 정전 | `docs/53` §5.3 |
| 8-9 | `async`는 lifetime 구조체; `async scope` 도입 | **정정** | §2.1 |
| 9 | 고아 task 금지(structured) | **정전(2026-09-03 착지)** | `type_checker_future_lifecycle.c`; `structured-spawn-lifecycle-test-smoke`; 런타임 `AsyncScope*`는 여전히 소비자 0이며 SoT가 아님 — §3.1 |
| 10 | `detach`는 별도 권한 | **정정** | `detach`는 zone 절(`src/parser/parser_domain_zone.c:251`); task detach는 `async` 블록 lowering에 암묵(`llvm_stmt_parallel_async.c:539-541`) — §2.5 |
| 11-12 | revocable capability; share/split/lend/move | **정정**(번역) | `docs/178` Copy/Channel/Exclusivity/Disjointness + `ref`/`own`/`inout` — §2.4; 런타임 캐리어 부재는 §3.5 |
| 13-14 | `LiveRef`는 await를 못 넘고 `SlotRef{slot,generation}`은 넘는다; resume 시 재검증 | 채택(절반은 정전) | pin/view는 이미 await·spawn·parallel·channel을 못 넘음(`docs/113`, `docs/114` §4); `SlotHandle{slotId,typeTag,generation}`(`src/runtime/slot_manager.h:122-127`), `PgyPinnedView{slotId,generation,mode,valid}`(`:167-175`); 재검증은 `docs/107`이 이름 붙인 "checked suspension contract" — §3.3 |
| 15 | 검사는 경계에 몰고 hot loop는 직접 접근 | 채택(현재 위반) | zone 접근자가 **매 접근**마다 `PGY_ZONE_RDLOCK` + stale 경고(`src/codegen/transpiler_zone_struct_emit.c:250-258`) — §3.6 |
| 16 | await 시 capability 3분류(SuspendSafe/Revocable/NonSuspendable) | 채택(2/3 정전) | NonSuspendable = pin/view/Token 규칙(정전); SuspendSafe = copy-only(정전); Revocable = §3.3의 재검증 계약(신규) |
| 17 | Zone = resource universe, lock 아님; 멤버 수명 개별 | 정전 | `docs/197` region/arena; `docs/203`; `docs/113` zone generation atomic 계약 |
| 18 | rwlock은 마지막 폴백; 동기화 사다리 | 채택 | `pthread_rwlock_t`는 `src/runtime/pgy_runtime_zone_sync_abi.h:17` 한 곳, `PGY_ZONE_THREADSAFE`로 컴파일 선택; 사다리 = `ExecutionLane`(`src/common/execution_lane_kind.h:17-26`) 확장 — §3.6 |
| 19 | 다중 Zone 정규 획득 순서 | 채택(저우선) | 실물 없음 — §3.7 |
| 20 | `parallel` 관측 의미 = 정규 순차 실행 | 채택(정의로) | `docs/181` R2 인덱스 순 수집, R4 고정 fold; `join with any`는 명시적 비결정 — §2.6 |
| 21 | 결정적 reduction 트리; `unordered`는 명시 | 정전 | `docs/181` R4; 비결정 escape는 이미 `join with any` |
| 22 | deterministic Zone에서 async 완료는 commit 경계에서 정규 순서로 반영 | 유보(베타 후) | replay/commit 실물 없음(조사 §7) — §3.8 |
| 23 | 의도적 비결정(select/race)은 capability | 채택(일부 착지) | `select`는 readiness arbitration(`docs/53` §5.5); Random/SeedRandom은 C-inline·LLVM runtime 모두 `PGY_CAP_RANDOM`을 require한다. select/any의 결정성 분류는 잔여 — §3.8 |
| 24 | 시스템 의존 그래프 compile-once | 유보 | `docs/181` 형 B(reactive) R2 미착수 — `docs/186` 구멍 4 |
| 25-26 | MIR에 병렬 의미 노드 유지; 스케줄이 아닌 제약만 | 정전+채택 | `docs/104` AIR boundary kind PARALLEL/CHANNEL/EXECUTION; 일반 의존 그래프는 AIR Phase 2 |
| 27 | IR 계층: AIR → explicit spawn/token graph → Backend | **정정** | AIR는 lowering 금지, 백엔드는 AIR를 include조차 못 함(`tests/air_backend_nonimpact_smoke.sh:60-73`) — §2.2 |
| 28 | 봉인 후 소거 | 정전 | `docs/178` evidence lifetime; 드라이버 메모리 벽 이력 |
| 29 | Intent는 effect system이 아니다; Effect ⊆ Capability ⊨ IntentPolicy | 정전(+보강) | `docs/114` §1; `docs/198`; effects는 callable의 `with effects`·step의 `causes:`, caps는 `with caps`, authority는 step의 `authorized by:` — §2.8 |
| 30-32 | Wasm/WASI/quota | 유보(예산 축만 채택) | wasm 백엔드 없음(C→Emscripten, `docs/23`); 예산 실물 4종+WALL(`src/runtime/pgy_runtime_budget.h:36-40`), ITER/TIME 축은 보드 등록 — §3.9 |
| 34 | 최소 표면 유지 | 정전 | `docs/107` 베타 폐쇄: 표면 확장 없음 |
| 35 | 6단계 구현 순서 | **정정**(재단) | §4 |
| 36 | 정리 5개 | 채택 | §5 |
| 37 | 위험 6개 | 채택(+Pergyra 구체화) | §6 |

---

## 2. 정정 — 정전과 충돌하는 지점

### 2.1 `async`는 lifetime 구조체가 아니다

제안 §8-9는 "`async`는 계산의 수명과 suspension을 구조화하는 키워드"라고
정의하고 `async scope`를 도입한다. 이는 `docs/114`가 문서 한 편을 들여
분해한 과부하를 `async`로 되돌리는 것이다. 정전:

- `async` = **suspension/coroutine 규율**, 병렬성도 수명도 아님(`docs/53` §5.3).
- 수명·작업 범위의 소유자 = `parallel { }`, 이름 있는 `spawn`, **intent
  step**(`docs/114` §1 표 "Work scope and lifetime").
- `await` = 완료 join **만**(`docs/113` §Future Await Contract).

정정된 문장: **"구조적 수명은 scope 구문(parallel 블록, intent step,
그리고 affine Future가 소유하는 lexical spawn 범위)가 소유한다. `async`는 '중단될 수 있다'는
가시성만 남긴다."** 제안이 `async scope`로 얻으려던 것 — 자식이 부모보다
오래 살 수 없다 — 는 §3.1의 구조적 spawn 범위로 얻는다. 새 키워드는 없다.

### 2.2 ConcurrencyPlan은 MIR fact다. AIR는 lowering되지 않는다

제안 §27의 계층은 `AIR → explicit spawn/token/await graph → Backend`다.
Pergyra에서 이것은 구조적으로 불가능하다:

- AIR는 "verification-only synthesis IR … 어떤 IR로도 lowering되지 않는다"
  (`docs/104` §1).
- 백엔드는 MIR만 소비한다(`docs/193` MIR-only/ABI-first).
- `air-backend-nonimpact` 게이트가 `src/codegen`의 AIR include를 grep으로
  거절하고, AIR 유무에 관계없이 방출물 byte-equal을 요구한다
  (`tests/air_backend_nonimpact_smoke.sh`).

정정된 계층(§3.10의 그림): 의미 분석이 접근·캡처 사실을 만들고
(`boundary_witness`, `BoundaryCaptureFact`), MIR이 **봉인된
ConcurrencyPlan**(task id·의존 id·접근 class id·capability edge id·lane
id — 문자열 없음)을 들고, AIR는 그 plan의 admission·lane·drift를
**검증**하며, 백엔드는 MIR의 plan만 보고 materialize한다. 지금도
`ExecutionLaneFact`는 AIR boundary node에 붙어 감사용으로 저장되고
(`src/compiler/air.h:216-223`), 백엔드 lowering은 별도 경로로 lane을
받는다 — 이 분리를 그대로 확장한다.

### 2.3 `parallel for`·`reduce`·`unordered`는 이미 있다

제안 §2·§21·§34는 `parallel for`, `reduce(+)`, `unordered`를 표면으로
제안한다. 정전의 데이터 병렬 표면은 `docs/181` 형 A로 **이미 확정·착지**
(R0–R5)되어 있다:

```text
parallel (x in xs) join with all      // 인덱스 순서로 Array<R> 수집
parallel (x in xs) join with sum      // 인덱스-순서 고정 left fold
parallel (x in xs) join with product | min | max
parallel (x in xs) join with any      // 명시적 비결정 — 첫 완료
parallel (i in lo..hi)                // index 모드
```

- `reduce(+)` = `join with sum`. fold 순서는 인덱스 순으로 **계약**이며
  "스케줄-적응형 금지"다(`docs/181` R4, `docs/186` §3).
- `unordered` = `join with any`. 비결정 escape는 이미 표면에 있고
  명시적이다.
- `parallel for`라는 철자는 `docs/00_engine_core_spec.md`에서도 "아직
  미래 범주"다. 형 A가 그 자리다.

따라서 §4 Phase 1("`parallel for`부터")은 대부분 **이미 끝난 일**이다.
남은 것은 chunk 분할 초과(2-분할 이상), field/index 도메인 서로소 증명,
`ReductionFact`의 일반 루프 확장이다(`docs/178` §2 DOP 잔여, `docs/168`
사다리).

### 2.4 share / split / lend / move는 증거 4종의 별명이다

제안 §11-12는 capability 이동 네 종을 컴파일러 fact로 두자고 한다(키워드
아님 — 여기까지는 정전과 같다). 번역표:

| 제안 | 정전 어휘 | 실물 |
|---|---|---|
| `share` (읽기 공유) | `ref`/`ref` 읽기 허용; **Copy** 증거(스냅샷) | `docs/113` Happens-Before; `docs/178` Copy |
| `split` (서로소 쓰기 분할) | **Disjointness** 증거 | `docs/178` rung 0 — slice 분할 쌍의 병렬 쓰기 admission |
| `lend` (임시 배타 이전, join 시 복귀) | **Exclusivity** 증거 — 단일 작성자 + 구조적 join | `docs/178` Exclusivity; `docs/113` "writes … visible after the join" |
| `move` (영구 이전) | `own` 캐리지; **Channel** 증거(프로토콜 이동) | `docs/113` Channel Contract; `Token<T>`는 spawn/channel을 못 넘음 |

새로 필요한 것은 어휘가 아니라 두 가지다: (a) `lend`의 **복귀**를 자식
종료 시점에 fact로 닫는 것 — 지금은 join 의미론에 암묵 — 과 (b) 이
capability edge를 **런타임이 실제로 나르는 것**(§3.5 — 지금은 나르지
않는다).

### 2.5 task `detach`는 zone `detach`와 충돌한다

`detach`는 이미 **zone 본문 절**의 문맥 키워드다
(`src/lexer/language_keyword_registry.def:188`, `ZONE_BODY` 문맥;
`llvm_domain_zone_sync_clauses.c:148`). task 분리는 표면 키워드 없이
익명 `async` 블록 lowering에 암묵으로 들어 있고(`pgy_async_detach_export`),
`pgy_lane_detach`는 `LOCAL_ASYNC` lane 외에서는 panic한다
(`src/runtime/pgy_lane_scheduler.h:172-173`). 익명 async 블록 자체가
베타에서 거절된 표면이다(`docs/113` "Anonymous async spawn bodies are
explicitly rejected").

정정: 제안 §10의 "분리는 별도 권한"이라는 **결정은 채택**하되, (1) 철자는
zone 절과 충돌하지 않는 것으로 §3.1에서 고른다, (2) 그 권한은 새
capability 비트(예: `PGY_CAP_DETACH`, 표면 `with caps detach`)로 두고,
(3) 베타 중에는 표면을 열지 않는다 — 구조적 spawn 범위(§3.1)가 먼저다.

### 2.6 결정성은 부분집합의 정의다

제안 §20-23은 결정적 병렬을 기본으로 삼고 commit 경계·nondeterminism
capability까지 편다. `docs/198`의 우선순위에서 Deterministic은 **마지막**
이다. 그러나 다음은 기능이 아니라 **정의**라서 비용 없이 채택한다:

> 허용된 `parallel` 부분집합(join with all/sum/product/min/max +
> 서로소/단일작성자/스냅샷 증거)의 관측 결과는 **정규 순차 실행과
> 동일**하다. 이것이 "순차 실행은 합법적 lowering"(§1 행 2)과 "실행기
> 교체가 결과를 바꾸면 실행기 버그"(`docs/186` §3)가 성립하는 이유다.

이 정의는 지금도 거의 정리(theorem)에 가깝다 — 서로소 쓰기 + join 전
비가시 + 인덱스-순서 fold. 다만 **검증되는 성질이 아니라 산문 계약**이다
(`pgy_lane_scheduler.h:12-16`; 프로그램을 병렬로 두 번 돌려 비교하는
게이트는 없다 — 조사 §7). §5의 정리 5번이 이것을 닫는다.

commit 경계(§22)·replay·deterministic zone은 **유보**한다. 게임 lockstep
목표가 확정되면 별도 문서로 연다. FP 결정성은 fold 순서 고정으로 절반만
닫혀 있고 SIMD/FMA/백엔드 차이는 numeric profile이 따로 필요하다(§6).

### 2.7 Affinity는 capability가 아니라 lane이다

제안 §6의 `Affinity {Main, Render, ThreadLocal}`은 "이 task가 어디서
돌 수 있는가"인데, Pergyra는 그 답을 이미 **ExecutionLane**으로 낸다:
`INLINE / PINNED_ZONE / BLOCKING_POOL / LOCAL_ASYNC / WORKER_POOL /
MOVABLE_SCHEDULER / REJECT`(`src/common/execution_lane_kind.h:17-26`),
그리고 lane은 `BoundaryCaptureFact{crosses_authority_boundary,
captures_live_view, captures_raw_slot, captures_raw_channel,
captures_value_only, captures_pin}`에서 순수 결정표로 파생된다
(`src/compiler/air_execution_lane.c:12-32`, `docs/146` §3).

제안의 `Portable(Task) = captured caps exclude ThreadLocal && carriage
valid`는 정확히 `MOVABLE_SCHEDULER` lane의 진입 조건("pure value, no
pin/raw, authority clear")이다. 따라서 이동성은 capability 트리에 넣지
않고 lane 파생으로 둔다. Host capability(File/Network/Clock/Random)는
기존 비트와 일치하고, Control(Spawn/Cancel/Detach)은 §2.5·§3.1의 신규
비트 후보다. Memory(Read/Write/Atomic/Reduce/Transfer)는 capability가
아니라 **증거 4종 + ReductionFact**다(§2.4).

### 2.8 Intent는 effect도, capability도 아니다 — 그리고 더 크다

제안 §29의 분리(Effect = 실제로 하는 것, Capability = 할 권한, Intent =
허용된 목적)는 정전이다. 실물 위치:

- effects: callable의 `with effects …`, intent step의 `causes:`.
- caps: callable의 `with caps io_read, …`(등록된 이름만, 중복 거절;
  `src/parser/parser_decl_clause.c:107-158`).
- authority: intent step의 `authorized by: …`
  (`src/parser/parser_intent_step.c:343-361`).

보강: Pergyra의 `intent`는 제안의 `IntentPolicy`보다 크다. step
순서·`pre/guard/post/invariant`·`compensate`·`success/failure`·admission
(`docs/53` §3, §6)을 소유한다. 즉 Intent Safety는 "권한 사용 허용"이
아니라 **"허용된 순서와 보상까지 닫힌 효과"**다 — `docs/198`이 최대
차별점으로 꼽은 이유.

---

## 3. 채택 — 제안에서 진짜 새로운 조각과 그 착지 지점

각 항목은 기존 구멍 목록(`docs/186` §2, `docs/182`)의 어느 자리에
꽂히는지, 어느 문서/게이트가 소유하게 될지를 적는다. 표면 문법은 베타
동안 열지 않는다(`docs/107`). 사실(fact)·증거·런타임 캐리어가 먼저다.

### 3.1 구조적 spawn 범위 — 고아 task 금지

2026-09-03 착지: **이름 있는 `spawn`의 `Future<T>`/`RemoteFuture<T>`는
affine lexical obligation이다.** 직접 immutable `let`으로 소유하거나 즉시
`await`해야 하며, 모든 정상 경로에서 scope/function exit 전에 `await` 또는
명시적 `own Future` 인계로 retire해야 한다. `Cancel`은 요청일 뿐 join/free가
아니므로 여전히 retire 의무가 남는다. 한 분기만 retire한 상태, bare spawn,
mutable Future, Future-to-Future `let` alias, borrowed/default Future parameter,
Future 반환은
`PGY_SEM_TASK_LIFECYCLE`로 fail closed한다.

소유자는 `src/semantic/type_checker_future_lifecycle.c`, 흐름 캐리어는
`ResourceConsumeSnapshot`, 실행 반증은
`make structured-spawn-lifecycle-test-smoke`다. 런타임의
`AsyncScopeCreate/Spawn/Cancel/WaitAll`은 여전히 생산 소비자 0이며 이 계약의
SoT가 아니다. 컴파일러가 숨은 wait-all/finalizer를 삽입하지도 않는다.

경로 합류는 정적 도달성까지 포함한다. `if true/false`, literal `match`,
최소 1회 literal range, `while true`는 불가능한 zero/alternate 경로를
Future 상태에 섞지 않는다. 조건이 동적이면 기존처럼 모든 가능한 정상
경로가 같은 retire 상태에 도달해야 한다. 정확히 0회인 literal range와 정적
분기 안의 도달 불가 loop exit는 상태 합류에서 제외하며, 이미 unavailable인
handle의 반복 사용은 후속 type-mismatch 없이 소유 진단 하나만 낸다. 전용
corpus는 C/LLVM 양쪽에서 19 positive, 18 fail-closed와 exact ABI transfer
출력을 고정한다.

중첩 병렬 pool-starvation은 이 수명 문제와 별개이며 이미 2026-07-17
help-first await와 `nested_parallel_witness_smoke.sh`로 닫혔다(`docs/186`
P-A1). 구조적 spawn 착지를 그 데드락의 미구현 방어라고 설명한 이전 문장은
정정한다.

분리(detach)의 철자는 여기서 정한다. 후보는 `spawn` 수식어(예: `spawn
background Worker(args)`)이며 zone `detach` 절과 충돌하지 않는다. 권한은
`PGY_CAP_DETACH` 신설, 샌드박스 매니페스트에서 기본 불허. 베타 후.

### 3.2 다섯 번째 증거 — Dynamic Disjointness

`docs/178`의 증거 4종은 전부 정적이다. 제안 §5의 "진입 전 1회 런타임 가드,
실패는 `ParallelAdmissionError`, 부분 실행 후 롤백 없음, 순차 폴백 없음"
을 **Dynamic Disjointness** 증거로 채택한다. 규율은 178과 같다: 경계에서
한 번 검사, 통과하면 hot loop는 무검사, 봉인 후 증거 소거. 정적 증명이
불가한 Slot 집합(예: 동적 유닛 목록)에 적용된다.

착지: `SemanticParallelCaptureBoundaryFact`에 행 종류 추가 → MIR plan에
guard id → 백엔드가 진입 전 검사 코드 방출. 게이트는 178의 형식을
따른다(허용 목격자 + 거절 목격자 × 양 백엔드).

### 3.3 재검증 계약 — `docs/107`이 이름 붙인 그것

pin/view가 `await`·`spawn`·`parallel`·channel을 못 넘는 것은 정전이다.
제안 §13-14·§16 Revocable이 더하는 것은 **넘어간 뒤의 복귀**: suspension
전에 반납하고 resume 시 `SlotHandle{slotId, generation}`으로 재해석해
generation이 바뀌었으면 stale로 처리한다. `docs/107`은 이미 이것을
"a later checked suspension contract"라 불러 자리를 비워 두었다.

현재 실물은 두 쪽 다 있다: `SlotHandle`에 generation
(`src/runtime/slot_manager.h:122-127`), `PgyPinnedView`에 generation과
`valid`(`:167-175`), `SlotFailure`가 generation을 나른다. 없는 것은
컴파일러의 "resume 지점에서 자동 재해석" 방출과 그 진단이다.

주의(anti-hype): 이것이 착지하기 전까지 "temporal world safety"를 외부
주장으로 쓰지 않는다. 지금 주장할 수 있는 것은 "pinned view는 suspension
을 넘지 못한다"까지다.

### 3.4 결정적 부분집합의 정리화

§2.6의 정의를 Rocq 정리로 옮긴다: 허용 부분집합의 두 스케줄이 같은 관측
상태를 낸다. 기존 spine에 `WitnessDataRace.v`(경계 목격자 형태)와
`IntentConflict.v sep_when_active`가 있으므로 그 위에 서로소·단일작성자·
인덱스-순서 fold를 합성하는 정리다. 실행 게이트로는 "같은 프로그램을
`PGY_WORKERS=1/2/4/8`로 돌려 byte-equal"이 대응한다 — 지금 없는 게이트다.

### 3.5 capability 흐름의 런타임 캐리어 — 1차 착지

제안 §11의 lend/revoke/return이 성립하려면 자식 task가 부모의 권한을
**받아야** 한다. 2026-09-03 첫 실행 rung은 이 런타임 구멍을 닫았다.
`pgy_runtime_context_capture_task()`가 생성 시 capability mask와 instance
identity를 복사하고, 정량 budget은 새 counter가 아니라 부모의 정확한
`budget_owner`를 공유한다. Inline/PinnedZone/BlockingPool/LocalAsync/
WorkerPool/MovableScheduler는 task body 전 해당 컨텍스트를 bind하고 종료 뒤
주변 TLS를 복원한다. LocalAsync는 yield/await 때 scheduler와 task 컨텍스트를
서로 복원한다.

실행 계약은 `docs/113`의 Spawn Runtime Authority Contract이며,
`runtime-spawn-context-propagation-test-smoke`가 inline/C-extern/LLVM runtime,
nested help-run, coroutine suspension을 실행한다. worker 기본 grant, 자식의
환경 재독, 독립 budget 초기화는 negative ratchet으로 금지된다.

남은 것: lend/move edge가 마스크를 더 좁히는 흐름 분석, unsupported detach의
독립 lifetime owner, 그리고 capability non-forgery의 정리다. 이름 있는 spawn의
부모 budget-owner 수명은 §3.1의 lexical Future 의무가 이제 보장한다.
따라서 이 착지는 Authority carriage이지 완전한 multi-tenant 격리 주장이
아니다.

### 3.6 검사 비용은 경계로 — zone 접근자의 매 접근 rdlock

제안 §15는 정전 정신(pin 블록 진입 시 검사, 내부는 직접 접근)이지만
현재 zone 접근자는 **매 접근**마다 `PGY_ZONE_RDLOCK` + generation stale
경고 + `UNLOCK`을 방출한다(`transpiler_zone_struct_emit.c:250-258`).
채택: intent step 진입·parallel 영역 진입·await resume·Zone 전이·commit·
Slot resolve에서만 검사하고, 그 안의 접근은 admitted operation으로 직접
접근. 제안 §18의 동기화 사다리(무동기/분할/atomic/reduction/의존 edge/
rwlock)는 `ExecutionLane` 열거의 확장으로 둔다 — rwlock은 `docs/113`이
"minimum fix"라 부른 베타 폴백이고, 제거는 measure-first(`docs/186` §3)다.

### 3.7 다중 Zone 정규 획득 순서

실물 없음. 채택하되 저우선. ConcurrencyPlan의 zone 집합을 ZoneId로 정렬해
획득하고, 가능하면 lock 대신 capability transfer/의존 edge로 푼다.

### 3.8 비결정은 권한이다 — Random 검사는 착지, arbitration은 잔여

현재 소스는 `Random`과 `SeedRandom`의 C-inline/LLVM runtime 양쪽에서
`pgy_cap_require_export(PGY_CAP_RANDOM, …)`를 호출하며, capability runtime
게이트가 grant/deny를 실행한다. 이 리뷰의 2026-08-21 조사 결과는 이후
착지된 소스보다 오래되어 정정한다.

잔여 채택 항목은 `join with any`·`select`(readiness arbitration)를 비결정
소스로 분류해 향후 deterministic zone에서 거절 또는 commit-순서화하는
것이다. 이것은 §2.6과 함께 베타 후로 유보한다.

### 3.9 예산 축 — quota는 capability가 아니라 budget이다

제안 §32 `SandboxBudget`의 번역:

| 제안 | 실물 | 상태 |
|---|---|---|
| memory | `PGY_BUDGET_ALLOC_BYTES`/`ALLOC_COUNT` | 있음 |
| maxTasks | `PGY_BUDGET_SPAWN_COUNT`(실제 생성된 pool task 기준, chunk 후) | 있음 — fork bomb은 ceiling을 넘는 그 spawn에서 fail |
| channels | `PGY_BUDGET_CHANNEL_COUNT` | 있음 |
| fuel | 없음 — ITER/TIME은 "별도 미래 축, 보드 등록" | 유보 |
| wall time | `PGY_BUDGET_WALL_MS` watchdog → abort | 있음 |
| maxParallelism / maxZones / maxSlots / maxHandles / ioBytes | 없음 | 채택(축 추가) |

전부 `docs/198` Resource/Budget Safety(#2)의 rung이고, "effect-budget
coupling"(예: `effect Network consumes NetworkBudget`)이 같은 문서의 미착지
방향이다. Wasm/WASI는 유보 — wasm 백엔드가 없고(C→Emscripten, `docs/23`),
WASI 0.3 관련 주장은 저장소 밖 사실이다.

### 3.10 정정된 사실 흐름

```text
Source
  │
  ▼
Semantic
  ├─ AccessSummary          ← boundary_witness(OpAcqR/OpAcqW/OpRel), 캡처 사실
  ├─ Evidence(Copy/Channel/Exclusivity/Disjointness/+Dynamic)   ← docs/178
  ├─ CapabilityFlow(share/split/lend/move edge)                  ← §2.4
  └─ ParallelAdmission(허용/거절, 이유는 항상 증거 하나)
  │
  ▼
MIR
  └─ ConcurrencyPlan  {planId, taskIds, dependencyIds,
                       accessClassIds, capabilityEdgeIds, laneIds}   ← 봉인, 문자열 없음
  │           (봉인 직후: AccessSummary·alias·capture 분석 소거 — docs/178)
  ├──────────────► AIR (검증 전용: admission·lane·drift 감사; lowering 없음)
  ▼
Backend (C / LLVM) — MIR의 plan만 소비
  │
  ▼
Runtime lanes: INLINE · PINNED_ZONE · BLOCKING_POOL · LOCAL_ASYNC ·
               WORKER_POOL · MOVABLE_SCHEDULER (+ 향후 partition/atomic/
               reduction/dependency 실행기)   — 관측 결과는 executor-invariant
```

---

## 4. 단계 재단 — 제안의 6 Phase를 기존 사다리에 맞춤

제안 §35의 순서는 Pergyra의 착지 상태를 모르고 짜인 것이다. 우선순위는
`docs/198`(Authority > Resource > Lifecycle > Fail-Closed > Intent > FFI >
Deterministic)과 `docs/186` §2 구멍 순서를 따른다.

**이미 끝난 것(제안 Phase 1의 대부분):** 형 A join 표면 R0–R5(`docs/181`),
read/write overlap 거절(`op_guard` 목격자), Disjointness rung 0(slice 분할),
Copy 스냅샷 증거, pin/view 경계 규칙, spawn 예산 계량, lane 결정표+scheduler
facade, 인덱스-순서 fold, 성능 기준선(n=10에서 Fortran OpenMP 상회 —
`docs/186` 전제).

**rung 상태(의존 순서대로):**

1. **컨텍스트 전파**(§3.5) — **착지(2026-09-03)**. 자식 task가 부모의
   capability snapshot과 exact shared budget owner를 받는다. 6개 lane ×
   inline/C-extern/LLVM runtime + nested/suspension 복원 게이트.
2. **구조적 spawn 범위**(§3.1) — Lifecycle. 함수 exit 시 live handle 거절;
   중첩 병렬 help/drain. `docs/186` 구멍 1을 닫는다.
3. **`PGY_CAP_RANDOM` require**(§3.8) — **현 소스에서 이미 착지 확인**.
4. **Dynamic Disjointness**(§3.2) — 증거 5종째. `docs/178` §2 DOP 잔여와
   함께.
5. **재검증 계약**(§3.3) — `docs/107`의 빈자리.
6. **결정적 부분집합 정리 + workers 1/2/4/8 byte-equal 게이트**(§3.4).
7. **경계 검사 집중 + 동기화 사다리**(§3.6) — measure-first.
8. **예산 축 추가**(§3.9), **다중 Zone 순서**(§3.7).

**베타 후로 유보:** 분리(detach) 표면·`PGY_CAP_DETACH`, commit 경계/replay/
deterministic zone, 반응형 형 B(`docs/182` §3 BDFL 결정 3건 대기), Wasm/WASI.

**하지 않는 것:** 새 표면 키워드(`parallel for`/`reduce`/`unordered`/
`atomic`/`async scope`/task `detach`), mode 주석 노출, AIR lowering,
rwlock 즉시 제거.

---

## 5. 증명 목표

| 제안의 정리 | Pergyra 대응 | 상태 |
|---|---|---|
| 1. Capability Non-Forgery | capability 마스크는 매니페스트/env grant로만 넓어짐; 생성 시 자식은 부모 마스크의 snapshot(향후 edge로 축소) | `AsyncContextCore.v`가 exact mask/budget-owner/instance capture와 lane·suspension 보존을 bounded model로 증명; runtime/adequacy gate가 구현에 결박. 전체 grant-source 정리는 아님 |
| 2. Data-Race Freedom | `docs/semantics/proofs/WitnessDataRace.v` + `op_guard` 목격자 | 허용 부분집합에 대해 형태 존재 |
| 3. Slot Temporal Safety | generation 검사 + §3.3 재검증 | 런타임 검사 있음, 정리 없음 |
| 4. Structured Task Containment | §3.1 | `AsyncLifecycleCore.v`가 live trace의 scope closure에는 await/explicit own transfer가 필요하고 suspend·Cancel은 retire하지 않으며 대안 경로 불일치는 fail-closed함을 bounded model로 증명 |
| 5. Deterministic Parallel Subset | §2.6·§3.4 | 산문 계약만 |

주장 범위는 `docs/113` "Explicitly Out Of Beta"를 그대로 따른다:
`unsafe`·베타 외 표면에 DRF를 약속하지 않는다.

---

## 6. 위험 — 제안 §37을 Pergyra에 구체화

1. **annotation 폭발** → 이미 원칙이 "파생 fact"(`docs/146`). 표면에
   mode를 내지 않는다.
2. **Zone이 너무 coarse** → 현재 접근자가 매 접근 rdlock(§3.6). 분할·field·
   index 도메인 증거로 내려가지 않으면 giant lock이 된다.
3. **컴파일러 메모리** → 봉인 후 소거는 정전(`docs/178`); 드라이버 메모리 벽
   이력(38.5GB→3GB)이 교훈이다. ConcurrencyPlan에 문자열을 넣지 않는다.
4. **await 후 자동 재검증의 마법화** → 먼저 `docs/107`의 이름 붙은 계약
   (§3.3)으로 명시 규칙, 자동화는 그 뒤.
5. **FP 결정성** → fold 순서는 고정됐으나 SIMD/FMA/백엔드 차이는 별도
   numeric profile 필요; lockstep 목표 시 필수.
6. **FFI** → `unsafe {}`는 현재 lexical marker뿐(`docs/132`). C 포인터
   하나가 증거를 무너뜨리므로 scoped unsafe capability가 선행되어야 한다.
7. **(추가) 무음 가용성 폴백** → 풀 미초기화 시 inline 실행은 stderr 경고뿐
   (`pgy_parallel_spawn.h:31-38`). 안전 폴백이 아니라 가용성 폴백이지만
   "관측 가능"의 기준을 진단 객체로 올릴지 결정이 필요하다.

---

## 7. 한 문장

> Pergyra의 동시성은 스레드를 다루는 언어가 아니라, **권한·수명·데이터
> 의존을 먼저 증명하고 그 증명에서 실행 계획을 파생하는** 언어다 —
> `parallel`은 증명 의무, `async`는 가시성, scope가 수명, capability가
> 권한, Zone·Slot이 자원과 시간적 정체성, Intent가 허용된 순서, 그리고
> 스레드·rwlock·work-stealing·fiber는 전부 봉인된 plan의 materialization.

이것은 `docs/198`의 한 문장 — 의도·권한·수명·예산이 닫히지 않은 효과는
실행되지 않는다 — 의 동시성 판이다.

---

## Related

- `docs/53_parallel_core_policy.md` — parallel 코어 정책(고정 계약 §5)
- `docs/113_memory_concurrency_model.md` — 동결 계약(실행 계약의 유일한 자리)
- `docs/114_async_model_positioning.md` — coloring decomposition
- `docs/146_sea_execution_lanes.md` — BoundaryCaptureFact / ExecutionLane
- `docs/168_fortran_parallel_evidence.md` — ReductionFact·ProjectionFallbackFact
- `docs/178_parallel_boundary_evidence.md` — 증거 4종, evidence lifetime
- `docs/181_parallel_surface_full_design.md` — 형 A join 표면(R0–R5)
- `docs/182_parallel_remaining_bones_work_orders.md`, `docs/186_parallel_full_implementation_plan.md` — 남은 구멍과 순서
- `docs/104_air_compiler_architecture.md` — AIR는 검증 전용
- `docs/196_content_instance_runtime_context.md` — 컨텍스트가 TLS인 현재
- `docs/198_market_safety_positioning.md` — 우선순위와 한 문장
- `docs/107_beta_stable_subset.md` — 베타 폐쇄, "checked suspension contract"
- `docs/132_unsafe_capability_scope.md` — FFI 경계

---

## 부록 A. 원문 — PSCC v0.1 제안 (2026-08-21, 사용자 분석)

아래는 심사 대상 원문이다. 본문 §0-§7의 판정이 이 원문을 대체한다. 각주의
외부 연구는 이 저장소에서 검증되지 않았으며, 외부 주장의 근거로 쓰지
않는다.

### Pergyra 동시성 모델 설계안 v0.1

제가 2025–2026년의 관련 PL 연구를 Pergyra의 기존 `World → Zone → Intent → Slot` 축에 맞춰 다시 설계한다면, 임시 명칭은 **Pergyra Structured Capability Concurrency — PSCC**로 잡겠습니다.

핵심은 한 문장입니다.

> **`parallel`은 병렬 실행 가능성을 증명하는 키워드이고, `async`는 계산의 수명과 suspension을 구조화하는 키워드다. Capability는 그 계산의 권한을, Zone은 자원 영역을, Slot은 시간적 identity를, Intent는 허용된 목적을 담당한다.**

이들을 하나로 뭉개지 않습니다.

#### A.1 최상위 의미론

```text
World
│
├─ Zone
│   │
│   ├─ Slot<T>
│   ├─ Resource
│   │
│   └─ TaskScope
│       ├─ Task
│       ├─ Task
│       └─ Task
│
├─ Intent
│      ↓ constrains
│   Capability
│
└─ ConcurrencyPlan
       ├─ parallel dependencies
       ├─ capability flow
       ├─ task lifetime
       ├─ cancellation
       └─ deterministic commit
```

| 축 | 의미 |
|---|---|
| `parallel` | 이 계산들이 **동시에 실행 가능함을 증명하라** |
| `async` | 이 계산은 **중단·재개될 수 있다** |
| `await` | child task와 다시 합쳐지는 synchronization boundary |
| `Zone` | resource/lifetime/concurrency domain |
| `Slot` | 객체의 identity + generation |
| `Capability` | 실제 행위 권한 |
| `Intent` | 어떤 종류의 행위를 허용하는가 |
| `TaskScope` | task lifetime tree |
| `ConcurrencyPlan` | compiler가 확정한 실행 제약 |
| backend | 실제 thread/job/fiber/Wasm task로 materialize |

```text
parallel != thread
async    != thread
Zone     != lock
Intent   != effect
Capability != mutex
```

#### A.2 `parallel`은 강하게 연다

Pergyra에서 CPU 병렬성의 주 surface keyword는 **`parallel` 하나로 충분합니다.**

```text
parallel for enemy in enemies
{
    enemy.nextPosition =
        enemy.position + enemy.velocity;
}
```

컴파일러가 이를 만났을 때 의미는 "스레드를 생성해라"가 아닙니다. 정확히는 **"이 반복의 iteration들이 동시에 실행될 수 있음을 증명하라."** 입니다.

```text
병렬 안전성이 증명됨
        │
        ▼
ParallelAdmission = Yes
        │
        ▼
runtime profitability
        │
   ┌────┴─────┐
   ▼          ▼
parallel    sequential
execute     execute
```

작업이 12개밖에 없으면 runtime이 sequential execution을 선택해도 됩니다. 하지만 "병렬 안전성을 증명하지 못함 → 몰래 sequential fallback"은 compile error여야 합니다.

> **안전하지 않아서 순차 실행하는 fallback은 금지. 안전하지만 병렬화할 가치가 없어서 순차 실행하는 것은 허용.**

MLIR Async도 비슷하게 실제 동시 실행을 강제하지 않고 sequential execution을 합법적인 lowering으로 인정하면서, 비동기 dependency는 반드시 명시하도록 설계되어 있습니다.[^1]

#### A.3 `parallel`의 실제 증명은 Access Effect로 한다

각 함수/블록에 compiler-only `AccessSummary`를 만듭니다. 사용자가 대부분 직접 쓰지 않습니다.

```text
AccessSummary {
    reads;
    writes;
    atomics;
    reductions;
    transfers;
    kills;
    hostEffects;
    nondeterministicEffects;
}
```

두 작업 `A`, `B`가 있을 때 기본 충돌 조건은 대략 `W_A ∩ (R_B ∪ W_B) ≠ ∅` 또는 `W_B ∩ R_A ≠ ∅`이면 병렬화 불가입니다. 단, Atomic / Reduction / Transfer / partition proof / explicit ordering이 있으면 별도 규칙을 적용합니다.

#### A.4 Pergyra에서는 이 분석을 field보다 더 일반적으로 해야 한다

`Enemy[10].Position`과 `Enemy[892].Position`은 동시에 실행할 수 있으므로 접근 identity는 최소한:

```text
AccessPath {
    zoneId;
    resourceId;
    slotDomain;
    fieldId;
    indexDomain;
}
```

```text
different Zone            → disjoint
different unique Slot     → disjoint
partitioned index range   → disjoint
SoA chunk [0..1024) vs [1024..2048) → disjoint
```

#### A.5 정적으로 모르는 경우도 병렬화를 포기할 필요는 없다

PLDI 2025의 **Dynamic Region Ownership for Concurrency Safety**는 region 단위 ownership을 동적으로 검사해 잘못된 ownership을 설명 가능한 deterministic failure로 바꾸는 방식을 연구했습니다.[^2]

```text
StaticParallelAdmission
        ↓
증명 완료?
   ┌────┴────┐
  yes       partial
   │          │
   │     RuntimeGuardPlan
   │          │
   └────┬─────┘
        ▼
ConcurrencyPlan
```

실제 Slot들이 겹치는지 compile time에는 모른다면 "모든 write partition이 disjoint인가"를 **작업 시작 전에 한 번** 검사합니다. 실패하면 `ParallelAdmissionError`입니다. 이미 일부 실행한 뒤 rollback하는 것이 아닙니다. 그리고 guard fail → sequential fallback은 기본적으로 하지 않습니다. 프로그래머가 `parallel`이라고 썼으므로 계약 위반입니다.

#### A.6 Capability는 "종류"가 있어야 한다

**Classifying Capabilities**는 Scala 3 capture checking에 capability classifier를 추가하여 Future가 thread-local capability를 capture하지 못하게 하는 식의 제약을 표현합니다.[^3]

```text
Capability
├─ Memory:   Read · Write · Atomic · Reduce · Transfer
├─ Host:     File · Network · Clock · Random
├─ Control:  Spawn · Cancel · Detach
└─ Affinity: Main · Render · ThreadLocal
```

async task가 생성될 때 capture set을 검사할 수 있습니다. `Read<AssetDB>`, `Network`, `ThreadLocal<Renderer>`를 capture했다면 일반 worker로 이동할 수 없습니다: `error: task captures non-portable capability Renderer`.

#### A.7 그런데 OxCaml처럼 모든 mode를 surface syntax에 노출할 필요는 없다

POPL 2025의 **Data Race Freedom à la Mode**는 `contention`과 `portability`라는 mode 축을 추가합니다.[^4] 연구적으로 훌륭하지만 Pergyra가 `portable / contended / uncontended / forkable / unique / shared …`를 사용자에게 요구하는 것은 반대합니다. Pergyra에서는 대부분 **파생 fact**여야 합니다.

```text
Portable(Task) = CapturedCapabilities excludes ThreadLocal && all resource carriage valid
Forkable(Task) = write sets partitioned || read-only || atomic/reduction admitted
```

#### A.8 async는 `parallel`과 완전히 다른 의미를 가진다

```text
parallel = 동시에 계산해도 되는가?
async    = 계산이 중간에 중단되고 나중에 계속될 수 있는가?
```

`async foo()`는 CPU 병렬성을 의미하지 않습니다. 실제로 같은 worker에서 실행될 수도 있습니다. Swift도 async와 parallel을 구별하고 Task/TaskGroup을 계층화하며, JDK 26의 JEP 525 역시 Structured Concurrency를 제공하고 있습니다.[^5]

#### A.9 Pergyra async는 Structured Concurrency를 기본으로 한다

**고아 task가 기본적으로 존재하지 않게 합니다.** 부모 scope는 children complete or cancelled 되기 전에 소멸할 수 없습니다.

```text
async scope
{
    let texture = async loadTexture();
    let mesh = async loadMesh();
    return createModel(await texture, await mesh);
}
```

#### A.10 `detach`는 별도의 권한이다

`async fireAndForget(); return;`은 허용하지 않습니다. 정말 background task가 필요하다면 `detach world.background { ... }`처럼 하고 내부적으로 `requires Capability<Detach>`를 요구합니다. sandboxed mod에는 이 Capability를 안 주면 됩니다.

#### A.11 비동기에서 핵심은 Revocable Capability다

PLDI 2026 **Typestate via Revocable Capabilities**는 capability 수명을 lexical scope에 고정하지 않고 함수가 capability를 **받고, revoke하고, 다시 반환**할 수 있도록 flow-sensitive하게 확장합니다.[^6]

```text
Parent  Write<ZoneA> ──lend──▶ Child  Write<ZoneA>
Parent: Write<ZoneA> = Revoked
Child completes → capability return → Parent Write<ZoneA> restored
```

#### A.12 Capability carriage는 네 종류면 대부분 해결된다

`share`(read capability), `split`(disjoint write partition), `lend`(임시 ownership transfer, Parent → Child → Parent), `move`(영구 transfer). 이것들을 처음부터 source keyword 네 개로 만들 필요는 없습니다. compiler fact로 먼저 만드세요. **Free to Move**가 참고자료입니다.[^7]

#### A.13 `LiveRef`는 `await`를 그냥 넘어가면 안 된다

```text
let enemy = resolve(enemySlot);
let path = await pathfind(...);
enemy.path = path;
```

사이에 Enemy가 죽었다가 Slot이 재사용될 수 있습니다. 따라서 **`LiveRef<T>`는 기본적으로 suspension boundary를 넘지 못한다.** 넘어갈 수 있는 것은 `SlotRef<T> { slotId; generation; }`이고, resume 후 generation check로 `LiveRef`를 다시 얻습니다.

#### A.14 stale async problem을 언어에서 막을 수 있다

```text
Enemy Slot #125 generation 17 → pathfinding async 시작 → Enemy despawn (generation 18)
→ pathfinding 완료 → resolve Slot(125,17), current = 18 → Stale
```

결과는 적용되지 않습니다. Pergyra는 **temporal world safety**를 언어 핵심으로 만들 수 있습니다.

#### A.15 매 field access마다 generation/Zone/capability를 검사하면 안 된다

```text
Concurrency boundary → Slot check → Zone epoch check → Capability admission
→ Intent admission → LiveRef / AdmittedOperation → HOT LOOP: direct access
```

검사 위치는 spawn / parallel region entry / await·resume / Zone transition / commit / Slot resolve입니다. **검사 비용을 경계에 몰아넣습니다.**

#### A.16 `await` 중 lock을 잡고 있지 못하게 해야 한다

`await` 시점에 capability는 세 부류가 됩니다: **SuspendSafe**(immutable value, SlotRef, pure data — 그냥 넘어감) · **Revocable**(suspend 전에 반납, resume 때 재취득/재검증) · **NonSuspendable**(compile error: `exclusive Zone capability cannot cross await`).

#### A.17 `Zone`은 lock이 아니라 "resource universe"여야 한다

**When Lifetimes Liberate**는 arena처럼 여러 resource를 하나의 lifetime group에 묶으면서도 필요한 resource는 별도로 non-lexical lifetime으로 관리할 수 있게 합니다.[^8] Zone member라고 해서 모두 정확히 같은 순간에 죽을 필요는 없습니다.

#### A.18 현재 `pthread_rwlock_t`는 결국 fallback이어야 한다

**Pergyra가 최종적으로 Zone마다 rwlock을 잡는 언어가 되면 실패입니다.** 현재 구현은 bootstrap으로서 의미가 있습니다.

```text
static disjointness        → no lock
partition                  → no lock
unique transfer            → no lock
reduction                  → specialized reduction
atomic                     → atomic instruction
unresolved shared mutation → runtime synchronization (rwlock = 마지막 fallback)
```

#### A.19 multi-Zone deadlock도 compiler가 구조적으로 막을 수 있다

`ConcurrencyPlan { zoneSet = { player, inventory }; }`을 만든 다음 runtime acquisition order를 canonicalize합니다(sort by ZoneId). 더 좋은 경우는 lock조차 필요 없이 capability transfer/dependency edge로 해결합니다.

#### A.20 `parallel`은 기본적으로 deterministic하게 만드는 것을 권한다

`parallel`의 observable semantics를 **canonical sequential execution과 동일**하게 정의합니다. 이를 깨는 코드에는 별도 권한(`parallel unordered`)을 요구합니다.

#### A.21 Reduction은 deterministic tree를 써야 한다

floating point는 연산 순서에 따라 결과가 달라지므로 기본은 logical index → fixed reduction tree입니다. 자유로운 reduction 순서를 원하면 `unordered`처럼 명시하는 편이 낫습니다.

#### A.22 async completion은 deterministic simulation을 직접 mutate하지 못하게 한다

완료 순서를 그대로 simulation mutation 순서로 쓰면 replay가 깨집니다. deterministic Zone에서는 Async completion → result queue → deterministic commit boundary → World mutation으로 합니다(canonical TaskId/input order).

#### A.23 의도적인 nondeterminism은 숨기지 말고 기능으로 만든다

`select / race / first`는 `Capability<Nondeterminism>`을 요구합니다. deterministic simulation Zone에서는 금지: `error: race completion order is nondeterministic inside deterministic Zone`.

#### A.24 DORADD의 아이디어도 게임 runtime에 참고할 만하다

PPoPP 2025의 **DORADD**는 single dispatcher가 deterministic dependency graph를 만들고 worker pool이 독립적인 작업을 실행하는 구조입니다.[^9] 게임 system dependency 상당수는 매 frame 새로 발견할 필요가 없으므로 compile once → ConcurrencyPlan → every tick instantiate chunks → execute로 갑니다.

#### A.25 Compiler IR에는 parallel을 끝까지 남긴다

Tapir의 통찰은 fork-join parallelism을 단순 runtime call로 일찍 낮추면 compiler가 parallel control structure를 보면서 최적화하기 어렵다는 것입니다.[^10] Pergyra MIR에는 최소한 `ParRegion · ParFor · TaskScope · TaskSpawn · TaskAwait · TaskCancel · CapSplit · CapLend · CapReturn · CapMove · Revalidate · Reduction` 같은 semantic node/fact가 있어야 합니다.

#### A.26 MIR에서는 schedule이 아니라 "제약"을 들고 있어야 한다

MIR은 `A → C, B → C` 같은 dependency만 semantic truth로 갖습니다. `A worker 3, B worker 7`은 semantics가 아닙니다. **Parallel Semantics Program Dependence Graph**도 같은 문제를 다룹니다.[^11]

#### A.27 IR 계층 추천

```text
Source → Semantic AST (AccessSummary · CaptureSet · CapabilityFlow · ParallelAdmission)
       → MIR (semantic dependency graph)
       → ConcurrencyPlan (TaskId · dependency IDs · access-set IDs · capability-edge IDs · reduction plan)
       → AIR (explicit spawn/token/await graph)
       → Backend
```

MLIR Async처럼 AIR 수준에서는 explicit token/value dependency로 내리는 것도 좋습니다.[^1]

#### A.28 Rich analysis 정보는 이 시점에 반드시 지운다

현재 Pergyra compiler의 가장 큰 약점 중 하나가 이미 **semantic state/IR live-set의 크기**입니다. rich AccessSummary → ParallelAdmission → seal ConcurrencyPlan → AccessSummary/alias graph/capture analysis ERASE. 남는 것은 `ConcurrencyPlan { planId; taskIds; dependencyIds; accessClassIds; capabilityEdgeIds; }` 정도. 문자열-rich 구조를 절대 backend까지 끌고 가지 마세요.

#### A.29 `Intent`는 effect system으로 만들지 말 것

```text
ActualEffectSummary  ─must be subset of─▶  CapabilitySet  ─must satisfy─▶  IntentPolicy
Effect = 실제로 무엇을 하는가 · Capability = 무엇을 할 권한이 있는가 · Intent = 어떤 권한 사용이 허용되는가
```

#### A.30–A.32 Sandbox / Wasm / quota

2026년 6월 WASI 0.3은 Component Model에 native async(`async func`, `future<T>`, `stream<T>`)를 넣었고, WASI application은 ambient authority가 없는 capability sandbox에서 실행됩니다.[^12][^13] threading built-ins는 아직 candidate이므로[^14] Pergyra `parallel`을 Wasm 내부 thread로 번역할 필요 없이 guest ConcurrencyPlan → host scheduler → worker execution으로 실행할 수 있습니다. sandbox는 `SandboxBudget { memory; fuel; maxTasks; maxParallelism; maxZones; maxSlots; maxHandles; ioBytes; }`를 capability로 관리합니다. Wasmtime은 `ResourceLimiter`와 fuel/epoch interruption을 제공하며, deterministic interruption이 필요하면 fuel을 쓰라고 명시합니다.[^15][^16]

#### A.33 전체 runtime 모델

```text
                    Pergyra Source
                          │
       ┌──────────────────┴───────────────────┐
    parallel                                async
       ▼                                      ▼
Parallel Admission                      Task Lifetime
  R/W sets · partitions ·                 TaskScope · cancellation ·
  reduction · determinism                 await · capture
       └───────────────┬──────────────────────┘
                       ▼
                Capability Flow  (share/split/lend/move)
                       ▼
                  Zone / Slot   (Zone epoch · Slot generation)
                       ▼
               ConcurrencyPlan
          ┌────────────┼─────────────┐
      native       LLVM/AIR       Wasm host
          ▼
   worker pool + async IO
```

#### A.34 최소 surface syntax는 오히려 작게 유지할 수 있다

처음에는 `parallel · parallel for · reduce · async · await`만 있어도 충분합니다. 고급 기능으로 나중에 `detach · atomic · unordered · select/race`. Capability와 access effects 대부분은 inference합니다. **Pergyra가 Rust처럼 사용자가 생명주기 문법을 배우는 언어가 될 필요는 없습니다.**

#### A.35 구현 순서

Phase 1 `parallel for`(read-only · disjoint Slot writes · SoA partitions · fixed deterministic reductions; 1/2/4/8 workers × 1k/10k/100k entities) → Phase 2 Capability Flow(share/split/lend/move를 내부 IR에, 표면 노출 없이) → Phase 3 Structured `async`(TaskScope/Task/await/cancel; no implicit detached task; no NonSuspendable capability across await) → Phase 4 Slot temporal safety(LiveRef cannot cross await; SlotRef can; resume → revalidate) → Phase 5 deterministic async commit(tick boundary · canonical commit) → Phase 6 multi-Zone + sandbox(Wasm Component · fuel · quota · host capability).

#### A.36 무엇을 증명해야 하는가

최소 theorem 다섯: **1. Capability Non-Forgery · 2. Data-Race Freedom · 3. Slot Temporal Safety · 4. Structured Task Containment · 5. Deterministic Parallel Subset.** 특히 "스케줄러가 달라도 deterministic subset의 최종 observable World state는 동일하다." DRFcaml 역시 Iris/Rocq 기반 semantic model로 data-race freedom soundness를 증명하고 있습니다.[^4]

#### A.37 가장 큰 위험 6개

**첫째, capability annotation 폭발**(classifier와 inference로 surface annotation 최소화). **둘째, Zone이 너무 coarse해지는 것**(모든 접근이 Zone write가 되면 giant lock; Slot/partition/field 분석 필요). **셋째, compiler 메모리 폭발**(concurrency graph를 200MB+급 MIR에 붙이면 안 됨; plan sealing 후 즉시 erasure). **넷째, `await` 후 자동 재검증을 너무 마법처럼 만드는 것**(LiveRef lifetime rule을 먼저 명확히). **다섯째, floating-point determinism**(SIMD/FMA/backend 차이; network lockstep이면 deterministic numeric profile 필요). **여섯째, FFI**(C pointer 하나가 alias/resource guarantee를 무너뜨림; `foreign/unsafe` boundary를 Capability와 별도로 격리).

#### A.38 관련 연구 매핑

| 연구 | Pergyra에 가져올 것 |
|---|---|
| Data Race Freedom à la Mode, POPL 2025 | contention/portability 개념, DRF |
| Dynamic Region Ownership, PLDI 2025 | static proof 불가능할 때 deterministic runtime ownership guard |
| Typestate via Revocable Capabilities, PLDI 2026 | task에 capability lend/revoke/return |
| Classifying Capabilities, 2026 | async capture 가능한 capability 종류 구분 |
| When Lifetimes Liberate, OOPSLA 2026 | Zone arena + 개별 resource lifetime |
| Free to Move, 2025/26 | use/kill/transfer semantics |
| Reachability Types 계열 | alias/separation 분석 |
| Tapir | parallel semantics를 IR에 보존 |
| PS-PDG | schedule이 아니라 최소 dependency constraint 보존 |
| DORADD, PPoPP 2025 | deterministic dependency scheduler |
| Structured Concurrency / Swift / JEP 525 | task tree, cancellation lifetime |
| WASI 0.3 | sandbox async + capability host boundary |

중요한 것은 **어느 한 논문을 베끼는 모델이 아니라는 점**입니다.

#### 최종 제안(원문)

> **1. `parallel` is a proof obligation.** `parallel`은 thread를 생성하라는 명령이 아니라, 해당 계산이 스케줄 순서와 무관하게 병렬 실행 가능함을 compiler가 증명하라는 선언이다.
> **2. `async` is a lifetime construct.** `async`는 병렬성을 뜻하지 않는다. 모든 async task는 구조화된 TaskScope에 속하고, capability의 capture·revocation·return과 Slot/Zone의 시간적 유효성을 따른다.
> **3. concurrency is materialized, not encoded in the language.** thread, rwlock, work-stealing, IOCP, Wasm future 등은 모두 `ConcurrencyPlan`의 backend materialization이다.

(본문 §2.1의 정정: 2번 문장의 "`async` is a lifetime construct"는 채택하지 않는다. 수명은 scope가 소유한다.)

[^1]: https://mlir.llvm.org/docs/Dialects/AsyncDialect/ — MLIR 'async' Dialect
[^2]: https://www.microsoft.com/en-us/research/publication/dynamic-region-ownership-for-concurrency-safety/ — Dynamic Region Ownership for Concurrency Safety
[^3]: https://arxiv.org/abs/2607.24504 — Classifying Capabilities (Extended Version)
[^4]: https://popl25.sigplan.org/details/POPL-2025-popl-research-papers/23/Data-Race-Freedom-la-Mode — Data Race Freedom à la Mode (POPL 2025)
[^5]: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/concurrency/ — Swift Concurrency
[^6]: https://pldi26.sigplan.org/details/pldi-2026-papers/80/Typestate-via-Revocable-Capabilities — Typestate via Revocable Capabilities (PLDI 2026)
[^7]: https://arxiv.org/abs/2510.08939 — Free to Move: Reachability Types with Flow-Sensitive Effects
[^8]: https://arxiv.org/abs/2509.04253 — When Lifetimes Liberate: A Type System for Arenas with Higher-Order Reachability Tracking
[^9]: https://www.microsoft.com/en-us/research/publication/doradd-deterministic-parallel-execution-in-the-era-of-microsecond-scale-computing/ — DORADD (PPoPP 2025)
[^10]: https://experts.illinois.edu/en/publications/tapir-embedding-fork-join-parallelism-into-llvms-intermediate-rep/ — Tapir
[^11]: https://arxiv.org/abs/2402.00986 — The Parallel Semantics Program Dependence Graph
[^12]: https://wasi.dev/releases/wasi-p3 — WASI 0.3
[^13]: https://wasi.dev/ — WASI introduction
[^14]: https://wasi.dev/roadmap — WASI roadmap
[^15]: https://docs.wasmtime.dev/api/src/wasmtime/runtime/limits.rs.html — Wasmtime ResourceLimiter
[^16]: https://docs.wasmtime.dev/api/src/wasmtime/config.rs.html — Wasmtime fuel / epoch interruption
