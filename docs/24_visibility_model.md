# Pergyra 가시성 모델 (설계 결정)

## 3단계 가시성

| 키워드 | 의미 | 적용 대상 |
|--------|------|-----------|
| `public` | 누구나 보고 사용/이행 가능 | 함수, 타입, ability, 필드 |
| `private` | 선언 모듈 내에서만 보임 | 함수, 타입, ability, 필드 |
| `innate` | 외부에서 보이지만 이행(impl) 불가 | **ability 전용** |

## innate -- 선천적 능력

`innate ability`는 "보이지만 외부에서 이행할 수 없는 자격"이다.

### 의미

- 외부 모듈에서 `innate ability`를 **참조**할 수 있음 (타입, dyn role slot 등)
- 외부 모듈에서 `innate ability`를 **impl**할 수 없음
- 같은 모듈 내 role만 해당 ability를 이행 가능

### 비유

유전형질: 같은 계통(모듈) 안에서만 발현되는 자격.
외부에서 관찰(참조)할 수 있지만, 외부에서 습득(impl)할 수 없다.

### 문법

```pergyra
// combat.pgy
innate ability Combatable {
    func Attack(self) -> Int;
}

role Warrior for Fighter impl Combatable {  // OK -- 같은 모듈
    func Attack(self) -> Int { return 10; }
}
```

```pergyra
// other.pgy
import "combat.pgy";

// 참조 가능
party Team {
    dyn role slot fighter: Combatable;  // OK -- 타입으로 참조
}

// 이행 불가
role Hacker for Player impl Combatable {  // 컴파일 에러
    // innate ability 'Combatable' cannot be implemented outside its declaring module
}
```

### ability visibility 규칙

```
ability          -> 보임 + impl 가능  (기본값)
innate ability   -> 보임 + impl 불가  (같은 모듈만)
private ability  -> 안 보임           (같은 모듈만)
```

`innate`는 `ability`와 `private ability`의 중간이 아니라, **직교하는 축**이다:
- `private`는 **가시성**(visibility)
- `innate`는 **이행 제한**(implementation restriction)

`public ability`는 문법상 쓸 수 있어도 권장 표면이 아니다.
능력 계약은 기본 공개이므로 예제와 문서에서는 그냥 `ability Foo { ... }`를 기본 표기로 쓴다.

## ability != interface

Pergyra의 `ability`는 OO 언어의 `interface`를 그대로 바꿔쓴 이름이 아니다.

핵심 차이:

- `interface`는 보통 "이 타입이 제공해야 하는 메서드 집합"을 직접 모델링한다
- `ability`는 "이 슬롯/참여자/권한 경로가 어떤 협업 프로토콜에 참여할 자격이 있는가"를 모델링한다
- Pergyra에서 구현 담당은 `role ... impl Ability`이고, 계약 소비 지점은 `requires Ability`, `dyn role slot: Ability`, `authority subjectSlot requires Ability` 쪽이다

즉, `ability`는 단순한 객체 표면형(type shape)보다 **협업 계약의 자격 조건**에 가깝다.

### interface처럼 읽으면 틀리는 지점

`interface`식 사고:

- 어떤 nominal type이 메서드 몇 개를 가지면 된다
- call site는 "이 객체가 이 메서드를 호출할 수 있는가"에 집중한다

Pergyra `ability`식 사고:

- 어떤 subject/role/slot이 특정 action/intent/authority 계약에 참여해도 되는가
- call site보다도 `requires`, `authorized by`, `party/role slot`, `zone authority` 같은 계약 소비 지점이 더 중요하다
- 같은 메서드 시그니처를 가졌더라도, 적절한 `role impl ability`가 없으면 그 참여자는 해당 계약을 만족하지 않는다

### ability는 슬롯과 계약 경로에 붙는다

다음 surface가 Pergyra의 canonical reading이다.

```pergyra
ability Payable {
    func Pay(self) -> Void;
}

subject Buyer {
    action Checkout(self) -> Void
        requires Payable
    {
        return;
    }
}

party Counterparty {
    dyn role slot payer: Payable;
}

zone PaymentZone {
    subject slot buyer: Buyer
    authority buyer requires Payable;
}
```

여기서 중요한 점:

- `Payable`은 `Buyer`라는 타입의 객체 표면을 직접 설명하는 것이 아니다
- `Checkout` action contract가 어떤 자격을 요구하는지 표현한다
- `party`의 `dyn role slot`이 어떤 협업 프로토콜을 받을 수 있는지 표현한다
- `zone authority`가 어떤 subject 자격을 통과시켜야 하는지 표현한다

즉, `ability`는 "이 타입은 이런 메서드를 가진다"보다
"이 참여자는 이 계약에 이런 자격으로 들어온다"를 표현한다.

### 구현은 role이 맡고, ability는 자격을 잠근다

```pergyra
ability Combatable {
    func Attack(self) -> Int;
}

subject Fighter {
    action Strike(self) -> Int
        requires Combatable
    {
        return 1;
    }
}

role Warrior for Fighter impl Combatable {
    func Attack(self) -> Int { return 10; }
}
```

이 구조에서:

- `Combatable`은 "Fighter interface"가 아니다
- `Warrior`가 `Fighter`를 대신해 그 ability contract를 이행한다
- `Fighter`는 action contract에서 그 ability를 요구할 수 있다
- intent/party/zone은 이 ability를 보고 참여 가능성, 권한, 계약 적합성을 판단한다

그래서 Pergyra의 `ability`는

- nominal type 자체의 shape를 설명하는 도구가 아니라
- subject/role/party/zone/world orchestration에 걸쳐 재사용되는 자격 계약이다

### 실무적 규칙

문서를 읽거나 예제를 쓸 때는 아래 규칙으로 해석한다.

- `ability`를 보면 먼저 "누가 이 ability를 요구하는가"를 본다
- 그 다음 "어떤 role impl이 이 ability를 만족시키는가"를 본다
- 마지막에야 메서드 시그니처를 본다

즉, 독해 순서는

`consumer contract -> participant slot/authority -> role impl`

이며, 단순 OO식

`type -> interface method`

순서가 아니다.

## 왜 sealed가 아닌가

- `sealed`는 Java/Kotlin의 "봉인된 클래스" 맥락에서 상속 제한을 의미
- Pergyra에는 상속이 없으므로 "봉인"이라는 은유가 맞지 않음
- `innate`는 "선천적"이라는 뜻으로, ability가 "자격 조건"이라는 Pergyra 철학에 부합
- "이 자격은 타고난 것이지 학습(impl)할 수 있는 것이 아니다"

## 결정 이력

- 2026-04-04: `innate` 키워드 채택, `sealed` 비채택
- 2026-04-09: lexer/parser/semantic 1차 구현 완료

## 구현 상태

- lexer: `innate` 예약 키워드 구현
- parser: `innate ability Foo { ... }` 파싱 구현
- semantic:
  - 같은 모듈 내 `impl innate ability` 허용
  - 다른 모듈에서의 `impl innate ability` 거부
  - 기준은 declaration node의 `origin_path` 비교
- 남은 일:
  - richer diagnostic
  - visibility/formatter/LSP 표면 정렬
