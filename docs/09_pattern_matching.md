# Pergyra 패턴 매칭 시스템

## 1. 기본 패턴 매칭

### 1.1 값 매칭
```pergyra
match value
{
    case 0:
        Log("Zero")
        
    case 1..10:
        Log("Small number")
        
    case n if n > 100:
        Log("Large number: ", n)
        
    default:
        Log("Other")
}
```

### 1.2 타입 매칭
```pergyra
match result
{
    case .Success(data):
        ProcessData(data)
        
    case .Error(code, msg):
        LogError(code, msg)
        
    case .Pending:
        Wait()
}
```

### 1.3 구조체 패턴
```pergyra
match player
{
    case Player { health: 0, .. }:
        OnPlayerDeath()
        
    case Player { health: h, shield: s } if h < 20 && s == 0:
        ShowLowHealthWarning()
        
    case Player { health: 100, shield: 100 }:
        ShowFullHealthStatus()
        
    default:
        UpdateNormalStatus()
}
```

## 2. 고급 패턴 매칭

### 2.1 중첩 패턴
```pergyra
match message
{
    case .Request(.Login(username, password)):
        AuthenticateUser(username, password)
        
    case .Request(.Data(id)) if IsValidId(id):
        FetchData(id)
        
    case .Response(.Success(data)):
        ProcessResponse(data)
        
    case .Response(.Error(ErrorCode.Timeout)):
        RetryRequest()
}
```

### 2.2 슬롯 패턴 매칭
```pergyra
match slot.TryRead()
{
    case .Some(value) if value > 0:
        ProcessPositive(value)
        
    case .Some(0):
        HandleZero()
        
    case .None:
        HandleEmpty()
}
```

### 2.3 제네릭 패턴
```pergyra
func ProcessResult<T, E>(result: Result<T, E>) -> String
{
    match result
    {
        case .Ok(value):
            return "Success: " + ToString(value)
            
        case .Err(error):
            return "Error: " + ToString(error)
    }
}
```

## 3. 패턴 가드

```pergyra
match request
{
    // 복잡한 조건을 가드로 표현
    case .Update(id, data) if HasPermission(id) && IsValid(data):
        PerformUpdate(id, data)
        
    case .Update(id, _) if !HasPermission(id):
        DenyAccess(id)
        
    case .Update(_, data) if !IsValid(data):
        RejectInvalidData(data)
}
```

## 4. 바인딩 패턴

```pergyra
match shape
{
    // @ 연산자로 전체 값 바인딩
    case rect @ Rectangle { width, height } if width == height:
        Log("Square: ", rect)
        
    // 부분 바인딩
    case Circle { radius: r @ 1..10 }:
        Log("Small circle with radius: ", r)
        
    // 변수 바인딩과 조건
    case Triangle { sides: [a, b, c] } if a + b > c:
        Log("Valid triangle")
}
```

## 5. Option/Result 패턴

```pergyra
// Option 체이닝
func GetUserName(id: Int) -> Option<String>
{
    match GetUser(id)
    {
        case .Some(user):
            match user.name
            {
                case .Some(name) if !name.IsEmpty():
                    return .Some(name)
                default:
                    return .None
            }
            
        case .None:
            return .None
    }
}

// Result 체이닝 (더 간결한 버전)
func ProcessFile(path: String) -> Result<Data, Error>
{
    match ReadFile(path)
    {
        case .Ok(content):
            match ParseData(content)
            {
                case .Ok(data):
                    return .Ok(data)
                    
                case .Err(e):
                    return .Err(ParseError(e))
            }
            
        case .Err(e):
            return .Err(FileError(e))
    }
}
```

## 6. 배열/컬렉션 패턴

```pergyra
match list
{
    case []:
        Log("Empty list")
        
    case [single]:
        Log("Single element: ", single)
        
    case [first, second]:
        Log("Two elements: ", first, ", ", second)
        
    case [head, ...tail]:
        Log("Head: ", head, ", Tail has ", tail.Length, " elements")
        
    case [...init, last]:
        Log("Last element: ", last)
        
    case [first, ...middle, last] if middle.Length > 0:
        Log("First: ", first, ", Last: ", last)
}
```

## 7. 완전성 검사

```pergyra
// 컴파일러가 모든 경우를 처리했는지 검사
enum Status
{
    Active,
    Pending,
    Completed,
    Failed
}

func GetStatusMessage(status: Status) -> String
{
    match status
    {
        case .Active:
            return "진행 중"
            
        case .Pending:
            return "대기 중"
            
        case .Completed:
            return "완료됨"
            
        // 컴파일 에러: Failed 케이스 누락!
    }
}
```

## 8. 성능 최적화

```pergyra
// 컴파일러가 점프 테이블로 최적화
match hashCode
{
    case 0x1234:
        HandleCase1()
        
    case 0x5678:
        HandleCase2()
        
    case 0x9ABC:
        HandleCase3()
        
    // 많은 케이스가 있을 때 O(1) 성능
}
```
