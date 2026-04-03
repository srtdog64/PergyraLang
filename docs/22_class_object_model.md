# Pergyra Subject Model (`class` Today)

## Overview

이 문서는 Pergyra의 `subject` 장기 의미론과 현재 구현 범위를 함께 정리한다.
이미 닫힌 규칙과 아직 목표 상태인 규칙을 분리해서 읽어야 한다.

핵심 전제는 다음과 같다.

- `struct`는 최소 값 타입이다
- `subject`는 상태와 identity를 가진 주체 타입이다
- 현재 구현 surface는 `subject`와 `class`를 같은 declaration으로 받는다
- `object`는 별도 본체 타입이 아니라 `subject`가 수동 문맥으로 해석된 모습이다
- `ability`는 subject 위의 행위 계약이다
- 장기 모델에서 `role`은 subject에 ability를 바인딩한다
- `entity`는 코어 언어 존재론이 아니라 프레임워크/도메인 용어로 남긴다

현재 컴파일러는 bare `subject/class`, `self` 메서드, positional constructor, class copy 제한까지는 구현했고,
role/ability/party 중심 객체 모델은 아직 이행 중이다.

즉 장기적으로 Pergyra에서 `class`라는 surface keyword가 가리키는 것은
OOP 의미의 class라기보다 `subject`다.
`subject`는 상태와 identity를 가진 주체이며,
role/ability/party/relation/effect/zone/world 체계의 실제 기준점이다.

## 1. 왜 subject가 필요한가

Pergyra는 도메인 파편화를 줄이기 위해
서로 다른 자원을 같은 사고 체계로 다루려는 언어다.

이때 `struct`만으로는 다음을 충분히 표현하기 어렵다.

- identity가 중요한 객체
- 상태를 가진 행위 주체
- ability를 수행하는 객체 셀
- party slot에 들어가 협력하는 actor/object

따라서 장기 모델에서 필요한 것은 `class`라는 OOP 이름이 아니라,
객체적 행위와 자원 셀을 연결하는 `subject` 중심 타입이다.
현재 구현은 그 역할을 `subject`와 `class` 두 표면이 함께 맡고 있다.

## 2. subject, object, dto, entity

- `subject`는 상태와 identity를 가지고 능동적으로 행위를 수행하는 코어 타입이다
- `object`는 별도 최상위 타입이라기보다, `subject`가 transfer / DTO / view / serialization 문맥에서 수동적으로 다뤄질 때의 해석이다
- `dto`는 그 object 표현 중 외부 API / IPC / persistence 경계를 넘기기 위해 축약된 projection이다
- 즉 subject는 본질적으로 능동적이지만, 특정 문맥에서 object화되면 행동은 피동적으로 소비된다
- `entity`는 이 둘을 묶는 넓은 프레임워크 용어가 될 수는 있지만, Pergyra 코어 존재론에는 넣지 않는다

## 3. 최종 정의

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

### subject

- 상태와 identity를 가지는 객체 타입
- ability를 수행하는 주체
- method와 role의 receiver
- party role slot에 배치되는 대상

현재 surface syntax에서는 `subject`와 `class`가 모두 허용되지만, 예시는 호환성 때문에 `class`를 자주 사용한다.

```pergyra
class Player {
    _health: Slot<Int>;
    name: String;
}
```

## 3. subject와 ability의 관계

Pergyra에서 `ability`는 일반적인 OOP 인터페이스와 완전히 같지 않다.

- 인터페이스: 타입이 어떤 메서드를 가지는가
- ability: 객체 셀이 어떤 자원/권한 규율 위에서 어떤 행위를 수행할 수 있는가

따라서 subject와 ability의 관계는 다음처럼 본다.

- subject는 ability의 host다
- ability는 subject가 수행하는 행위 계약이다
- role은 그 subject가 해당 ability를 어떻게 수행하는지 구체화한다

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

## 4. subject와 self cell

Pergyra에서 subject method는 개념적으로 항상 `self object cell` 위에서 실행된다.

즉:

- method는 값을 복사해서 다루는 함수가 아니라
- 특정 객체 셀의 상태를 읽고 쓰는 행위다

이 관점이 중요한 이유는 다음과 같다.

- slot 철학과 자연스럽게 연결된다
- subject를 값 타입과 구분할 수 있다
- ability의 `require`를 객체 내부 자원 셀과 연결할 수 있다
- role/party/world가 object 협력 모델로 읽힌다

## 5. copy / identity / storage

`subject`를 정의할 때 identity와 저장 방식을 같은 층으로 묶으면 안 된다.

구분은 이렇다.

- `subject`는 무엇인가: identity-bearing host type
- stack / heap / box / slot 은 어디에 사는가: storage / ownership 문제

### 고정할 의미론

- `struct`는 value semantics가 기본이다
- `subject`는 object semantics가 기본이다
- `subject`는 plain structural copy의 기본 대상으로 보지 않는다
- `object`는 subject와 다른 본체가 아니라, subject를 수동적으로 다루는 문맥적 해석이다

### 현재 단계의 권장 해석

- 지역 `subject/class` 바인딩은 “현재 스코프에 놓인 subject cell”로 본다
- sharing/indirection/escape는 `Box<T>` 또는 별도 handle 계층으로 푼다
- 최적화 차원에서 stack 또는 heap으로 내려가는 것은 backend 결정이다

즉:

`subject != heap`

하지만 동시에:

`subject != plain copied struct`

이다.

## 6. subject와 Box / Slot의 관계

세 타입의 역할은 다르다.

### subject

- 객체 의미론
- 상태와 identity
- ability / role의 주체

### Box<T>

- 명시적 간접화 / escape / heap ownership
- 저장 위치와 수명 제어를 드러냄
- `Box<class>`가 현재 surface에서 subject를 장기 저장/간접 참조하는 기본 경로다

### Slot<T>

- 자원 셀
- 점유 / 접근 / 이동 / 해제 / 보호 규율
- 메모리 박스가 아니라 규율 셀
- 현재 단계에서는 `Slot<class>`를 허용하지 않는다
- 이유는 subject가 identity-bearing object이기 때문에, 이를 Slot에 값처럼 넣으면 object semantics와 resource-cell semantics가 섞이기 때문이다

정리하면:

- `subject`는 객체의 의미를 준다
- `Box<T>`는 저장/소유 방식을 준다
- `Slot<T>`는 자원 규율을 준다

이 셋은 서로 대체 관계가 아니라 서로 다른 축이다.

## 7. subject와 role / party / actor / world의 관계

### role

- 장기 모델에서 role은 subject에 ability를 붙인다
- 현재 구현에서는 role이 `struct`에 바인딩되면 경고만 낸다
- role은 “그 subject가 어떤 자격으로 행동하는가”를 정의한다

### party

- 장기 모델에서 party는 role slot에 subject를 꽂아 협력시키는 실행 단위다
- party는 struct 값 모음이 아니라 subject collaboration unit이다

### actor

- actor는 subject와 병렬인 존재론적 계층이 아니다
- actor는 simulation loop, mailbox, scheduler semantics가 붙은 subject의 실행 프로파일이다
- 즉 장기 모델에서는 `actor`보다 `subject`가 먼저다

### systemic / world

- systemic은 party를 묶는 시스템 단위다
- world는 systemic을 묶는 최상위 조율 단위다

즉 `subject`는 이 계층 전체의 leaf object가 아니라,
실제로는 role/party/world 체계를 떠받치는 실행 주체다.

## 8. 구현 방향

### 현재 구현

현재 구현은 대체로 다음에 가깝다.

- `subject/class` surface를 현재는 C/LLVM에서 struct처럼 lower
- method를 free function으로 lower
- identity/copy/object-cell 의미론은 아직 약함

즉 현재 declaration surface는 “syntax는 subject/class, 의미론 목표는 subject, 구현은 아직 struct-like”다.

### 목표 구현

다음 의미론으로 수렴해야 한다.

1. `subject`는 nominal object type
2. method는 self object cell 기준 실행
3. role은 subject에만 바인딩하는 것을 기본 모델로 삼음
4. party slot은 subject를 담음
5. plain subject copy는 점진적으로 제한하거나 명시화
6. actor는 subject의 실행 프로파일로 다룸

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
- `Box<class>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>`를 함수 파라미터/리턴 타입으로 명시적으로 전달 가능
- generic class codegen (단형화 전략: `Pair<Int>` → `Pair_Int` struct + methods)

### 아직 닫히지 않은 범위

- class inheritance
- `super`
- 복잡한 object hierarchy
- `class`와 `subject`의 장기 alias/deprecation 전략
- actor를 subject profile로 재정렬하는 surface
- subject/object view 전환을 표면 문법으로 드러낼지 여부
- `Slot<class>`를 실제 object-handle cell로 승격할지 여부

## 9. transitional rule

아키텍처적으로는 `subject`가 ability를 수행하는 객체 타입이 맞다.
다만 구현 이행기에는 bare `class`도 허용될 수 있다.

즉:

- 장기 모델: subject는 ability-hosting identity-bearing type
- 이행기 구현: `subject`와 `class`가 같은 subject surface로 동작하며, role/ability 없이도 bare declaration은 허용 가능
- actor는 독립 타입축이 아니라 subject profile로 정리하는 것이 목표

하지만 문서와 설계의 중심은 후자가 아니라 전자다.

## 10. one-line definition

Pergyra의 현재 declaration surface는 `subject`와 `class`이고,
장기 의미론 이름은 `subject`다.

**“ability를 수행하는 상태와 identity의 객체 타입이며, role/party/world 체계의 실제 실행 주체”**

이다.
