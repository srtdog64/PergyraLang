# Pergyra 에러 처리 시스템

## 1. Result 타입 (Rust 스타일)

```pergyra
enum Result<T, E>
{
    Ok(T),
    Err(E)
}

// 사용 예시
func DivideNumbers(a: Float, b: Float) -> Result<Float, String>
{
    if b == 0
    {
        return .Err("Division by zero")
    }
    return .Ok(a / b)
}
```

## 2. Try 연산자 (?)

```pergyra
func ProcessFile(path: String) -> Result<Data, Error>
{
    // ? 연산자는 에러를 자동으로 전파
    let content = ReadFile(path)?
    let parsed = ParseJson(content)?
    let validated = ValidateData(parsed)?
    
    return .Ok(validated)
}
```

## 3. Option 타입

```pergyra
enum Option<T>
{
    Some(T),
    None
}

// 안전한 슬롯 읽기
func SafeRead<T>(slot: Slot<T>) -> Option<T>
{
    if slot.IsValid()
    {
        return .Some(Read(slot))
    }
    return .None
}
```

## 4. Panic과 Recovery

```pergyra
// Panic은 복구 불가능한 에러
func AssertNotNull<T>(value: Option<T>) -> T
{
    match value
    {
        case .Some(v):
            return v
            
        case .None:
            Panic("Unexpected null value")
    }
}

// Panic 핸들러 설정
SetPanicHandler(func(message: String)
{
    LogError("PANIC: ", message)
    SaveCrashDump()
    GracefulShutdown()
})
```

## 5. 에러 타입 계층

```pergyra
// 기본 에러 트레잇
trait Error
{
    func Message() -> String
    func Source() -> Option<Error>
    func StackTrace() -> String
}

// 구체적인 에러 타입들
class FileError : Error
{
    private _path: String
    private _kind: FileErrorKind
    
    enum FileErrorKind
    {
        NotFound,
        PermissionDenied,
        AlreadyExists,
        InvalidPath
    }
}

class NetworkError : Error
{
    private _url: String
    private _statusCode: Int
    private _timeout: Bool
}
```

## 6. 에러 체이닝

```pergyra
// 여러 에러를 연결하여 컨텍스트 제공
func LoadUserData(userId: Int) -> Result<User, Error>
{
    return FetchFromDatabase(userId)
        .MapErr(e => DatabaseError("Failed to fetch user", e))
        .AndThen(data => DeserializeUser(data))
        .MapErr(e => DeserializationError("Invalid user data", e))
}
```

## 7. Defer와 Cleanup

```pergyra
func ProcessWithCleanup() -> Result<(), Error>
{
    let resource = AcquireResource()?
    
    defer
    {
        // 함수 종료 시 무조건 실행
        ReleaseResource(resource)
    }
    
    // 에러가 발생해도 defer 블록은 실행됨
    DoSomethingDangerous(resource)?
    
    return .Ok(())
}
```

## 8. 커스텀 에러 매크로

```pergyra
// 에러 생성을 간편하게
@[error_type]
enum AppError
{
    InvalidInput(field: String, reason: String),
    DatabaseError(query: String, error: Error),
    NetworkTimeout(url: String, duration: Float),
    ConfigError(key: String)
}

// 자동 생성되는 메서드들:
// - Message() -> String
// - ToResult<T>() -> Result<T, AppError>
// - Chain(source: Error) -> AppError
```

## 9. 에러 처리 베스트 프랙티스

```pergyra
/// [What]: 사용자 생성 with 완전한 에러 처리
/// [Why]: 각 단계의 실패를 명확히 처리하고 로깅
func CreateUser(request: CreateUserRequest) -> Result<User, AppError>
{
    // 입력 검증
    ValidateEmail(request.email)
        .MapErr(e => AppError.InvalidInput("email", e.Message()))?
    
    ValidatePassword(request.password)
        .MapErr(e => AppError.InvalidInput("password", e.Message()))?
    
    // 중복 확인
    let existing = CheckUserExists(request.email)?
    if existing.IsSome()
    {
        return .Err(AppError.InvalidInput("email", "Already exists"))
    }
    
    // 트랜잭션 내에서 생성
    return Transaction(func()
    {
        let hashedPassword = HashPassword(request.password)?
        let user = User
        {
            email: request.email,
            passwordHash: hashedPassword,
            createdAt: Now()
        }
        
        SaveToDatabase(user)
            .MapErr(e => AppError.DatabaseError("INSERT user", e))
    })
}
```

## 10. 비동기 에러 처리

```pergyra
// Future/Promise 스타일 에러 처리
async func FetchDataAsync(url: String) -> Result<Data, Error>
{
    try
    {
        let response = await HttpGet(url)
        if response.StatusCode != 200
        {
            return .Err(NetworkError(url, response.StatusCode))
        }
        
        let data = await response.ReadBodyAsync()
        return .Ok(data)
    }
    catch timeout: TimeoutError
    {
        return .Err(NetworkTimeout(url, timeout.Duration))
    }
    catch e: Error
    {
        return .Err(NetworkError("Unknown error", e))
    }
}
```

## 11. 슬롯 에러 처리

```pergyra
// 슬롯 관련 특화 에러
enum SlotError
{
    InvalidToken,
    ExpiredToken,
    SlotReleased,
    SecurityViolation,
    TypeMismatch
}

func SecureSlotOperation<T>(slot: SecureSlot<T>, token: Token) -> Result<T, SlotError>
{
    // 토큰 검증
    if !ValidateToken(slot, token)
    {
        return .Err(.InvalidToken)
    }
    
    // 슬롯 상태 확인
    if slot.IsReleased()
    {
        return .Err(.SlotReleased)
    }
    
    // 안전한 읽기
    return .Ok(ReadSecure(slot, token))
}
```
