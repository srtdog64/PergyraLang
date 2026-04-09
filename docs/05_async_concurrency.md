# Pergyra 병렬 실행 시스템 (현재 구현 기준)

이 문서는 현재 구현된 문법/시맨틱에 맞춘 **실제 동작 기준**이다.  
아래 예시는 현재 C/LLVM 공통 surface를 기준으로 정리했다.
전체 언어 기능 parity가 완전히 닫힌 것은 아니며, parity는 smoke/compare 대상에서 계속 확인한다.

## 1. parallel

`parallel`은 Pergyra의 코어 실행 primitive다.

- 실제 병렬 실행을 뜻한다
- semantic 단계에서 slot/resource 충돌 검사를 요구한다
- backend/runtime lowering을 직접 가진다
- `spawn`, `async`, `await`, `select`는 이 병렬 실행 계층 아래의 표면이다

```pergyra
let s: Slot<Int> = 0;
parallel {
    s = s + 1;
    s = s + 1; // write/write 충돌: 에러 또는 경고
}
```

## 2. spawn / Future

`spawn`은 병렬 task를 생성하는 surface다. `Future<T>`를 반환하고, 결과는 `await`로 join한다.

```pergyra
func Work(x: Int) -> Int { return x + 1; }

async func Main() -> Void {
    let f: Future<Int> = spawn Work(10);
    let out: Int = await f;
    Log(out);
}
```

## 3. async / await

```pergyra
async func Fetch() -> Int {
    let ch: Channel<Int> = Channel(4);
    async { ch <- 11; }
    let value: Int = await spawn ReadOnce(ch);
    return value;
}

func ReadOnce(ch: Channel<Int>) -> Int {
    select {
        case v = <-ch:
            return v;
        default:
            return 0;
    }
}
```

### RemoteFuture와 Result

`RemoteFuture<T>`는 실패 가능성이 있으므로 `await` 결과가 `Result<T>`로 래핑된다.

```pergyra
async func FetchDevice() -> Void {
    let dev: DeviceSlot<Int> = ClaimDeviceSlot();
    DeviceWrite(dev, 11);
    let pending: RemoteFuture<Int> = SubmitDeviceRead(dev);
    let value: Int = (await pending)?;
    ReleaseDeviceSlot(dev);
    Log(value);
}
```

## 4. select / channel

```pergyra
func Main() -> Void {
    let ch: Channel<Int> = Channel(4);
    parallel {
        ch <- 7;
    }
    select {
        case v = <-ch:
            Log(v);
        default:
            Log(0);
    }
}
```

`select`는 현재 readiness 기반이며 `default`는 논블로킹 경로다.
여러 case가 동시에 준비되어 있으면 고정 우선순위 대신 **round-robin 시작 인덱스**로 검사해 앞 case starvation을 줄인다.

```pergyra
select {
    case v = <-ch:
        Log(v);
    default:
        Log(0);
}
```

채널 convenience built-in도 있다.

```pergyra
func Poll(ch: Channel<Int>) -> Void {
    let maybe: Option<Int> = TryRecv(ch);
    let timed: Option<Int> = RecvTimeout(ch, 1_000_000);
    let ok: Bool = TrySend(ch, 7);
    let sent: Bool = SendTimeout(ch, 9, 1_000_000);
    let tryStatus: Option<Bool> = TrySendStatus(ch, 7);
    let timeoutStatus: Option<Bool> = SendTimeoutStatus(ch, 9, 1_000_000);
    let len: Int = ChannelLength(ch);
    let cap: Int = ChannelCapacity(ch);
    let space: Int = ChannelSpace(ch);
    let full: Bool = ChannelFull(ch);
    let closed: Bool = ChannelClosed(ch);

    match maybe {
        case .Some(v):
            Log(v);
        case .None:
            Log(0);
    }
}
```

## 5. cancellation

현재 cancellation 표면은 cooperative/best-effort다.

```pergyra
func Worker() -> Int {
    if (IsCancelled()) {
        return 9;
    }
    return 0;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Worker();
    let cancelled: Bool = Cancel(pending);
    Log(cancelled);
}
```

- `Cancel(task)`는 `Future<T>` / `RemoteFuture<T>`에 취소 요청을 건다.
- `IsCancelled()`는 현재 실행 중 task 안에서 그 요청을 읽는다.
- spawned child task는 부모 task의 cancellation chain을 상속한다.
- 그래서 `Cancel(parent)` 이후 child가 `IsCancelled()`를 확인하면 `true`를 관측할 수 있다.
- 현재 모델은 cooperative다. task가 `IsCancelled()`를 확인하거나 자연스럽게 종료해야 실제 종료로 이어진다.

## 6. 계층 정리

현재 실행 계층은 다음 순서로 읽는다.

1. `parallel`
2. `spawn`
3. `async`
4. `await`
5. `select`
6. `channel`
7. `cancel`

의미:

- `parallel`은 core execution primitive
- `spawn`은 task-producing surface
- `async`는 suspension/coroutine surface
- `await`는 completion join surface
- `select`는 readiness arbitration surface

## 7. 현재 지원 / 미지원

지원:
- `parallel`
- `spawn`, `async func`, `await`
- `async { ... }` 블록
- `Channel<T>`, `select`
- `TryRecv/RecvTimeout -> Option<T>`
- `TrySend/SendTimeout -> Bool`
- `TrySendStatus/SendTimeoutStatus -> Option<Bool>`
- `ChannelLength/ChannelCapacity/ChannelSpace -> Int`
- `ChannelFull/ChannelClosed -> Bool`
- `Cancel(task)` / `IsCancelled()` cooperative cancellation
- spawned descendant에 대한 cancellation propagation
- `RemoteFuture<T>` → `Result<T>`

`TrySendStatus/SendTimeoutStatus`는 send 실패를 한 가지 `false`로 뭉개지 않고 값으로 분리한다.

- `Some(true)` = send 성공
- `Some(false)` = channel closed
- `None` = 아직 열려 있지만 full 또는 timeout

미지원 또는 비공식:
- `await for`, `TaskGroup`, `CancellationToken` 같은 구조화된 동시성 API
- OS 전용 I/O 스케줄러 강결합
- movable resource channel에 대한 non-blocking/timeout transfer surface
- preemptive cancellation, blocked thread task interruption, structured cancellation scope/lattice
