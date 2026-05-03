# Pergyra Keyword Orthogonality

Last updated: 2026-05-04

이 문서는 Pergyra의 핵심 키워드가 서로 어떤 질문을 담당하는지 고정한다.
목표는 키워드 수를 줄이는 것이 아니라, 같은 의미를 두 키워드가 동시에
소유하지 않게 만드는 것이다.

## 0. 네 개의 상위 축

| Axis | Question | Surface |
| --- | --- | --- |
| Resource | 어떤 자원/핸들을 누가 어떤 경계에서 보유하는가 | `slot`, `own`, `ref`, `pin`, `unsafe`, `extern` |
| Execution | 작업은 언제, 어디서, 어떤 동시성 관계로 실행되는가 | `parallel`, `spawn`, `async`, `await`, `select`, `channel` |
| Domain | 현실 도메인의 주체, 경계, 권한, 관계는 무엇인가 | `subject`, `intent`, `zone`, `world`, `authority`, `relation`, `effect`, `projection` |
| Type/Contract | 어떤 형태와 능력 계약을 만족해야 하는가 | `class`, `struct`, `ability`, `role`, generic `where` |

이 네 축은 대체재가 아니다. 통합은 verifier graph에서 하고, ownership은 각
자기 축 owner가 끝까지 소유한다.

## 1. Core Definitions

| Keyword | Orthogonal meaning |
| --- | --- |
| `subject` | 도메인 안에서 정체성과 상태 전이를 갖는 행동 주체 |
| `class` | 일반 기능/형태 제공자. 도메인 주체임을 주장하지 않는다 |
| `struct` | 순수 데이터 shape. 행위와 도메인 정체성을 갖지 않는다 |
| `vessel` | `subject` 내부 상태 컨테이너. 독립 도메인 주체가 아니다 |
| `object` | 내부 읽기/조회 projection surface |
| `tobject` | 경계 밖 전달/게시 projection surface |
| `relation` | 두 존재 사이의 지속 관계 |
| `effect` | 행위나 조건이 만든 지속 영향 상태 |
| `zone` | 행위가 허용되는 실행/권한 경계 |
| `world` | zone들을 포함하는 최외곽 실행/스케줄/실패 경계 |
| `ability` | 무엇을 할 수 있어야 하는지에 대한 타입/계약 축 |
| `role` | ability를 구체 subject/class에 배치하는 구현 축 |
| `action` | 검증 가능한 행위. `requires/within/authorized by/causes`를 가질 수 있다 |
| `intent` | action들을 왜, 어떤 순서와 보상 경로로 실행하는지 묶는 orchestration spine |

## 2. Intent Is Not A Universal Owner

`intent`는 코드의 척추지만 모든 권한의 owner는 아니다. intent는 다른 축의
계약을 조합하고 provenance를 남긴다.

| Clause | Final owner |
| --- | --- |
| `who` | participant / subject binding |
| `where` / `within` | zone/world boundary |
| `requires` | ability/capability contract |
| `authorized by` | authority boundary |
| `causes` | effect lifecycle |
| `success` / `failure` / `rollback` / `compensate` | intent orchestration path |

이 규칙을 깨면 intent가 범용 workflow VM이 되고, `zone`, `authority`,
`effect`의 직교성이 무너진다.

## 3. Current Pain Point: Inference, Not Orthogonality

현재 intent 표면의 장황함은 직교성 실패가 아니다. 문제는 컴파일러가 이미
알 수 있는 `who`, `where`, `requires`, `authorized by`, `using`을 사용자에게
반복 입력하게 만든다는 점이다.

Beta+ 방향:

- `on: hero.Guard()`는 receiver `hero`에서 `who`를 추론할 수 있다.
- action header의 `within BattleZone`은 step `where`를 추론할 수 있다.
- intent parameter/value 중 `BattleZone` 인스턴스가 하나면 `using`을 추론할 수 있다.
- action header의 `authorized by self`는 receiver alias로 추론할 수 있다.
- 추론 실패 또는 충돌 시 verbose clause가 우선이며, 진단은 어느 축 owner가
  부족했는지 표시한다.

## 4. Important Distinctions

### ability/role vs authority

`ability`와 `role`은 타입/계약 축이다. `authority`는 특정 zone/resource 경계에서
그 능력을 행사할 권한이 있는지 검증하는 domain/resource 축이다.

### zone vs world

`zone`은 실행/권한의 지역 경계다. `world`는 여러 zone을 포함하고 handoff,
scheduler, failure propagation을 소유하는 최외곽 경계다.

### Slot / Pin vs Static Lifetime

`slot`은 Rust식 정적 lifetime이 아니다. runtime-validated handle이고,
정적 안전성은 CFG/AIR boundary verifier와 `pin` cleanup facts가 보강한다.

## 5. Orthogonality Audit Procedure

새 키워드나 새 clause를 추가하려면 다음 질문을 통과해야 한다.

1. 이 키워드는 Resource / Execution / Domain / Type-Contract 중 어느 축인가?
2. 같은 질문을 이미 다른 키워드가 소유하고 있지 않은가?
3. 최종 owner fact가 DIR/RIR/MIR/AIR 중 어디에서 확정되는가?
4. owner fact가 버려지면 안 된다. backend가 AST를 다시 걸어 의미를 재발견하면
   orthogonality violation으로 본다.
5. AIR는 통합 검증 레이어이지 domain 키워드의 새 owner가 아니다.
   AIR is not the owner of domain semantics; it verifies boundary evidence.
