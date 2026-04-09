# Resource Scheduler Async Probe

`resource_scheduler_async_probe` is a strict-resource testdoc for async and
parallel scheduling.

It is not a UI/gameplay scenario. Its purpose is narrower:

- prove that multiple resources can be scheduled concurrently without losing
  resource discipline
- prove that helper-based `ref Slot<subject>` mutation is still caught by the
  parallel context analyzer
- exercise async runtime paths with multiple outstanding `DeviceSlot<Int>` and
  `RemoteFuture<Int>` values

## Scenario

The example models a tiny scheduler with:

- two work lanes (`Channel<Int>`)
- one shared budget (`Slot<BudgetLedger>`)
- two worker ledgers (`Slot<WorkerLedger>`)
- four claimed device slots and four remote futures

The program:

1. dispatches work into both channels with `parallel`
2. drains the channels and reserves worker/budget state through helper
   functions that accept `ref Slot<subject>`
3. launches four remote reads from four different device slots
4. awaits all results
5. finalizes worker state and writes a transcript report

## Features Exercised

- `Channel<Int>`
- `parallel`
- `async func`
- `await`
- `Slot<subject>`
- `ref Slot<subject>` helper mutation
- `DeviceSlot<Int>`
- `RemoteFuture<Int>`

## Fixes Driven By This Probe

### 1. Async function parameter parsing

`async func` parameters with `ref` / `own` modifiers were not parsed like
regular function parameters. This is now fixed in the parser.

### 2. Helper-based parallel context slot conflict detection

The parallel context analyzer originally detected only direct `Read/Write/Release`
inside each task. It missed helper calls such as:

```pergyra
Reserve(left, 3);
Reserve(left, 5);
```

when `Reserve` mutated the slot via a `ref Slot<WorkerLedger>` parameter.
The analyzer now chases top-level helper bodies conservatively and rejects this
pattern.

## Remaining Sharp Edges

- `spawn` with async functions that take `ref Slot<subject>` parameters still
  has a C transpiler wrapper mismatch on some paths.
- `SecureSlot<subject>` / view-style mutation across `await` boundaries still
  needs deeper runtime validation. This probe intentionally uses stable plain
  `Slot<subject>` paths instead.

## Regression Coverage

- exact stdout: [`examples/resource_scheduler_async_probe/expected_stdout.txt`](/mnt/e/PergyraLang/examples/resource_scheduler_async_probe/expected_stdout.txt)
- exact results: [`examples/resource_scheduler_async_probe/expected_results.txt`](/mnt/e/PergyraLang/examples/resource_scheduler_async_probe/expected_results.txt)
- example smoke:
  [`tests/example_contract_smoke.sh`](/mnt/e/PergyraLang/tests/example_contract_smoke.sh)
- failure smoke:
  [`tests/module_smoke.sh`](/mnt/e/PergyraLang/tests/module_smoke.sh)
