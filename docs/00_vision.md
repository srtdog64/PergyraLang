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

## Compiler-Owned Ownership Cleanup

이 핵심 기제의 이름은 **소유권 기반 자동 메모리 관리**
(Ownership-Based Automatic Memory Management)다. 목표는 **GC 같은 편의성을
소유권 증거로 제공하는 것**이다. 개발자는 값과 실제 자원 경계를 작성하고,
컴파일러가 이동·대여·필요한 복사와 정리를 파생한다. 이 기제는 퍼질라의
권한·자원·수명 모델을 받치는 일반 값의 기본 관리 방식이며, 별도의 추적 GC를
붙이거나 사용자가 정리 증명을 작성하는 방식이 아니다.

2026-10-08에 선택한 방향은 **GC가 아닌 소유권 기반 자동 정리**다. 값의 실제
소유자, 이동, 대여, 반환과 자원 경계를 컴파일러가 추론하고, 남아 있는 정리
의무를 정의된 종료 경로에 합성한다. 일반 코드를 작성하기 위해 사용자가
`ArrayDrop`, 필드 추출·복원, 추가 `own`/`ref`로 증명 절차를 손으로 수행하는
것을 기본 모델로 삼지 않는다. 실질적인 권한·이동·FFI·비용 경계는 계속
명시적이며, 추론 실패를 숨기거나 공유 backing을 독점 소유로 간주하지 않는다.

이는 현재 자동 해제가 구현됐다는 선언이 아니다. 소유권/대여 사실과 CFG
정리 의무를 내부 IR에서 결합하는 방향이며, 추적 GC나 전역 참조 카운팅을
기본 해법으로 도입하지 않는다. 필드 `inout`, readonly 임시값, 값 복사/공유
의미와 cleanup 순서는 전체 소유 사슬을 정한 뒤 검증해야 한다. self-host
컴파일러의 코딩 스타일 때문에 언어 계약을 확장하거나, 거부된 지점마다
표기·복사를 추가해 해결했다고 기록하지 않는다. 설계 및 재검증 근거는
[`소유권 자동 정리 아키텍처 재검토`](audits/ownership_dx_architecture_recheck_2026-10-08.md)에 있다.

안전성 목표는 허용된 프로그램의 사용 후 해제·이중 해제 방지, 마지막 사용
이후의 정확한 정리와 관찰 결과 보존이다. 올바른 GC도 메모리 안전할 수 있으므로
"일반 GC보다 무조건 안전하다"고 선언하지 않는다. 성능 이점은 같은 값·복사·
할당/회수 비용을 둔 명시적 비교 모델에서 생존 검사와 지연 보유량을 줄이는
것으로 검증한다. 실제 실행 시간, 최대 메모리, 큰 집합체의 해제 지연과 컴파일
비용은 C/LLVM 실행 증거가 필요하다. 이름과 방향의 계약은
[27번 문서 §0](semantics/27_ownership_clean.md#0-adopted-name-and-comparison-boundary),
비교 명제와 반례는 [알고리즘 문서 §12](207_compiler_owned_cleanup_algorithm.md#12-소유권-기반-자동-메모리-관리와-gc-비교)에 둔다.

Slot·자동 소유권 정리·공유 그래프는 **하나의 저장소 소유자, 그 소유자에 묶인
접근 권한, 하나의 정리 경로**로 연결한다. 연결은 공유·순환할 수 있지만 정리
책임을 복제하지 않는다. Slot은 실제 수명·권한 경계에 쓰며 일반 값이나 링크마다
요구하지 않는다. owner 종료 뒤 링크 접근은 거부하고, 살아 있는 owner 내부의
고립된 노드 자동 회수와는 구분한다. 결합 요건은
[28번 통합 계약](semantics/28_memory_boundary_composition.md)에 둔다.
제한 모델의 결합 증명은 실제 그래프 지원·자동 해제·런타임 원자성의 완료가 아니다.

## Real-Workload Admission And Authoring Experience

실제 Pergyra Agents 작성 경험도 비전의 검증 입력이다. 기준은 Agents
`fa136620f94b73bd1e330b108546d4efba892ae6`가 사용한 compiler pin
`8c3f074aae035d1b6c069daf3cf00f14e89cba6f`이며, 과거 발견을 현재 컴파일러의
결함으로 자동 승격하지 않는다. 현재 소스와 설치 경로를 다시 확인하여
`FIXED`, `OPEN`, 설계 제약, 미검증을 구분한다. 오래된 어휘 감사보다 실제
`HarnessWorld`, `CompleteTask`, `Delegate`, `PursueGoal`과 실행 증거가 우선한다.

유지할 강점은 `subject/action/vessel/tobject`의 책임·상태·결과 구분,
`intent`의 참여자·단계·typed terminal, `zone/authority/effect`의 명시성이다.
기계적 선택을 줄이는 것은 이 의미 경계를 클래스나 함수로 평탄화하는 일이
아니다. 좋은 어휘가 SRP를 자동 강제하지도 않는다. 구성체 선택은 작은 예제와
실제 workload에서 목적·상태·값·자원·실패의 소유자가 드러나는지로 검증한다.

다음 일곱 축을 실사용 입장에서 닫는다. 우선순위는 의미 동등성과
소유권/FFI 안전 경계, 그다음 진단·API 발견성과 DX다.

1. **경로 동등성:** native/self-host와 적용 C/LLVM 경로의 acceptance,
   rejection, 결과, 안정적 diagnostic code/span은 같은 owner 사실을 소비해야
   한다. enum 배열(PP-071), 긴 문자열 식의 parser overflow(PP-072), 임시
   aggregate의 addressable-storage 요구, import/constructor/authority 차이는
   동일 입력으로 재검증한다. 해결된 항목에는 실제 revision과 회귀 gate를
   연결하고, 의도된 제한은 사용자에게 설명한다. 다른 경로로 조용히 재시도해
   성공처럼 보이게 하지 않는다.
2. **권한 집행:** caps의 선언, 정적 호출 전이 보증, runtime grant 집행,
   외부 OS sandbox는 다른 주장이다. `with caps` 생략이 빈 권한 상한을 뜻하지
   않는 현재 설계와 추론된 manifest를 구분한다. extern 경계(PP-024)의 실제
   집행은 별도 증거가 필요하다. zone 이름이나 effect 선언만으로 파일 격리,
   도구 권한, 비밀 보호가 완성됐다고 표현하지 않는다.
3. **수명과 FFI:** aggregate/collection의 field extraction, borrow, move,
   Clone, alias, return, retention, release는 같은 소유자와 마지막 소비자를
   가리켜야 한다. FFI String/handle의 수명과 ABI/layout도 그 경계에서
   검증한다. PP-067 같은 발견은 현재 ownership 사슬의 반증으로 연결하되,
   좁은 성공 시험을 전체 메모리 안전성 증명으로 세지 않는다. C shim에 의미를
   복제하거나 guessed fallback으로 증거를 대신하지 않는다.
4. **발견 가능한 API:** canonical API owner로부터 문서, signature, 예제,
   실패·소유권 설명이 일치해야 한다. 이미 존재하는 `SubIndexOf`와
   `SubIndexOfWithLen`을 찾지 못해 복사 기반 우회를 만든 PP-066은 발견성
   문제다. JSON/HTTP/process/env/console의 boilerplate는 실제 작성 비용으로
   검토하되 stdlib/runtime/OS adapter를 compiler core에 무조건 합치지 않는다.
   linker, SDK, packaging과 editor/LSP도 기존 소유 경계를 유지한다.
5. **원인을 설명하는 진단:** 사용자 오용, 의도된 제약, 미지원 projection,
   compiler 내부 결함을 구분하고 원인·위치·올바른 관용구를 안내한다. 익숙한
   match/temporary/constructor 모양이 다른 의미를 가질 때 이를 숨기지 않는다.
   새 문법·설정·API보다 기존 정식 문법과 owner를 먼저 확인한다.
6. **작성 경험과 SRP:** 중요한 결정에는 한 owner를 둔다. Agents의 AgentRun
   내부 SRP는 Agents 저장소 책임이며, compiler 비전에는 이를 지원하는 작성
   경험과 진단 원칙으로 연결한다. 함수 길이, 구성체 수, 키워드 사용량만으로
   언어 품질이나 self-host 대체 진행률을 판정하지 않는다.
7. **반증 가능한 주장:** workload CI green은 그 입력의 scoped acceptance다.
   다른 언어 대비 성능 우위나 전 범위 안전성 증명이 아니다. 같은 workload,
   target, toolchain, 권한과 provenance에서 latency/throughput/memory/compile
   time 및 tail regression을 분리한다. 합성/loopback과 실제 운영/API 검증은
   섞지 않는다. 성능 작업은 다음 활성 폐쇄 단계가 관측 비용에 막혔을 때만
   그 owner의 막힌 연산을 바꾸고, 같은 입력의 반증 gate를 다시 실행한다.

각 축의 현상·revision·분류·우선순위·fact owner·last consumer·금지 fallback·
gate/반증·완료 조건은
[`실사용 비전 검증 카드`](agent_work_directives/pergyra_agents_vision_admission_2026-10-07.md)에
연결한다. 이 절과 작업 카드는 semantic SoT가 아니며 문서 작성만으로
registry를 `CLOSED` 처리하지 않는다. 이미 열린 MIR/collection executable rung의
수정·검증·설치 순서를 보존하고, 확인된 다음 결함은 그 사슬의 합법적 경계에서
착수한다. 외부 SDK/서비스나 다른 저장소 변경은 별도 작업 범위다.

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

## Compiler World Vision — Pergyra 자체를 닮은 컴파일러

Pergyra로 작성된 컴파일러의 목표는 기존 C 컴파일러의 폴더와 pass를
`world`, `zone`, `subject`, `action`, `intent`로 이름만 바꾸는 것이 아니다.
언어의 구문을 사용했다는 사실과 언어가 추구하는 구조를 실제로 따랐다는
사실은 다르다. 최종 컴파일러는 **목적에 따라 소유된 fact를 만들고, 검증된
projection을 거쳐, typed outcome과 artifact를 발행하는 compiler world**여야
한다.

### 구성체를 쓰는 기준

- `world`는 한 compiler singleton이나 모든 fact의 거대한 저장소가 아니다.
  compile, check, format, debug처럼 외부에 의미 있는 목적과 그 목적이 사용하는
  resource/authority 경계를 조합하는 composition root다.
- `intent`는 pass 목록이 아니다. `CompileProgram`, `CheckProgram`,
  `FormatSource`, `DebugProgram`처럼 성공, 실패, 참여자, authority, effect,
  compensation, trace의 귀속이 필요한 실제 목적을 묶는다. 모든 도구 동작을 한
  거대한 compiler intent에 넣지 않는다.
- `zone`은 IR 이름을 분류하는 namespace가 아니다. compilation revision,
  target environment, artifact transaction처럼 lifetime, resource, authority가
  실제로 갈리는 경계에만 둔다. Fact 묶음은 별도 lifetime이나 authority가
  없다면 zone이 아니라 typed graph/value다.
- `subject`는 compilation session이나 artifact publisher처럼 stable identity와
  authority를 가진 주체에 사용한다. Lexer, parser, type solver가 순수 변환일
  뿐이라면 억지로 subject로 승격하지 않는다.
- `action`은 admitted request, state, resource, artifact의 실제 전이를 소유할 때
  사용한다. 단순 readiness Bool이나 한 번의 함수 위임은 action을 정당화하지
  않는다.
- 순수한 lexing, parsing, unification, graph construction, CFG analysis,
  optimization은 기본적으로 `func`와 `struct`로 남는다. Pergyra의 도메인
  구성체는 모든 계산을 감싸는 장식이 아니다.

### Slot과 저수준 identity

Slot은 Pergyra의 중요한 resource-boundary 도구지만 compiler 전체의 보편적인
저수준 ontology는 아니다. Zone 안에서 자원 점유와 전송을 표현하는 Slot,
소스 프로그램이 선언한 binding slot, compiler 내부의 SSA value와 syntax
identity를 서로 같은 개념으로 취급하지 않는다.

Compiler 내부의 규범적 저수준 형태는 다음과 같다.

```text
CompilationRevisionId
  -> immutable arena / fact table
  -> typed stable handle
  -> semantic and execution overlays
  -> verified projection plan
```

Raw pointer, 주소, 컨테이너 재할당 위치, Slot ordinal, generation counter,
display spelling은 semantic identity의 주인이 될 수 없다. 구현 내부에서 주소를
일시적으로 사용하더라도 owner 경계를 넘어 의미 권위로 운반해서는 안 된다.
Cross-stage와 serialized artifact에는 revision에 결속된 typed handle과 명시적인
identity relation을 사용한다. 이 원칙은 resource Slot 모델을 약화하는 것이
아니라, Slot을 실제 resource boundary에만 강하게 남기는 규칙이다.

### 소유된 fact graph와 projection

목표 구조는 복사된 tree의 선형 pass 사슬이 아니라, 같은 stable identity를
공유하는 직교 fact graph다.

```text
PgyCompilerWorld
  -> Compile / Check / Format / Debug intent
  -> CompilationRevision boundary
       -> Source facts and provenance
       -> TypeDag + HIR semantic entities
       -> DIR domain and purpose relations
       -> RIR resource, authority, and transfer facts
       -> MIR execution, CFG, SSA, ownership, and cleanup facts
       -> AIR evidence certificate
  -> ABI and target-capability facts
  -> VerifiedProjectionPlan
       -> C projection
       -> LLVM projection
       -> self-host projection
  -> ArtifactTransaction boundary
       -> published artifact | typed rejection
```

DIR, RIR, MIR, AIR은 같은 것을 다른 파일에 복사한 계층이 아니다. Domain
relation, resource transition, executable behavior, proof/evidence라는 서로 다른
의미 축을 소유한다. Backend는 source text, AST payload, 이름, Slot 모양에서
그 의미를 복구하지 않는다. AIR 자체도 backend input이 아니다. AIR의 compact
certificate와 MIR/ABI/target facts를 소비해 만든 하나의
`VerifiedProjectionPlan`을 C, LLVM, self-host가 peer projection으로 소비한다.

Generic은 semantic boundary와 target specialization에서 강하게 사용하되,
실행 hot path의 projection plan은 구체적이어야 한다.

> **Generic at semantic boundaries, concrete in hot paths.**

### SoT 폐쇄 장치와 최종 구조의 분리

SoT registry, bridge receipt, parity oracle, negative gate는 migration을 안전하게
닫기 위한 장치다. 이것들의 개수나 파일 수는 compiler architecture의 진척이
아니다. Owner identity는 책임과 evidence lifetime을 고정하지만 owner마다
반드시 별도 파일, wrapper, serializer, plan을 만들라는 뜻이 아니다.

한 row가 닫힐 때는 consumer migration, missing-fact refusal, old-path deletion,
negative gate가 남아야 한다. 반대로 임시 dual-read, compatibility wrapper,
fixture-shaped plan, 반복 validation과 중간 serialization은 줄어야 한다.
폐쇄할수록 compiler의 production path가 더 단순해져야 하며, 이행 구조를
영구적인 architecture로 굳혀서는 안 된다.

이 비전의 현재 구현 등급은 항상
[`self_hosted/17_pergyra_native_dogfood_contract.md`](self_hosted/17_pergyra_native_dogfood_contract.md),
[`semantics/sot_owner_spine_registry.md`](semantics/sot_owner_spine_registry.md),
[`current_work_handoff.md`](current_work_handoff.md)의 현재 evidence로 판정한다.
[`self_hosted/14_target_compiler_world.md`](self_hosted/14_target_compiler_world.md)와
[`180_compiler_logical_spine_handles_gates.md`](180_compiler_logical_spine_handles_gates.md)는
목표 frame과 migration protocol을 제공하지만, 그 자체로 self-hosting 완료나
production substitution을 증명하지 않는다.

### `WHAT MUST HOLD`와 외부 설계 근거의 경계

Pergyra의 언어 의미론은 컴파일러가 판정할 수 있는 현재 사실과 계약만
소유한다. `state`, `invariant`, `authority`, `ownership`, `capability`,
`effect`, `transition`, `intent`, `type`, `boundary`가 여기에 해당한다.
`intent` 역시 자연어 설명이 아니라 참여자, 권한, 효과, 성공과 실패,
보상과 trace 의무를 검사할 수 있는 목적 경계다.

반대로 어떤 선택의 조직적·사업적 이유, 기각된 대안, 회의 기록은 언어
semantics가 아니다. ADR, issue, requirement, design document, commit과 같은
외부 engineering artifact가 그 이력을 소유한다. IDE는 stable semantic
identity를 통해 이 자료를 연결해 보여줄 수 있지만, 그 prose를 컴파일러의
권위나 판정 근거로 승격하지 않는다.

기계가 발행하는 `reason`은 예외다. 닫힌 code/domain을 가지며 owner fact와
gate로 검증되는 diagnostic·projection reason은 설명문이 아니라 판정 결과다.
자유 형식 rationale 문자열은 여기에 해당하지 않는다.

따라서 Pergyra는 Naur가 말한 programmer theory 전체를 언어 안에 저장하려
하지 않는다. 인간이 유지하는 theory가 깨졌을 때 드러나는 구조적 위반을
검증 가능한 semantic constraint로 최대한 일찍 거부한다. 목표는 더 많은
설명문이 아니라, 잘못된 구현을 표현할 수 없게 하는 더 적고 강한 사실이다.

## 한 문장 정의

**Pergyra는 포인터를 숨기기 위한 언어가 아니라, 추적하기 어려운 자원을 슬롯 단위로 통제하기 위한 언어다.**

---

## Intent-First 설계 철학

Pergyra의 가장 큰 특징은 **Intent를 최상위 설계 축으로 둔다**는 것이다.
대부분의 언어는 함수/타입/클래스를 1차로 두지만, Pergyra는 "누가 무엇을 위해 행동하는가"를 먼저 정의한다.

자세한 설계 철학과 좋은 Intent를 정의하는 방법은 [`docs/01_intent_first_design.md`](01_intent_first_design.md)를 참조하라.

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
