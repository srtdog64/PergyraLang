# Pergyra 비동기/동시성 시스템 (현재 구현 기준)

이 문서는 현재 구현된 문법/시맨틱에 맞춘 **실제 동작 기준**이다.  
아래 예시는 현재 C/LLVM 공통 surface를 기준으로 정리했다.
전체 언어 기능 parity가 완전히 닫힌 것은 아니며, parity는 smoke/compare 대상에서 계속 확인한다.

## 1. async / await

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

## 2. spawn / Future

`spawn`은 `Future<T>`를 반환한다. `await`는 `Future<T>` → `T`를 꺼낸다.

```pergyra
func Work(x: Int) -> Int { return x + 1; }

async func Main() -> Void {
    let f: Future<Int> = spawn Work(10);
    let out: Int = await f;
    Log(out);
}
```

## 3. 채널

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

## 4. select

`select`는 현재 readiness 기반이며 `default`는 논블로킹 경로다.

```pergyra
select {
    case v = <-ch:
        Log(v);
    default:
        Log(0);
}
```

## 5. parallel

`parallel`은 실제 병렬 실행이며, 슬롯 충돌은 시맨틱 단계에서 검출한다.

```pergyra
let s: Slot<Int> = 0;
parallel {
    s = s + 1;
    s = s + 1; // write/write 충돌: 에러 또는 경고
}
```

## 6. 현재 지원 / 미지원

지원:
- `async func`, `await`, `spawn`
- `async { ... }` 블록
- `Channel<T>`, `select`, `parallel`
- `RemoteFuture<T>` → `Result<T>`

미지원 또는 비공식:
- `await for`, `TaskGroup`, `CancellationToken` 같은 구조화된 동시성 API
- OS 전용 I/O 스케줄러 강결합
