# Slot은 언어의 가장 낮은 공통 추상화여야 한다 (2026-04-06)

Anti-hype status note (2026-04-29):

- This document describes a design thesis: Slot should become the common
  resource boundary across memory, authority, projection, intent, and world
  transfer.
- It is not a claim that the entire thesis is implemented today. Current
  implementation must be checked against `docs/118_slot_model_rigor_audit.md`
  and `docs/100_beta_readiness_checklist.md`.
- Avoid marketing words such as "innovation" unless the sentence also names the
  implemented subset and evidence source.

## 한 줄 요약

> Slot이 예제 몇 개의 gimmick이면 안 된다. 언어 전체를 관통하는 최저 공통 추상화여야 한다.

---

## 0. 베타 기준 Thesis — 주소 소유권이 아니라 모듈형 자원 경계

Pergyra의 목표는 Rust처럼 메모리를 **주소/참조/lifetime 소유권**으로
사용자에게 직접 노출하는 것이 아니다. 같은 메모리 문제를 더 높은 층에서
**모듈화된 자원 경계** 문제로 푼다.

```text
Pergyra does not expose memory as address ownership.
Pergyra exposes memory as a modular resource boundary.
A Slot is the stable language-level boundary; the backend handle below it is replaceable.
```

즉 `Slot<T>`는 "안전한 포인터"가 아니라, 하위 주소/핸들/버퍼/원격 자원
identity를 언어 의미론에서 숨기는 경계다. backend는 필요에 따라 C pointer,
arena index, generational handle, GPU buffer id, file/device handle, DB row
handle, remote world handle로 갈아끼울 수 있어야 한다. 사용자 코드는 그 하위
표현을 믿지 않고 Slot contract를 믿는다.

이 관점에서 Slot은 다음 네 가지를 합친다.

```text
Slot = address abstraction + ownership boundary + capability gate + replaceable backend handle
```

따라서 Slot은 Rust borrow checker와 경쟁하는 층이 아니다. Slot은 다익스트라식
structured boundary를 언어의 가장 낮은 공통 자원 경계로 만든다. 정적 분석은
그 경계를 증명 가능한 구간에서 닫고, 불명확한 구간은 보수적으로 거절하며,
하위 runtime은 generation/token/pin-state로 fail-safe를 제공한다.

---

## 1. 현재 상태 — Slot이 표면에만 있다

```
있는 곳:
  ✓ 메모리 관리    — Slot<T>, SecureSlot<T>, DeviceSlot<T>, QubitSlot
  ✓ Zone 선언     — subject slot, relation slot, effect slot
  ✓ 런타임        — pgy_runtime.h의 slot 매크로

없는 곳:
  ✗ RIR           — slot 중심이 아님. 별도 IR 노드
  ✗ Lattice       — slot 상태가 lattice point로 추적되지 않음
  ✗ Projection    — object/tobject가 slot의 뷰라는 관계가 명시적이지 않음
  ✗ Authority     — 토큰/권한이 slot 단위로 통합되지 않음
  ✗ Intent handoff — step 간 상태 전달이 slot 이동으로 표현되지 않음
  ✗ Zone/World transfer — cross-boundary 이동이 slot 마이그레이션이 아님
```

**문제: slot이 "기능 하나"로 머물러 있다.** 장기 목표가 성립하려면 slot이 모든 계층의 공통 언어여야 한다.

---

## 2. 목표 — Slot이 관통해야 하는 계층

```
Layer                Slot의 역할
─────────────────────────────────────────────────────
Surface syntax       subject slot x: T (현재 있음)
RIR (lowered IR)     모든 자원 = slot 연산 (claim, bind, release, transfer)
Lattice analysis     slot 상태 = lattice point (claimed, bound, released, transferred)
Projection           object/tobject = slot의 읽기 전용 뷰
Authority            who holds the slot token = 누가 접근 가능한가
Intent handoff       step 전이 = slot 소유권 이전
Zone/World transfer  cross-boundary = slot 마이그레이션 + 직렬화
```

---

## 3. 각 계층별 Slot 관통 설계

### 3.1 RIR — 모든 것이 Slot 연산

현재 RIR은 slot과 별도의 IR 노드를 가지고 있다. 이를 통합:

```
RIR 연산의 기본 단위가 slot이 되어야 한다:

  SLOT_CLAIM    — 자원 점유 (subject 생성, zone 진입)
  SLOT_BIND     — slot에 값 바인딩 (let x = ...)
  SLOT_READ     — slot 읽기 (projection, 필드 접근)
  SLOT_WRITE    — slot 쓰기 (상태 변경)
  SLOT_RELEASE  — slot 반환 (스코프 탈출, zone 퇴장)
  SLOT_TRANSFER — slot 소유권 이전 (intent step 전이, cross-world)
  SLOT_PROJECT  — slot의 읽기 전용 뷰 생성 (object, tobject)
  SLOT_GUARD    — slot 접근 권한 검증 (authority, requires)
```

```
현재:                    목표:
  let x = 5              SLOT_CLAIM x
                          SLOT_BIND x, 5

  subject slot p: Player  SLOT_CLAIM p (type=Player, kind=subject)

  object view: PView      SLOT_PROJECT view FROM p (read-only)

  zone 진입               SLOT_CLAIM zone_handle
                          SLOT_BIND zone.slots...
  zone 퇴장               SLOT_RELEASE zone_handle
```

### 3.2 Lattice — Slot 상태 추적

slot의 자원 경계 상태를 lattice로 정적 분석:

```
Slot State Lattice:

  ⊥ (unknown)
  ↓
  Unclaimed → Claimed → Bound → Active → Released
                                  ↓
                              Transferred → Bound (다른 zone/world)
                                  ↓
                              Projected (read-only view)

위반 감지:
  Released → Read   = use-after-free (컴파일 에러)
  Released → Write  = use-after-free (컴파일 에러)
  Active → Transfer without Release = leak (경고)
  Projected → Write = immutability violation (컴파일 에러)
```

### 3.3 Projection — object/tobject = Slot의 뷰

```
object = slot의 로컬 읽기 전용 뷰
         → SLOT_PROJECT view FROM source (read-only, local)

tobject = slot의 경계 전송 가능 읽기 전용 뷰
          → SLOT_PROJECT view FROM source (read-only, serializable)
          → SLOT_TRANSFER view TO other_world
```

```pergyra
zone ShopZone
{
    subject slot buyer: Member;
    object slot summary: MemberView;       // SLOT_PROJECT summary FROM buyer
    tobject slot receipt: OrderReceipt;     // SLOT_PROJECT receipt FROM buyer (transferable)

    refresh summary from buyer;            // SLOT_READ buyer → SLOT_WRITE summary
    publish receipt from buyer;            // SLOT_READ buyer → SLOT_WRITE receipt + SLOT_TRANSFER
}
```

### 3.4 Authority — Slot 토큰 = 접근 권한

```
authority = "이 slot에 접근할 수 있는 토큰을 누가 가지고 있는가"

zone BattleZone
{
    subject slot attacker: Player;     // SLOT_CLAIM (token held by zone)
    authority attacker requires Combatable;
    // → SLOT_GUARD attacker: check token holder has Combatable
}

action Attack(self)
    requires Combatable               // SLOT_GUARD: caller must hold Combatable token
    authorized by attacker            // SLOT_GUARD: attacker's slot token required
{
    // slot에 접근하려면 토큰이 있어야 한다
    // SecureSlot의 토큰 모델과 동일한 원리
}
```

### 3.5 Intent Handoff — Step 전이 = Slot 소유권 이전

```
intent Purchase(buyer: Member)
{
    step browse
    {
        where: ShopZone;
        // buyer의 slot이 ShopZone에 bind됨
        // SLOT_BIND ShopZone.buyer_slot FROM buyer
    }

    step pay
    {
        where: PaymentZone;
        // buyer의 slot이 ShopZone에서 release → PaymentZone에 bind
        // SLOT_RELEASE ShopZone.buyer_slot
        // SLOT_TRANSFER buyer TO PaymentZone
        // SLOT_BIND PaymentZone.buyer_slot FROM buyer
    }
}

즉 intent의 step 전이 = slot의 release + transfer + bind
```

### 3.6 Zone/World Transfer — Cross-boundary = Slot 마이그레이션

```
world ShopWorld { zone shop: ShopZone; }
world PaymentWorld { zone pay: PaymentZone; }

// cross-world 이동:
// buyer slot이 ShopWorld에서 PaymentWorld로 마이그레이션
//
// SLOT_RELEASE ShopWorld.shop.buyer
// SLOT_SERIALIZE buyer → tobject (wire format)
// SLOT_TRANSFER buyer_data TO PaymentWorld
// SLOT_DESERIALIZE buyer_data → buyer
// SLOT_CLAIM PaymentWorld.pay.buyer
// SLOT_BIND PaymentWorld.pay.buyer FROM buyer
```

---

## 4. 통합 — Slot이 모든 것의 공통 언어

```
표면 문법:     subject slot x: T
               ↓ parsing
AST:           DomainSlot(name="x", type=T, kind=subject)
               ↓ lowering
RIR:           SLOT_CLAIM x (type=T, kind=subject)
               ↓ analysis
Lattice:       x: Unclaimed → Claimed → Bound → Active
               ↓ codegen
C/LLVM:        T *x = pgy_slot_claim(sizeof(T)); ...

Projection:    SLOT_PROJECT view FROM x
Authority:     SLOT_GUARD x (requires Combatable)
Intent:        SLOT_TRANSFER x FROM zone_a TO zone_b
World:         SLOT_SERIALIZE x → SLOT_TRANSFER → SLOT_DESERIALIZE
```

모든 연산이 slot 연산으로 표현된다. slot이 "메모리 관리 도구"가 아니라 **자원 추상화의 공통 프로토콜**이 된다.

---

## 5. 현재 상태 vs 목표

| 계층 | 현재 | 목표 |
|------|------|------|
| Surface syntax | ✓ | ✓ |
| Runtime macros | ✓ | ✓ |
| RIR slot 연산 | ✗ | SLOT_CLAIM/BIND/READ/WRITE/RELEASE/TRANSFER/PROJECT/GUARD |
| Lattice 분석 | ✗ | slot state lattice (use-after-free, leak 감지) |
| Projection 연결 | △ (refresh/publish 있음) | SLOT_PROJECT 연산으로 통합 |
| Authority 통합 | △ (authority 절 있음) | SLOT_GUARD로 통합 |
| Intent handoff | ✗ | step 전이 = SLOT_TRANSFER |
| Cross-world | ✗ | SLOT_SERIALIZE + SLOT_TRANSFER |

---

## 6. 구현 우선순위

```
1단계: RIR에 SLOT 연산 도입 (가장 낮은 레이어 → 위로 전파)
2단계: Lattice 분석 (slot state 추적 → use-after-free 감지)
3단계: Projection 통합 (object/tobject = SLOT_PROJECT)
4단계: Authority 통합 (requires/authorized by = SLOT_GUARD)
5단계: Intent handoff (step 전이 = SLOT_TRANSFER)
6단계: Cross-world transfer (SLOT_SERIALIZE + SLOT_TRANSFER)
```

---

## 7. 설계 가치

```
Rust:     소유권이 메모리를 관통한다 (borrow checker)
          → 메모리 안전을 타입 시스템에 넣었다

Pergyra:  Slot이 자원 전체를 관통한다 (slot protocol)
          → 자원 추상화를 타입 시스템에 넣었다

Rust가 "이 메모리를 누가 소유하는가"를 컴파일 타임에 추적하듯,
Pergyra는 "이 자원(subject/zone/world/intent)을 누가 점유하고 있는가"를
컴파일 타임에 추적한다.

차이:
  Rust  = 메모리 하나의 소유권
  Pergyra = 도메인 자원 전체의 점유/이전/투영/권한
```
