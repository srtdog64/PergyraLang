# 키워드 직교성 정의 (2026-04-06)

## 원칙

> 모든 선언 키워드는 다른 키워드로 대체할 수 없는 고유한 존재론적 역할을 가져야 한다.

직교성은 키워드 수를 줄인다는 뜻이 아니다. Pergyra의 목표는 현실 도메인을
하나의 넓은 `class`나 하나의 넓은 workflow primitive로 뭉개지 않고, 서로
다른 의미 축으로 나누는 것이다. 따라서 표면은 넓을 수 있지만, 같은 의미를
두 키워드가 동시에 소유하면 안 된다.

핵심 판정식:

```
같은 상황을 두 키워드로 똑같이 표현할 수 있으면 비직교다.
두 키워드가 함께 쓰이지만 서로 다른 질문에 답하면 직교다.
```

---

## 0. 네 개의 상위 축

Pergyra의 키워드는 먼저 아래 네 축으로 나뉜다.

| 축 | 질문 | 대표 표면 |
|----|------|-----------|
| Resource | 어떤 자원/핸들을 누가 어떻게 보유하는가 | `slot`, `own`, `ref`, `pin`, `unsafe`, `extern` |
| Execution | 작업이 언제, 어디서, 어떤 동시성 관계로 실행되는가 | `parallel`, `spawn`, `async`, `await`, `select`, `channel` |
| Domain | 현실 도메인의 의미와 경계가 무엇인가 | `subject`, `intent`, `zone`, `world`, `authority`, `relation`, `effect`, `projection` |
| Type/Contract | 어떤 형태와 능력 계약을 만족하는가 | `class`, `struct`, `ability`, `role`, `where`, generic |

이 축들은 서로 대체재가 아니다.

- `intent`는 Domain 축의 orchestration spine이지 Execution 축의 `async` 대체재가 아니다.
- `slot`은 Resource 축의 runtime-validated handle이지 Rust식 static borrow checker가 아니다.
- `authority`는 Domain/Resource 경계의 승인 주체이지 `effect`나 `relation`이 아니다.
- `ability`는 Type/Contract 축의 capability contract이지 runtime authority 그 자체가 아니다.

따라서 Pergyra 코드는 여러 축이 한 화면에 보일 수 있다. 이것은 혼재가 아니라
stack이 visible한 상태다. 다만 각 축이 답하는 질문은 반드시 달라야 한다.

---

## 1. 선언 키워드 공식 정의

### 주체와 도구

| 키워드 | 공식 정의 |
|--------|----------|
| **subject** | 세계 안에서 상태와 정체성을 가진 능동 주체. 스스로 상태를 변화시키는 본체. |
| **class** | 기능/행위를 제공하는 도구형 값. 세계의 주체는 아니지만 행위를 호스팅한다. |

subject는 "참조 타입"이라서 subject인 게 아니다. **세계 안에서 정체성을 가지고 스스로 행동하기 때문에** subject다. class는 "값 타입"이라서 class인 게 아니다. **행위를 제공하지만 세계의 주인공은 아니기 때문에** class다.

### 데이터

| 키워드 | 공식 정의 |
|--------|----------|
| **struct** | 순수 데이터. 행위 없음. func를 가질 수 없다. |
| **vessel** | subject의 내부 상태. 구조는 struct와 같지만, "이것은 이 subject의 내면"이라는 소속 관계가 있다. subject 안에서만 사용 가능. |

struct에 func를 넣으려 하면 class로 승격해야 한다.
vessel은 subject 없이 독립적으로 존재할 수 없다.

### 투영

| 키워드 | 공식 정의 |
|--------|----------|
| **object** | 시스템 내부에서 관찰과 조회를 위해 유지되는 projection surface. 내부 읽기 모델. |
| **tobject** | 시스템 경계를 넘어 전달·게시·직렬화를 위해 유지되는 projection surface. 외부 전달 모델. |

```
object  = 내부 읽기 모델 (관찰/조회)
tobject = 외부 전달 모델 (전달/게시/직렬화)
```

#### 선택 규칙 (단호)

```
경계를 넘지 않으면 → object
경계를 넘으면     → tobject
예외 없음.
```

#### 컴파일러 강제 규칙

| 위반 | 결과 |
|------|------|
| `publish`/`transfer`/`export` 위치에 `object` 사용 | 컴파일 에러: "경계를 넘으려면 tobject를 사용하라" |
| `refresh`/`bind` (내부 sync) 위치에 `tobject` 사용 | 컴파일 경고: "내부 projection에는 object를 사용하라" |
| tobject를 로컬에서만 읽고 전달하지 않음 | 컴파일 경고: "전달되지 않는 tobject는 object로 바꿔라" |

```pergyra
zone ShopZone
{
    subject slot buyer: Member;
    object slot summary: MemberView;         // 내부 관찰 — OK
    tobject slot receipt: OrderReceipt;       // 외부 전달 — OK

    refresh summary from buyer;              // object + refresh — OK
    publish receipt from buyer;              // tobject + publish — OK

    // publish summary from buyer;           // object + publish → 컴파일 에러
    // refresh receipt from buyer;           // tobject + refresh → 컴파일 경고
}
```

이 규칙이 없으면 둘은 이름만 다른 projection이 된다.
이 규칙이 있으면 **object는 내부, tobject는 외부**가 컴파일러 수준에서 강제된다.

### 도메인 프리미티브

| 키워드 | 공식 정의 |
|--------|----------|
| **relation** | 두 존재 사이의 지속적 연결 상태. between 절로 양쪽 엔드포인트를 명시한다. |
| **effect** | 어떤 행위/조건이 남긴 지속적 영향 상태. 대상에게 적용되고, 시간이 지나면 사라질 수 있다. |

relation은 **연결 자체**다 — 결혼, 동맹, 소유.
effect는 **영향 자체**다 — 독, 축복, 스턴, 오라.

```
교체 테스트:
  "A와 B가 결혼했다"     → relation (연결). effect로 대체 불가.
  "A가 독에 걸렸다"      → effect (영향). relation으로 대체 불가.
  "A가 축복을 받았다"    → effect (영향). 축복은 연결이 아니라 상태.
  "A와 B가 사제 관계다"  → relation (연결). 이건 영향이 아니라 구조.
```

### 경계

| 키워드 | 공식 정의 |
|--------|----------|
| **zone** | 행위가 허용되는 구역. 자격/승인/효과가 검증되는 실행 경계. |
| **world** | 실행/신뢰/실패의 최외곽 경계. zone들을 포함한다. |
| **party** | 함께 묶인 subject들의 군집 단위. 집계 경계. |
| **roster** | party들의 컨테이너. 편성 제한과 관리. |

### 계약

| 키워드 | 공식 정의 |
|--------|----------|
| **ability** | 행위 수행 자격의 계약 선언. "무엇을 할 수 있는가." |
| **role** | ability 계약의 구체적 이행. "이 subject는 어떻게 하는가." ability 없이 role은 의미 없다. |

ability와 role은 직교가 아니라 **계층**이다. 계약(ability) → 이행(role).

### 행위

| 키워드 | 공식 정의 |
|--------|----------|
| **action** | 맥락이 검증된 행위. requires/within/authorized by/causes 계약을 가질 수 있다. |
| **intent** | 사용자의 의도. subject를 움직이는 이유. action들의 상위 오케스트레이션. |

action은 **무엇을 하는가**. intent는 **왜 하는가**.

intent는 모든 권한의 owner가 아니다. intent는 `who`, `where`, `requires`,
`authorized by`, `causes`, `success/failure/rollback`을 한 실행 척추로 묶지만,
각 clause의 최종 의미 owner는 별도로 남는다.

| clause | 최종 owner |
|--------|------------|
| `who` | participant / subject binding |
| `where` / `within` | zone/world boundary |
| `requires` | ability/capability contract |
| `authorized by` | authority boundary |
| `causes` | effect lifecycle |
| `success` / `failure` / `rollback` / `compensate` | intent orchestration path |

이 규칙이 깨지면 intent가 범용 workflow VM이 되고, `zone`, `authority`,
`effect`의 직교성이 무너진다.

---

## 2. 직교성 매트릭스

```
           subject class struct vessel object tobject relation effect
subject      —      ✓     ✓      계층    ✓      ✓       ✓       ✓
class              —      △      ✓      ✓      ✓       ✓       ✓
struct                    —      △      ✓      ✓       ✓       ✓
vessel                          —      ✓      ✓       ✓       ✓
object                                 —      ⚠       ✓       ✓
tobject                                       —       ✓       ✓
relation                                             —       ✓
effect                                                       —

✓ = 직교 (축이 다름, 대체 불가)
△ = 약한 겹침 (구조 유사, 의미론적 구분으로 유지)
⚠ = 위험 (실전에서 혼동 가능, 모니터링 필요)
계층 = 직교가 아니라 포함 관계 (vessel은 subject의 내부)
```

---

## 3. 가장 조심해야 하는 경계

### 3.1 object vs tobject — ⚠ 모니터링 필요

현재는 분리. 실전에서 "이거 object야 tobject야?" 혼동이 반복되면 합칠 준비.
백업 플랜: object에 transfer 수식어 → `transfer object`.

### 3.2 struct vs class — △ 선 유지

struct에 func 넣으려 하면 → "class로 바꿔라" 컴파일 에러.
이 강제가 있으면 경계가 명확해진다.

### 3.3 vessel vs struct — △ 소속 관계로 구분

vessel은 subject 안에서만 사용 가능을 컴파일러가 강제하면 구조적 차이가 생긴다.

### 3.4 intent vs authority/effect/zone — ⚠ 과밀 모니터링 필요

intent는 코드의 척추라서 여러 clause를 모으는 것이 정상이다. 그러나 clause를
모은다는 것과 그 clause의 의미를 소유한다는 것은 다르다.

- intent는 `authorized by`를 기록하고 검증 경로에 넣지만 authority owner가 아니다.
- intent는 `causes`를 선언하지만 effect lifecycle owner가 아니다.
- intent는 `where`/`within`을 선언하지만 zone/world state owner가 아니다.
- intent는 rollback path를 만든다. 이 부분만 intent의 고유 실행 의미다.

따라서 intent compression이나 자동 추론을 추가하더라도, 추론 결과는 각 owner의
계약으로 다시 검증되어야 한다. intent가 모든 것을 대신 승인하면 비직교다.

### 3.5 slot/pin vs lifetime — ⚠ Rust식 오독 금지

`slot`과 `pin`은 lifetime annotation을 대체하는 문법이 아니다.

- `slot`은 포인터 주소를 직접 노출하지 않는 resource handle이다.
- `pin`은 block-scoped lease다.
- `own/ref`는 anchored boundary subset에서만 안정 표면이다.
- business object graph의 전체 lifetime을 컴파일 타임에 예측하지 않는다.

Pergyra의 정적 검사는 "객체가 언제 죽는가"가 아니라 "이 경계를 넘어도 되는가",
"이 handle이 escape해도 되는가", "이 authority가 유효한가"를 잡는다. 나머지는
generation/token/runtime state로 검증한다.

### 3.6 ability/role vs authority — ⚠ 계약과 승인의 혼동 금지

`ability`와 `role`은 Type/Contract 축의 자격 계약이다. `authority`는 Domain/Resource
경계에서 실제 mutation이나 handoff를 승인하는 주체다.

- `ability`는 "무엇을 할 수 있는가"를 말한다.
- `role`은 "이 subject가 그 ability를 어떻게 이행하는가"를 말한다.
- `authority`는 "이 경계에서 누가 승인하는가"를 말한다.

따라서 ability bound가 만족되어도 authority가 필요한 mutation은 자동 허용되지
않는다. 반대로 authority subject가 있어도 필요한 ability/capability 계약이 없으면
실행 자격은 없다.

### 3.7 zone vs world — 포함 관계지만 같은 경계가 아님

`world`는 실행/신뢰/실패의 최외곽 경계이고, `zone`은 그 안의 행위 허용 구역이다.
둘은 포함 관계를 갖지만 서로 대체하면 안 된다.

- `world`는 zone들을 포함하고, handoff/embedding/failure propagation의 큰 경계다.
- `zone`은 authority, projection freshness, state transition을 검증하는 실행 경계다.
- zone-local authority 실패를 world failure로 즉시 승격할지 여부는 runtime propagation 정책이 결정한다.

이 구분이 흐려지면 모든 상태 전이가 world-level global state처럼 보이고,
zone이 가진 국소 검증 장점이 사라진다.

---

## 4. 직교성 audit 절차

새 키워드나 새 clause를 추가하거나, 기존 키워드의 의미를 넓힐 때는 아래 질문을
모두 통과해야 한다.

1. 이 표면은 Resource / Execution / Domain / Type-Contract 중 어느 축에 속하는가?
2. 이미 같은 질문에 답하는 키워드가 있는가?
3. 이 표면의 최종 semantic owner는 어느 계층인가? (`HIR`, `DIR`, `RIR`, `MIR`, DAG/AIR)
4. backend가 이 의미를 AST 재탐색으로 재발견해야 하는가? 그렇다면 설계 실패다.
5. 이 표면이 `intent` 아래에 들어가더라도 실제 owner fact로 다시 materialize되는가?
6. compressed/inferred form이 explicit form과 같은 owner fact를 만드는가?
7. 실패 진단이 "어느 축이 실패했는가"를 말할 수 있는가?

합격 기준:

```
문법은 짧아질 수 있다.
owner fact는 흐려지면 안 된다.
```

실무적으로는 AIR가 여러 축의 evidence를 모으는 verifier graph가 될 수 있지만,
AIR가 각 축의 owner를 흡수하면 안 된다. AIR는 통합 검증 레이어이지
도메인/자원/실행/타입 의미의 단일 소유자가 아니다.

핵심 규칙:

> 통합은 verifier graph에서 하고, ownership은 각 의미 축 owner에 남긴다.

---

## 5. 현재 위험 등록표

| 경계 | 현재 판정 | 베타 전 관리 방식 |
|------|-----------|------------------|
| `intent` vs `authority/effect/zone` | 의도적으로 과밀하지만 직교 가능 | compressed intent는 각 owner fact로 expansion해야 함 |
| `object` vs `tobject` | 직교하나 사용자 혼동 위험 큼 | refresh/publish/boundary 진단을 강하게 유지 |
| `ability/role` vs `authority` | 계약/승인 혼동 위험 | ability 만족과 authority 승인을 별도 진단 |
| `zone` vs `world` | 포함 관계 때문에 drift 가능 | world propagation과 zone-local validation을 분리 |
| `slot/pin` vs lifetime | 외부 오독 위험 큼 | static borrow checker 마케팅 금지, layered model 유지 |
| `parallel` vs `async/spawn/await` | execution 계층 내부 혼동 가능 | `parallel`은 실행 관계, async family는 suspension/task surface로 고정 |

이 표의 항목은 "나쁜 설계"라는 뜻이 아니다. beta 이후에도 계속 회귀 테스트와
문서 검토가 필요한 고압 경계라는 뜻이다.

---

## 6. 비직교적이면 안 되는 이유

```
비직교 키워드가 있으면:
  1. 사용자가 "이거 뭘로 선언하지?" 매번 고민
  2. 같은 것을 2가지로 쓸 수 있어서 코드 스타일 분열
  3. 컴파일러 진단 메시지가 모호해짐
  4. IR/codegen에서 불필요한 분기 발생

직교하면:
  1. 키워드 = 역할. 읽으면 안다.
  2. 한 가지 방법만 있다.
  3. 에러 메시지가 정확하다.
  4. IR이 깔끔하다.
```
