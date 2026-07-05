# 162. Target #4 — 양 백엔드 통합 MIR 소비 실행 설계도

Status: `execution-blueprint`. 작성 2026-07-05. 레드팀 R2(C/LLVM 비대칭 = 가장
깊은 리스크)의 **구조적 치료**를 파일·rung 단위로 고정. 상위: docs/137 R2,
docs/38(MIR-only 마이그레이션 — **stale, 2026-04-12**), docs/125(SoT spine —
현 SoT), docs/36(IR 아키텍처 최소성). ★정직 우선순위: 이건 **최고-비용·최저-
사용자가치** 트랙(내부 parity 구조, 사용자 표면 아님) — 다른 세 트랙(M2/stdlib/
WASM) 뒤에 시퀀스. 그러나 dual-backend가 **dominant cost(113k LOC = thesis 2배
세금)**라 근본 부채 감축의 유일한 구조적 수.

---

## 0. 한 장 요약

- **문제(R2)**: C backend(194 `transpiler_*.c`) + LLVM backend(210 `llvm_*.c`)는
  거의 **독립적 이중 구현**. parity 게이트(behavioral output 비교)는 **divergence를
  사후에 잡을 뿐 구조적으로 막지 못한다** = 필요조건이지 충분조건 아님. 이번
  세션의 버그 전부가 C/LLVM divergence를 parity가 사후 포착한 사례(LLVM Args
  크래시, budget 멀티인스턴스, generic 역-비대칭).
- **★구조적 발견(2026-07-05 실측)**: MIR-only 파이프라인(HIR→DIR→RIR→MIR)은
  됐고 body fallback도 제거됐다. **그런데 MIR 소비자가 백엔드별로 분리돼 있다** —
  `transpiler_mir_inventory_*.{c,h}`(C가 읽음) vs `llvm_mir_*.c`(LLVM이 읽음).
  같은 MIR fact를 **두 독립 리더가 각자 해석** → 리더가 drift 가능 = 남은 ~17%
  divergence surface.
- **치료(target #4)**: 양 백엔드가 **같은 MIR inventory를 소비**하게. 두 방향:
  (A) 남은 **AST-carried 소비 경로**를 MIR/ABI fact 소비로 닫기(divergence 원천
  제거), (B) 중복되는 두 MIR 리더를 **공유 neutral 소비자**로 추출(divergence를
  "두 리딩"→"한 리딩의 두 lowering"으로 이동 = 구조적 parity).
- **현 진척 ~83%**(R2). MIR-only 마이그레이션·decl-IR residue는 닫힘(docs/125
  row 617, 2026-06-23). 남은 것은 §2.

---

## 1. 실측 현재 상태

- **파이프라인 고정**(docs/38 §1): `HIR→DIR→RIR→MIR→backend`. backend는 HIR
  shape로 분기 안 함, HIR fallback 없음. C=`transpile_with_mir`, LLVM=
  `llvm_codegen_*_with_mir` 진입.
- **닫힌 것**(docs/125, memory project_mir_only_migration): body fallback 3패밀리
  (decl/methods/slots) 폐기, driver가 mir==NULL이면 hard-fail(production-dead
  fallback), row 617 "Dedicated declaration IR" Closed. IR 최소성 기계증명
  (IRMinimality.v): codegen=HIR→RIR→MIR 3층 최소.
- **분리된 MIR 소비자(divergence 원천)**:
  - C: `transpiler_mir_inventory_intent{,_collect,_alias_collect}.{c,h}` +
    `transpiler_mir_{emission_contract,func_emit,signature,pending_uses}.c` 등.
  - LLVM: `llvm_mir_emit.c`, `llvm_mir_await_emit.c` 등.
  - 둘이 같은 `MirModule`을 읽되 **각자의 순회·해석 코드**. 이 중복이 drift 표면.
- **parity 게이트(현 안전망, behavioral)**: `backend-compare`(output 동일),
  `air-strict-backend-compare`, `fuzz-backend-parity{,-matrix}`,
  `backend-compare-{inventory,llvm-coverage}`. **전부 산출물/행동 비교** — 구조가
  아니라 결과를 잠근다. 필요조건.

⚠️ **docs/38은 stale**(2026-04-12): §2 "아직 남은 것"의 declaration-inventory
항목은 이후 닫힘(docs/125 row 617). 현 residue는 docs/125 SoT spine이 정본 —
docs/38의 잔여 목록을 현재로 인용 금지.

---

## 2. 남은 divergence 표면 (닫을 대상)

target #4의 ~17%는 "양 백엔드 재작성"이 아니라 **특정 divergence 원천을 닫는 것**.
착수 전 docs/125 SoT spine에서 현 open row를 재확인(이 문서 작성 시점 추정):

1. **분리된 MIR 순회 로직 중복** — `transpiler_mir_inventory_*`와 `llvm_mir_*`가
   같은 MIR을 각자 순회. **최대 표면.** 두 리더가 같은 fact를 다르게 해석하면
   divergence(이번 세션 generic 역-비대칭이 이 계열).
2. **AST-carried 소비 잔여**(docs/38 §2, docs/125로 현재값 확인): main wrapper /
   top-level scheduling metadata의 MIR-중심화 여지, 일부 declaration
   registration/naming helper가 AST shape 소비, transpiler의 ABI-metadata 직접
   소비 경로 약함.
3. **소비자별 특수 경로** — 한 백엔드만 가진 lowering(예: LLVM aggregate
   lowering의 movable-handle, class_field 역-비대칭 — G-5). 이건 divergence가
   **의도된**(백엔드 능력 차) 경우와 **버그**인 경우를 구분해야(전자는 fail-closed
   거절로 잠금, 후자는 닫음).

---

## 3. Rung 사다리 (T4)

status ∈ {planned, landed}.

| rung | 내용 | 방법 | 게이트 |
|---|---|---|---|
| **T4-0** | **divergence 표면 측정** — 두 MIR 소비자(transpiler_mir_* vs llvm_mir_*)가 각자 소비하는 MIR fact 종류를 인벤토리화, 겹치는 순회 로직 식별 | 소스 센서스(순수 텍스트) | `dual-mir-consumer-inventory-smoke`(신설) |
| **T4-1** | **AST-carried 잔여 닫기** — main wrapper/naming/ABI-metadata를 MIR/ABI fact 소비로(§2-2). 각각 docs/125 row | row별 SoT 닫기(build-source-inventory 패턴) | 기존 mir-declaration-inventory-smoke + docs/125 row Closed |
| **T4-2** | **공유 neutral 소비자 추출** — 두 리더의 중복 순회를 `mir_consume_*`(백엔드-중립)로 뽑아 C/LLVM이 그 산출(neutral 소비 구조)을 lower만 | 리팩터: MIR→neutral consume→{C emit, LLVM emit} | backend-compare가 구조 불변 증명 |
| **T4-3** | **의도된 비대칭 명문화** — 백엔드 능력 차(LLVM aggregate 등)는 fail-closed 거절로 잠그고 divergence 등록부에 기록(버그 아님을 구분) | 등록부 + reject fixture | generic-nested-failclosed-smoke 계열 |

**핵심 rung = T4-2**(공유 소비자 추출). 이게 divergence를 "두 리딩"→"한 리딩의
두 lowering"으로 옮겨 **구조적 parity**를 만든다. T4-0(측정)이 선행 — 무엇이
중복인지 재야 뽑는다.

**정직한 규모:** T4-2는 큰 리팩터(두 백엔드의 MIR 순회를 통합). ~17%지만 dual-
backend 404파일에 걸친 수술. **점진적**으로: fact 종류별로(intent→decl→slot→
signature) 하나씩 공유 소비자로 이관, 매 이관마다 backend-compare green 유지.

---

## 4. 왜 이걸 하는가 / 안 하는가 (시퀀싱 판단)

**하는 이유:** dual-backend는 dominant cost(113k LOC, thesis 2배 세금,
project_complexity_management_gates). parity 게이트는 wallclock·유지보수 세금을
매 커밋 물린다. 공유 소비자는 그 세금의 구조적 감축 — R2의 유일한 근본 수.

**안 하는(미루는) 이유:** **사용자 표면 아님.** M2(self-host)·stdlib·WASM은
사용자가 보는 것(부트스트랩 증명, 도메인 어휘, 던전크롤러). target #4는 내부
parity 구조 — 끝나도 사용자에겐 "버그가 덜 난다"뿐. 로드맵상 후순위 정당.

**권고 시퀀스:** 다른 세 트랙(M2/stdlib/WASM)이 진행 중일 때 **T4-0(측정)만
먼저**(싼 센서스, divergence 표면을 수치화 = 앞으로의 divergence 버그를 이해하는
계기판). T4-1(AST 잔여)은 SoT 닫기 흐름에 편승. T4-2(공유 소비자 추출)는 **BDFL이
dual-backend 세금을 근본 감축하기로 결정할 때** 착수 — 그전엔 parity 게이트가
필요-충분은 아니어도 필요조건으로 작동 유지.

---

## 5. WO 등록

- **WO-T4-0** — divergence 표면 측정(dual-mir-consumer-inventory, 순수 텍스트).
  ★싸고 계기판 가치 — 다른 트랙 진행 중에도 가능.
- **WO-T4-1** — AST-carried 소비 잔여 닫기(docs/125 row별, SoT 흐름 편승).
- **WO-T4-2** — 공유 neutral MIR 소비자 추출(대형 리팩터, BDFL 결정 후).
- **WO-T4-3** — 의도된 백엔드 비대칭 등록부 + reject 잠금.

## Related

docs/137 R2(C/LLVM 비대칭 = 근본 리스크) · docs/38(MIR-only 마이그레이션 —
stale, 원 계획) · docs/125(SoT spine — 현 residue 정본) · docs/36(IR 최소성) ·
IRMinimality.v(codegen 3층 최소 기계증명) · project_complexity_management_gates
(dual-backend = dominant cost) · backend-compare 게이트 패밀리(현 behavioral
안전망) · task #42(보드 pending)
