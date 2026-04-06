# 키워드 직교성 정의 (2026-04-06)

## 원칙

> 모든 선언 키워드는 다른 키워드로 대체할 수 없는 고유한 존재론적 역할을 가져야 한다.

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

---

## 4. 비직교적이면 안 되는 이유

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
