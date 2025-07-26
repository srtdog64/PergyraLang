# Pergyra 모듈 시스템

## 1. 모듈 정의 (BSD Style)

```pergyra
// math.pgy
module Math
{
    // Public exports
    export func Add(a: Int, b: Int) -> Int
    {
        return a + b
    }
    
    export func Multiply(a: Int, b: Int) -> Int
    {
        return a * b
    }
    
    // Private helper (not exported)
    func ValidateNumber(n: Int) -> Bool
    {
        return n >= 0 && n <= MAX_INT
    }
    
    // 상수 export
    export const PI = 3.14159265359
    export const E = 2.71828182846
}
```

## 2. 모듈 가져오기

```pergyra
// 전체 모듈 가져오기
import Math

func Calculate()
{
    let sum = Math.Add(5, 3)
    let area = Math.PI * radius * radius
}

// 선택적 가져오기
import Math.{Add, Multiply, PI}

func Calculate()
{
    let sum = Add(5, 3)  // Math. 접두사 불필요
    let product = Multiply(4, 5)
}

// 별칭으로 가져오기
import Math as M
import VeryLongModuleName as VLMN

// 와일드카드 가져오기 (권장하지 않음)
import Math.*
```

## 3. 중첩 모듈

```pergyra
module Game
{
    export module Graphics
    {
        export class Renderer
        {
            public func Draw(object: GameObject)
            {
                // 구현
            }
        }
        
        export func Initialize()
        {
            // 그래픽 초기화
        }
    }
    
    export module Audio
    {
        export func PlaySound(name: String)
        {
            // 사운드 재생
        }
    }
    
    export module Physics
    {
        export func Simulate(deltaTime: Float)
        {
            // 물리 시뮬레이션
        }
    }
}

// 사용
import Game.Graphics.{Renderer, Initialize}
import Game.Audio
import Game.Physics as Phys
```

## 4. 모듈 레벨 초기화

```pergyra
module Database
{
    // 모듈 초기화 블록 (모듈 로드 시 한 번 실행)
    init
    {
        Log("Initializing Database module...")
        ConnectToDatabase()
        PrepareStatements()
    }
    
    // 모듈 정리 블록 (프로그램 종료 시)
    deinit
    {
        Log("Cleaning up Database module...")
        CloseConnections()
        ReleaseResources()
    }
    
    export func Query(sql: String) -> Result<Data, Error>
    {
        // 쿼리 실행
    }
}
```

## 5. 조건부 컴파일

```pergyra
module Platform
{
    @[if(OS == "Windows")]
    export func GetSystemPath() -> String
    {
        return "C:\\Windows\\System32"
    }
    
    @[if(OS == "Linux")]
    export func GetSystemPath() -> String
    {
        return "/usr/bin"
    }
    
    @[if(DEBUG)]
    export func DebugLog(message: String)
    {
        PrintToConsole("[DEBUG] " + message)
    }
    
    @[if(!DEBUG)]
    export func DebugLog(message: String)
    {
        // 릴리즈 빌드에서는 아무것도 안 함
    }
}
```

## 6. 모듈 가시성 제어

```pergyra
module Security
{
    // 모듈 내부에서만 사용 가능
    internal const SECRET_KEY = "..."
    
    internal func ValidateInternal(data: String) -> Bool
    {
        // 내부 검증 로직
    }
    
    // 같은 패키지 내에서만 사용 가능
    package func PackageOnlyFunction()
    {
        // 패키지 레벨 기능
    }
    
    // 공개 API
    export func ValidatePublic(data: String) -> Bool
    {
        return ValidateInternal(data) && CheckSignature(data)
    }
}
```

## 7. 제네릭 모듈

```pergyra
module Collections<T>
    where T: Comparable
{
    export class SortedList<T>
    {
        private _items: Array<T>
        
        public func Add(item: T)
        {
            // 정렬된 위치에 삽입
        }
        
        public func Contains(item: T) -> Bool
        {
            // 이진 검색
        }
    }
    
    export func Sort(items: Array<T>)
    {
        // 퀵소트 구현
    }
}

// 사용
import Collections<Int> as IntCollections
let sortedInts = IntCollections.SortedList<Int>()
```

## 8. 모듈 재내보내기

```pergyra
// utils.pgy
module Utils
{
    // 다른 모듈들을 재내보내기
    export import StringUtils.*
    export import MathUtils.{Min, Max, Clamp}
    export import DateUtils as Date
    
    // 자체 유틸리티도 추가
    export func Swap<T>(a: inout T, b: inout T)
    {
        let temp = a
        a = b
        b = temp
    }
}

// 이제 Utils를 통해 모든 유틸리티 접근 가능
import Utils

func Example()
{
    let clamped = Utils.Clamp(value, 0, 100)
    let formatted = Utils.FormatString(template, args)
    let now = Utils.Date.Now()
}
```

## 9. 모듈 테스트

```pergyra
module Math
{
    export func Add(a: Int, b: Int) -> Int
    {
        return a + b
    }
    
    // 테스트는 같은 파일에 포함 가능
    @[test]
    module Tests
    {
        func TestAdd()
        {
            Assert(Add(2, 3) == 5)
            Assert(Add(-1, 1) == 0)
            Assert(Add(0, 0) == 0)
        }
        
        func TestOverflow()
        {
            // MAX_INT + 1 should panic or return error
            AssertPanic(() => Add(MAX_INT, 1))
        }
    }
}
```

## 10. 순환 의존성 방지

```pergyra
// 컴파일러가 순환 의존성 감지
// a.pgy
module A
{
    import B  // 에러: 순환 의존성!
    
    export func FuncA()
    {
        B.FuncB()
    }
}

// b.pgy
module B
{
    import A  // 에러: 순환 의존성!
    
    export func FuncB()
    {
        A.FuncA()
    }
}

// 해결: 인터페이스 분리
module AInterface
{
    export trait IA
    {
        func FuncA()
    }
}
```

## 11. 모듈 문서화

```pergyra
/// [What]: 수학 연산을 위한 유틸리티 모듈
/// [Why]: 자주 사용되는 수학 함수들을 중앙화
/// [Example]:
/// ```
/// import Math
/// let result = Math.Add(5, 3)
/// ```
module Math
{
    /// [What]: 두 정수를 더함
    /// [Why]: 기본적인 산술 연산 제공
    /// [Complexity]: O(1)
    export func Add(a: Int, b: Int) -> Int
    {
        return a + b
    }
}
```
