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
```

## 3. 토큰 정의

### 3.1 키워드 (Keywords)
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

### 3.1.1 내장 함수 (Built-in Functions)
- `ClaimSlot`: 슬롯 할당
- `Write`: 슬롯에 값 쓰기
- `Read`: 슬롯에서 값 읽기
- `Release`: 슬롯 해제
- `Parallel`: 병렬 실행 블록
- `Log`: 출력 함수

### 3.2 연산자 (Operators)
- `=`: 할당
- `()`: 함수 호출/그룹핑
- `{}`: 블록
- `<>`: 타입 매개변수
- `.`: 멤버 접근
- `;`: 문장 종료

### 3.3 리터럴 (Literals)
- 정수: `42`, `-10`, `0`
- 문자열: `"hello"`, `'world'`
- 부울: `true`, `false`

### 3.4 식별자 (Identifiers)
- 변수명: `variableName`, `slotA` (camelCase)
- 함수명: `FunctionName`, `ProcessData` (PascalCase)
- 타입명: `Type`, `Int`, `String`, `Vector` (PascalCase)
- 상수명: `CONSTANT_NAME`, `MAX_VALUE` (UPPER_SNAKE_CASE)

## 4. 제네릭 문법

### 4.1 기본 제네릭
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

### 4.2 제약조건 (Where 절)
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

### 4.3 Associated Types
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

### 4.4 제네릭 기본값
```pergyra
class Cache<K = String, V = Any> {
    // K의 기본값은 String, V의 기본값은 Any
}

let cache1 = Cache()              // Cache<String, Any>
let cache2 = Cache<Int>()         // Cache<Int, Any>
let cache3 = Cache<Int, User>()   // Cache<Int, User>
```

## 5. 타입 시스템

### 4.1 기본 타입
- `Int`: 32비트 정수
- `Long`: 64비트 정수  
- `Float`: 32비트 부동소수점
- `Double`: 64비트 부동소수점
- `String`: 문자열
- `Bool`: 부울값

### 4.2 슬롯 타입
- `slot<Type>`: 타입이 지정된 슬롯
- 예: `slot<Int>`, `slot<String>`

## 5. 메모리 모델

### 5.1 슬롯 테이블 구조
```
SlotId | TypeTag | Occupied | DataBlockRef | TTL
```

### 5.2 생명주기
- **스코프 기반**: `with` 블록 종료 시 자동 해제
- **명시적 해제**: `Release()` 호출
- **TTL 기반**: 시간 초과 시 자동 해제

## 6. 명명 규칙 (Naming Conventions)

### 6.1 BSD 스타일 + C# 컨벤션
- **변수**: camelCase (예: `counterSlot`, `userData`)
- **함수/메서드**: PascalCase (예: `ProcessData`, `GetValue`)
- **타입/클래스**: PascalCase (예: `TreeNode`, `SecureSlot`)
- **상수**: UPPER_SNAKE_CASE (예: `MAX_SLOTS`, `DEFAULT_TTL`)
- **프로퍼티**: 
  - Public: PascalCase (예: `Health`, `Name`)
  - Private: camelCase with underscore (예: `_health`, `_name`)

### 6.2 예제
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
