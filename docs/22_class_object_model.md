# Pergyra Subject Model (`class` Today)

## Overview

이 문서는 Pergyra의 `subject` 장기 의미론과 현재 구현 범위를 함께 정리한다.
이미 닫힌 규칙과 아직 목표 상태인 규칙을 분리해서 읽어야 한다.

핵심 전제는 다음과 같다.

- `struct`는 최소 값 타입이다
- `vessel`은 subject 안에서 상태/자원을 피동적으로 담는 수용체다 (2026-04-04 추가)
- `subject`는 의사결정과 오케스트레이션을 담당하는 능동 주체 타입이다
- `subject`는 `func`가 아닌 `action`으로 행동한다 — action은 zone/ability/effect와 연동되는 플롯 행위다(2026-04-04 추가)
- 현재 구현 surface는 `subject`와 `class`를 서로 다른 nominal declaration flavor로 기록하고, semantic/lowering도 둘을 점진적으로 다르게 다룬다
- 현재 구현에서 `subject`는 별도 선언 키워드이지만 semantic에서는 subject execution profile로 취급된다
- `object`는 intent를 시작하지 않는 피동 상태 대상이다. `object`는 상태와 `func`(메서드)를 가질 수 있다.
- `ability`는 subject 위의 행위 계약이다
- 장기 모델에서 `role`은 subject에 ability를 바인딩한다
- `entity`는 코어 언어 존재론이 아니라 프레임워크/도메인 용어로 남긴다
- 상세 설계: [26_vessel_action_model.md](26_vessel_action_model.md)

현재 컴파일러는 bare `subject/class`, `self` 메서드, positional constructor, subject-only projection/domain checks, participant subject-profile semantic, subject/class 저장·복사·dispatch 분기 1단계까지는 구현했고,
role/ability/party 중심 객체 모델은 아직 이행 중이다.

즉 장기적으로 Pergyra에서 중심 이름은 `class`보다 `subject`다.
`class`는 남겨두되 더 수동적이고 보조적인 nominal surface로 다루고,
`subject`는 상태와 identity를 가진 주체이며,
role/ability/party/relation/effect/zone/world 체계의 실제 기준점이다.

## 1. 왜 subject가 필요한가

Pergyra는 도메인 파편화를 줄이기 위해
서로 다른 자원을 같은 사고 체계로 다루려는 언어다.

이때 `struct`만으로는 다음을 충분히 표현하기 어렵다.

- identity가 중요한 객체
- 상태를 가진 행위 주체
- ability를 수행하는 객체 셀
- party slot에 들어가 협력하는 participant/object

따라서 장기 모델에서 필요한 것은 `class`라는 OOP 이름이 아니라,
객체적 행위와 자원 셀을 연결하는 `subject` 중심 타입이다.
현재 구현은 그 역할을 `subject` 중심으로 옮기기 시작했고, `class`는 별도 nominal surface로 남겨두고 있다.

## 2. subject, object, tobject, entity

- `subject`는 상태와 identity를 가지고 능동적으로 행위를 수행하는 코어 타입이다
- `object`는 intent를 시작하지 않는 피동 상태 대상이다
- `object`는 상태를 가질 수 있고 effect를 받을 수 있으며 relation의 대상이 될 수 있고 시간에 따라 반응할 수 있다
- 현재 compiler surface는 `effect Name for object target: T` / `relation Name for object a: A, object b: B`를 받아 object를 직접 layer contract 대상으로 삼을 수 있다
- 현재 compiler surface는 domain-local projection sync(`refresh` / `publish` / `bind`)에서 object를 projection source로도 허용한다. 다만 `tobject`는 sink이며 source로는 허용하지 않는다
- `tobject`는 그 object 표현 중 외부 API / IPC / persistence 경계를 넘기기 위한 별도 boundary projection contract다
- 현재 compiler surface는 `object` / `tobject`에 struct-style declaration syntax를 재사용하지만, 둘은 같은 nominal/value 계약이 아니다
- 현재 compiler surface는 `object`를 local/internal passive projection contract로, `tobject`를 더 좁은 boundary transfer/publish contract로 취급하며 helper `func`는 허용한다
- 현재 compiler surface는 `ToObject(TargetObject, subjectBinding)`으로 subject를 local passive object view로 투영할 수 있고 C/LLVM 모두에서 lower된다
- 현재 compiler surface는 `ToTObject(TargetDto, subjectBinding)` 최소 projection built-in을 지원하고 C/LLVM 모두에서 lower된다
- lowering은 이제 `object borrow-first / tobject materialize-first`로 갈라진다. `ToObject(...)` local binding이 non-escaping이면 backend는 source subject field를 직접 읽는 borrowed projection alias로 다루고, whole-value가 실제로 필요할 때만 object literal을 만든다. `ToTObject(...)`는 boundary contract라 여전히 materialized transfer value를 만든다.
- 다만 `ToObject` / `ToTObject`를 relation/effect/zone/world 바깥 일반 함수에서 직접 쓰면 semantic warning을 내고, 권장 경로는 domain-local slot + projection sync 흐름이다
- relation/effect는 현재 subject projection 문맥을 담는 overlay nominal host로도 동작하며, positional constructor와 runtime instance method call이 C/LLVM에 연결돼 있다
- 즉 subject는 본질적으로 능동적이지만, 특정 문맥에서 object화되면 intent 생성 능력을 잃고 피동 상태 대상으로 소비된다
- `entity`는 이 둘을 묶는 넓은 프레임워크 용어가 될 수는 있지만, Pergyra 코어 존재론에는 넣지 않는다
- projection의 중심은 `tobject` 본체가 아니라 `relation/effect/zone/world` 문맥과 그 안의 projection sync 흐름이다
- 즉 현재 구현에서 `object`는 단순 projection 결과물이 아니라 passive state host이면서 layer target/source가 될 수 있고, `tobject`는 여전히 더 좁은 외부 경계 projection 형식으로 남는다

## 3. 최종 정의

### struct

- 최소 값 타입
- 복사/비교 중심
- identity 없음
- 좌표, 설정값, 스냅샷, 작은 데이터 묶음에 적합
- `func` (메서드) 허용

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}
```

### vessel (2026-04-04 추가)

- subject 안에서 상태/자원/행위를 피동적으로 담는 수용체
- `func` (메서드) 허용 -- 수동 실행 (호출당해서 상태 변경)
- 스스로 의사결정하지 않음
- 5대 피동 축: 상태, 행위, 자원, 투영, 규칙

```pergyra
vessel HealthState {
    current: Int;
    max: Int;

    func ApplyDamage(self, amount: Int) -> Void {
        self.current = Max(self.current - amount, 0);
    }
}
```

### subject

- 의사결정, 오케스트레이션, 승인을 담당하는 능동 주체 타입
- ability를 수행하는 주체
- 일반 `func`와 공적 `action`의 host
- `func`는 계산/보조 판단/국소 상태 갱신을 담당하고, `action`은 zone/authority/effect와 연결되는 공적 오케스트레이션 동사다
- role의 receiver
- party role slot에 배치되는 대상
- `subject`는 `vessel`뿐 아니라 일반 `class` field도 값으로 소유할 수 있다. `let weapon: Item;`은 subject-owned tool/thing surface다.

현재 surface syntax에서는 `subject`와 `class`가 모두 허용되지만, 둘은 더 이상 같은 declaration으로 기록되지 않는다.
`subject slot`, `ToObject`, `ToTObject`처럼 주체성을 요구하는 표면은 현재 subject host (`subject`, `subject`)만 받는다.
또한 plain copy / plain value parameter / plain value return은 `subject`에 대해서는 금지되고, `class`에 대해서는 허용된다.

```pergyra
subject Player {
    _health: Slot<Int>;
    name: String;
    weapon: Item;
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

Pergyra에서 subject action/method는 개념적으로 항상 `self object cell` 위에서 실행된다.

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
- `class`는 현재 passive nominal value로서 plain copy / value parameter / value return이 가능하다
- `object`는 subject와 같은 능동 주체는 아니지만, 수동 상태를 담는 host가 될 수 있다
- `tobject`는 `object`보다 더 좁은 경계 전달/투영 형식이다
- `subject` 내부에서 `class`는 별도 키워드 없이 일반 field로 embed된다. `vessel health: HP;`는 내부 상태 수용체를 뜻하고, `let weapon: Item;`는 도구/사물을 뜻한다.

### 작성 순서 권장

새 host를 만들 때는 구현보다 먼저 존재론을 고르는 편이 맞다.

- `subject`: 능동 주체, identity, `action`, zone/world orchestration
- `class`: 피동 도구/사물, value semantics, hosted `func`
- `object`: 읽기 전용 또는 수동 상태 대상
- `tobject`: 경계 밖 전송 표면

즉 실전 authoring과 scaffold의 첫 질문은 보통 "이것이 `subject`인가 `class`인가 `object`인가"여야 한다.

### 현재 단계의 권장 해석

- 지역 `subject` 바인딩은 “현재 스코프에 놓인 identity-bearing self cell”로 본다
- 지역 `class` 바인딩은 “수동적 nominal value/object”로 본다
- sharing/indirection/escape는 `Box<T>` 또는 별도 handle 계층으로 푼다
- 최적화 차원에서 stack 또는 heap으로 내려가는 것은 backend 결정이다
- C/LLVM lowering 모두에서 `subject` method는 pointer-self, `class` method는 value-self로 내려간다

즉:

`subject != heap`

하지만 동시에:

`subject != plain copied struct`

이다.

### object

- intent를 시작하지 않는 피동 상태 대상
- 상태를 가질 수 있고 helper `func`로 시간에 따라 반응할 수 있다
- effect를 받을 수 있고 relation의 대상이 될 수 있다
- 현재 구현에서는 `struct` 호환 nominal declaration alias로 시작하지만, 장기 의미론은 단순 projection 결과물보다 넓다

```pergyra
object Door {
    isOpen: Bool;

    func Toggle(self) -> Void {
        self.isOpen = !self.isOpen;
    }
}
```

## 6. subject와 Box / Slot의 관계

세 타입의 역할은 다르다.

### subject

- 객체 의미론
- 상태와 identity
- ability / role의 주체

### Box<T>

- 명시적 간접화 / escape / heap ownership
- 저장 위치와 수명 제어를 드러냄
- `Box<class>`가 현재 surface에서 passive class/object를 장기 저장/간접 참조하는 기본 경로다

### Slot<T>

- 자원 셀
- 점유 / 접근 / 이동 / 해제 / 보호 규율
- 메모리 박스가 아니라 규율 셀
- 현재 단계에서는 plain/secure `Slot<subject>`와 `Slot<subject>`를 local object-cell anchor로 허용한다
- 현재 단계에서는 `func F(ref s: Slot<SubjectHost>)`, `func G(own s: SecureSlot<SubjectHost>)`처럼 subject-host slot을 함수 경계로 넘길 수 있다
- 이 경계 전달은 semantic, C backend, LLVM smoke까지 닫혔다
- secure token 모델은 local anchor와 함수 경계 전달까지는 붙었고, 남은 공백은 richer handle semantics다

정리하면:

- `subject`는 객체의 의미를 준다
- `Box<T>`는 저장/소유 방식을 준다
- `Slot<T>`는 자원 규율을 준다

이 셋은 서로 대체 관계가 아니라 서로 다른 축이다.

## 7. subject와 role / party / participant / world의 관계

### role

- 장기 모델에서 role은 subject에 ability를 붙인다
- 현재 구현에서는 role이 non-subject nominal declaration에 바인딩되면 semantic error를 낸다
- role은 “그 subject가 어떤 자격으로 행동하는가”를 정의한다

### party

- 장기 모델에서 party는 role slot에 subject를 꽂아 협력시키는 실행 단위다
- party는 struct 값 모음이 아니라 subject collaboration unit이다
- 현재 구현에서는 party role slot에 적은 ability가 실제 subject-bound role impl로 뒷받침되지 않으면 semantic error를 낸다

### participant

- participant는 subject와 병렬인 존재론적 계층이 아니다
- participant는 simulation loop, mailbox, scheduler semantics가 붙은 subject의 실행 프로파일이다
- 현재 semantic은 participant를 subject host로 취급하며, role binding, subject slot, `ToObject` / `ToTObject`, subject copy restriction에 participant를 포함한다
- 현재 parser surface는 standalone `subject Counter { ... }`와 subject-first `subject Counter { ... }`를 모두 받는다
- standalone `subject Counter { ... }`는 semantic warning과 함께 transitional syntax로 남아 있고, 권장 표면은 `subject Counter { ... }`다
- 즉 장기 모델에서도 구현 상태에서도 `subject`보다 `subject`가 먼저다

### roster / world

- roster은 party를 묶는 시스템 단위다
- world는 roster을 묶는 최상위 조율 단위다

즉 `subject`는 이 계층 전체의 leaf object가 아니라,
실제로는 role/party/world 체계를 떠받치는 실행 주체다.

## 8. 구현 방향

### 현재 구현

현재 구현은 대체로 다음에 가깝다.

- `subject`와 `class`는 parser/semantic뿐 아니라 C/LLVM method lowering, 저장/복사 규칙에서도 1차 분기됐다
- method를 free function으로 lower
- deeper identity/object-cell/runtime propagation semantics는 아직 약함

즉 현재 declaration surface는 “subject/class 분기는 시작됐고, 의미론 중심은 subject이며, lowering도 `subject=self-cell`, `class=value self`까지는 닫혔다”에 가깝다.

### 목표 구현

다음 의미론으로 수렴해야 한다.

1. `subject`는 nominal object type
2. method는 self object cell 기준 실행
3. role은 subject에만 바인딩하는 것을 기본 모델로 삼음
4. party slot은 subject를 담음
5. plain subject copy는 점진적으로 제한하거나 명시화
6. participant는 subject의 실행 프로파일로 다룸

현재 구현에서도 이 방향으로 일부 정렬했다.
- plain subject copy와 plain subject value parameter/return은 시맨틱에서 거부한다
- class는 plain copy / value parameter / value return을 허용한다
- participant는 semantic에서 subject-profile로 동작하므로 plain participant copy와 plain participant value parameter/return도 subject와 같은 제약을 따른다
- subject func/action은 C/LLVM backend에서 `self*` 기반 객체 셀 호출로 lower된다
- class func는 C/LLVM backend에서 value-self 호출로 lower된다
- bare field name과 bare hosted helper call은 subject/class/relation/effect/zone/world body 안에서 각각 현재 host의 `self` 대상으로 해석된다. `self.` 표기는 여전히 선택적으로 허용된다
- 클래스 생성자는 현재 "필드 순서 기반 positional initialization"으로 동작한다
- role이 non-subject nominal declaration에 바인딩되면 semantic error를 낸다
- party role slot은 subject-bound role impl이 없는 ability를 받을 수 없다
- participant type도 subject-bound role/party/projection 계약에 참여한다

### 현재 닫힌 범위

- 필드 선언
- hosted `func` / `action`과 선택적 `self`
- bare field access / bare helper call의 `self` 해석
- subject direct copy 금지
- subject plain value parameter/return 금지
- class value copy / value parameter / value return 허용
- `Vec2(3, 7)` 형태의 positional constructor
- `Box<class>`용 C helper 생성
- `Box<class>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>`를 함수 파라미터/리턴 타입으로 명시적으로 전달 가능
- generic class codegen (단형화 전략: `Pair<Int>` → `Pair_Int` struct + methods)
- participant를 subject host로 인식하는 semantic predeclaration / constructor / projection / domain check
- plain/secure `Slot<subject>` / `Slot<subject>` local object-cell anchor
- `own/ref Slot<subject-host>` / `own/ref SecureSlot<subject-host>` 함수 경계 전달
- secure boundary forwarding call에서 paired token 전파
- LLVM nested member assignment (`self.zone.subject.field = value`) runtime parity
- participant constructor가 C backend에서도 compound literal로 lowering됨

### 아직 닫히지 않은 범위

- class inheritance
- `super`
- 복잡한 object hierarchy
- `class`와 `subject`의 deeper behavioral split
- standalone `subject Name { ... }`를 언제까지 유지할지에 대한 최종 surface 정책
- subject/object view 전환을 표면 문법으로 드러낼지 여부
- richer handle/object-cell propagation semantics

## 9. transitional rule

아키텍처적으로는 `subject`가 ability를 수행하는 객체 타입이 맞다.
다만 구현 이행기에는 bare `class`도 계속 허용된다.

즉:

- 장기 모델: subject는 ability-hosting identity-bearing type
- 이행기 구현: `subject`와 `class`는 서로 다른 nominal flavor로 기록되지만, lowering과 runtime semantics는 아직 크게 공유한다
- participant는 독립 타입축이 아니라 이미 semantic에서는 execution model로 취급되고, 남은 일은 surface 재배치와 deeper runtime semantics다

하지만 문서와 설계의 중심은 후자가 아니라 전자다.

## 10. one-line definition

Pergyra의 현재 declaration surface는 `subject`와 `class`이고,
둘은 이제 같은 선언이 아니라 서로 다른 nominal flavor다.
장기 의미론의 중심 이름은 `subject`다.

**“ability를 수행하는 상태와 identity의 객체 타입이며, role/party/world 체계의 실제 실행 주체”**

이다.
