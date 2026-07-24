# Market Safety Positioning: The Category After Memory Safety

Status: `strategy-input, proposed` — 시장 포지셔닝 분석 입력. 시맨틱 권위 아님.
Date: 2026-07-24

이 문서는 외부 주장 라이선스가 아니다. 외부 표현은 여전히
`docs/118_slot_model_rigor_audit.md`, `docs/119_pergyra_lineage_positioning.md`,
`docs/120_vision_and_capability_audit.md`의 anti-hype 규칙을 따른다. 이 문서
자체도 그 규칙에 부합한다: "Rust보다 나은 메모리 안전"을 주장하지 않는다.

핵심 논지 요약 (전문은 아래):

- Rust의 한 문장: "Safe Rust에서는 메모리 안전 위반을 허용하지 않는다."
- Pergyra가 노릴 한 문장: **"의도·권한·수명·예산이 닫히지 않은 효과는 실행되지
  않는다"** (No effect without intent, authority, lifetime, and budget).
- "UB"라는 용어는 좁게(표준 UB 계열에만) 쓴다. 권한/수명/자원/도메인 위반은
  Authority / Lifecycle / Resource / Intent Safety로 분리해 부른다.
- 첫 시장 wedge 권고: **Authority-safe concurrent workflows** (AI agent/tool
  orchestration, 서버 workflow, plugin runtime, 금융·업무 상태 전이, 게임 서버,
  산업 자동화).
- 검증은 게이트 이름이 아니라 bug corpus로: authority(wrong tenant, confused
  deputy)/lifecycle(orphan task, cancellation leak)/budget(spawn storm, API cost
  bomb)/intent(invalid step order, partial commit) 사례별로 C#·TS·Rust 대비
  compile-time rejection·runtime guard·ceremony·성능 비용을 비교한다.
- 우선순위: Authority > Resource/Cost > Task/Lifecycle > Fail-Closed >
  Intent(최대 차별점, 설명 난이도 높음) > FFI/ABI > Deterministic > (full memory
  safety는 직접 차별점 아님).

기존 구현 축과의 대응 (참조용):

| 문서 축 | 현재 구현 앵커 |
|---|---|
| Authority Safety | `authority`, capability mask (`docs/196` PgyRuntimeContext) |
| Resource/Budget Safety | `PgyBudgetState`, effect-budget 결합은 미구현 방향 |
| Lifecycle Safety | zone/slot lifecycle, cancellation tree(§4 runtime rung 미완) |
| Fail-Closed Safety | 전역 fail-closed 규율, `docs/107` 실패 정책 |
| Intent Safety | `intent` step/compensation/pre/post — 최대 차별점 |

---

## 원문 (사용자 분석, 2026-07-24)

# 결론
**있습니다.** 다만 엄밀히 말하면 대부분은 C/C++ 표준의 `undefined behavior`가 아니라, 시장이 실제로 비용을 지불하고 없애려는 **운영·권한·수명·도메인 의미의 안전성 위반**입니다.
Rust가 잡은 질문은 매우 선명했습니다.
```text
메모리를 잘못 사용해도
프로그램이 조용히 계속 실행되어서는 안 된다.
```
Pergyra가 노릴 수 있는 다음 질문은 이것입니다.
```text
권한·수명·자원·도메인 계약을 위반한 효과가
조용히 실행되어서는 안 된다.
```
내 판단으로는 시장에 남아 있는 가장 큰 공백은 **Authority + Lifecycle + Resource Safety**, 그리고 이를 묶는 **Intent Safety**입니다.

## 1. 먼저 "UB"라는 말은 좁게 써야 한다
엄밀한 프로그래밍 언어 의미의 UB는 다음 계열입니다.
```text
use-after-free
out-of-bounds
invalid pointer provenance
data race
uninitialized read
signed overflow
잘못된 ABI / calling convention
```
접근권한 오류, 결제 순서 오류, task leak, 무제한 CPU 사용은 보통 "정의되지 않은 동작"은 아닙니다. 동작은 정의되어 있지만 결과가 위험한 것입니다.
따라서 Pergyra가 다음을 전부 UB라고 부르면 기술적으로 신뢰를 잃을 수 있습니다.
```text
권한 위반
업무 흐름 위반
취소 실패
리소스 폭주
상태 전이 오류
```
외부 표현은 다음처럼 분리하는 것이 좋습니다.
```text
Memory Safety
Authority Safety
Lifecycle Safety
Resource Safety
Transition / Intent Safety
Projection / ABI Safety
```

# 2. 메모리 안전 자체도 아직 시장에서 끝난 문제는 아니다
Rust가 강한 해법을 제시했어도 시장 전체가 메모리 안전해진 것은 아닙니다. CISA는 여전히 제조사와 오픈소스 프로젝트에 memory-safe language 로드맵을 요구하고 있고, 외부 의존성까지 포함한 전환을 권고하고 있습니다. 즉 메모리 안전은 해결된 연구 문제가 아니라 **도입과 레거시 전환이 남은 시장 문제**입니다. [CISA]
다만 Pergyra가 시장에 들어가며 정면으로:
```text
Rust보다 더 나은 메모리 안전 언어
```
를 주장하는 것은 좋지 않습니다.
Rust는 이미 그 카테고리의 이름을 소유하고 있습니다. Pergyra에는 더 고유하고 현재 언어들이 통합적으로 해결하지 못한 문제가 있습니다.

# 3. 시장성이 큰 미해결 안전 문제

## 3.1 Authority Safety — 가장 강한 시장 수요
```text
누가
어떤 자원에
어떤 조건에서
무슨 효과를 발생시킬 수 있는가
```
현재 시장에서 가장 큰 애플리케이션 보안 문제 중 하나는 권한입니다. OWASP Top 10:2025에서도 Broken Access Control이 1위를 유지했고, 테스트된 애플리케이션 전체에서 어떤 형태로든 발견됐다고 보고합니다. OWASP는 접근 통제를 기본 거부로 구현하고, 단순 CRUD 권한이 아니라 레코드 소유권과 애플리케이션 고유의 업무 제한을 도메인 모델에서 강제하라고 권고합니다. [OWASP A01:2025]
NIST의 Zero Trust도 위치나 소유권을 근거로 암묵적 신뢰를 주지 않고, 사용자·기기·자원에 대한 인증과 인가를 명시적으로 수행하는 것을 핵심으로 둡니다. [NIST ZTA]
일반 언어에서는 보통 다음처럼 흩어집니다.
```text
annotations
middleware
RBAC library
database policy
framework interceptor
application if statements
```
Pergyra는 이것을 언어 의미로 묶을 수 있습니다.
```text
Intent
Subject
Authority
World / Zone
Effect
Resource
```
### Pergyra가 제공할 수 있는 보장
```text
외부 효과는 반드시:
  - 실행 Intent
  - 행위 주체
  - 필요한 Authority
  - 대상 Resource
  - 허용되는 World / Zone
를 가진다.
증명되지 않으면:
  static reject
  또는 explicit runtime guard
```
이것은 Pergyra가 가장 강하게 소유할 수 있는 시장 카테고리입니다.

## 3.2 Resource Safety — 클라우드 시대의 실질적인 "운영 UB"
다음 코드는 메모리 안전할 수 있습니다.
```text
무한 task 생성
무한 queue 증가
무한 API 호출
무한 로그 출력
무한 모델 토큰 소비
무제한 외부 서비스 결제
```
하지만 시스템은 죽거나 비용이 폭발합니다.
OWASP API Security Top 10은 CPU, 메모리, 스토리지, 파일 디스크립터, 프로세스, 요청 수, 제3자 API 비용을 제한하지 않는 문제를 `Unrestricted Resource Consumption`으로 분류합니다. [OWASP API4:2023]
Wasm조차 자동으로 이 문제를 해결하지 않습니다. USENIX Security 2025 연구는 악성 Wasm instance가 host 자원을 대량 소비하고 다른 instance와 운영체제 구성요소까지 성능 저하시킬 수 있음을 보였습니다. [USENIX 2025]
### Pergyra가 제공할 수 있는 보장
```text
모든 고비용 효과는:
  authority
  +
  quantitative budget
를 가진다.
```
예:
```text
allocateBytes
spawnCount
channelCount
hostCallCount
networkBytes
storageOperations
updateFuel
```
그리고 중요한 것은 단순 runtime limit가 아니라 **언어의 effect와 budget을 연결하는 것**입니다.
```text
effect Network
    requires NetworkAuthority
    consumes NetworkBudget
effect Spawn
    requires ConcurrencyAuthority
    consumes TaskBudget
```
Rust는 use-after-free를 막아도 무제한 `spawn`이나 API 비용 폭주를 막지 않습니다. 이 영역은 시장성이 큽니다.

## 3.3 Lifecycle Safety — 동시성 언어의 다음 큰 문제
메모리 lifetime은 Rust가 강하게 다룹니다. 하지만 다음은 여전히 어렵습니다.
```text
부모보다 오래 사는 task
취소되지 않는 child
blocked channel에서 영구 대기
scope 종료 후 계속 실행되는 async work
실패한 형제 task의 cleanup 누락
timeout 이후 살아 있는 작업
```
OpenJDK가 structured concurrency를 여러 JDK 릴리스에 걸쳐 계속 preview하는 이유도 관련 task를 하나의 작업 단위로 다뤄 cancellation, shutdown, error handling, reliability, observability를 개선하려는 것입니다. 명시된 목표에는 thread leak과 cancellation delay 같은 위험 제거가 포함됩니다. [JEP 499]
### Pergyra의 대응
```text
모든 task는 owner scope를 가진다.
모든 resource는 lifecycle owner를 가진다.
owner scope 종료 전:
  child 완료
  또는 실패 전파
  또는 취소 + cleanup
```
Pergyra에서는 이를 다음과 묶을 수 있습니다.
```text
Intent lifecycle
Zone lifecycle
Slot lifecycle
Task tree
Cancellation tree
ExecutionLane
```
이건 단순한 async 문법 편의가 아니라 **구조적 실행 안전성**이 될 수 있습니다.

## 3.4 Fail-Closed Safety — 비정상 상태에서 계속 실행하지 않기
시장에는 다음 문제가 매우 많습니다.
```text
필수 상태가 없는데 기본값 사용
권한 검사가 실패했는데 계속 실행
invalid lifecycle을 warning으로만 처리
부분 commit 후 실패
예외를 삼키고 성공으로 보고
```
OWASP Top 10:2025에는 `Mishandling of Exceptional Conditions`가 새 카테고리로 들어갔습니다. 이 범주는 잘못된 오류 처리, 논리 오류, fail-open 동작, 필수 파라미터 처리 실패, 권한 부족의 부적절한 처리 등을 포함합니다. [OWASP A10:2025]
Pergyra의 최근 channel lifecycle 문제도 정확히 이 범주였습니다.
```text
invalid channel operation
  → warning + false
  → caller가 무시
  → 무한 재시도와 로그 폭주
```
Pergyra가 다음 규칙을 언어/runtime 전체에 적용하면 강한 시장 가치가 생깁니다.
```text
expected failure:
  Result / Option / status
contract violation:
  deterministic trap or rejection
missing semantic evidence:
  compiler invariant failure
절대:
  warning 후 임의 계속 실행
```

## 3.5 Intent / Business-Flow Safety — Pergyra의 가장 독창적인 영역
많은 보안 사고는 개별 API 호출이 불법이어서가 아닙니다.
```text
각 호출은 합법
하지만 순서가 불법
각 권한은 존재
하지만 현재 업무 상태에서는 허용되지 않음
함수는 정상 종료
하지만 일부 상태만 commit됨
```
예:
```text
결제 승인 전에 배송
취소 후 재청구
이미 환불된 거래의 재환불
Zone 종료 뒤 state mutation
승인자와 실행자가 분리되어야 하는 작업을 한 주체가 모두 수행
```
OWASP도 접근 통제에서 애플리케이션 고유의 business restriction을 domain model에서 강제하라고 명시하고, API Security Top 10은 민감한 business flow의 무제한 자동 사용을 별도 위험으로 다룹니다. [OWASP A01:2025]
현재 일반 언어들은 이것을 거의 전적으로 프레임워크와 애플리케이션 코드에 맡깁니다.
Pergyra가 잘 닫히면:
```text
Intent
  precondition
  authority
  participant
  effect
  step order
  compensation
  rollback
  postcondition
```
을 executable contract로 만들 수 있습니다.
이것이 **Intent Safety**입니다.

# 4. AI agent 시대에는 이 문제가 더 커진다
AI agent는 단순히 텍스트를 생성하지 않고 다음을 수행합니다.
```text
파일 읽기·삭제
메일 발송
결제
클라우드 변경
코드 실행
데이터베이스 수정
외부 API 호출
```
OWASP의 `Excessive Agency`는 excessive functionality, permissions, autonomy 때문에 예기치 않거나 조작된 출력이 실제 피해 행동으로 이어지는 문제를 다룹니다. [OWASP LLM06:2025]
NIST도 2026년 AI Agent Standards Initiative를 시작하며 agent identity와 authorization을 핵심 보안 주제로 다루고 있습니다. [NIST AI Agent 2026]
이것은 Pergyra에 매우 중요한 기회입니다.
AI가 생성한 코드나 agent workflow에 대해 다음을 보장하는 언어는 강한 가치가 있습니다.
```text
agent가 무엇을 하려는지 Intent로 명시
사용할 수 있는 tool/effect 제한
authority 명시
resource budget 명시
destructive action에 approval 요구
실패 시 rollback/compensation
```
이건 특정 제품과 무관한 일반 플랫폼 문제입니다.

# 5. Pergyra가 노려야 할 "Rust급 한 문장"
Rust의 한 문장은 대략 이렇습니다.
```text
Safe Rust에서는 메모리 안전 위반을 허용하지 않는다.
```
Pergyra도 시장에 전달할 한 문장이 필요합니다.
권장 문장:
> **Pergyra에서는 증명되거나 명시적으로 guarded되지 않은 효과가 실행되지 않는다.**
더 제품적으로:
> **No effect without intent, authority, lifetime, and budget.**
한국어:
> **의도·권한·수명·예산이 닫히지 않은 효과는 실행되지 않는다.**
이 문장은 Pergyra의 주요 축을 하나로 묶습니다.
```text
Intent       무엇을 하려는가
Authority    누가 허용했는가
Lifetime     언제까지 유효한가
Budget       얼마나 실행할 수 있는가
Effect       실제로 무엇이 바뀌는가
```

# 6. 다만 처음부터 네 개를 모두 주력으로 잡으면 실패할 수 있다
Rust의 성공 이유 중 하나는 promise가 매우 선명했다는 점입니다.
```text
memory safety without GC
```
Pergyra가 처음부터 다음을 모두 약속하면 메시지가 흐려집니다.
```text
권한
동시성
자원 예산
도메인 상태
소거
ABI
AI 안전
호환성
```
## 가장 좋은 첫 시장 wedge
```text
Authority-safe concurrent workflows
```
즉:
```text
외부 효과는 권한이 있어야 한다.
child task는 owner scope를 벗어나지 않는다.
실행은 정량 budget을 가진다.
실패는 fail-closed 또는 compensation된다.
```
초기 적용 시장:
```text
AI agent / tool orchestration
서버 workflow
plugin runtime
금융·업무 상태 전이
게임 서버와 simulation logic
산업 자동화
```
이후 범위를 확대하는 것이 낫습니다.

# 7. 시장은 "UB 제거"가 아니라 사고 감소를 산다
시장 사용자는 AIR이나 DIR이라는 이름을 구매하지 않습니다.
구매하는 것은 다음입니다.
```text
권한 사고 감소
클라우드 비용 폭주 방지
task leak 감소
취소 신뢰성
부분 commit 방지
감사 가능성
AI agent blast radius 제한
```
따라서 Pergyra의 검증도 이 형태여야 합니다.
## 필요한 bug corpus
```text
Authority:
  wrong tenant
  missing ownership
  confused deputy
  excessive agent permission
Lifecycle:
  orphan task
  cancellation leak
  channel deadlock
  resource after scope
Budget:
  spawn storm
  allocation storm
  host-call amplification
  external API cost bomb
Intent:
  invalid step order
  partial commit
  missing compensation
  stale-zone mutation
```
각 사례에 대해 비교해야 합니다.
```text
C# / TypeScript / Rust:
  필요한 코드량
  runtime에서 발견되는 시점
  테스트 없을 때 놓치는지
Pergyra:
  compile-time rejection
  runtime guard
  diagnostic quality
  ceremony
  performance cost
```
이 데이터가 있어야 "시장에 필요한 안전성"이라는 주장이 성립합니다.

# 8. Pergyra에 가장 유망한 미해결 안전 카테고리
| 순위 | 카테고리 | 시장 수요 | Pergyra 적합성 |
| -: | --- | ---: | ---: |
| 1 | Authority / Access Safety | 매우 높음 | 매우 높음 |
| 2 | Resource / Cost Safety | 매우 높음 | 높음 |
| 3 | Task / Lifecycle Safety | 높음 | 매우 높음 |
| 4 | Fail-Closed Exceptional Safety | 높음 | 높음 |
| 5 | Intent / Business-Flow Safety | 높지만 설명이 어려움 | Pergyra의 최대 차별점 |
| 6 | Safe FFI / ABI / Projection | 시스템 시장에서 높음 | 높음 |
| 7 | Deterministic execution | 특정 시장에서 높음 | 중상 |
| 8 | Full memory safety | 여전히 매우 높음 | 현재 Pergyra의 직접 차별점은 아님 |

# 최종 판정
**시장에는 Rust의 메모리 안전 다음으로 원하는 문제가 분명히 있습니다.**
가장 강한 것은 다음 네 가지입니다.
```text
Authority Safety
Lifecycle Safety
Resource Safety
Intent Safety
```
이들은 지금도 각각 접근 제어 사고, task/cancellation 문제, 자원·비용 폭주, 잘못된 업무 흐름으로 나타나며, 기존 주류 언어는 이를 하나의 언어 계약으로 통합하지 못하고 있습니다.
Pergyra의 가장 좋은 정의는 "UB 없는 언어"가 아닙니다.
> **Pergyra는 허가되지 않고, 수명이 닫히지 않고, 예산이 없으며, 선언된 의도에 속하지 않는 효과를 실행하지 않는 언어다.**
이 보장이 실제 compiler/runtime gate로 닫힌다면, Pergyra는 Rust를 복제하지 않고도 **Rust가 개척한 것과 비슷한 크기의 새로운 안전성 카테고리**를 만들 가능성이 있습니다.

참고 출처: CISA Product Security Bad Practices (2025-01), OWASP Top 10:2025
A01 Broken Access Control / A10 Mishandling of Exceptional Conditions, NIST Zero
Trust Architecture, OWASP API Security Top 10 API4:2023 Unrestricted Resource
Consumption, USENIX Security 2025 (Wasm resource isolation), OpenJDK JEP 499
Structured Concurrency, OWASP GenAI LLM06:2025 Excessive Agency, NIST AI Agent
Standards Initiative (2026-02).
