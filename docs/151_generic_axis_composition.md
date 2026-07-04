# 151. Generic 의미축 합성 결정표 (Generic Semantic Composition Matrix)

Status: `design-draft, decision-gated`. BDFL 초안(2026-07-04 대화)과
레드팀 판정을 병합한 문서. **게이트 없음** — 두 미결정(§2 Decision-0,
§3 GATE 판정값)이 닫히기 전에는 이 표는 계약이 아니라 설계 초안이다.
게이트 조건은 §7. 배경: Swift 제네릭 교훈 — 원칙의 단순함이 아니라
**원칙과 언어 나머지의 합성**이 실패 지점이다. 제네릭이 만날 모든
축과의 조약을 제네릭보다 먼저 체결한다.

## 0. 한 줄 원칙 (BDFL 초안, 채택 — 문구 1건 수정)

> Pergyra에서 T는 타입 변수가 아니라 의미축 운반체다.
> 제네릭은 T의 의미축을 숨기지 못한다. 숨기려면 **선언**해야 하고,
> **증명은 컴파일러가 한다**.

- 첫 문장은 docs/121(타입 = 도메인 좌표 carrier)의 제네릭 따름정리다.
  초안이 canon을 독립 재유도했다 — 일관성 신호로 기록.
- 셋째 문장이 레드팀 수정: 초안의 "숨기려면 증명해야 한다"는 표면
  proof-term 언어(사용자가 증명 객체를 쓰는 방향)로 오독될 수 있다 —
  docs/121이 거절한 off-axis(dependent-type 노선)다. Rice 노선
  (docs/semantics/19 §✦: 의미를 구문 선언으로 끌어내려 결정가능화,
  잔차는 fail-close)에 맞춰 증명 주체를 컴파일러로 명시한다.

형식형 — **기계화 완료** (`docs/semantics/proofs/GenericAxisCarriage.v`,
coqc 0 admits/0 axioms, 2026-07-04, `formal-semantics-smoke` 배선):
축-운반은 기본 상향 단조, 유일하게 허용되는 하강은 ERASE 판정 지점뿐.

- `carriage_monotone` — erase-free spine에서 leaf 축 전부 생존
  (wrapper가 실수로 축을 잃을 수 없다)
- `descent_is_declared` — root에서 사라진 leaf 축은 반드시 어느 ERASE
  layer의 선언 집합에 있다 (**무음 하강 불가** = 세탁 불가)
- `erase_declared_scope` — ERASE는 선언한 것만 지운다 (미선언 축 생존)
- `carriage_no_conjuring` — root의 모든 축은 leaf 또는 생성자 자신의
  마크에서 온다 (출처 전수 — 축이 무에서 안 생긴다)
- `hiding_requires_declaration` — §0 문장 그 자체: 숨김 ⇒ 선언한
  ERASE layer 실존 ∧ spine은 erase-free가 아님

주의(negative scope, AIRBinding.v 전통): 이 파일이 증명하는 것은
운반 **법칙**이지 운반 **방식**(§2 Decision-0)이 아니다 — 법칙은
어느 carriage 방식 위에서도 성립하므로 Decision-0이 열린 지금도
증명 가능했다. §4 간선/컴파일러 구현 일치는 미래 matrix-lock 몫.

## 1. 7축 (초안 대비 개명 2건)

| # | 축 | 핵심 질문 | 초안 대비 |
|---|---|---|---|
| 1 | World | 어느 세계/도메인 모델의 값인가 | 유지 |
| 2 | Zone | 어느 실행/보안 경계 안의 값인가 | 유지 |
| 3 | Actor/Role | 누가 어떤 자격으로 다루는가 | 유지 |
| 4 | Authority/Capability | 이 값으로 무엇을 할 수 있는가 | 유지 |
| 5 | Intent/Effect | 어떤 목적·작용으로 쓰이는가 | 경계는 §4 간선으로 |
| 6 | **Site** | 어디에 꽂히는가 (field/param/handle/channel) | **개명** (초안 "Slot") |
| 7 | **Phase** | 이 의미는 어느 단계까지 살아 있는가 | **개명** (초안 "Phase/Lifetime") |

개명 사유:

- **Site**: 축 이름 Slot은 타입 `Slot<T>`와 같은 문서 안에서 충돌한다
  ("`Slot<T>`의 Slot축 판정은 Slot이 강함" 같은 문장이 성립 불가).
  자리-민감성 축은 Site로 부르고, Slot은 primitive 이름으로 보존한다.
- **Phase**: "Lifetime" 단어는 수명 주석 영구금지 결정(docs/118 §2.1,
  BDFL 2026-06-29)이 차단한 프레임을 문서 제목에 초대한다. 이 축의
  실제 의미는 값 수명이 아니라 **의미의 소거 시점**(compile-proof →
  MIR/AIR marker → erase/compress)이므로 Phase 단독 명명. 값-수명
  갭은 여전히 slot/zone/handle primitive 소관이다.

Phase의 위상(초안의 "Phase가 전체를 감싼다" 직관, 채택·구체화):
Phase는 표의 열이 아니라 **판정의 codomain**이다 — 각 셀에서 Phase는
ERASE 판정의 파라미터(어느 IR 단계에서, 어느 버킷으로 소거되는가)로
나타나고, erasure dashboard(docs/14)가 그 실측 장부다.

## 2. Decision-0 — 축의 운반 방식 (OPEN, BDFL 결정 대기)

초안 전문("T may carry: World membership, Zone locality, …")은 축이
**값에 실린다(value-carried)**를 전제한다. 현행 언어의 실물은 다르다:

- 대부분의 축은 **자리에 붙는다(positional)** — authority는
  zone/step(`authorized by:`), caps는 함수 effect mask, intent는 블록.
- 일부는 **런타임 태그(runtime-tag)** — slot 안전 태그(항상-on),
  lifecycle state tag(docs/12).
- zone만 **value-typed로 이동 중** — zone-bound handle typed
  evidence(WO-B4).

선례: 도메인-라이프사이클(docs/12)은 typestate(값-실림의 극단)를
거절하고 contract 선언 + 정적 N-상태 트래커 + 런타임 태그를 택했다.
값-실림은 공짜가 아니다 — Rust typestate의 `.clone()` 부채가 반례.

따라서 이 표의 row-0은 셀 판정 이전에 **축마다** 운반 방식을 정하는
결정이다:

```text
carriage(axis) ∈ { positional, value-typed, runtime-tag }
```

이 결정이 닫히기 전의 셀 판정은 전부 가설이다. value-typed 축이
늘수록 표는 typestate에 가까워지고(비용 선례 있음), positional로
남을수록 위험은 생성자 자리(Slot/Channel/spawn 경계)로 이동한다 —
후자가 현행 언어의 결(경계에서 증거)과 맞다. **레드팀 권고:
positional 기본, value-typed 승격은 zone(WO-B4)처럼 축별 증거로만.**

## 3. 판정값 — 초안 4값 채택 + 1값 제안 (GATE, OPEN)

| 판정 | 의미 | 실물 선례 |
|---|---|---|
| ALLOW | 그대로 허용 | — |
| REJECT | 정적 거절 | generic-over-constructed fail-closed 가드 (**현행**) |
| DEFER | MIR/AIR 증명까지 **정적** 유예 | AIR off-path 검증, machine-neutral facts |
| ERASE(bucket) | 검증 후 의미 소거 — docs/14 버킷 명명 의무 | 축 어휘 100% 소거 실측 |
| **GATE** (제안) | 허용하되 런타임 fail-closed 검사를 pin | caps의 동적디스패치/FFI backstop(docs/15), slot 검사 항상-on |

- **GATE가 필요한 이유**: 현행 강제 체계는 이미 2층이다 — 정적
  declared⊇used + 런타임 게이트 backstop. 이 판정값이 없으면 그
  셀들을 ALLOW(과대주장)나 REJECT(실프로그램 과소허용)로 오기입하게
  된다. DEFER는 정적-유예로 유지해 런타임-유예(GATE)와 구분한다.
- **ERASE 규율**: 모든 ERASE 셀은 소거 버킷(A 환원불가 / B 설계
  fail-closed / C 정적 미완성)과 소거 IR 단계를 명명하고 erasure
  dashboard에 실린다. **ERASE가 이 표의 진짜 신규성이다** — Swift는
  이 판정값이 없어서 "P인 값"이 세 철자(`<T:P>`/`some P`/`any P`)로
  갈라졌다. 소거를 판정값으로 가지면 표기가 갈라질 이유가 없다.

## 4. 간선 등록부 — 축만으로는 표가 닫히지 않는다

초안에서 effect는 Intent축("effect label")과 Authority축("permission
effect")에 양다리를 걸친다. 구현 실물은 effect 하나에 caps가
refinement로 붙는 구조(docs/15)다. AxisOwnership.v의
exactly-one-owner를 지키려면 축 목록만이 아니라 **사인된 cross-axis
간선**이 표에 병기되어야 한다:

| 간선 | 의미 | 실물 |
|---|---|---|
| cap ⊇ effect | capability가 effect를 게이트 | declared⊇used semantic error |
| authority ⊢ step | authority가 intent step을 승인 | world.pgy `authorized by:` |
| zone ⊃ site | zone이 site를 수용 | zone-bound handle |
| phase ⊒ * | 모든 축은 소거 단계를 갖는다 | erasure dashboard |

간선을 타는 검사(예: declared⊇used)는 두 축의 합성으로 기록하고, 한
축 칸에 몰아넣지 않는다. 간선 등록부에 없는 cross-axis 검사가
생기면 그것이 곧 설계 드리프트 신호다.

## 5. 1층 표 — 실존 생성자 (출발점 = 전-REJECT 래칫)

**현행 컴파일러의 실제 판정은 이미 존재한다: 구성타입 위 generic은
전부 REJECT다** (fail-closed 서명 가드, docs/147 발견-1). 따라서 이
표는 위시리스트가 아니라 **REJECT에서 시작해 증거로 셀을 여는
래칫**이다. 셀 개방의 전제: Decision-0 닫힘 + 해당 축 carriage 결정
+ MIR type-text seam 해소(§7).

| 생성자 | World | Zone | Actor | Auth | Intent/Eff | Site | 현행 판정 |
|---|---|---|---|---|---|---|---|
| `Option<T>`/`Result<T,E>` | 보존 | 보존 | 보존 | 보존 | 보존 | 약 | REJECT(중첩 generic), per-type 우회(stdlib/option) |
| `List<T>` | 보존 | 보존+T제약 | 보존 | 보존 | — | 약 | REJECT |
| `Slot<T>` | 보존 | 강 | — | **승격**: T가 auth-bearing이면 Slot도 auth-aware | 강 | 강 | REJECT + 런타임 GATE(항상-on) |
| **`Channel<T>`** | **유일 합법 cross-World 운반체** | 경계 통과=증거 | — | 위임 위험 | send/recv | 강 | REJECT |
| `own`/`ref T` | 보존 | 이동 기본 거절 | — | **위임**(ocap: 참조 전달=권한 위임) | — | 강 | 현행 own/ref 규칙 |
| `StrView` | 보존 | 차용 | — | 축소 | inspect | 표시 | WO-P1 lifetime fact = 첫 Phase fact |

초안 대비 수정 2건:

- **`Channel<T>` 행 신설.** cross-World는 Channel-only(분산 설계
  4원칙)라 언어에서 가장 하중이 큰 generic인데 초안 표에 없었다 —
  존재하지 않는 Future/Command/View가 자리를 차지하는 동안. World축
  합성의 본판정이 일어나는 곳이 바로 이 행이다.
- **`Ref`의 Authority 셀 상향** ("운반 가능" → "위임"). ocap 관점에서
  참조를 건네는 것은 권한을 위임하는 것이다. `Slot<T>` 행과 최소
  동급 위험으로 캘리브레이션. own/ref 명시 증거가 완화 요인이지
  면제 사유가 아니다.

## 6. 2층 — 사변 생성자 (판정 금지, 어휘 선점만)

`Future<T>` / `Command<T>` / `View<T>`는 표면에 존재하지 않는다.
docs/148 원장 규율대로 **sketch tier**: 판정 셀을 가질 수 없고
어휘만 선점한다.

- `Future<T>` — 초안의 "실행 Zone/Phase 고정 필요"는 실제로는 SEA
  lane fact(docs/146)의 **타입-가시 승격**을 요구한다. lane은 현재
  연산-증거(ExecutionLaneFact)지 타입-축이 아니다 — 이 승격은
  Decision-0급의 별도 결정이며, 표 셀 하나가 조용히 안고 갈 무게가
  아니다.
- `Command<T>` — intent의 값-재화(reification). intent는 현행
  선언 블록이지 1급 값이 아니다. 별도 결정.
- `View<T>` — 첫 실물은 이미 1층에 있다: **StrView**. 일반화는
  StrView의 Phase fact(WO-P1) 증거가 쌓인 뒤에.

## 7. 착수 조건 & 게이트 조건

- **셀 개방 작업 착수**: MIR type-text seam(docs/147 발견-1 —
  generic 특수화가 lowering-시점 타입 텍스트 fact를 소비하는 구조)
  해소 이후. 그 전의 셀 개방은 기계 부채 위에 의미 부채를 쌓는 것.
- **표의 계약 승격(게이트 생성)**: §2 Decision-0과 §3 GATE 채택이
  BDFL로 닫힌 뒤 — docs/148/150 계열의 matrix-lock smoke(판정 어휘
  5값 잠금, ERASE 셀의 버킷 명명 의무, 2층 행 판정 금지, 행 소실
  금지). **그 전에는 게이트를 만들지 않는다** — 열린 결정을 게이트로
  잠그는 것은 가짜 확정이다.

## Related

docs/121(타입=도메인 매체 — §0의 뿌리) · docs/42 + AxisOwnership.v
(축 소유권 exactly-one-owner — §4 간선의 근거) · docs/14(소거 계기판
— ERASE 버킷) · docs/15(caps=effect refinement — GATE 판정의 선례) ·
docs/12(typestate 거절 — Decision-0의 선례) · docs/147(surface sugar
감사 + MIR seam) · docs/146(SEA lane fact — Future 행이 요구하는
승격) · docs/118 §2.1(수명 주석 영구금지 — Phase 개명 사유) ·
docs/148·150(wiring-doc 규율 선례) · TODO 보드 A-13
