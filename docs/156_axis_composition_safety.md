# 156. 축-조합 안전성 매트릭스 — 6축 15쌍 전수 배치 (A-15)

Status: `AC-0/AC-1 landed, rung-gated`. BDFL 시퀀스(docs/155 §2)의 ②단계.
진입 조건(§3 검증 완성 체크리스트)은 2026-07-04 성립, 같은 날 착수.
`axis-composition-test-smoke`가 이 표의 15행·간선 등재·커널 실측 판정을
잠근다.

## 0. 스코프 — R4 잔여의 정확한 정의

AxisOwnership.v는 축-**소유권**(exactly-one-owner · no-silent-override ·
update-commutation)만 기계화했다. 열려 있던 것은 **전 쌍별 축-조합**
(6축 → 15쌍)의 안전: 두 축이 한 프로그램 지점에서 만날 때 그 만남이
(a) 등재된 간선을 타는가, (b) 서로 간섭하지 않는가, (c) 무음 결합이
숨어 있는가. 방법은 이번 주에 검증된 것의 재적용 — 등재 간선은 간선의
정리/커널로, 미등재 쌍은 비간섭 실측 커널(axis-carriage probe의 축×축
판)로, 전부 matrix-lock 계보로 잠근다.

## 1. 형식 우산 — 이미 서 있는 것 (커버리지 정직 기재)

- **AxisOwnership.v — 사실-분리.** 6축은 docs/42 사실에 사상된다:
  World→FWhere, **Zone→FWhere(공유)**, Actor→FWho, Auth→FAuthorizedBy,
  Intent/Effect→FCauses, slot→FResourceHeld. 사실이 다른 쌍은
  ownership_unique + no_silent_override + axis_updates_commute가 쌍별
  쓰기-안전을 준다. **유일한 사실-공유 쌍 = World×Zone** — 정확히 그
  쌍이 등록부에서 가장 강한 간선(world⊃zone, 전용 진단 실측)을 갖는다.
  공유가 간선으로 승격되어 있으므로 무음이 아니다.
- **WholeProgramCore.v / UnifiedCore.v — 합성 기계.** zone-cross · effect
  · slot · authority · compensation · coordination 게이트가 **한 기계**
  안에서 합성된 상태로 progress(step_iff_guard) + preservation
  (whole_program_safety)이 증명되어 있다 — "전 축이 동시에 켜져도 기계가
  건전"의 형식 반쪽. AIRBinding.gate_locality가 게이트 간 무간섭(각
  action은 정확히 한 fact family만 읽음)을 더한다.
- **ReadingConfluence.v — 읽기측.** 완전한 축 읽기는 순서 무관 수렴.
- 이 우산이 **주지 않는 것**: 표면 구문 수준의 쌍별 결합(아래 §4 커널이
  측정하는 것), 그리고 컴파일러가 모델대로 구현됐다는 사실(parity/smoke
  몫 — negative scope 전통).

## 2. 15쌍 배치표 (처분 = {간선 | 커널 | 소유권+부재} — 전 쌍 배정, 무배정 0)

| # | 쌍 | 처분 | 증거/게이트 |
|---|---|---|---|
| 1 | World×Zone | 간선 `world ⊃ zone` (기등재) | world_pos_share STATIC("implicitly copies zone binding"+Clone 요구), probe 잠금 |
| 2 | World×Actor | 소유권 정리(FWhere≠FWho) + 표면 결합 구문 부재 | AxisOwnership; 커널은 AC-2 |
| 3 | World×Authority | 간접 — world⊃zone 진단의 authority 근거 문구 경유, 직접 간선 없음 | 동 진단 실측; AC-2 |
| 4 | World×Intent | **커널 실측 — 등록 발견: SILENT WRITE-THROUGH** (§4) | comp_world_intent, 본 smoke 잠금; 방향 닫기=AC-3(BDFL 셀) |
| 5 | World×slot | 합성 경유(`world⊃zone` ∘ `zone⊃slot`) — 직접 간선 없음 | 두 기등재 간선의 합성; 우회 경로는 §4 발견이 커버 |
| 6 | Zone×Actor | 매개 — zone `authority <subject-slot>` 선언이 actor를 **Auth축 경유**로 지명 (직접 검사 없음) | authority⊢step의 zone-측 바인딩; 05_zone_intent |
| 7 | Zone×Authority | **간선 `cap ⊢ zone-cross` (본 감사 신규 등재)** | AIRBinding.air_zone_gate + ZoneCrossingCore.v + authority-mismatch panic + 05_zone_intent |
| 8 | Zone×Intent | 간선 `intent ⊨ transfer` (기등재) | transfer 구문 + 관측 from_zone/to_zone + comp_world_intent control leg |
| 9 | Zone×slot | 간선 `zone ⊃ slot` (기등재) + **SILENT-COPY 열림 셀**(docs/151 §2.1 발견) | zone-bound handle(WO-B4) + zone_pos_share probe 잠금 |
| 10 | Actor×Authority | **간선 `role ⊨ ability` (본 감사 신규 등재)** + 형식 잔여=WO-F3(who≠approval 기계화) | witness 시스템(docs/semantics/10), G-6 반증 배터리 role-전파 진단 |
| 11 | Actor×Intent | **커널 실측 — who-swap 완전 비간섭 (양방향)** (§4) | comp_actor_intent, 본 smoke가 출력-동일성 자체를 assert |
| 12 | Actor×slot | 소유권 정리(FWho≠FResourceHeld) + 부재(subject slot은 타입 수용이지 actor-조건 검사 아님) | AxisOwnership; AC-2 |
| 13 | Authority×Intent | 간선 `cap ⊇ effect` + `authority ⊢ step` (기등재 2건) | declared⊇used semantic error(interproc 실측), EffectAuthorityCore.v, AIR strict-evidence(§4 거절 방향이 라이브 데모) |
| 14 | Authority×slot | **간선 `cap ⊢ slot-op` (본 감사 신규 등재)** | AIRBinding.air_acquire_gate + SecureSlot 런타임 GATE(invalid-secure-token) + AIR `slot_capability_retain_count` + 소거 fixtures 03/08 |
| 15 | Intent×slot | 간선 `intent ⊨ transfer`(slot 성분: from_slot/to_slot) + **간선 `effect ⊸ comp-slots` (본 감사 신규 등재)** | 관측 스키마 + CompensationCore.v/air_comp_targets |

읽는 법: **간선-커버 8쌍**(1·7·8·9·13·14·15 + 10)은 상호작용이 설계-
합법이며 그 법이 등재·게이트되어 있다는 뜻이고, **커널 2쌍**(4·11)은
오늘 컴파일러의 측정된 행동이 잠겨 있다는 뜻이며, **소유권+부재
5쌍**(2·3·5·6·12)은 형식 우산 + 결합 구문의 현재 부재(2026-07-04 기준)
로 서 있고 결합 구문이 생기면 이 표가 RED로 알린다는 뜻이다.

## 3. 등록부 정합 감사 — AIRBinding 5족 vs docs/151 §4 (신규 간선 4)

간선 등록부의 자기 규칙: "등록부에 없는 cross-axis 검사가 생기면
그것이 곧 설계 드리프트 신호". 본 감사가 **기계-측 게이트 인터페이스**
(AIRBinding.v의 5개 fact family — 그 자체가 기계 수준의 간선 목록)를
표면-측 등록부와 처음으로 대조했고, **검사는 실존하는데 등록이 안 된
간선 4개**를 찾아 등재했다(2026-07-04, world⊃zone 감사 선례의 반복):

| 신규 간선 | 쌍 | 실물 |
|---|---|---|
| `cap ⊢ zone-cross` | Zone×Auth | air_zone_gate(SCross), authority-mismatch panic |
| `cap ⊢ slot-op` | Auth×slot | air_acquire_gate(SAcquire/SRollback), SecureSlot 항상-on GATE |
| `effect ⊸ comp-slots` | Intent×slot | air_comp_targets, CompensationCore 롤백 재획득 |
| `role ⊨ ability` | Actor×Auth | witness 만족 검사, role-impl 전파 진단(G-6 배터리) |

교훈: 표면 등록부와 기계 인터페이스는 **독립적으로 자랐고 4간선만큼
어긋나 있었다** — 이제 양쪽이 같은 목록을 가리키며, smoke가 등재를
잠근다. dep_graph(task→tasks)는 Intent축 내부 간선이라 쌍 표 밖.

## 4. 커널 실측 (2026-07-04, C==LLVM 동일 목소리)

### World×Intent — 등록 발견: cross-world transfer SILENT WRITE-THROUGH

`transfer: cart -> payment`를 **서로 다른 두 world가 소유한 zone**에
겨냥하면(월드 멤버 접근 `w1.cart`, `w2.payment`): 진단 0으로 컴파일·
실행되고, 출력 `2/1/2` — **소스 world 내부는 불변(1), 데스트 world
내부는 변이(2)**. 즉 Channel 없이 world 외부의 intent 실행이 world
내장 zone 상태를 바꿨고, 소스/데스트가 비대칭이다. 분산 4원칙
(cross-World = Channel-only)의 측정된 우회 경로이며, world 생성자
경로가 정적으로 거절하는 것(world_pos_share)을 **저장-매개 읽기 →
transfer 경로가 우회**한다 — docs/151 §5 개념 노트의 "저장-매개 흐름"
세탁 채널의 world-급 실측 사례. 닫는 방향(정적 거절 vs transfer를
선언된 Channel-급 경계로 승격)은 BDFL 결정 셀 = **AC-3**, zone
SILENT-COPY 방향 결정과 같은 배치. 그때까지 본 smoke가 이 판정을
고정한다(변경 시 leg FAIL → 같은 커밋에서 행 갱신 의무).

### Actor×Intent — who-swap 완전 비간섭 (양방향 측정)

`who: buyer` ↔ `who: observer`만 다른 두 프로그램이 **동일 출력**
(`2/100`) — who는 서술적 참여 표기이며 동작도 승인도 바꾸지 않는다.
거절 방향도 측정됨: zone authority 사실이 없으면 **두 leg가 같은
진단**(`expected authority participant(s): buyer` — who와 무관하게
authorized-by/on-대상에서 유도)으로 거절된다. docs/42 §2.2.1의
"local who는 authorization을 만들지 않는다"가 통과·거절 양방향에서
실측 확인 — 형식 기계화는 WO-F3(AC-4)로.

부수 실측: 이 거절이 곧 **AIR strict-evidence의 authority⊢step 강제**
라이브 데모다(13행 증거로 등재). intent 파라미터 subject는 by-ref
(Main 로컬이 step의 on:으로 변이), zone slot 생성은 copy — 기존
SILENT-COPY 실측과 정합.

## 5. AC-rung 사다리 (smoke가 landed 정직성 잠금)

status ∈ {planned, landed}. landed = artifact+gate 실존.

| rung | 내용 | status | artifact | gate |
|---|---|---|---|---|
| AC-0 | 15쌍 전수 배치 + 간선 등록부 정합(신규 4) + matrix-lock | landed | docs/156 §2–3 | tests/axis_composition_smoke.sh |
| AC-1 | 커널 2쌍 실측(W×I 발견 + A×I 비간섭) C==LLVM | landed | tests/cases/axis_composition | tests/axis_composition_smoke.sh |
| AC-2 | 부재-쌍 커널(W×Actor, W×Auth, Actor×slot, Z×Actor 직접-잔여) | planned | - | - |
| AC-3 | W×I write-through 방향 BDFL 결정 + 닫힘 구현 (zone SILENT-COPY 방향과 같은 배치) | planned | - | - |
| AC-4 | WO-F3 who≠approval 기계화 + role⊨ability Coq 조각 | planned | - | - |
| AC-5 | G-4 합류 — 생성자 경계 검사(Slot/Channel 행) 개방의 전제 충족 | planned | - | - |

## Related

docs/155 §4(스코프의 출처 — BDFL 시퀀스 ②) · docs/151 §4(간선 등록부 —
본 감사가 4간선 등재) · docs/42 + AxisOwnership.v(사실-분리 우산) ·
WholeProgramCore.v/AIRBinding.v(합성 기계 + gate_locality) ·
docs/semantics/00(정적 경계 vs 런타임 존재 — W×I 발견의 존재론 좌표) ·
분산 설계 4원칙(Channel-only — W×I 발견이 겨냥한 원칙) · TODO 보드 A-15
