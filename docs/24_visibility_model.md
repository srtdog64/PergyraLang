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

### public vs private vs innate

```
public ability   -> 보임 + impl 가능  (기본값)
innate ability   -> 보임 + impl 불가  (같은 모듈만)
private ability  -> 안 보임           (같은 모듈만)
```

`innate`는 `public`과 `private`의 중간이 아니라, **직교하는 축**이다:
- `public`/`private`는 **가시성**(visibility)
- `innate`는 **이행 제한**(implementation restriction)

## 왜 sealed가 아닌가

- `sealed`는 Java/Kotlin의 "봉인된 클래스" 맥락에서 상속 제한을 의미
- Pergyra에는 상속이 없으므로 "봉인"이라는 은유가 맞지 않음
- `innate`는 "선천적"이라는 뜻으로, ability가 "자격 조건"이라는 Pergyra 철학에 부합
- "이 자격은 타고난 것이지 학습(impl)할 수 있는 것이 아니다"

## 결정 이력

- 2026-04-04: `innate` 키워드 채택, `sealed` 비채택
- 구현 상태: 미구현 (Tier 2 TODO)
