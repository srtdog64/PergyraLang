# 10. Ability / Witness / Evidence / Contract — surface vs IR

**Status:** spec pinned 2026-06-20 (BDFL). The vocabulary and layering are
decided; this doc is the contract the implementation must conform to.
Companion: `08_slot_capability_calculus.md` (the token=evidence root),
`07_air_abstraction_safety.md` (AIR = evidence-as-audit), `00_proof_contract.md`.

## 0. The decision in one line

> **Surface polymorphism is `ability` (+ `role`). The compiler-internal proof
> that something satisfies an ability / carries the required facts is a
> `witness` (IR/DAG only, never surface syntax).** This is the
> typeclass/dictionary-passing architecture (Haskell class + dictionary; Rust
> trait + monomorphized impl): the user writes the *class*, the compiler
> synthesizes the *dictionary*. Pergyra is NOT trait-as-nominal-membership; a
> value passes a check by *presenting a witness of a proposition*
> (proof-carrying / capability).

## 1. Surface (user writes this)

```pergyra
ability Renderable {
    func Render() -> Void;
}

role PlayerCombat for Player impl ability Damageable
```

- **ability** — a behavioral contract a value/role provides (operations).
- **role** — the bundle of abilities a `subject` performs in a given context,
  and the binding of a subject to ability implementations there.
- The user never writes `witness`. Ability-polymorphism is the surface idiom.

## 2. Internal / advanced semantics (IR/DAG)

```
ability   satisfied by   witness     (구조 명제의 증명)
boundary  accepted by    evidence    (경계/권한 명제의 사실)
intent    checked by     contract    (실행요건 명제)
```

| 용어 | 명제의 종류 | 위치 |
|---|---|---|
| **ability** | 구조적: "T가 Render()를 제공" | surface |
| **evidence** | 경계/권한: "이 값은 parallel/channel/authority를 통과 가능" | semantic/runtime (AIR) |
| **contract** | 실행요건: "이 intent는 effect/authority/coordination을 요구" | semantic (intent) |
| **witness** | 위 *세 종류 명제 전부*를 증명하는 **단일 증명항** | **IR/DAG (신규)** |
| **role** | subject↔ability 문맥 바인딩(어느 구현이 적용되나) | surface |

**핵심 통일:** 표면엔 명제 3종(구조/경계/실행)이 있어도 **내부 증명 기계는
`witness` 하나**다. 그래서 5개 용어가 내부 복잡도를 5배로 만들지 않는다 — witness
하나가 ability satisfaction · evidence possession · contract fulfilment을 *균일하게*
증명한다.

**Coq 정합:** 런타임/IR witness ↔ 증명항 witness가 1:1 (Curry-Howard식). data-race
불변식(`AxisOwnership.ownership_unique` @ `AxExecution` on slot write-cap)이
denote하는 것 = "모든 동시성 경계 crossing은 *single-writer evidence*의 witness를
carry한다." 즉 witness-IR이 곧 형식증명의 분모.

## 3. 현재 상태 (구현 = reify + 연결, greenfield 아님)

| 조각 | 현존 | 갭 |
|---|---|---|
| ability/role 표면 | 31 parser + 60 semantic 파일 | — |
| ability satisfaction | `type_checker_ability_where.c` (`ability_generic_arg_satisfies`) — *암묵적 witness가 이미 거기* | **1급 IR witness로 reify** |
| evidence | AIR (3파일, evidence-as-audit) | witness가 evidence를 produce/consume하게 연결 |
| contract | intent 요건 (effect/authority/coordination, 432건) | witness가 contract 충족을 증명하게 연결 |
| **witness IR/DAG** | **없음 (grep 0)** | **신규 — 핵심 작업** |

→ "전체 구현"의 실체: **기존 암묵 satisfaction-check를 named 1급 witness IR 노드로
reify하고, ability/evidence/contract 체크가 witness를 생산·소비하게 잇는 것.** 바운디드.

## 4. 두 위험 — 결정적 규칙으로 닫아야 (락인 전 필수)

### 4.1 Coherence — role-scoped ambiguity → **설계로 우회됨 (audit 2026-06-20)**
*우려*: 한 subject가 여러 role에서 같은 ability를 다르게 구현하면 "어느 witness가 이기나?"
모호 — Haskell이 전역 유일성으로 닫고 Scala implicit이 안 닫아 악명 높은 지점.

*audit 결과*: Pergyra는 **이미 구조적으로 닫혀 있다.** ability는 전역 dispatch되지 않는다 —
`party`/`zone`의 **`dyn role slot: Ability`** 에 **명시적 `bind slot = Role`** 을 통해서만
호출된다(`tests/cases/backend_compare/party_role_bind_dispatch`). **bare subject 직접
호출(`p.Hello()`)은 fail-closed**(`Unknown method`, 진단 확인, C/LLVM 동일). 따라서 전역
instance가 없어 *coherence ambiguity가 발생할 수 없다* — "어느 witness가 이기나" = "명시적으로
bind된 그것". 동적 rebind는 `llvm_dyn_role_vtable_swap`이 게이트. → **결정적 규칙 불필요;
explicit-named-binding이 그 자체로 규칙.** (Scala가 못 닫은 걸 named-slot으로 닫은 셈.)

*잔여 확인*: slot 하나에 두 role을 bind 시도하거나, 한 role이 ability를 중복 impl할 때도
fail-closed인지 — BDFL이 role 활성 모델 소유자라 확인 위임.

### 4.2 Dispatch — static vs dynamic
witness IR은 둘 다 표현해야:
- **static (기본):** monomorphize, inline, zero-cost (Rust식). data-race "접근당 체크
  비용" 우려를 여기서 해소.
- **dynamic (`dyn`식, 필요시):** 런타임 witness dictionary, 유연·비용.
IR 노드가 static/dynamic을 *명시 carry*. 기본은 static.

## 5. proliferation 규율 (5개 용어 비대 방어)
- **ability↔witness 척추 = 잠금** (검증된 typeclass/dictionary).
- role/evidence/contract는 "ability+witness로 표현되나?"로 시험: 되면 *sugar*(label),
  안 되면 *primitive*(유지). evidence(경계)·contract(실행)는 ability(구조)와 방향이 달라
  (provide vs require/cross) primitive로 생존. role은 4.1 규칙과 함께 결정.
- **불변 규칙: witness는 표면에 절대 누출 금지.** 누출되면 사용자가 dictionary를 손으로
  꿰게 되어 (추론되는) trait보다 verbose = "경량" 파탄.

## 6. 구현 시퀀스 (staged, 각 게이트)
1. **witness IR 노드 정의** + 기존 `ability_generic_arg_satisfies`를 그 노드를 *생산*하게 reify (semantic→IR). C==LLVM parity 게이트.
2. **dispatch**: static monomorphize 경로 먼저(zero-cost), dynamic은 후속.
3. **coherence 규칙**(4.1) 구현 + ambiguity fail-closed 테스트.
4. **evidence/contract 연결**: 경계(parallel/channel/authority) 통과 + intent 체크가 witness를 소비. AIR에 witness 기록(queryable).
5. **Coq**: SlotCalculus.v에 witness/concurrency step 추가, data-race single-writer witness 건전성 (capstone, 설계 안정 후).

각 단계는 독립 게이트로 박는다 — 325k는 게이트로 운영
([[project_complexity_management_gates]]).

## 7. Refinement audit — boundary 타이핑 → WitnessDataRace step 모양 (2026-06-20)

`WitnessDataRace.v`는 **aliasing-xor-mutability**(slot은 *단일 writer 배타* 또는 *다중 reader
공존*) 불변식이 성립하면 data-race-free임을 기계증명했다 — **write-write *와* read-write 둘 다**
(`xor_mut_no_data_race`). 모델 step = `acquire-write`(배타) / `acquire-read`(공존) /
`release`, 셋 다 불변식 보존(`xor_mut_preserved`), 다중 reader 공존도 확인(`readers_share_ok`,
over-restriction 아님). 이 audit는 Pergyra 실제 경계 타이핑이 이 불변식을 강제하는지(refinement
의무)를 좁힌다.

| 경계 | 메커니즘 | step 대응 | 증거 |
|---|---|---|---|
| spawn / async | body+capture **move/consume** | `step_move` | `type_checker_async_channel.c` (move/consume 다수); *익명 spawn 캡처는 fail-closed 제한* ("move the body into a named async function") |
| channel send | ownership **transfer** | `step_move` | `slot_analyzer.c` (transfer own / channel send) |
| parallel / slot-view / world | **cannot-cross** fail-closed | (forbid) | `type_checker_flow_parallel.c:156`, `type_checker_slot_view_boundary.c:49`, `world_roster.h` Borrowed-handle |
| borrowed handle (pin/view) | **배타적: 원본은 view/pin live 동안 write 불가** | single-writer 강제 | `type_checker_builtins_slotops.c:111` `"Cannot write slot while ... is live"` (`PGY_CAUSE_PIN_PARALLEL_CONFLICT`); `SlotCalculus.v` Pin Non-Eviction |
| `shared` 필드 | **atomic**(동기화) | 메모리모델상 race-free | `docs/113:51` "atomic shared" |

**결론**: 경계 규율이 step 모양에 *맵핑된다* — move/consume(transfer) + **pin/view 배타성**(원본은
view live 동안 write 불가 = single-writer, `PIN_PARALLEL_CONFLICT`로 fail-closed) + atomic
shared + cannot-cross fail-closed. refinement 의무가 *"미지"→"맵핑됨 + 잔여 명시"*로 좁혀졌다.

**capstone 진행 (2026-06-20, WitnessDataRace.v에 기계검증):**
- **(a) 두-calculus 연결 — done.** SlotCalculus `ModePin`/Pin Non-Eviction의 배타성을
  `pin_exclusive` 규율로 정식화하고 `pin_exclusive_xor_mut`/`pin_exclusive_no_data_race`로
  증명 — §7의 "pin/view 배타성 = single-writer" 매핑이 이제 *정리*다. (두 .v의 타입을 literal
  통합하진 않고, 배타성 *명제*를 잇는 형태 — 형식적 충분.)
- **(b) boundary 타이핑 건전성 — done.** typed boundary calculus(`Op` =
  acquire-write/acquire-read/release, `op_guard` = checker가 강제해야 할 precondition)에서
  **well-typed 경계 프로그램 ⟹ data-race-free**(`well_typed_data_race_free`) 기계증명. "checker가
  안전 step만 방출"이 *비형식적 매핑*에서 *증명된 규율*로 승격.

**남은 단 하나의 갭 (정직, over-claim 방지):**
- **C checker ↔ op_guard refinement**: 실제 C 타입체커가 `op_guard`(no-current-access /
  no-current-writer)를 *정확히* 강제함을 보이는 것 — RustBelt가 λRust를 증명하고 rustc는 별개인
  바로 그 갭. *원리적으로 Coq로 C-impl을 증명할 수 없음* → §6 게이트(backend_compare, 경계
  fail-closed 테스트)로 경험적으로 지키는 것이 실용 종착.
- **익명 spawn 캡처**: fail-closed 제한(named async 강제), 완전 캡처-lifetime 분석은 미래(구멍 아님).
