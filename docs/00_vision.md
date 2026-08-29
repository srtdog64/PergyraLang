# Pergyra 언어 비전

## Developer Experience Prime Directive

> **개발자가 즐거워야 유저도 즐겁다.**

Pergyra는 개발자 경험을 안전성이나 성능 뒤에 붙는 편의 기능으로 보지
않는다. DX는 언어의 핵심 불변식이다. 개발자는 목적, 자원, 권한, 손실 허용
범위처럼 프로그램의 의미를 선언한다. 증명 전략, 실행 lane, materialization,
ABI projection처럼 컴파일러가 소유할 수 있는 기계적 선택은 기본적으로
컴파일러가 파생한다.

이 원칙은 검사를 숨기거나 실패를 런타임으로 미룬다는 뜻이 아니다. 파생된
결정은 diagnostics, explain output, AIR/MIR facts로 관찰 가능해야 하며, 증거가
부족하면 fail closed 해야 한다. 사용자의 선택이 필요한 실제 권한·비용·외부
경계에서는 하나의 안전한 기본 경로와 하나의 명시적 escape hatch를 제공한다.

## Machine-Neutral Compute Vision

Pergyra should not make the von Neumann CPU the shape of the language. C and
LLVM are the first validation projections, not the final execution ontology.
The CPU is therefore a current projection target, not the language's ontology.

The long-term source of truth is the fact pipeline: `intent`, `effect`,
`authority`, `coordination`, `slot`, `world`, and `zone` must survive as
AIR/MIR/ABI owner facts until a backend either consumes them or fails closed.
That is what makes future dataflow, actor, tensor, capability-machine,
reconfigurable, and event-driven substrates plausible without changing source
semantics.

The useful sharp edge is projection replacement. A future NPU/tensor backend
should not need a new source language; it should consume the same intent,
effect, authority, coordination, slot, layout, loss-budget, and materialization
facts that the C and LLVM projections consume today. If that backend cannot
accept a program, the rejection or CPU fallback must be fact-backed and visible,
not a hidden backend convenience.

The governing contract is
[`docs/semantics/18_machine_neutral_compute.md`](semantics/18_machine_neutral_compute.md).
Do not advertise those substrates as current support; they are future backend
projections that must consume the same owner facts and pass their own golden
tests.

The same contract also defines the IR boundary: AIR/evidence is the
machine-neutral fact layer, while MIR is the CPU-family projection layer for
C/LLVM-style backends. Future NPU, tensor, dataflow, or GPU projections must
consume the same owner facts through their own projection IR instead of making
CPU-shaped MIR the universal ontology.

## Release Binary Reconstruction Target

Pergyra의 `--opt=release` 배포 바이너리는 같은 target과 host toolchain으로
빌드한 일반적인 **최적화·strip된 C++ 릴리스보다 소스 수준 구조를 더 쉽게
복원할 수 없어야 한다.** 이 목표는 원본의 주석, 로컬 이름, 소스 경로,
모듈 경계와 Pergyra의 `world` / `zone` / `subject` / `action` / `intent`
구조가 릴리스 전용 메타데이터 때문에 그대로 노출되지 않는 수준을 뜻한다.

이는 디컴파일이나 역공학이 불가능하다는 약속이 아니다. 실행 흐름, 상수,
문자열 리터럴, 공개 FFI/ABI는 C++ 바이너리에서도 일정 부분 복원되며,
클라이언트 바이너리에 포함된 비밀을 보호하는 보안 경계로 취급할 수 없다.
현재 상태는 `OPEN TARGET`이다. C와 LLVM 배포 경로가 동일한 정책을 소비하고,
동일 툴체인의 C++ 기준군과 누출 항목별로 비교하는 실행 게이트가 닫힐 때까지
구현 완료를 주장하지 않는다.

목표의 단일 계약과 판정 조건은
[`docs/release/binary_reconstruction_resistance_target.md`](release/binary_reconstruction_resistance_target.md)가
소유한다. 이 목표를 달성하기 위해 언어 의미를 난독화하거나 packer,
anti-debugging, self-modifying code를 기본 기능으로 도입하지 않는다.

## Beta Then Self-Hosting

Self-hosting is a post-beta validation target, not a beta blocker.

Dedicated self-hosting preparation lives under
[`docs/self_hosted/README.md`](self_hosted/README.md). That folder is the
handoff entry point for future agents and should be read only after the beta
source-of-truth documents.

The beta goal is to close the core first: CFG body safety, AIR evidence,
DAG resolution, MIR/C/LLVM parity, ABI ownership, and the dogfood path. After
that closure, Pergyra should start dogfooding with compiler-adjacent tools
written in Pergyra and checked against the existing C implementation.

The intended order is:

1. Finish beta closure and freeze the stable core surface.
2. Dogfood small tools first: diagnostic catalog checks, AIR graph JSON
   validation, MIR dump diffing, backend output comparison, and module/package
   resolver helpers.
3. Move to beta+ self-hosting work only after those tools can be compiled by
   the existing compiler and compared against the C implementation.
4. Treat full compiler self-hosting as a long-term proof of the language, not
   as the first dogfood milestone.

This keeps C and LLVM as validation anchors. Pergyra code should be compared
against the existing C compiler behavior before any self-hosted component is
allowed to become authoritative.

The ownership model is also intentionally not Rust-style lifetime programming.
Pergyra does not try to statically predict every business-object lifetime.
Instead, it uses Slot as a resource boundary: static checks reject unsafe
boundary transitions, while runtime handles validate generation, token, and
resource state. This is a deliberate design choice, not a missing Rust borrow
checker.

## 한 문장 정의

**Pergyra는 포인터를 숨기기 위한 언어가 아니라, 추적하기 어려운 자원을 슬롯 단위로 통제하기 위한 언어다.**

---

## Intent-First 설계 철학

Pergyra의 가장 큰 특징은 **Intent를 최상위 설계 축으로 둔다**는 것이다.
대부분의 언어는 함수/타입/클래스를 1차로 두지만, Pergyra는 "누가 무엇을 위해 행동하는가"를 먼저 정의한다.

자세한 설계 철학과 좋은 Intent를 정의하는 방법은 [`docs/01_intent_first_design.md`](docs/01_intent_first_design.md)를 참조하라.

---

## 두 가지 정체성

### 1차 정체성 — 도메인 모델링 언어

복잡한 도메인에서 **왜(intent), 어떤 세계/장면에서(world/zone), 누가(subject), 무슨 자격으로(ability), 무슨 결과로(effect)** 행동하는가를 선언하고 컴파일 타임에 검증하는 언어.

중요한 기준:

- `subject`는 core host다
- 하지만 **문서와 예제의 첫 축은 `intent`** 여야 한다
- 독자는 예제를 `intent -> world -> zone -> subject` 순서로 읽어야 한다
- supporting declaration은 그 계약을 닫기 위해 뒤따르는 구조다

### 2차 정체성 — A2M(Agent-to-Machine) 인터페이스 언어

AI 에이전트가 기계, 장비, 공정, 외부 시스템을 **안전하게** 통제하기 위해 사용하는 인터페이스 언어.

```
A2M 핵심 요구                    Pergyra의 대응
──────────────────────────────────────────────────────────
의도 표현이 명확해야 한다         intent 선언 — 왜 하는가
자원 점유/해제가 추적 가능        Slot 프로토콜 — claim/release 추적
승인과 실행 자격이 분리           requires + authorized by — 자격/승인 분리
원격/지연/실패/보상 경로가 보여야 함  Result<T> + compensate + rollback policy
닫힌 시스템으로 완결 가능          intent-first — 필요한 것만, 업데이트 전제 안 함
```

AI 에이전트가 Pergyra intent를 발행하면:
- **의도가 명시적** — "이 기계를 가동하라"가 아니라 "StartMachine intent: 자격 MachineOperator, zone FactoryFloor, 승인 supervisor"
- **자원이 추적됨** — 기계의 slot을 claim하고, 작업 후 release. 점유 중 다른 에이전트 접근 차단
- **실패가 보상됨** — step 실패 시 compensate로 안전 상태 복원
- **인간이 읽을 수 있음** — intent 선언을 읽으면 에이전트가 뭘 하려는지 인간도 안다

```pergyra
// AI 에이전트가 발행하는 intent
intent StartProduction(operator: AIAgent)
{
    exclusive;
    who: operator;

    step prepare
    {
        where: FactoryFloor;
        requires: MachineOperator;
        authorized by: supervisor;     // 인간 승인 필요
        on: operator.InitMachine();
        compensate: operator.EmergencyStop();
    }

    step run
    {
        where: FactoryFloor;
        on: operator.RunCycle();
        post: machine.output > 0;
        guard: machine.temperature < 100;  // 실시간 안전 조건
        compensate: operator.CoolDown();
    }

    success: machine.output >= target;
    failure: rollback;
}
```

에이전트가 아무리 복잡한 intent를 발행해도, Pergyra의 계약(requires, authorized by, guard, compensate)이 안전 경계를 보장한다. 인간은 intent 선언을 읽어서 에이전트의 의도를 감사(audit)할 수 있다.

**"인간도 잘 쓰면 좋고"** — Pergyra는 AI-first가 아니라 intent-first다. intent가 명확하면 발행자가 AI든 인간이든 상관없다.

### 예제 제시 규칙

비전 문서와 튜토리얼은 다음 규칙을 따른다.

1. 먼저 `intent` 계약을 보여 준다
2. 그 intent가 놓일 `world` / `zone` 경계를 보여 준다
3. 마지막에 그 계약을 수행하는 `subject`와 supporting type을 보여 준다

즉 **예제의 설명 순서**는 항상 `intent -> world -> zone -> subject`다.

컴파일 가능한 파일의 선언 순서가 이와 다를 수는 있다.
하지만 그것은 parser/normalization 제약일 뿐이고, 문서가 독자에게 가르쳐야 할 사고 순서와는 다르다.

---

## 핵심 통찰

### 메모리는 자원의 한 종류일 뿐이다

전통적 시스템 언어(C, Rust)는 "메모리 주소"를 프로그래밍의 기본 단위로 삼는다.
포인터를 역참조하고, 주소를 알고, 읽어도 상태가 변하지 않는다는 전제 위에 서 있다.

이 전제는 클래식 컴퓨팅에서는 성립하지만, 영원하지 않다.

- 큐비트: 관측하면 상태가 붕괴한다. "들여다보는" 메모리 모델이 성립하지 않는다.
- 분산 자원: 네트워크 너머의 상태는 로컬 주소로 참조할 수 없다.
- 하드웨어 가속기: GPU/TPU/FPGA의 메모리는 호스트와 주소 공간이 다르다.

공통점: 자원이 거기 있다는 건 알지만, 주소로 직접 접근하는 것은 불가능하거나 위험하다.

### 세마포어/뮤텍스의 교훈

Dijkstra의 세마포어는 자원을 이렇게 다뤘다:

```text
자원이 어디 있는지는 모른다.
자원이 존재한다는 것만 안다.
P(acquire) - 자원을 점유한다.
V(release) - 자원을 반환한다.
```

세마포어는 자원의 주소를 묻지 않는다. 자원의 점유 상태만 추적한다.
이것이 동시성 문제를 풀 수 있었던 이유다.

Pergyra의 Slot은 같은 철학을 메모리/상태 관리에 적용한다:

```text
Slot이 어디 있는지는 모른다.
Slot이 존재한다는 것만 안다.
Claim   - 자원을 점유한다.
Read    - 자원의 현재 값을 얻는다.
Write   - 자원의 값을 변경한다.
Release - 자원을 반환한다.
```

현재 구현에서는 이 추상화가 한 타입으로 완전히 수렴한 것은 아니다.

- `Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`는 로컬에 고정된 anchored resource handle이다.
- `QubitSlot`은 복사 불가 move-only resource handle이다.
- 원격/지연 계산은 슬롯 자체를 직접 노출하기보다 `RemoteFuture<T>`를 `await`해 `Result<T>`로 회수한다.

---

## Slot = 자원 점유권

`Slot<T>`는 "T를 저장하는 메모리 위치"가 아니라 "T 타입 자원에 대한 점유권"이다.
현재 구현에서는 이 규율이 특히 로컬 anchored handle 계열(`Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`)에 직접 적용된다.

| 개념 | 포인터 모델 | Slot 모델 |
|------|-------------|-----------|
| 정체 | 메모리 주소 | 자원 핸들 |
| 읽기 | 주소 역참조 (`*ptr`) | 점유 상태에서 값 요청 (`Read(slot)`) |
| 쓰기 | 주소에 값 저장 (`*ptr = v`) | 점유 상태에서 값 변경 (`Write(slot, v)`) |
| 해제 | 주소 무효화 (`free(ptr)`) | 점유권 반환 (`Release(slot)`) |
| 복사 | 주소 복사 (별칭 발생) | 금지, 단일 소유권 |
| 전제 | 주소를 알고, 언제든 접근 가능 | 점유했을 때만 접근 가능 |

### SecureSlot<T> = 권한 기반 자원 접근

`SecureSlot<T>`는 점유권에 토큰(권한)을 추가한다:

```text
올바른 토큰 없이는 Read도 Write도 불가능하다.
```

이것은 단순한 접근 제어가 아니라, 능력(capability) 기반 보안 모델이다.
자원에 대한 접근은 주소를 아는 것이 아니라, 권한을 가진 것으로 결정된다.

### RemoteFuture<T> = 원격 작업 결과 경계

분산 자원이나 디바이스 작업은 로컬 `Slot<T>`처럼 즉시 `Read/Write`하지 않는다.
현재 구현에서는 `SubmitDeviceRead(slot)` 같은 연산이 `RemoteFuture<T>`를 만들고,
`await` 결과는 항상 `Result<T>`가 된다.

즉:

```text
로컬 Future<T>       -> await -> T
원격 RemoteFuture<T> -> await -> Result<T>
```

---

## 타입 관계의 아름다운 표현

복잡한 자원 관계를 명시적으로 모델링하는 것이 Pergyra의 두 번째 축이다.

```text
ability = 자원이 할 수 있는 것 (인터페이스)
role    = 자원이 실제로 하는 것 (구현)
party   = 자원들의 협력 단위 (합성)
world   = 자원 시스템의 경계 (격리)
```

이 계층은 "자원이 무엇이고, 무엇을 할 수 있고, 누구와 협력하고, 어디까지가 경계인가"를 문법으로 표현한다.

---

## 미래 확장 (v2 계획): 큐비트와 양자 자원 추적

> ⚠️ **양자 연산(Qubit/Measure/Entangle)은 현재 구현되지 않았습니다.**
> 현재 런타임의 `PgyQubit` 구조체는 간단한 시뮬레이션 스케줄톤일 뿐이며,
> 실제 양자 연산 시맨틱스는 지원되지 않습니다.
> **전체 양자 자원 모델은 Pergyra v2의 핵심 기능으로 계획되어 있습니다.**

큐비트 대응까지 생각하면 Slot 위에 추가로 필요한 것들:

| 속성 | 현재 Slot | 미래 확장 |
|------|-----------|-----------|
| 복사 금지 | `Claim/Release` 중심 | Linear/Affine 타입으로 강제 |
| 이동 중심 | Release 후 재Claim | Move 시맨틱 내장 |
| 관측 후 상태 변화 | 미지원 | `Measure(slot)` 이후 상태 붕괴 |
| 얽힘 (관계 추적) | Party로 합성 가능 | `Entangle(slotA, slotB)` 같은 관계 등록 |
| 권한 기반 접근 | `SecureSlot` 토큰 | Capability 타입 시스템 확장 |

확장 방향:

```pergyra
// 미래 - 양자 자원 슬롯
let q: QubitSlot = ClaimQubit();
let result: Bool = Measure(q);    // 관측 -> 상태 붕괴 -> 이후 Read 불가
// Read(q);  // 컴파일 에러: 측정된 큐비트는 읽을 수 없음

// 미래 - 얽힘
let a: QubitSlot = ClaimQubit();
let b: QubitSlot = ClaimQubit();
Entangle(a, b);                   // 관계 등록
let ra: Bool = Measure(a);        // a 측정 -> b의 상태도 결정됨
```

현재 구현 메모 (v1 기준):

- `PgyQubit` struct는 런타임에 존재하지만 **시뮬레이션 스케줄톤**일 뿐이다.
- `ClaimQubit()`, `Measure()`, `Entangle()` 함수는 기술적으로 컴파일되지만 **양자 시맨틱스를 보장하지 않는다**.
- 현재 런타임은 두 큐비트가 같은 값으로 붕괴하는 simple same-value pair 모델만 제공한다.
- 목적은 양자 물리의 완전한 재현이 아니라, "주소가 아닌 자원 점유권" 모델이 이런 자원에도 적용될 수 있음을 보여주는 것이다.
- **진정한 양자 연산(Linear 타입, Measure 후 상태 붕괴 추적, 얽힘 관계 검증)은 v2에서 구현된다.**

---

## 요약

```text
Pergyra는:
  복잡한 타입 관계는 아름답게 표현하되,
  메모리 모델은 인간에게 덜 적대적인 언어.

그 근거는:
  자원은 주소가 아니라 점유권과 전송 경계로 다룬다.
  세마포어가 P/V로 자원을 추상화했듯이,
  로컬 anchored handle은 Claim/Read/Write/Release로,
  원격 작업은 Submit/Await/Result로 자원을 추상화한다.

그 미래는:
  클래식 메모리든, 큐비트든, 분산 자원이든,
  추적하기 어려운 자원은 모두 Slot으로 통제할 수 있다.
  포인터는 클래식 컴퓨팅의 유물이고,
  Slot은 자원 추상화의 시작이다.
```
 
---

## Post-1.0 Self-Hosting Vision

Self-hosting is a long-term credibility goal, not a beta or 1.0
requirement.

Current state:

- The compiler core is implemented in C.
- The stable compiler contract is C/LLVM dual emission, not a
  Pergyra-written compiler.
- External claims must not say "self-hosted", "written in Pergyra", or
  "self-hosting language" as current capability.

Why it still belongs in the vision:

- A language that models intent, zones, resource handles, ABI boundaries,
  diagnostics, and compiler evidence should eventually be able to express
  parts of its own toolchain.
- Self-hosting would test whether Pergyra can model real systems work
  without collapsing into compiler-specific shortcuts.
- Partial self-hosting is the pragmatic path: formatter, package metadata,
  diagnostic fixtures, small IR transforms, and smoke tools should come
  before parser/type-checker/codegen migration.

Trajectory:

1. Soft self-host: compiler-adjacent tools and generated fixtures in
   Pergyra.
2. Partial self-host: selected analysis or transform passes that consume
   stable AIR/MIR/diagnostic JSON.
3. Hard self-host: frontend or backend migration only after the stable
   subset, AIR, CFG/body dataflow, DAG, ABI, and backend parity are already
   frozen.

This vision is governed by
[`docs/117_backend_strategy_positioning.md`](117_backend_strategy_positioning.md)
and
[`docs/120_vision_and_capability_audit.md`](120_vision_and_capability_audit.md).
If those documents say self-host is deferred, this document must not be
quoted as a current roadmap commitment.
