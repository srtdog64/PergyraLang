# Pergyra Class Object Model

## Overview

이 문서는 Pergyra의 `class`를 어떻게 정의할지 고정한다.

핵심 전제는 다음과 같다.

- `struct`는 최소 값 타입이다
- `class`는 ability를 수행하는 객체 타입이다
- `ability`는 객체 셀 위의 행위 계약이다
- `role`은 class에 ability를 바인딩한다

즉 Pergyra에서 `class`는 단순히 “필드가 있는 큰 struct”가 아니다.
`class`는 상태와 identity를 가진 객체이며,
role/ability/party/world 체계의 실제 수행 주체다.

## 1. 왜 class가 필요한가

Pergyra는 도메인 파편화를 줄이기 위해
서로 다른 자원을 같은 사고 체계로 다루려는 언어다.

이때 `struct`만으로는 다음을 충분히 표현하기 어렵다.

- identity가 중요한 객체
- 상태를 가진 행위 주체
- ability를 수행하는 객체 셀
- party slot에 들어가 협력하는 actor/object

따라서 `class`는 문법적 사치가 아니라,
객체적 행위와 자원 셀을 연결하는 중심 타입으로 필요하다.

## 2. 최종 정의

### struct

- 최소 값 타입
- 복사/비교 중심
- identity 없음
- 좌표, 설정값, 스냅샷, 작은 데이터 묶음에 적합

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}
```

### class

- 상태와 identity를 가지는 객체 타입
- ability를 수행하는 주체
- method와 role의 receiver
- party role slot에 배치되는 대상

```pergyra
class Player {
    _health: Slot<Int>;
    name: String;
}
```

## 3. class와 ability의 관계

Pergyra에서 `ability`는 일반적인 OOP 인터페이스와 완전히 같지 않다.

- 인터페이스: 타입이 어떤 메서드를 가지는가
- ability: 객체 셀이 어떤 자원/권한 규율 위에서 어떤 행위를 수행할 수 있는가

따라서 class와 ability의 관계는 다음처럼 본다.

- class는 ability의 host다
- ability는 class object가 수행하는 행위 계약이다
- role은 그 class가 해당 ability를 어떻게 수행하는지 구체화한다

```pergyra
ability Damageable {
    require _health: Slot<Int>;
    func TakeDamage(self, amount: Int) -> Void;
}

role PlayerDamageable for Player {
    impl ability Damageable {
        func TakeDamage(self, amount: Int) -> Void {
            self._health = self._health - amount;
        }
    }
}
```

## 4. class와 self cell

Pergyra에서 class method는 개념적으로 항상 `self object cell` 위에서 실행된다.

즉:

- method는 값을 복사해서 다루는 함수가 아니라
- 특정 객체 셀의 상태를 읽고 쓰는 행위다

이 관점이 중요한 이유는 다음과 같다.

- slot 철학과 자연스럽게 연결된다
- class를 값 타입과 구분할 수 있다
- ability의 `require`를 객체 내부 자원 셀과 연결할 수 있다
- role/party/world가 object 협력 모델로 읽힌다

## 5. copy / identity / storage

`class`를 정의할 때 identity와 저장 방식을 같은 층으로 묶으면 안 된다.

구분은 이렇다.

- `class`는 무엇인가: identity-bearing object type
- stack / heap / box / slot 은 어디에 사는가: storage / ownership 문제

### 고정할 의미론

- `struct`는 value semantics가 기본이다
- `class`는 object semantics가 기본이다
- `class`는 plain structural copy의 기본 대상으로 보지 않는다

### 현재 단계의 권장 해석

- 지역 `class` 바인딩은 “현재 스코프에 놓인 객체 셀”로 본다
- sharing/indirection/escape는 `Box<T>` 또는 별도 handle 계층으로 푼다
- 최적화 차원에서 stack 또는 heap으로 내려가는 것은 backend 결정이다

즉:

`class != heap`

하지만 동시에:

`class != plain copied struct`

이다.

## 6. class와 Box / Slot의 관계

세 타입의 역할은 다르다.

### class

- 객체 의미론
- 상태와 identity
- ability / role의 주체

### Box<T>

- 명시적 간접화 / escape / heap ownership
- 저장 위치와 수명 제어를 드러냄
- `Box<class>`가 class object를 장기 저장/간접 참조하는 기본 경로다

### Slot<T>

- 자원 셀
- 점유 / 접근 / 이동 / 해제 / 보호 규율
- 메모리 박스가 아니라 규율 셀
- 현재 단계에서는 `Slot<class>`를 허용하지 않는다
- 이유는 class가 identity-bearing object이기 때문에, 이를 Slot에 값처럼 넣으면 object semantics와 resource-cell semantics가 섞이기 때문이다

정리하면:

- `class`는 객체의 의미를 준다
- `Box<T>`는 저장/소유 방식을 준다
- `Slot<T>`는 자원 규율을 준다

이 셋은 서로 대체 관계가 아니라 서로 다른 축이다.

## 7. class와 role / party / world의 관계

### role

- role은 class에 ability를 붙인다
- role은 “그 class가 어떤 자격으로 행동하는가”를 정의한다

### party

- party는 role slot에 class object를 꽂아 협력시키는 실행 단위다
- party는 struct 값 모음이 아니라 object collaboration unit이다

### systemic / world

- systemic은 party를 묶는 시스템 단위다
- world는 systemic을 묶는 최상위 조율 단위다

즉 `class`는 이 계층 전체의 leaf object가 아니라,
실제로는 role/party/world 체계를 떠받치는 실행 주체다.

## 8. 구현 방향

### 현재 구현

현재 구현은 대체로 다음에 가깝다.

- `class`를 C/LLVM에서 struct처럼 lower
- method를 free function으로 lower
- identity/copy/object-cell 의미론은 아직 약함

즉 현재 `class`는 “구현상 struct-like”다.

### 목표 구현

다음 의미론으로 수렴해야 한다.

1. `class`는 nominal object type
2. method는 self object cell 기준 실행
3. role은 class에만 바인딩하는 것을 기본 모델로 삼음
4. party slot은 class object를 담음
5. plain class copy는 점진적으로 제한하거나 명시화

현재 구현에서도 이 방향으로 일부 정렬했다.
- plain class copy와 `=` 재대입은 시맨틱에서 거부한다
- class method는 C backend에서 `self*` 기반 객체 셀 호출로 lower된다
- bare field name은 class method 안에서 `self` field로 해석된다
- 클래스 생성자는 현재 "필드 순서 기반 positional initialization"으로 동작한다
- role이 `struct` 값 타입에 바인딩되면 경고한다

### 현재 닫힌 범위

- 필드 선언
- 메서드와 `self`
- bare field access의 `self` 해석
- direct copy 금지
- `=` 재대입 금지
- `Vec2(3, 7)` 형태의 positional constructor
- `Box<class>`용 C helper 생성

### 아직 닫히지 않은 범위

- generic class codegen
- class inheritance
- `super`
- 복잡한 object hierarchy
- `Slot<class>`를 실제 object-handle cell로 승격할지 여부

## 9. transitional rule

아키텍처적으로는 `class`가 ability를 수행하는 객체 타입이 맞다.
다만 구현 이행기에는 bare class도 허용될 수 있다.

즉:

- 장기 모델: class는 ability-hosting object
- 이행기 구현: role/ability 없이도 class 선언은 허용 가능

하지만 문서와 설계의 중심은 후자가 아니라 전자다.

## 10. one-line definition

Pergyra의 class는

**“ability를 수행하는 상태와 identity의 객체 타입이며, role/party/world 체계의 실제 실행 주체”**

이다.
