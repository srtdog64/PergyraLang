# Pergyra Layered Stdlib and Domain Kits

마지막 업데이트: 2026-04-12

이 문서는 앞으로의 확장 원칙을 고정한다.

- 코어 언어에는 새 도메인 키워드를 더 넣지 않는다
- 공통 개념은 표준 라이브러리(common stdlib)로 올린다
- 업종별 패턴은 domain kit로 분리한다
- 전체 언어 surface 계층은 `docs/99_language_module_taxonomy.md`를 따른다

핵심 원칙:

- 코어: `intent / world / zone / relation / effect / projection / authority / handoff / generic contract / ownership / parallel`
- foundation: primitive values / control flow / callable baseline / `Option` / `Result` / stable collections
- execution family: `parallel` 아래 `spawn / async / await / select / channel / cancel`
- common stdlib: `money / time / timer / versioning`
- domain kit: `ledger / obligation / device_adapter`

즉 다음 방향이다.

```pergyra
use pgy.std.money;
use pgy.std.datetime;
use pgy.std.timer;
use pgy.std.versioning;
use pgy.kit.ledger;
use pgy.kit.obligation;
use pgy.kit.device_adapter;
```

이지 다음 방향이 아니다.

```pergyra
transaction ...
ledger ...
interrupt ...
deadline ...
```

## 1. 계층

### 1.1 Builtins

런타임 또는 컴파일러 내장.

- `Int`, `String`, `Bool`
- `Slot`, `Channel`, `DeviceSlot`
- `Log`, `Read`, `Write`, `Release`
- `Now`, `Sleep`

### 1.2 Common Stdlib

여러 도메인에서 공통으로 쓰이는 라이브러리.

| 모듈 | 역할 | 현재 표면 |
|------|------|-----------|
| `datetime` | civil time + monotonic instant | `LocalDate`, `LocalTime`, `DateTime`, `Duration`, `Instant` |
| `money` | 금액 value object | `Money`, `MoneyAdd`, `MoneySub`, `MoneyGte` |
| `timer` | deadline/tick helper | `TimerAfter`, `TimerEvery`, `TimerExpired`, `TimerTick` |
| `versioning` | optimistic concurrency + idempotency | `VersionStamp`, `IdempotencyKey` |
| `http` | transport adapter | request/response surface |
| `storage` | persistence adapter | snapshot/write surface |
| `page` | UI/page adapter | route/action/message surface |

### 1.3 Domain Kits

도메인별 표준 패턴 묶음.

| 모듈 | 역할 | 의존 |
|------|------|------|
| `ledger` | append-only financial posting pattern | `money`, `versioning` |
| `obligation` | deadline/violation compliance pattern | `datetime` 또는 monotonic ms |
| `device_adapter` | register/sample/command adapter pattern | 없음 |

## 2. 모듈 설계 원칙

- common stdlib 모듈은 최대한 generic value object / helper 중심으로 유지한다
- domain kit는 코어 semantics를 바꾸지 않고 예제 가능한 패턴을 제공한다
- domain kit는 storage/runtime dependency를 직접 숨기지 않는다
- hidden control flow를 만들지 않는다
- 런타임 강제 의미론이 필요한 항목은 코어가 아니라 adapter contract로 푼다

## 3. 이번에 고정한 구체 방침

### 3.1 Money

- `Decimal` 코어 키워드는 지금 넣지 않는다
- v0 `Money`는 `minorUnits: Int + currency: String`으로 둔다
- precision/rounding 정책은 이후 `Decimal` 또는 numeric library 단계에서 올린다

### 3.2 Time

- civil time은 `LocalDate`, `LocalTime`, `DateTime`
- monotonic time은 `Instant`, `Duration`
- 법률/컴플라이언스/스케줄링은 우선 monotonic `ms` 기반 패턴으로 닫는다

### 3.3 Timer

- `Timer`는 언어 키워드가 아니라 common stdlib helper다
- interrupt/tick scheduling runtime은 별도 adapter로 연결한다

### 3.4 Version / Idempotency

- 금융/스토리지/외부 API에서 공통으로 쓰이므로 common stdlib에 둔다
- isolation level 같은 저장소 정책은 코어가 아니라 store adapter가 책임진다

### 3.5 Ledger / Obligation / Device Adapter

- `ledger`는 double-entry posting 패턴의 기준 예제다
- `obligation`은 규정 준수에서 `deadline -> violation` 흐름의 기준 예제다
- `device_adapter`는 register/sample/command 표면의 기준 예제다

## 4. 예제 source of truth

이번 계층화와 함께 아래 예제를 추가한다.

- `examples/finance_ledger_probe/main.pgy`
- `examples/compliance_obligation_probe/main.pgy`
- `examples/iot_device_adapter_probe/main.pgy`

이 예제들은 새 키워드가 아니라 라이브러리 계층으로 도메인을 올리는 기준 샘플이다.

## 5. 다음 단계

- `money`를 `Decimal` 기반으로 확장할지 결정
- `ledger`에 storage adapter 연결 예제 추가
- `obligation`에 intent history 연동 예제 추가
- `device_adapter`에 `event/channel/zone` bridge 예제 추가
