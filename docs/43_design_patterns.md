# Pergyra 중요 설계 패턴 8가지

이 문서는 Pergyra 언어의 **반복 가능한 설계 골격**을 정의합니다.
각 패턴은 예제가 아니라 **판정 기준과 금지 규칙**을 포함합니다.

---

## 1. Subject-Orchestrates, Class-Executes

### 패턴

- `subject`는 책임과 조율을 맡는다
- `class`는 계산, 검사, 변환, 도구 실행을 맡는다

### 왜 중요하냐

이게 무너지면 `subject`가 비대해지고, 결국 기존 OOP의 거대한 God Object로 돌아갑니다.
반대로 이 패턴이 굳으면 `subject`는 의도 중심, `class`는 메커니즘 중심으로 안정됩니다.

### 판정 기준

- ✅ subject는 다른 subject/class를 호출하여 조율한다
- ✅ class는 독립적인 계산/변환 로직을 가진다
- ❌ subject에 복잡한 계산/변환 로직이 직접 포함된다

### 예시

```pergyra
subject Buyer {
    let cart: ShoppingCart;
    func Checkout() -> Bool {
        let validator = OrderValidator();
        if !validator.Validate(cart) { return false; }
        let processor = PaymentProcessor();
        return processor.Process(cart);
    }
}

class OrderValidator {
    func Validate(cart: ShoppingCart) -> Bool { ... }
}

class PaymentProcessor {
    func Process(cart: ShoppingCart) -> Bool { ... }
}
```

---

## 2. Subject-Owns-Vessel

### 패턴

- `subject` 내부의 가변 상태는 `vessel`로 묶는다
- 주체의 의미적 정체성과 실행 중 상태를 분리한다

### 왜 중요하냐

이 패턴이 없으면 상태가 전부 subject 필드로 퍼지고, "왜 이 값이 존재하는가"가 흐려집니다.

### 판정 기준

- ✅ vessel은 subject의 내부 상태만 포함한다
- ✅ subject 필드는 책임/참조, vessel 필드는 상태 변수
- ❌ vessel이 외부에 직접 노출된다

---

## 3. Subject → Object → TObject Projection Chain

### 패턴

- 내부 실체는 바로 외부로 나가지 않는다
- 먼저 `object`로 내부 해석용 투영
- 외부 경계를 넘길 때만 `tobject`로 승격

### 왜 중요하냐

이 패턴이 있어야 `object`와 `tobject`가 단순 이름 차이가 아니라 **경계 규칙**이 됩니다.

### 판정 기준

- ✅ object는 zone 내부에서만 유효
- ✅ tobject는 직렬화 가능, 경계 너머 전달
- ❌ object가 zone 경계를 직접 넘는다

---

## 4. Intent-as-Workflow, Func-as-Local-Logic

### 패턴

- 여러 단계, 보상, 가드, 권한, zone 이동이 있으면 `intent`
- 계산, 보조 판단, 내부 처리면 `func`

### 왜 중요하냐

이 기준이 없으면 모든 것이 intent가 되거나, 반대로 intent가 그냥 큰 함수가 됩니다.

### 판정 기준

- ✅ intent는 step, guard, compensate, priority를 가진다
- ✅ func는 단일 계산/변환 책임을 가진다
- ❌ intent에 복잡한 계산 로직이 직접 포함된다

---

## 5. Action-is-Public, Func-is-Private

### 패턴

- 외부 세계/zone/authority/effect와 연결되는 주체의 공적 동사는 `action`
- 내부 계산/보조 판단/국소 갱신은 `func`

### 왜 중요하냐

이 구분이 있어야 `subject`가 모든 메서드를 같은 급으로 갖지 않습니다.

### 판정 기준

- ✅ action은 requires/within/causes/authorized by와 연결된다
- ✅ func는 내부 보조 역할만 한다
- ❌ action에 내부 계산 로직이 직접 포함된다

---

## 6. Zone-as-Policy-Boundary

### 패턴

- `zone`은 단순 데이터 묶음이 아니라
- projection, authority, relation/effect wiring, lifecycle policy가 걸리는 실행 경계다

### 왜 중요하냐

`zone`이 그냥 컨테이너가 되면 의미가 약해집니다.

### 판정 기준

- ✅ zone은 authority, refresh/publish/bind, apply/detach/link/unlink/maintain을 가진다
- ✅ zone은 state alias, lifecycle shorthand를 지원한다
- ❌ zone이 단순 데이터 컨테이너로만 사용된다

---

## 7. World-Composes-Zones

### 패턴

- `world`는 큰 상태 저장소가 아니라
- 여러 zone과 world state를 조합하는 상위 운영 경계다

### 왜 중요하냐

이 패턴이 있어야 `world`가 "큰 zone"으로 붕괴하지 않습니다.

### 판정 기준

- ✅ world는 zone slot, zone lifecycle, derived state를 조합한다
- ✅ world는 all/any composition, sync helper를 가진다
- ❌ world가 단일 zone처럼 동작한다

---

## 8. Projection Sync Is Explicit

### 패턴

- projection은 암묵적으로 갱신되지 않는다
- `refresh`, `publish`, `bind` 같은 명시적 sync 흐름을 쓴다

### 왜 중요하냐

이 패턴이 없으면 `object/tobject`가 그냥 데이터 복사본이 됩니다.

### 판정 기준

- ✅ projection 갱신은 명시적 호출로 이루어진다
- ✅ refresh/publish/bind가 코드에서 명확히 보인다
- ❌ projection이 암묵적으로 갱신된다

---

## 우선순위

### 최상위 필수

1. Subject-Orchestrates, Class-Executes
2. Subject-Owns-Vessel
3. Subject → Object → TObject Projection Chain
4. Intent-as-Workflow, Func-as-Local-Logic

### 그다음 필수

5. Action-is-Public, Func-is-Private
6. Zone-as-Policy-Boundary
7. World-Composes-Zones
8. Projection Sync Is Explicit
