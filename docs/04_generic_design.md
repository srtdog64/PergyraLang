# Pergyra 제네릭 문법 확장 제안

## 1. 제네릭이 기본인 언어

### 1.1 모든 것이 제네릭

```pergyra
// 기본 타입도 사실 제네릭의 특수화
type Int = Numeric<32, Signed>
type UInt = Numeric<32, Unsigned>
type Float = Numeric<32, IEEEFloat>
type Bool = Enum<True, False>

// 슬롯도 항상 제네릭
let slot = ClaimSlot<T>()  // T는 필수
```

### 1.2 제네릭 문법 확장

#### A. Where 절 (제약조건)

```pergyra
func Sort<T>(items: Array<T>) -> Array<T> 
    where T: Comparable {
    // T는 반드시 비교 가능해야 함
}

// 다중 제약
func Process<T, U>(input: T) -> U
    where T: Readable + Sized,
          U: Writable {
    // 구현
}
```

#### B. Associated Types

```pergyra
ability Container<T> {
    type Item = T
    type Index = Int
    
    func Get(index: Self.Index) -> Self.Item
}

// 슬롯도 associated type 활용
ability SecureStorage {
    type Token
    type Value
    
    func Store(value: Self.Value, token: Self.Token)
}
```

#### C. Higher-Kinded Types / Functor 추상 — 도입 유보 (soft-no)

> **입장**: Pergyra 는 진짜 Functor 추상(Haskell `fmap` 스타일) 및 그 기반인
> HKT(higher-kinded type) 를 **현재 도입하지 않는다**. 영구 거부는 아니고
> "현재 언어 스타일과 맞지 않는다" 수준의 soft-no 이며, 베타 이후에도
> 낮은 우선순위로 분류한다.
>
> 이 결정은 [docs/00_engine_core_spec.md](00_engine_core_spec.md) 의
> "초기 금지 범위 — higher-kinded type" 조항과 일치하며, 본 섹션은 그
> 근거와 대체 방식을 문서화한다.

**설계 제안 (참고용, 미구현)**:

```pergyra
// 타입 생성자를 받는 제네릭 — 설계 초안
func Map<F<_>, A, B>(container: F<A>, transform: (A) -> B) -> F<B> {
    // F는 타입 생성자 (Array, Option, Result 등)
}

// 사용 예 (가상)
let numbers: Array<Int> = [1, 2, 3]
let strings: Array<String> = Map(numbers, ToString)
```

**유보 근거**:

1. **에러 메시지 품질** — Pergyra 는 intent-first / DDD 철학상 "문제 위치 ·
   원인 · 해결힌트" 가 모두 명확한 진단을 1순위로 둔다. HKT 를 도입하면
   Haskell/Scala 에서 익숙한 `No instance for (Functor (Either e))`
   류의 긴 추론 실패 메시지가 발생해 이 정체성에 역행한다.
2. **타입 추론 복잡도** — HKT 는 단형화(monomorphization) 전략과
   associated type constructor 추론 재설계를 요구한다. 현 베타의
   "exact / ability / multi-bound + default type argument" 안정
   closure 를 흔들 위험이 크다.
3. **교육 부담** — 게임 도메인(던전 크롤러 killer use case) 개발자 대상
   입문성을 유지하려면 범주이론 개념 전제를 줄이는 것이 유리하다.
4. **실용 대체 수단 존재** — 아래 "§ 대체 패턴" 참조. per-container map
   함수와 ability<T> + associated type 조합으로 실제 워크로드의 99% 가
   커버된다.

**대체 패턴**:

```pergyra
// 1) per-container map 함수 (현재 방식)
let doubled: Array<Int> = ArrayMap(nums, Double)
let label: Option<String> = OptionMap(maybeId, FormatId)
let saved: Result<User> = ResultMap(parsed, Persist)

// 2) ability<T> + associated type 로 컨테이너 계약 공유
ability Container<T> {
    type Item = T
    type Index = Int
    func Get(index: Self.Index) -> Self.Item
}

// 3) Option/Result 편의는 구문설탕으로 (향후 검토)
//    - ?. null-safe chaining
//    - when Some(x) = ...
//    - Result match sugar
```

**유사 언어 비교**:

| 언어 | HKT | Functor 추상 | 포지션 |
|---|---|---|---|
| Haskell | ✓ | ✓ (`Functor` typeclass) | HKT 를 언어 중심축으로 |
| Scala | ✓ | ✓ (Cats / ZIO 계열) | HKT 있지만 생태계 고급 용법으로 분리 |
| OCaml | 부분 (module functor) | 없음 | module functor 로만, 값 레벨 Functor 없음 |
| F# | 없음 | 없음 | DDD 언어로 성공, HKT 없이 번영 |
| Gleam | 없음 | 없음 | 최근 소형 언어, 의도적으로 HKT 배제 |
| **Pergyra** | **없음 (soft-no)** | **없음 (per-container 대체)** | F# / Gleam 계열 포지션 |

**재검토 조건**:

아래 3가지 중 **2개 이상** 충족 시에만 HKT 재검토를 연다:

- per-container map/filter/fold 함수가 30개 이상으로 늘어 중복 유지 부담이 체감
- killer use case 에서 HKT 없이 표현 불가능한 패턴이 반복 관찰
- 타입 추론 / 에러 메시지 인프라가 HKT 도입 후에도 품질을 유지할 수 있도록 개선됨

이 3조건 중 1개만 만족해서는 HKT 를 열지 않는다 — "구현 가능함" 과 "언어에 맞음" 은 다르다.

### 1.3 제네릭 슬롯 고급 기능

```pergyra
// 제네릭 보안 슬롯
class SecureSlot<T, Security: SecurityLevel = Hardware> {
    private _slot: RawSlot
    private _token: Token<Security>
    
    func Write(value: T) where T: Serializable {
        // Security 레벨에 따라 다른 처리
        match Security {
            case Basic => BasicWrite(value)
            case Hardware => HardwareWrite(value)
            case Encrypted => EncryptedWrite(value)
        }
    }
}

// Phantom Type으로 상태 추적
struct Slot<T, State> {
    // State는 컴파일 타임에만 존재
}

let slot1: Slot<Int, Empty> = ClaimSlot()
let slot2: Slot<Int, Filled> = slot1.Write(42)  // 타입이 바뀜!
// slot1.Read() // 컴파일 에러! Empty 슬롯은 읽을 수 없음
```

### 1.4 Variance (공변성/반공변성)

```pergyra
// 공변성 (+)
class Producer<+T> {
    func Produce() -> T
}

// 반공변성 (-)
class Consumer<-T> {
    func Consume(value: T)
}

// 무공변성 (기본)
class Storage<T> {
    func Store(value: T)
    func Retrieve() -> T
}

// 사용 예
let stringProducer: Producer<String> = ...
let anyProducer: Producer<Any> = stringProducer  // OK! (공변)
```

## 2. 제네릭 타입 추론

### 2.1 양방향 타입 추론

```pergyra
// 명시적 타입 없이도 추론
let slot = ClaimSlot()  // 사용처에서 타입 추론
slot.Write(42)          // 이제 Slot<Int>로 확정

// 함수에서도
func Identity<T>(value: T) -> T = value

let x = Identity(5)     // T = Int로 추론
let y = Identity("hi")  // T = String으로 추론
```

### 2.2 제네릭 기본값

```pergyra
// 제네릭 매개변수 기본값
class Cache<K = String, V = Any> {
    // 구현
}

let cache1 = Cache()           // Cache<String, Any>
let cache2 = Cache<Int>()      // Cache<Int, Any>
let cache3 = Cache<Int, User>() // Cache<Int, User>
```

## 3. 제네릭과 병렬 처리

```pergyra
// 제네릭 병렬 컨테이너
class ParallelArray<T> where T: Send + Sync {
    func Map<U>(f: (T) -> U) -> ParallelArray<U> 
        where U: Send + Sync {
        parallel {
            // 자동 병렬화
        }
    }
}

// 제네릭 액터
participant Storage<T> where T: Sendable {
    private var items: Array<T> = []
    
    func Add(item: T) {
        items.Append(item)
    }
}
```

## 4. 컴파일 타임 제네릭

```pergyra
// 컴파일 타임 계산
func ArraySize<const N: Int>() -> Type {
    return Array<T, N>  // 고정 크기 배열
}

// 사용
let arr: ArraySize<5>() = [1, 2, 3, 4, 5]
// let bad: ArraySize<5>() = [1, 2, 3]  // 컴파일 에러!

// 컴파일 타임 조건
func OptimizedSort<T, const Size: Int>(arr: Array<T, Size>) {
    if const Size < 10 {
        InsertionSort(arr)  // 작은 배열
    } else {
        QuickSort(arr)      // 큰 배열
    }
}
```

## 5. 제네릭 매크로 (미래)

```pergyra
// 타입 수준 프로그래밍
macro DeriveSerializable<T> {
    // T의 모든 필드를 순회하며 직렬화 코드 생성
    impl Serializable for T {
        func Serialize() -> Bytes {
            // 자동 생성된 코드
        }
    }
}

@[DeriveSerializable]
struct User<T> {
    name: String,
    data: T
}
```

## 6. 구현 우선순위

1. **기본 제네릭** (현재)
   - `<T>` 문법 ✓
   - 제네릭 함수/클래스 ✓

2. **제약조건** (다음)
   - where 절
   - Trait bounds

3. **고급 기능** (미래)
   - Associated types
   - Higher-kinded types
   - Variance

4. **컴파일 타임** (장기)
   - Const generics
   - Type-level programming

## Generic Classes (구현 완료)

제네릭 클래스는 단형화(monomorphization) 전략으로 구현되었다.

### 문법

```pergyra
class Pair<T> {
    let first: T;
    let second: T;

    func GetFirst(self) -> T {
        return self.first;
    }
}
```

### 단형화 전략

컴파일러는 사용처의 타입 인자를 기반으로 구체화된 struct와 메서드를 생성한다.

- `Pair<Int>` → C struct `Pair_Int` + 메서드 `Pair_Int_GetFirst(Pair_Int* self)`
- `Pair<Float>` → C struct `Pair_Float` + 메서드 `Pair_Float_GetFirst(Pair_Float* self)`
- 여러 특수화가 동일 번역 단위에 공존 가능

### 생성자

타입 어노테이션이 필수이며, bare constructor 형태로 호출한다.

```pergyra
let p: Pair<Int> = Pair(3, 7);
```

### 메서드 호출

단형화된 이름으로 C 함수가 생성된다.

```c
// 생성된 C 코드
int Pair_Int_GetFirst(Pair_Int* self) {
    return self->first;
}
```

### 제한사항

- 자기 참조 타입(self-referential types)은 아직 지원하지 않음
- 타입 추론 없이 사용처에서 타입 인자를 명시해야 함
