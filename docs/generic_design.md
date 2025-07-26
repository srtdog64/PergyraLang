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
trait Container<T> {
    type Item = T
    type Index = Int
    
    func Get(index: Self.Index) -> Self.Item
}

// 슬롯도 associated type 활용
trait SecureStorage {
    type Token
    type Value
    
    func Store(value: Self.Value, token: Self.Token)
}
```

#### C. Higher-Kinded Types

```pergyra
// 타입 생성자를 받는 제네릭
func Map<F<_>, A, B>(container: F<A>, transform: (A) -> B) -> F<B> {
    // F는 타입 생성자 (Array, Option, Result 등)
}

// 사용 예
let numbers: Array<Int> = [1, 2, 3]
let strings: Array<String> = Map(numbers, ToString)
```

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
        Parallel {
            // 자동 병렬화
        }
    }
}

// 제네릭 액터
actor Storage<T> where T: Sendable {
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
