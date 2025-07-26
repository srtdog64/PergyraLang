# Pergyra 언어 문법 정의

<!--
Copyright (c) 2025 Pergyra Language Project
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice
2. Redistributions in binary form must reproduce the above copyright notice
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.
-->

## 1. 핵심 철학
- **Slot-based Memory Management**: 포인터 대신 슬롯으로 메모리 관리
- **의미 기반 선언**: 모든 코드는 의미 단위로 구성
- **내장 병렬성**: 언어 문법 차원에서 병렬 실행 지원
- **타입 안전성**: 컴파일 타임 타입 검증
- **구조화된 비동기**: Structured Effect Async (SEA) 모델

## 2. 기본 문법 (BSD Style)

### 2.1 슬롯 관리
```pergyra
let slot = ClaimSlot<Type>()
Write(slot, value)
let val = Read(slot)
Release(slot)
```

### 2.2 스코프 기반 슬롯
```pergyra
with slot<Type> as s
{
    s.Write(42)
    Log(s.Read())
}
```

### 2.3 병렬 실행
```pergyra
let result = Parallel
{
    A()
    B()
    C()
}
```

### 2.4 함수 정의 (BSD Style)
```pergyra
func ProcessData(input: String) -> Int
{
    // 함수 본문
    return 42
}

// 제네릭 함수
func Map<T, U>(items: Array<T>, transform: (T) -> U) -> Array<U>
    where T: Readable,
          U: Writable
{
    // 구현
}

// 비동기 함수
async func FetchData(url: String) -> Result<Data, Error>
{
    let response = await HttpClient.Get(url)
    return response.ToData()
}
```

### 2.5 클래스 정의 (BSD Style)
```pergyra
class SecureContainer<T>
    where T: Serializable
{
    private _slot: SecureSlot<T>
    private _token: SecurityToken
    
    public func Store(value: T)
    {
        Write(_slot, value, _token)
    }
    
    public func Retrieve() -> T
    {
        return Read(_slot, _token)
    }
}

// 액터 정의
actor BankAccount
{
    private _balance: Decimal = 0
    
    public func Deposit(amount: Decimal) -> Result<Decimal, Error>
    {
        if amount <= 0
        {
            return .Err(InvalidAmount)
        }
        _balance += amount
        return .Ok(_balance)
    }
}
```

### 2.6 제어 구조 (BSD Style)
```pergyra
// If 문
if condition
{
    // true 블록
}
else if otherCondition
{
    // else if 블록
}
else
{
    // else 블록
}

// For 루프
for i in 0..10
{
    Log(i)
}

// While 루프
while IsRunning()
{
    Process()
}

// Match 문
match value
{
    case .Success(data):
        ProcessData(data)
        
    case .Error(msg):
        LogError(msg)
        
    default:
        HandleUnknown()
}

// Select 문 (채널)
select
{
    case value = <-ch1:
        Log("Got from ch1: ", value)
        
    case msg = <-ch2:
        Log("Got from ch2: ", msg)
        
    case <-timeout:
        Log("Timeout!")
        
    default:
        Log("No data available")
}
```

## 3. 비동기/동시성 문법

### 3.1 Async/Await
```pergyra
// 비동기 함수 호출
let data = await FetchData("https://api.example.com")

// 여러 비동기 작업 동시 실행
let (a, b, c) = await Parallel
{
    FetchA()
    FetchB()
    FetchC()
}
```

### 3.2 채널 (Channels)
```pergyra
// 채널 생성
let channel = Channel<Int>(bufferSize: 10)

// 송신
await channel.Send(42)

// 수신
let value = await channel.Receive()

// 채널 연산자
ch <- 42        // Send
let v = <-ch    // Receive
```

### 3.3 구조화된 동시성
```pergyra
async func ProcessItems(items: Array<Item>) -> Array<Result>
{
    return await WithTaskGroup(of: Result.self)
    {
        group in
        
        for item in items
        {
            group.AddTask
            {
                await ProcessItem(item)
            }
        }
        
        var results = Array<Result>()
        for await result in group
        {
            results.Append(result)
        }
        
        return results
    }
}
```

### 3.4 Actor 사용
```pergyra
let account = BankAccount()
await account.Deposit(1000)
let balance = await account.GetBalance()
```

## 4. 토큰 정의

### 4.1 키워드 (Keywords)
#### 기본 키워드
- `let`: 변수/슬롯 선언
- `with`: 스코프 기반 슬롯 선언
- `as`: 슬롯 별칭
- `func`: 함수 선언
- `if`, `else`: 조건문
- `for`, `while`: 반복문
- `return`: 함수 반환
- `match`, `case`: 패턴 매칭
- `class`, `struct`: 타입 선언
- `public`, `private`: 접근 제어자

#### 비동기 키워드
- `async`: 비동기 함수 선언
- `await`: 비동기 작업 대기
- `actor`: 액터 선언
- `channel`: 채널 타입
- `select`: 채널 선택문
- `spawn`: 태스크 생성
- `default`: select문 기본 케이스

### 4.1.1 내장 함수 (Built-in Functions)
- `ClaimSlot`: 슬롯 할당
- `Write`: 슬롯에 값 쓰기
- `Read`: 슬롯에서 값 읽기
- `Release`: 슬롯 해제
- `Parallel`: 병렬 실행 블록
- `Log`: 출력 함수
- `Channel`: 채널 생성
- `WithTaskGroup`: 태스크 그룹 생성

### 4.2 연산자 (Operators)
- `=`: 할당
- `()`: 함수 호출/그룹핑
- `{}`: 블록
- `<>`: 타입 매개변수
- `.`: 멤버 접근
- `;`: 문장 종료
- `->`: 함수 반환 타입
- `<-`: 채널 수신 연산자

### 4.3 리터럴 (Literals)
- 정수: `42`, `-10`, `0`
- 문자열: `"hello"`, `'world'`
- 부울: `true`, `false`

### 4.4 식별자 (Identifiers)
- 변수명: `variableName`, `slotA` (camelCase)
- 함수명: `FunctionName`, `ProcessData` (PascalCase)
- 타입명: `Type`, `Int`, `String`, `Vector` (PascalCase)
- 상수명: `CONSTANT_NAME`, `MAX_VALUE` (UPPER_SNAKE_CASE)

## 5. 제네릭 문법

### 5.1 기본 제네릭
```pergyra
// 제네릭 함수
func Identity<T>(value: T) -> T {
    return value
}

// 제네릭 클래스
class Container<T> {
    private _value: T
    
    func Get() -> T {
        return _value
    }
}

// 다중 타입 파라미터
func Swap<A, B>(a: A, b: B) -> (B, A) {
    return (b, a)
}
```

### 5.2 제약조건 (Where 절)
```pergyra
func Sort<T>(items: Array<T>) -> Array<T>
    where T: Comparable {
    // T는 Comparable을 구현해야 함
}

// 다중 제약
func Process<T, U>(input: T) -> U
    where T: Readable + Sized,
          U: Writable {
    // 구현
}
```

### 5.3 Associated Types
```pergyra
trait Iterator<T> {
    type Item = T
    func Next() -> Option<Self.Item>
}

// 구현
impl Iterator<Int> for Range {
    type Item = Int
    func Next() -> Option<Int> {
        // ...
    }
}
```

### 5.4 제네릭 기본값
```pergyra
class Cache<K = String, V = Any> {
    // K의 기본값은 String, V의 기본값은 Any
}

let cache1 = Cache()              // Cache<String, Any>
let cache2 = Cache<Int>()         // Cache<Int, Any>
let cache3 = Cache<Int, User>()   // Cache<Int, User>
```

## 6. 타입 시스템

### 6.1 기본 타입
- `Int`: 32비트 정수
- `Long`: 64비트 정수  
- `Float`: 32비트 부동소수점
- `Double`: 64비트 부동소수점
- `String`: 문자열
- `Bool`: 부울값

### 6.2 슬롯 타입
- `Slot<Type>`: 타입이 지정된 슬롯
- `SecureSlot<Type>`: 보안 슬롯
- 예: `Slot<Int>`, `SecureSlot<String>`

### 6.3 비동기 타입
- `Channel<T>`: 채널 타입
- `Future<T>`: 미래값 타입
- `Result<T, E>`: 결과 타입
- `AsyncIterator<T>`: 비동기 이터레이터

## 7. 메모리 모델

### 7.1 슬롯 테이블 구조
```
SlotId | TypeTag | Occupied | DataBlockRef | TTL
```

### 7.2 생명주기
- **스코프 기반**: `with` 블록 종료 시 자동 해제
- **명시적 해제**: `Release()` 호출
- **TTL 기반**: 시간 초과 시 자동 해제
- **구조화된 동시성**: 부모 스코프 종료 시 자식 태스크 취소

## 8. 명명 규칙 (Naming Conventions)

### 8.1 BSD 스타일 + C# 컨벤션
- **변수**: camelCase (예: `counterSlot`, `userData`)
- **함수/메서드**: PascalCase (예: `ProcessData`, `GetValue`)
- **타입/클래스**: PascalCase (예: `TreeNode`, `SecureSlot`)
- **상수**: UPPER_SNAKE_CASE (예: `MAX_SLOTS`, `DEFAULT_TTL`)
- **프로퍼티**: 
  - Public: PascalCase (예: `Health`, `Name`)
  - Private: camelCase with underscore (예: `_health`, `_name`)

### 8.2 예제
```pergyra
class Player {
    private _healthSlot: SecureSlot<Int>
    private _nameSlot: Slot<String>
    
    public Health: Int {
        get { return Read(_healthSlot) }
        set { Write(_healthSlot, value) }
    }
    
    public func TakeDamage(damage: Int) {
        let currentHealth = Read(_healthSlot)
        Write(_healthSlot, Max(0, currentHealth - damage))
    }
}
```

## 9. Effect System

### 9.1 Effect 타입
```pergyra
// I/O Effect
effect IO {
    func Read(fd: Int, buffer: Buffer, count: Size) -> Result<Size, Error>
    func Write(fd: Int, buffer: Buffer, count: Size) -> Result<Size, Error>
}

// Time Effect
effect Time {
    func Sleep(duration: Duration)
    func Now() -> Timestamp
}
```

### 9.2 Effect 사용
```pergyra
async func ReadFile(path: String) -> Result<String, Error>
    with effects IO
{
    let fd = await IO.Open(path)
    let content = await IO.ReadAll(fd)
    await IO.Close(fd)
    return .Ok(content)
}
```
