# Pergyra 설계 철학

## 0. Pergyra는 시스템 언어다 — 모든 것이 그 위에 얹힌다

이 문서의 모든 다른 chapter, 그리고 docs/106 / 114 / 117 / 118 같은 sister
positioning doc들이 다루는 *추상화 / 도메인 / 동시성 / 분산* 모델은
**전부 시스템 언어 baseline 위에 얹힌 layer**다. baseline이 깨지면 layer는
의미 없다.

### 0.1 Core Identity

> **Pergyra is a systems language with domain extensions.
> The systems-language baseline (no GC, predictable memory, C FFI, ABI
> stability, raw escape, optional runtime, compile-time determinism)
> is non-negotiable. The domain primitives (intent / zone / world /
> authority / handoff / Channel / parallel) are first-class but
> layered on top of the systems baseline, not replacing it.**

이건 marketing 문구가 아니라 *설계 결정 우선순위*다. 새 기능 추가 시
baseline 7가지가 흔들리면 *그 기능이 잘못된 것*이지 baseline을 양보하는
것이 아니다.

### 0.2 왜 시스템 언어여야 하는가 — 추상화 portability와의 동치성

표면적으로 Pergyra는 *abstraction portability* 언어로 보인다 (docs/117):
"같은 추상화가 모든 plat에서 같은 의미." 하지만 이 약속을 *지키려면*:

- 모든 plat에서 *돌아야* 한다 → 시스템 언어급 portability 필수
- C FFI로 모든 외부 자원 호환 → 시스템 언어급 ABI 호환성 필수
- GC pause 없이 결정론적 → 시스템 언어급 perf 모델 필수
- 임베디드/freestanding/kernel까지 reach 가능 → 시스템 언어 trajectory 필수
- Bare metal부터 분산까지 같은 syntax → 시스템 언어 substrate 필수

→ **abstraction portability와 systems language identity는 *같은 사실의 두
면*이다.** 추상화를 모든 plat에 동일하게 전달하려면, 그 모든 plat에
도달할 수 있어야 하고, 그건 시스템 언어로만 가능하다.

C를 *빌려 쓰는* 이유도 같다: C가 universal substrate이기 때문이다. 시스템
언어가 아니면 C를 빌릴 자격조차 없다 (FFI 깨짐, ABI 불일치, runtime
의존성). Pergyra가 C 위에 얹는 도메인 layer는 *시스템 언어 자격으로*
얹는 것이다.

### 0.3 시스템 언어 7가지 baseline

Modern systems language criteria (Rust 1.0이 만든 baseline, Pergyra 적용):

| 기준 | 의미 | Pergyra 상태 |
|---|---|---|
| Mandatory GC 없음 | runtime overhead pay-per-use | ✅ fiber + arena, GC zero |
| 예측 가능한 메모리 layout | ABI 안정, alignment 통제 | ✅ pgy_abi_spec.h + static_assert 28종 |
| Direct memory ops 가능 | raw pointer / pin / inline asm escape | 🟡 pin block escape 있음. 시스템-tier raw escape 미정 |
| C FFI 1급 | extern + ABI 호환 | ✅ dual-emit C 백엔드 자체 |
| Bare-metal trajectory | freestanding / no_std-equivalent 가능성 | 🔴 베타 미지원, 1.0 후 lift |
| Runtime optional / scalable | 기능 안 쓰면 비용 0 | 🟡 baseline. fiber/arena가 default 들어감 |
| Compile-time determinism | 모든 codegen 결정론적 | ✅ baseline. 검증 필요 |

→ 베타 시점 4 ✅, 2 🟡, 1 🔴. **🟡 두 자리가 베타 closure 위험. 🔴 한
자리는 1.0 후 lift trajectory.** 이 분포가 *정직하게* 시스템 언어 baseline.

### 0.4 4가지 위험 자리 — baseline에서 멀어질 수 있는 drift 지점

이 4가지는 *지금 깨져있는* 게 아니라 *깨질 수 있는* 자리. 베타 closure
직전 모두 점검 필수.

#### 위험 1: Slot이 시스템-tier raw escape 없음

`pin slot as view {}` 블록은 *lexical* escape다. 시스템 코드 (드라이버,
OS module, embedded ISR, 메모리-매핑 IO) 작성에는 부족할 수 있음:

```pergyra
// 필요할 가능성 — 현재 정의 없음
unsafe {
    let raw_ptr: ptr<u8> = SlotRawPointer(slot)
    asm volatile("mov %0, %%cr3" : : "r"(raw_ptr))
}
```

베타 안에 *시스템-tier raw escape*가 정의되어야 함. `unsafe { }` block
또는 비슷한 형태. 정의 안 되면 시스템 언어 status 흔들림.

Current beta-gated state:

- `unsafe { ... }` is a lexical marker only. It type-checks and lowers its body,
  but it does not grant raw pointer capability.
- `SlotRawPointer(...)` is reserved as the obvious future raw escape spelling
  and currently rejects with `PGY_SEM_RAW_ESCAPE_UNSTABLE`.
- The accepted hot-path answer remains typed Pin/Lease views; raw pointer,
  inline-asm operand, MMIO, and pointer arithmetic escape still require a
  separate ABI/lowering contract.

#### 위험 2: Runtime이 default로 들어감

fiber + arena가 default라면, 베타 안에 *runtime 없는 컴파일 모드* 정의
필요:

```
pgyc --runtime=none main.pgy   # parallel/intent/Channel reject, no_std 모드
```

이 모드 정의되어 있으면 임베디드 / kernel 작성 *가능성*이 박혀있음. 1.0
완전 작동 아니어도 *문법으로 정의*는 베타 안에 필요.

Current beta-gated state:

- `--runtime=none` is now parsed by the driver and routed through
  `PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED`.
- Runtime-dependent surfaces (`parallel`, `spawn`, `Channel`, async/future,
  select/task-group, `intent`, `zone`, `world`, and event runtime contracts)
  are explicitly rejected instead of silently compiling through the default
  runtime.
- Pure source that does not use those surfaces is still rejected at the
  freestanding-lowering blocker. This is intentional: until C/LLVM no-runtime
  lowering exists, the compiler must not claim a no-runtime binary.

#### 위험 3: ABI 안정성이 도메인 primitive에 의해 흔들림

intent / zone / world layer 변경이 C FFI ABI에 leak되면 시스템 언어
status 깨짐. 현재 pgy_abi_spec.h가 보호 중. 이걸 *시스템 언어 contract*로
격상해야 함:

> *"intent/zone/world의 어떤 변경도 C FFI ABI를 깨면 안 된다.
> ABI가 안 깨지는 한도 내에서 도메인 layer 진화 가능."*

이걸 베타 contract에 명시해야 함.

#### 위험 4: Compile-time determinism

type inference / generic resolution / AIR drift check가 *비결정론적*
codegen 만들면 (e.g., hash map iteration order에 의존) → predictable perf
깨짐. 베타 closure 전 *모든 codegen이 결정론적*인지 검증 필요.

### 0.5 도메인 primitive와 시스템 언어 baseline의 관계

| Layer | 책임 | 구체 |
|---|---|---|
| Substrate (C-level) | 시스템 언어 baseline 7가지 | pgy_abi_spec, slot_manager, runtime ABI |
| Slot abstraction | 시스템 자원 통합 인터페이스 | Slot / SecureSlot / DeviceSlot / QubitSlot |
| Static layer | 5컴포넌트 (ownership/CFG/pin/channel/token) | semantic + CFG + pin |
| Domain layer | 도메인 오케스트레이션 | intent / zone / world / authority / handoff |
| Verification layer | abstraction safety | AIR (drift check) |

→ **Substrate가 무너지면 모든 layer 무너짐.** 도메인 primitive 강화는
substrate를 *훼손하지 않는 한도 내에서만* 의미 있음.

이게 Pergyra가 "Java + intent" / "Go + intent" / "Python + intent"가 아닌
이유다. 같은 도메인 primitive를 다른 언어에 얹으면 *그 언어의 substrate
한계*에 갇힌다 — Java는 GC pause 강제, Go는 embedded 차단, Python은
predictable perf 깨짐. Pergyra는 substrate가 시스템 언어급이라 도메인
primitive가 모든 plat에 *온전히* 옮겨갈 수 있음.

### 0.6 한 줄

**Pergyra는 *시스템 언어 자격으로* 도메인 추상화를 모든 기계에 동일하게
전달하는 언어다.** 시스템 언어 정체성을 잃으면 abstraction portability도
의미 없어지고, abstraction portability 없으면 도메인 primitive도 한 plat에
갇힌다. 둘은 분리 불가.

이 §0이 다른 모든 chapter 위에 위치하는 이유다.

### 0.7 계보 (lineage) — C#이 아버지, 나머지는 substrate borrow

§0.1-0.6이 *시스템 언어 정체성*을 박는다면, 외부에서 본 *계보*는 별개
좌표다. 정직한 계보 statement는:

> **C#이 Pergyra의 아버지(target shape and feel)다.** 다재다능한
> multi-paradigm(OOP+FP+DOP), async/await, properties, generics, records,
> partial class, pattern matching, LINQ-style — 이게 Pergyra가 *되고
> 싶어하는 모양*. 이 모양을 시스템 언어 substrate(C + Rust 1.0 + Vale +
> Pony/Verona + Erlang/Koka + OCaml + MLIR)로 다시 짠 것이 Pergyra다.
> intent / zone / world / authority / handoff는 그 위에 1급으로 얹은
> *unique synthesis* (어느 부모에도 없음).

이 계보 좌표의 정식 위치는 `docs/119_pergyra_lineage_positioning.md`.
§0.5의 layer 표는 *내부 구조*, §0.7의 계보는 *외부 좌표*. 두 좌표가
충돌하지 않는 이유: C# (father)이 *목표 모양*이고 layer 표의 substrate /
Slot / static / domain / verification은 *그 모양을 시스템급으로 만드는
구현*이기 때문.

### 0.8 비전과 현재의 분리 — capability audit

§0.1-0.7은 *Pergyra가 무엇이 되어야 하는가*를 박는다. 별개로 *지금 무엇이
구현됐고 무엇이 비전인가*는 `docs/120_vision_and_capability_audit.md`에
정식 위치한다. 이 분리가 중요한 이유: 비전을 *현재 capability*로 인용하면
거짓이 되고, 현재를 *비전*으로 축소하면 자기검열이 된다. 둘은 서로 다른
좌표라 둘 다 정직하게 박혀 있어야 함.

3-pair negative-space 프로토콜:

| Pair | 관심 | Anchor |
|---|---|---|
| Vocabulary | "표현이 정적/runtime/proof을 정직하게 구분하는가?" | docs/118 §8 |
| Lineage | "계보 비교가 어느 부모에서 왔는지 정직한가?" | docs/119 §11 |
| Capability | "기능 주장이 *현재* vs *비전*을 정직하게 구분하는가?" | docs/120 |

외부 description (README / 블로그 / 비교 글 / 학술) 발행 전 3개 모두 점검
필수. 한 개라도 실패하면 외부 표면이 거짓이 됨.

---

## 1. 가장 중요한 전제

Pergyra의 핵심은 "포인터를 더 예쁘게 숨기는 것"이 아니다.
핵심은 **추적하기 어려운 자원을 주소가 아니라 점유권으로 다루는 것**이다.

즉 Pergyra는 메모리 중심 언어라기보다, 점차 **자원 중심 언어**를 지향한다.

이 관점에서 중요한 질문은 다음과 같다.

- 이 값이 어느 주소에 있는가
- 가 아니라, 이 자원을 지금 누가 점유하고 있는가
- 누가 접근 권한을 가지는가
- 그 점유는 언제 끝나는가

이 철학은 클래식 메모리뿐 아니라 다음 영역까지 염두에 둔다.

- 일반 메모리
- 보안 자원
- 채널과 스케줄러 같은 런타임 자원
- 분산 자원
- 장기적으로는 큐비트 같은 관측 민감 자원

## 2. 슬롯의 의미

`Slot<T>`는 "T를 저장한 메모리 칸"이 아니라, **T 타입 자원에 대한 점유권 핸들**이다.

핵심 연산은 주소 조작이 아니라 생명주기 조작이다.

```text
Claim
Read
Write
Release
```

이 모델의 초점은:

- 자원이 존재한다는 사실
- 자원을 점유했다는 사실
- 점유 중에만 접근할 수 있다는 규칙

이다.

따라서 Pergyra는 포인터처럼 "언제든 역참조 가능한 주소"를 기본 단위로 삼지 않는다.

## 3. 세마포어의 직관을 언어로 올린다

Pergyra의 슬롯 철학은 세마포어와 닮아 있다.

세마포어의 핵심은 자원의 주소를 다루지 않고, 자원의 점유 상태를 다루는 것이다.

```text
P(resource)
  use resource
V(resource)
```

Pergyra는 이 감각을 타입 시스템과 런타임 모델로 끌어올린다.

```text
ClaimSlot<T>()
  Read / Write / Move / Invoke
Release(slot)
```

즉 슬롯은 단순 저장소가 아니라:

- 점유권
- 사용 권한
- 생명주기 추적

의 묶음이다.

## 4. SecureSlot은 권한 모델이다

`SecureSlot<T>`는 슬롯에 권한 토큰을 결합한다.

이것은 단순 편의 기능이 아니라, Pergyra가 지향하는 **capability 기반 접근 모델**의 출발점이다.

핵심 규칙은 단순하다.

- 자원을 안다고 접근할 수 있는 것이 아니다
- 올바른 권한을 가지고 있어야 접근할 수 있다

이 점에서 `SecureSlot<T>`는 장기적으로 자원 언어 철학을 확장하는 중요한 축이다.

## 5. 제네릭은 표현력의 중심축이다

Pergyra가 슬롯만으로 구성되는 언어는 아니다.
또 다른 핵심 축은 **제네릭을 통한 아름다운 타입 관계 표현**이다.

제네릭의 역할은 문법 장식이 아니라:

- 자원 계열을 일반화하고
- 계약을 추상화하고
- 조합 가능한 타입 구조를 만드는 것

이다.

예를 들어:

- `Slot<T>`는 자원 핸들의 일반형
- `SecureSlot<T>`는 권한 제약이 있는 자원 핸들
- 미래의 `QubitSlot<T>` 같은 특수 자원도 같은 축에서 확장 가능

즉 Pergyra의 중심은 다음 두 축의 결합이다.

- 제네릭을 통한 구조적 표현력
- 슬롯을 통한 자원 점유권 모델

## 6. struct, subject, ability, role, party, world의 위치

이 계층은 Pergyra 안에서 중요하지만, 모두 같은 무게의 코어는 아니다.

```text
struct   = 값 타입
subject  = 상태와 identity를 가진 주체 타입
ability  = 무엇을 할 수 있는가
role     = 어떤 자격으로 수행하는가
party    = 누구와 협력하는가
relation = 누구와 어떤 관계인가
effect   = 어떤 지속 규칙의 영향을 받는가
zone     = 어느 지역 규칙 안에 있는가
world    = 전체 시스템의 경계
```

여기서 `entity`는 코어 언어 존재론 용어로 넣지 않는다.
그것은 프레임워크나 도메인 모델이 필요할 때 쓸 수 있는 바깥 어휘에 가깝다.
반대로 `object`는 별도 능동 주체가 아니라, 상태를 가질 수 있고 effect를 받을 수 있으며 relation의 대상이 될 수 있지만 스스로 intent를 시작하지 않는 피동 상태 대상이다.
`tobject`는 그 object 표현 중 외부 경계를 넘기기 위한 boundary transfer contract다.
현재 구현에서는 `object`와 `tobject` keyword를 우선 `struct` 호환 nominal declaration alias로 받지만, 의미론적으로는 둘을 다르게 본다.
`object`는 passive state target이고 helper `func`와 국소 상태를 가질 수 있으며, `tobject`는 그보다 더 좁은 transfer/projection form이다.
중요한 점은 `tobject`가 zero-copy telemetry borrow를 뜻하지 않는다는 것이다. generation/lease/snapshot ticket 정책은 별도 계층으로 둔다.
즉 projection의 중심은 `tobject` 자체가 아니라 `relation/effect/zone/world` 문맥과 `ToObject` / `ToTObject` / `refresh` / `publish` 같은 투영 동작이다.
그래서 direct `ToObject` / `ToTObject`는 존재하더라도 보조 surface로 남고, 권장 흐름은 domain context 안의 projection wiring과 lifecycle sync다.
`Void`는 빈 일반 값이라기보다 "결과가 없음"을 나타내는 반환 타입이고, `return`은 그 결과 타입과 별개로 현재 실행을 종료하는 제어 문장이다.
그래서 `return;`은 `Void` 경로의 조기 종료이고, `return expr;`은 값을 돌려주는 종료다.

여기서 설계 우선순위는 다음처럼 보는 것이 맞다.

### 코어에 가까운 것

- `Slot<T>`
- `SecureSlot<T>`
- 제네릭
- `subject`
- ability
- 소유권과 생명주기 규칙

### 코어와 DSL 사이

- role
- include
- context
- execution model

### 상위 도메인 계층

- party
- relation
- effect
- zone
- world
- roster (이행기 조립 단위)

즉 Pergyra의 가장 깊은 정체성은 `party/world`가 아니라, 슬롯과 제네릭과 subject/ability 계약 시스템이다.
`party/relation/effect/zone/world`는 그 위에 올라가는 문맥 계층으로 보는 것이 맞다.
`roster`은 현재 구현에 존재하지만 최종 존재론 계층이라기보다 이행기 조립 단위로 보는 편이 자연스럽다.

## 7. 컨테이너 격리는 결과이지 출발점이 아니다

기존 문서에서 Pergyra를 "컨테이너 격리" 중심으로 설명해 왔지만, 이제는 그보다 더 아래의 이유를 분명히 해야 한다.

컨테이너 격리는 중요한 설계 결과다.
하지만 출발점은 이것이다.

- 자원은 위험하다
- 자원은 추적이 어렵다
- 주소 기반 모델은 일부 자원에서 부적절하다
- 따라서 점유권과 권한 중심 모델이 필요하다

그 결과로 컨테이너 격리가 나온다.

| 계층 | 철학적 의미 |
|------|-------------|
| `Slot<T>` | 단일 자원 점유 |
| `SecureSlot<T>` | 권한이 필요한 자원 점유 |
| `Channel<T>` | 공유하지 않고 이동시키는 자원 흐름 |
| `Subject` | 상태와 identity를 가진 주체 |
| `Object` | intent를 시작하지 않는 피동 상태 대상 |
| `tobject` | object의 외부 경계용 축약 투영 |
| `Participant` | subject의 실행 프로파일 |
| `Party` | 협력하는 자원 묶음 |
| `World` | 격리된 시스템 경계 |

즉 격리는 목적의 전부가 아니라, **자원 추적을 가능하게 만드는 구조적 방법**이다.

## 8. 뮤텍스와 세마포어에 대한 입장

Pergyra는 뮤텍스나 세마포어를 부정하지 않는다.
오히려 그들이 보여준 중요한 통찰을 언어 차원으로 일반화하려 한다.

### 런타임 내부

채널, 스케줄러, 태스크 풀 구현에서는 여전히 저수준 동기화 프리미티브가 필요할 수 있다.
이것은 구현 세부사항이다.

### 언어 모델

사용자가 `.pgy` 코드에서 직접 저수준 동기화 구조를 조합하는 대신,
더 높은 수준의 자원 모델을 기본으로 사용하게 하는 것이 목표다.

즉:

- 뮤텍스를 더 잘 쓰게 하는 언어가 아니라
- 자원 점유와 전달을 더 명확히 표현하는 언어를 지향한다

## 9. 저수준 탈출구는 필요하지만 중심은 아니다

시스템 프로그래밍 언어인 이상 저수준 제어를 완전히 배제할 수는 없다.
따라서 다음은 필요하다.

- `extern "C"` FFI
- 장기적으로는 `unsafe` 블록

하지만 이것들은 정체성이 아니다.
정체성은 여전히 안전한 기본 모델 쪽에 있다.

철학은 다음과 같다.

- 기본: 슬롯과 타입 규칙을 통한 안전한 자원 제어
- 탈출: 명시적 위험 감수

## 10. 미래 확장 방향

장기적으로 Pergyra가 정말 자원 언어가 되려면 슬롯만으로는 부족하다.
다음 축이 따라와야 한다.

- 복사 금지 또는 제한 복사
- move 중심 전이
- affine/linear 제약
- effect/resource transition tracking
- 관계 추적

큐비트 같은 자원을 생각하면 특히 다음이 중요하다.

- 관측 이후 상태 변화
- 중복 불가능성
- 자원 간 관계 변화

즉 미래 방향은:

```text
slot + ownership + capability + transition tracking
```

이다.

## 11. 설계 문답

### Q. 왜 포인터를 중심에 두지 않는가?

포인터는 클래식 메모리 환경에서는 강력하지만, 모든 자원 모델의 보편적 기초는 아니다.
Pergyra는 더 넓은 자원 문제를 다루기 위해 주소보다 점유권을 우선한다.

### Q. 그러면 Pergyra는 시스템 언어가 아닌가?

시스템 언어다.
다만 시스템을 주소 조작의 집합으로 보지 않고, 자원 통제의 집합으로 보려는 시스템 언어다.

### Q. `party/world`가 핵심인가?

중요하지만 최심부는 아니다.
그것들은 합성 계층이고, 더 깊은 핵심은 슬롯, 제네릭, ability, 권한, 생명주기다.

### Q. 이 접근의 위험은?

- 런타임과 타입 시스템 복잡도가 증가할 수 있다
- 기존 포인터 중심 사고에 익숙한 사용자에게 낯설 수 있다
- 자원 모델을 지나치게 넓게 잡으면 언어가 흐려질 수 있다

그래서 언제나 다음 질문으로 돌아가야 한다.

- 이 기능은 자원 점유를 더 명확하게 만드는가
- 이 기능은 제네릭 자원 표현을 더 아름답게 만드는가
- 이 기능은 주소 중심 사고를 줄이는가

그렇지 않다면 Pergyra의 중심이 아닐 가능성이 크다.

## 12. 최종 정리

Pergyra의 설계 철학은 다음 문장으로 요약된다.

**Pergyra는 복잡한 타입 관계를 아름답게 표현하면서, 추적하기 어려운 자원을 슬롯 단위로 통제하려는 언어다.**

다시 말해:

- 포인터보다 점유권
- 주소보다 권한
- 메모리보다 자원
- 문법 과시보다 추적 가능성

이 기준이 앞으로의 모든 설계 결정을 통과하는지 검증해야 한다.
