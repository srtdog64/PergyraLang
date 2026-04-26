# Async And Concurrency

Last updated: 2026-04-26

This document is the user-facing guide for the current beta async/concurrency
surface. The frozen contract is `docs/113_memory_concurrency_model.md`; the
design rationale is `docs/114_async_model_positioning.md`.

Pergyra does not treat `async` as the umbrella concept for every concurrent
operation. The model is decomposed:

- `parallel` expresses structured parallel execution.
- named `spawn Worker(args...)` creates a task and returns a checked future.
- `await` joins a future; it is not a lifetime or cancellation policy.
- `Channel<T>` / `select` express streaming transport.
- `Cancel` / `IsCancelled` express cooperative cancellation.
- `Result<T>` expresses fallible completion.
- `pin` / `ReadView<T>` / `WriteView<T>` express resource lifetime across
  suspension boundaries.

## 1. `parallel`

`parallel` is the core execution primitive. A `parallel` block joins before
control continues after the block.

```pergyra
func Main() -> Void {
    parallel {
        Log("left");
        Log("right");
    }
    Log("joined");
}
```

Semantic analysis rejects ownership-bearing conflicts across parallel tasks,
including `own`/`own`, `ref`/`own`, and `WriteView<T>` conflicts. Shared
copy-only reads and accepted `ref`/`ref` reads remain valid.

## 2. Named `spawn` And `Future<T>`

Named `spawn` is the stable beta task-producing surface:

```pergyra
func Work(x: Int) -> Int {
    return x + 1;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Work(10);
    let out: Int = await pending;
    Log(out);
}
```

Beta intentionally keeps the stable task-producing form named. Anonymous async
spawn bodies are parser-accepted in some places but semantically rejected for
beta because capture lifetime and cleanup summaries are not closed.

## 3. `async` / `await`

`async func` marks a coroutine/suspension-capable declaration. `await` joins a
future.

```pergyra
async func Worker() -> Int {
    return 7;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Worker();
    let value: Int = await pending;
    Log(value);
}
```

The important rule is that `await` only owns completion join. It does not own
resource lifetime, cancellation, error classification, or parallel structure.
Those are separate contracts.

Anonymous detached async blocks are implemented in the runtime path, but they
are not the beta-stable task creation model when they capture local state. Use
named `spawn Worker(args...)` for beta-stable task creation so parameter,
ownership, and cleanup facts have a declaration boundary.

Use named `spawn Worker(args...)` for beta-stable task creation.

## 4. `RemoteFuture<T>` And `Result<T>`

Local and remote futures have different join results:

- `Future<T> -> await -> T`
- `RemoteFuture<T> -> await -> Result<T>`

```pergyra
func FetchDevice() -> Void {
    let dev: DeviceSlot<Int> = ClaimDeviceSlot();
    DeviceWrite(dev, 11);

    let pending: RemoteFuture<Int> = SubmitDeviceRead(dev);
    let result: Result<Int> = await pending;
    let value: Int = Unwrap(result);

    ReleaseDeviceSlot(dev);
    Log(value);
}
```

Remote completion is fallible by contract. The failure is data, not an
implicit exception path hidden inside `await`.

## 5. `select` And `Channel<T>`

Channels are the stable streaming transport surface.

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

Beta channel rules:

- Blocking send/receive is the stable ownership-transfer path for named
  ownership-bearing payloads.
- Non-blocking and timeout receive are copy-only.
- `TrySend`, send-timeout, and status send helpers reject movable resources
  and authority-bearing tokens.
- Channel buffering/fairness beyond current FIFO/runtime behavior is not a
  beta promise unless covered by a named backend-compare fixture.

## 6. Cancellation

Cancellation is cooperative and explicit:

```pergyra
func Worker() -> Int {
    if (IsCancelled()) {
        return 9;
    }
    return 0;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Worker();
    let requested: Bool = Cancel(pending);
    Log(requested);
}
```

Rules:

- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` request cancellation.
- `IsCancelled()` observes cancellation inside the current task.
- Spawned async descendants inherit the cancellation chain in the current
  runtime model.
- Cancellation payloads are copy-only for beta.
- Preemptive cancellation and blocked-thread interruption are out-of-beta.

## 7. Pin/View Boundaries

Pinned views and slot views cannot cross suspension or task boundaries.

Rejected patterns include:

- returning a `ReadView<T>` / `WriteView<T>` from its valid scope,
- holding a view across `await`,
- sending a view through a channel,
- moving a token through `spawn`,
- acquiring conflicting views inside `parallel`.

These checks are the reason Pergyra preserves suspension visibility: the
compiler needs the boundary to reject futurelock-class bugs.

## 8. Stable vs Out Of Beta

Stable beta surface:

- `parallel`
- named `spawn`
- `async func`
- checked `await`
- `Future<T>`
- `RemoteFuture<T> -> await -> Result<T>`
- `Channel<T>`
- `select`
- copy-only non-blocking channel helpers
- `Cancel(task)` / `IsCancelled()`
- descendant cancellation propagation

Out of beta:

- anonymous async spawn capture/lifetime analysis,
- capture-bearing detached async block stability,
- `await for`,
- `TaskGroup`,
- user-visible cancellation token lattice,
- ownership-bearing non-blocking receive,
- ownership-bearing cancellation payload cleanup,
- preemptive cancellation,
- user-selectable memory ordering,
- scheduler fairness guarantees beyond tested fixtures.

## 9. Mental Model

Do not read Pergyra as "async without coloring." Read it as "colored
boundaries split by responsibility."

`async` and `await` are only one part of the execution family. Resource
lifetime belongs to pin/view rules, failure belongs to `Result<T>`,
streaming belongs to channels, cancellation belongs to `Cancel` and intent
compensation, and parallel structure belongs to `parallel` or intent step DAGs.
