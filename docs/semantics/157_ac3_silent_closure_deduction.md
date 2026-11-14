# 157. AC-3 — 두 SILENT 발견의 닫힘 방향, 연역 도출

Status: `FULL THEOREM T LANDED (2026-07-05) — S1+S2, twice re-ratified
(§5)`. 방법은 BDFL 지시(2026-07-04): "무음 쪽은
최소 논증으로 연역법으로 올라간다" — 선호 비교가 아니라, 이미 canon인
전제들에서 결론을 **도출**하고, 결론을 뒤집으려면 어느 전제를 부정해야
하는지까지 명시한다.

대상 사례 2건 (둘 다 실측·게이트 고정 상태):

- **S1 — zone SILENT-COPY** (docs/151 §2.1 발견): 한 live binding을 두
  zone 생성자에 꽂으면 무진단 컴파일·복사-격리(`5 5 / 7 5`).
- **S2 — world SILENT WRITE-THROUGH** (docs/156 §4 발견): world 멤버
  zone을 transfer에 겨냥하면 무진단으로 데스트 world 내부 변이
  (`2/1/2`, 소스/데스트 비대칭).

## 1. 전제 (전부 기존 canon — 신규 가정 0)

| # | 전제 | 출처 | 검증 상태 |
|---|---|---|---|
| P1 | no-silent-override: 한 축의 전이가 다른 축 소유 사실을 무음으로 못 바꾼다 | docs/42, AxisOwnership.v | 기계증명 |
| P2 | 무음 하강 불가: 축-사실의 소실/포크는 선언된 지점에서만 (`descent_is_declared`) | GenericAxisCarriage.v | 기계증명 |
| P3 | cross-World 값 통과는 Channel-only | 분산 설계 4원칙 (레드팀 계보) | 등록 결정 |
| P4 | world 진입(생성자)의 live zone binding 암시 복사 = 정적 거절 + `Clone` 명시 요구 | world_pos_share 실측 진단 | probe 잠금 |
| P5 | 절단선 = 정적 경계 vs 런타임 존재: 경계(world/zone)는 정적 선언의 자리 | docs/semantics/00 | canon |
| P6 | Decision-0: carriage는 positional 기본 — 위험은 생성자/경계 자리로 이동, 검사도 거기에 | docs/151 §2 | CLOSED |
| P7 | 의미 있는 이전은 표면 단어를 갖는다 (own/ref, Clone, authorized by, with caps) | 언어 결 전반 | 관측 일반화 |

전제 보강 실측 2건 (2026-07-04, C==LLVM, tests/cases/axis_composition/):

- **P-A** (`probe_clone_zone`): `Clone`은 zone 생성 자리에서 **이미
  동작**한다 (`CartZone(Clone(sb), …)` → `5 5 / 7 5`). 선언된-복사
  escape hatch는 신규 기제가 아니라 실존 표면이다.
- **P-B** (`probe_world_member`): `let escaped = w.cart`는 **분리
  복사**다 (escaped 변이 2, world 내부 불변 1). 따라서 S2의
  write-through는 멤버 읽기 일반이 아니라 **transfer-인자 위치에서만
  live alias가 되는 위치별 의미 분기**다 — 같은 표현이 소비 위치에
  따라 copy/alias로 갈라지며, 어느 쪽도 선언되지 않았다.

## 2. 연역

### 보조정리 L1 — 두 사례는 같은 사건의 두 인스턴스다

S1: live binding이 containment 경계(zone) **안으로** 들어가며 identity가
무선언 포크된다. S2: live binding이 containment 경계(world) **밖으로**
나오며(멤버 읽기→인자) identity가 무선언 포크되거나(P-B의 copy 자리)
무선언 노출된다(transfer-인자의 alias 자리). 공통 사건:

> **containment 경계를 가로지르는 live binding의 무선언 identity
> 포크/노출.**

P2에 의해 이 사건은 선언을 요구한다(무음 하강 불가 — identity 사실의
포크는 하강이다). P4에 의해 언어는 world 진입 방향에서 **이미 이
사건을 거절하고 선언(Clone)을 요구**한다. 즉 질문은 "새 규율을
만들까"가 아니라 "이미 있는 규율이 왜 세 자리(zone 진입, world 진출,
인자 위치)에서만 침묵하는가"다.

### 보조정리 L2 — 무음-합법화 방향은 전제와 모순된다

"zone slot은 복사가 계약이다"로 무음을 명문화하는 방향을 가정하면:

- P4와 즉시 충돌 — 같은 사건(암시 복사)이 world 진입에서는 거절인데
  zone 진입에서는 계약이 된다. 같은 containment 사슬(world⊃zone⊃slot)
  의 두 층이 같은 질문에 반대로 답한다 = P1의 axis-level 정신 위반.
  해소하려면 P4를 **철회**(기존 정적 보장 약화 = 래칫 역행)해야 한다.
- P7과 충돌 — identity 포크라는 의미 이전이 표면 단어 없이 일어난다.
- S2에는 아예 적용 불가 — S2의 절반은 복사가 아니라 **alias 관통**
  이고(데스트 world 내부 변이), 이를 계약으로 명문화하면 P3(Channel-
  only)을 정면 철회하게 된다. P3는 레드팀 계보의 등록 결정이다.

∴ 무음-합법화는 P1·P3·P4·P7 중 최소 둘을 철회해야만 성립한다. 철회
없이 남는 방향은 하나뿐이다.

### 보조정리 L3 — S2의 최소 닫힘 지점은 소비자가 아니라 경계다

S2를 transfer-전용 검사(인자 위치에서 world-멤버 zone 금지)로 닫으면:
(a) P-B가 보인 위치별 의미 분기(copy vs alias)가 다른 미래 소비자에
그대로 남고, (b) zone 값에 world-소속 사실을 실어야 해서(값-실림)
P6(positional 기본)과 어긋난다. 반면 **경계에서 닫으면**(world 멤버
zone의 live 유출 = 진입과 동일하게 선언 요구) 한 검사로 전 소비자가
커버되고, 검사는 자리(멤버 접근/생성자 인자)에 붙는다 — P5·P6과 정합.

### 정리 T (도출된 통일 규칙) — 경계-포크 선언 규칙

> **containment 경계(world⊃zone⊃slot)를 가로질러 live binding이
> 들어가거나(생성자) 나오는(멤버 유출) 사건은 identity 포크/노출이며,
> 반드시 선언을 요구한다: `Clone` = 선언된 복사, `Channel` = 선언된
> 통과. 무선언 = REJECT (fail-close).**

사례 적용:

- **S1 판정**: zone 생성자에 live binding → **REJECT + `Clone` 요구**
  (P4의 world 진단과 같은 family; escape hatch는 P-A로 실존 확인).
  fresh 값 생성(`CartZone(Buyer(5), …)`)은 포크가 없으므로 그대로 합법.
- **S2 판정**: world 멤버 zone의 live 유출(let-init·인자 위치 불문) →
  **REJECT + `Clone` 요구**. 이것으로 (a) write-through 경로 소멸
  (Clone 후엔 분리 사본 위 transfer — world 내부 불가침), (b) P-B의
  위치별 의미 분기 소멸(유출 자체가 선언을 요구하므로 copy/alias
  우연성이 사라짐), (c) Channel이 **유일한 무-Clone cross-World 통과**
  로 복원 — P3가 구문 수준에서 강제된다.
- free-standing zone 간 transfer(control 커널)는 규칙 무관 — 그대로.

### 따름정리 — G-4 입력

G-4(생성자 경계 검사)의 검사 지점과 진단 family가 이 규칙에서 나온다:
Slot/Channel 생성자 자리의 축-합성 검사 = 정리 T의 "경계를 가로지르는
live binding은 선언을 요구"를 생성자 위치에 인스턴스화한 것. 값 태깅
없이 자리 검사만으로 성립한다(P6 준수).

## 3. 반증 가능성 — 결론이 뒤집히는 조건

| 뒤집으려면 | 대가 |
|---|---|
| P4 철회 (world 진입 Clone 요구 폐지) | 실측·잠금된 정적 보장의 약화 — 래칫 역행 |
| P3 철회 (Channel-only 폐지) | 레드팀 계보 등록 결정의 철회 |
| "Channel이 정당한 cross-world 흐름을 다 못 싣는다" 입증 | 그 경우에도 결론은 무음 유지가 아니라 Channel rung 추가 (표현력 갭은 무음의 면허가 아님) |
| P2 철회 (무음 하강 허용) | GenericAxisCarriage 기계증명 체계와의 결별 |

어느 것도 지불할 수 없는 대가이므로, 전제를 받아들이는 한 정리 T는
강제된다. 이 연역의 신규 가정은 0이다 — 결론이 마음에 안 들면
반박할 것은 결론이 아니라 위 전제 중 하나다.

## 4. 구현 스케치 (비준 후 착수 — 별도 rung)

- 검사 2지점, 둘 다 semantic 층: ① zone/world 생성자 인자의 live
  binding 검사(기존 world_pos_share 진단의 zone-일반화), ② world 멤버
  zone 표현의 유출-위치 검사(let-init/인자/반환). 진단 문구는 기존
  family 재사용: "implicitly copies zone binding … use Clone".
- 실측 잠금 갱신 의무(같은 커밋): `zone_pos_share` leg(SILENT-COPY →
  REJECT), `comp_world_intent/cross` leg(`2/1/2` → REJECT), docs/151
  §2.1 발견 행·docs/156 4행/9행 — 게이트가 정확히 이 목적으로 잠가
  두었다.
- premise probe 2건(probe_clone_zone/probe_world_member)은 구현 후
  신규 진단의 회귀 픽스처로 승격(P-A는 합법 유지 leg, P-B는 REJECT
  leg로 전환).

## 5. 구현 기록 + corpus 재심 (2026-07-05)

**S1 — LANDED.** zone 생성자의 live subject binding = STATIC 거절
("implicitly copies subject binding" + Clone fix), world 검사와 같은
family. 구현 = `type_checker_world_embedding.c`(경계-포크 파일로 확장)
+ 생성자 검사 훅(중첩 `World(Zone(subject))` 인자 하강 포함 — 첫
구현이 놓쳐 sweep이 잡음). corpus 전수 sweep: **188 컴파일 통과, S1
개종 28사이트**(examples 11파일 + abi/backend_compare/커널), 전부
행동 보존(Clone = 기존 암시 복사의 선언형; logistics golden diff 공백
실측). `zone_pos_share`는 예고대로 같은 커밋에서 REJECT 잠금 전환 —
docs/151 §2.1 발견 2 닫힘.

**S2 — 재심 2회 후 전면 T로 착지(2026-07-05).** 재심 경위와 최종 결정:

- **재심 1(플래그십 관용구)**: 아래 원문대로 — 전면 유출 금지가
  shopping_mall·logistics·abi의 plain-func API층 통과 패턴과 충돌.
  BDFL 판정: **(A) 전면 T 유지**.
- **재심 2(handoff = 설계 기능)**: backend_compare의
  `handoff_*_state_frontier` 픽스처가 이것이 우연이 아님을 증명 —
  intent `step Handoff` + `transfer:` + `expect: payment.buyer.hp == 7`
  (데스트 zone의 사후 상태를 계약으로 읽음) + world의 frontier state
  기계(`state paymentLayer: zone payment layer charged`)가 정확히 이
  write-through의 효과를 관측하도록 설계돼 있다. 즉 same-world 절반의
  선언은 이미 존재했다(intent 계약) — 정리 T의 선언 집합 {Clone,
  Channel}이 불완전했다는 반증. BDFL 판정: **그래도 (A) 전면 T** —
  handoff 기능 자체를 재설계 대상으로 안고 간다(모순의 명시적 수용,
  보드 등재).
- **착지 내용**: S2 훅(call 인자 + let-init) 강제, cross 커널·
  probe_world_member = REJECT 회귀 픽스처, handoff 픽스처 2건·
  logistics 3사이트·shopping_mall 11사이트 Clone 개종, 행동 변화분
  golden 재생성, semantic 유닛 2791/0.

(재심 1 시점의 원문 기록, 보존:) 전면 유출 금지(값-위치의
`w.zone` 전부)를 구현하고 sweep하자 **플래그십 관용구와 정면 충돌**:
shopping_mall(7사이트)·logistics(3)·abi_pipeline(8파일)·
backend_compare(2파일)가 world 소유 zone을 **plain API-layer func**로
통과시키는 same-world 오케스트레이션 패턴을 지배적으로 쓴다
(`HandleCheckoutIntent(cart:…)`는 intent가 아니라 func — intent-한정
사면으로도 구제 불가). 이는 L1의 "무선언" 분류가 same-world 매개
사용에 대해 corpus와 어긋난다는 신호다(측정이 전제를 재개방 — 반증
규율). 훅은 회수했고 측정 상태로 복귀: cross 커널의 write-through
(`2/1/2`)는 `axis_composition_smoke`가 계속 고정한다. 재심 선택지:

- **(A) 정리 T 전면**: 모든 값-위치 유출 = Clone 요구. 플래그십 전
  사이트 개종 + write-through 의존 데모는 행동 변화 → golden 재생성
  + 후속 리라이트(world 메서드/Channel화) 필요. 원칙 최대 일관.
- **(B) cross-world-mix만 REJECT**: 한 호출의 world-멤버 zone 인자들이
  **서로 다른 world base**를 섞으면 거절(= 커널이 실측한 Channel-only
  우회의 정확한 닫힘), same-world 매개 사용은 **사인된 face**로 등록
  (간선 후보: world ⊢ orchestration — world가 자기 zone을 자기
  스코프의 호출에 빌려준다). P-B의 copy/alias 비일관은 face 계약으로
  명명·문서화 + 의미 통일은 후속 rung. corpus churn 0.

부수 등록: S2 진단의 위치정보가 일부 인자 노드에서 0:0으로 강등
(member-access 노드 line 소실) / 중첩 base(`w.zone.subject`)는 양
방향 모두 미커버 — 어느 방향이든 구현 시 함께 조일 것.

## Related

docs/151 §2.1(S1 원 발견)·§4(간선 등록부) · docs/156 §4(S2 원 발견,
AC-rung) · docs/42 + AxisOwnership.v(P1) · GenericAxisCarriage.v(P2) ·
분산 설계 4원칙(P3) · docs/semantics/00(P5) · TODO 보드 A-15 AC-3
