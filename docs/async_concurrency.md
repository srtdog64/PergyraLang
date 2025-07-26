# Pergyra 비동기/동시성 시스템

## 1. Async/Await (BSD Style)

```pergyra
// 비동기 함수 정의
async func FetchUserData(userId: Int) -> Result<User, Error>
{
    // HTTP 요청
    let response = await HttpClient.Get("/users/" + userId)
    
    if response.StatusCode != 200
    {
        return .Err(HttpError(response.StatusCode))
    }
    
    // JSON 파싱
    let userData = await response.ParseJsonAsync<User>()
    return .Ok(userData)
}

// 사용
async func Main()
{
    match await FetchUserData(123)
    {
        case .Ok(user):
            Log("User: ", user.name)
            
        case .Err(error):
            LogError("Failed to fetch user: ", error)
    }
}
```

## 2. 액터 모델 (Actor Model)

```pergyra
// 액터는 동시성 안전한 상태를 가진 객체
actor BankAccount
{
    private var _balance: Decimal = 0
    private let _accountId: String
    
    public func Deposit(amount: Decimal) -> Result<Decimal, Error>
    {
        if amount <= 0
        {
            return .Err(InvalidAmount)
        }
        
        _balance += amount
        return .Ok(_balance)
    }
    
    public func Withdraw(amount: Decimal) -> Result<Decimal, Error>
    {
        if amount > _balance
        {
            return .Err(InsufficientFunds)
        }
        
        _balance -= amount
        return .Ok(_balance)
    }
    
    // 읽기 전용 접근
    public func GetBalance() -> Decimal
    {
        return _balance
    }
}

// 사용 - 모든 메서드 호출은 자동으로 직렬화됨
async func TransferMoney()
{
    let account1 = BankAccount("ACC001")
    let account2 = BankAccount("ACC002")
    
    // 동시에 여러 액터에 접근해도 안전
    await Parallel
    {
        account1.Deposit(1000)
        account2.Deposit(500)
    }
    
    // 송금
    match await account1.Withdraw(200)
    {
        case .Ok(newBalance):
            await account2.Deposit(200)
            Log("Transfer complete")
            
        case .Err(e):
            LogError("Transfer failed: ", e)
    }
}
```

## 3. 채널 (Channels)

```pergyra
// Go 스타일 채널
func ProducerConsumer()
{
    // 버퍼드 채널 생성
    let channel = Channel<Int>(bufferSize: 10)
    
    // 생산자
    Task.Spawn(async func()
    {
        for i in 0..100
        {
            await channel.Send(i)
            await Task.Delay(100)
        }
        channel.Close()
    })
    
    // 소비자
    Task.Spawn(async func()
    {
        // 채널이 닫힐 때까지 수신
        await for value in channel
        {
            Log("Received: ", value)
            ProcessValue(value)
        }
    })
}

// Select 문 (여러 채널 동시 대기)
async func MultiChannelSelect()
{
    let ch1 = Channel<Int>()
    let ch2 = Channel<String>()
    let timeout = Timer.After(5000)  // 5초 타임아웃
    
    select
    {
        case value = <-ch1:
            Log("Got int: ", value)
            
        case msg = <-ch2:
            Log("Got string: ", msg)
            
        case <-timeout:
            Log("Timeout!")
            
        default:
            // 논블로킹 - 즉시 실행
            Log("No data available")
    }
}
```

## 4. 구조화된 동시성

```pergyra
// 태스크 그룹으로 구조화된 동시성 관리
async func ProcessItemsConcurrently(items: Array<Item>) -> Array<Result>
{
    return await withTaskGroup(of: Result.self)
    {
        group in
        
        // 각 아이템에 대해 태스크 생성
        for item in items
        {
            group.AddTask
            {
                await ProcessItem(item)
            }
        }
        
        // 모든 결과 수집
        var results = Array<Result>()
        for await result in group
        {
            results.Append(result)
        }
        
        return results
    }
}

// 취소 가능한 태스크
async func CancellableOperation(token: CancellationToken)
{
    for i in 0..1000
    {
        // 주기적으로 취소 확인
        if token.IsCancelled()
        {
            Log("Operation cancelled at ", i)
            return
        }
        
        await DoWork(i)
    }
}
```

## 5. 병렬 슬롯 연산

```pergyra
// 슬롯도 비동기/병렬 안전
actor SecureDataStore<T>
{
    private let _slots: Array<SecureSlot<T>>
    private let _tokens: Array<SecurityToken>
    
    public async func ParallelProcess(processor: async (T) -> T)
    {
        await Parallel
        {
            for i in 0.._slots.Length
            {
                let value = Read(_slots[i], _tokens[i])
                let processed = await processor(value)
                Write(_slots[i], processed, _tokens[i])
            }
        }
    }
}
```

## 6. Future/Promise 패턴

```pergyra
// Future는 미래의 값을 나타냄
func CreateFuture<T>() -> (Future<T>, Promise<T>)
{
    let promise = Promise<T>()
    let future = promise.GetFuture()
    return (future, promise)
}

// 사용 예
async func FutureExample()
{
    let (future, promise) = CreateFuture<Int>()
    
    // 다른 태스크에서 값 설정
    Task.Spawn(async func()
    {
        await Task.Delay(1000)
        promise.SetValue(42)
    })
    
    // 값 대기
    let value = await future.Get()
    Log("Got value: ", value)
}
```

## 7. 동시성 유틸리티

```pergyra
// 세마포어
let semaphore = Semaphore(permits: 3)

async func LimitedConcurrency()
{
    await semaphore.Acquire()
    defer { semaphore.Release() }
    
    // 최대 3개의 동시 실행
    await DoLimitedWork()
}

// 뮤텍스 (액터 대신 낮은 수준 제어가 필요할 때)
let mutex = Mutex<SharedData>()

async func CriticalSection()
{
    await mutex.Lock()
    defer { mutex.Unlock() }
    
    // 임계 구역
    ModifySharedData()
}

// 읽기/쓰기 락
let rwLock = ReadWriteLock<Config>()

async func ReadConfig() -> Config
{
    await rwLock.ReadLock()
    defer { rwLock.ReadUnlock() }
    
    return GetCurrentConfig()
}

async func UpdateConfig(newConfig: Config)
{
    await rwLock.WriteLock()
    defer { rwLock.WriteUnlock() }
    
    SetCurrentConfig(newConfig)
}
```

## 8. 비동기 스트림

```pergyra
// 비동기 이터레이터
async func* GenerateNumbers() -> AsyncIterator<Int>
{
    for i in 0..
    {
        await Task.Delay(100)
        yield i
        
        if i >= 100
        {
            break
        }
    }
}

// 사용
async func ConsumeStream()
{
    await for number in GenerateNumbers()
    {
        Log("Got: ", number)
        
        if number > 50
        {
            break  // 조기 종료 가능
        }
    }
}
```

## 9. 에러 처리와 동시성

```pergyra
// 여러 비동기 작업의 에러 처리
async func RobustConcurrentOperation() -> Result<Data, Error>
{
    // 첫 번째 성공 또는 모든 실패 대기
    let results = await Race
    {
        FetchFromPrimary(),
        FetchFromSecondary(),
        FetchFromCache()
    }
    
    // 적어도 하나가 성공했는지 확인
    for result in results
    {
        if case .Ok(data) = result
        {
            return .Ok(data)
        }
    }
    
    // 모두 실패
    return .Err(AllSourcesFailed)
}
```

## 10. 성능 최적화

```pergyra
// 작업 스케줄링 힌트
@[cpu_intensive]
async func HeavyComputation()
{
    // 컴파일러/런타임이 CPU 집약적 작업으로 인식
    await PerformComplexCalculation()
}

@[io_bound]
async func NetworkOperation()
{
    // I/O 바운드 작업으로 최적화
    await HttpClient.Get(url)
}

// 작업 우선순위
async func PriorityExample()
{
    await Task.WithPriority(.High)
    {
        await CriticalOperation()
    }
    
    await Task.WithPriority(.Background)
    {
        await BackgroundCleanup()
    }
}
```
